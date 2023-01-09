/*
 * Copyright (C) 2022 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "libCps/CpsRunEvm.h"
#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsContext.h"
#include "libCps/CpsExecutor.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/EvmClient.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/Metrics.h"

#include <boost/algorithm/hex.hpp>

#include <future>

namespace libCps {
CpsRunEvm::CpsRunEvm(evm::EvmArgs protoArgs, CpsExecutor& executor,
                     const CpsContext& ctx, CpsRun::Type type)
    : CpsRun(executor.GetAccStoreIface(), type),
      mProtoArgs(std::move(protoArgs)),
      mExecutor(executor),
      mCpsContext(ctx) {}

bool CpsRunEvm::IsResumable() const {
  return mProtoArgs.has_continuation() && mProtoArgs.continuation().id() > 0;
}

CpsExecuteResult CpsRunEvm::Run(TransactionReceipt& receipt) {
  if (!IsResumable()) {
    // Contract deployment
    if (GetType() == CpsRun::Create) {
      const auto fromAddress = ProtoToAddress(mProtoArgs.origin());
      const auto contractAddress = mAccountStore.GetAddressForContract(
          fromAddress, TRANSACTION_VERSION_ETH);
      mAccountStore.AddAccountAtomic(contractAddress);
      LOG_GENERAL(WARNING,
                  "ACC HAS (1): "
                      << fromAddress.hex() << ", NONCEE: "
                      << mAccountStore.GetNonceForAccountAtomic(fromAddress));
      mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);
      LOG_GENERAL(WARNING,
                  "ACC HAS (2): "
                      << fromAddress.hex() << ", NONCEE: "
                      << mAccountStore.GetNonceForAccountAtomic(fromAddress));
      LOG_GENERAL(WARNING, "RUNNING WITH FIRST CONTRACT ADDR: "
                               << contractAddress.hex() << ", AND CODE SIZE: "
                               << mProtoArgs.code().size());
      *mProtoArgs.mutable_address() = AddressToProto(contractAddress);
      const auto baseFee = Eth::getGasUnitsForContractDeployment(
          {}, DataConversion::StringToCharArray(mProtoArgs.code()));
      mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - baseFee);
      LOG_GENERAL(WARNING, "New gas limit for CREATEType is: "
                               << mProtoArgs.gas_limit());
    } else if (GetType() == CpsRun::Call) {
      const auto code =
          mAccountStore.GetContractCode(ProtoToAddress(mProtoArgs.address()));
      *mProtoArgs.mutable_code() =
          DataConversion::CharArrayToString(StripEVM(code));
      mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);
      LOG_GENERAL(WARNING,
                  "New gas limit for CAllType is: " << mProtoArgs.gas_limit());
    }

    if (!mAccountStore.TransferBalanceAtomic(
            ProtoToAddress(mProtoArgs.origin()),
            ProtoToAddress(mProtoArgs.address()),
            Amount::fromWei(ProtoToUint(mProtoArgs.apparent_value())))) {
      return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
    }
  }

  mAccountStore.AddAddressToUpdateBufferAtomic(
      ProtoToAddress(mProtoArgs.address()));

  LOG_GENERAL(WARNING,
              "ACC HAS (3): " << ProtoToAddress(mProtoArgs.address()).hex()
                              << ", NONCEE: "
                              << mAccountStore.GetNonceForAccountAtomic(
                                     ProtoToAddress(mProtoArgs.address())));

  LOG_GENERAL(WARNING,
              "ACC HAS (4): " << ProtoToAddress(mProtoArgs.origin()).hex()
                              << ", NONCEE: "
                              << mAccountStore.GetNonceForAccountAtomic(
                                     ProtoToAddress(mProtoArgs.origin())));

  const auto invokeResult = InvokeEvm();
  if (!invokeResult.has_value()) {
    // Timeout
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    return {};
  }

  LOG_GENERAL(WARNING, "After invoke remaining gas: "
                           << invokeResult.value().remaining_gas());

  const evm::EvmResult& evmResult = invokeResult.value();
  LOG_GENERAL(WARNING, EvmUtils::ExitReasonString(evmResult.exit_reason()));

  const auto& exit_reason_case = evmResult.exit_reason().exit_reason_case();

  if (exit_reason_case == evm::ExitReason::ExitReasonCase::kTrap) {
    return HandleTrap(evmResult);
  } else if (exit_reason_case == evm::ExitReason::ExitReasonCase::kSucceed) {
    HandleApply(evmResult, receipt);
    return {TxnStatus::NOT_PRESENT, true, evmResult};
  } else {
    // Allow CPS to continune since caller may expect failures
    if (GetType() == CpsRun::TrapCall || GetType() == CpsRun::TrapCreate) {
      return {TxnStatus::NOT_PRESENT, true, evmResult};
    }
    return {TxnStatus::NOT_PRESENT, false, evmResult};
  }
}

std::optional<evm::EvmResult> CpsRunEvm::InvokeEvm() {
  evm::EvmResult result;
  const auto worker = [args = std::cref(mProtoArgs), &result]() -> void {
    try {
      EvmClient::GetInstance().CallRunner(EvmUtils::GetEvmCallJson(args),
                                          result);
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING, "Exception from underlying RPC call " << e.what());
    } catch (...) {
      LOG_GENERAL(WARNING, "UnHandled Exception from underlying RPC call ");
    }
  };

  const auto metrics = Metrics::GetInstance().CreateInt64Metric(
      "zilliqa_accountstroe", "invocations_count", "Metrics for AccountStore",
      "Blocks");
  const auto fut = std::async(std::launch::async, worker);
  // check the future return and when time out log error.
  switch (fut.wait_for(std::chrono::seconds(EVM_RPC_TIMEOUT_SECONDS))) {
    case std::future_status::ready: {
      LOG_GENERAL(WARNING, "lock released normally");
      if (zil::metrics::Filter::GetInstance().Enabled(
              zil::metrics::FilterClass::ACCOUNTSTORE_EVM)) {
        metrics->Add(1, {{"lock", "release-normal"}});
      }
      return result;
    } break;
    case std::future_status::timeout: {
      LOG_GENERAL(WARNING, "Txn processing timeout!");
      if (LAUNCH_EVM_DAEMON) {
        EvmClient::GetInstance().Reset();
      }
      metrics->Add(1, {{"lock", "release-timeout"}});
      return std::nullopt;
    } break;
    case std::future_status::deferred: {
      LOG_GENERAL(WARNING, "Illegal future return status!");
      metrics->Add(1, {{"lock", "release-deferred"}});
      return std::nullopt;
    }
  }
  return {};
}

CpsExecuteResult CpsRunEvm::HandleTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  if (trap_data.has_create()) {
    return HandleCreateTrap(result);
  }
  // Call trap
  else {
    return {TxnStatus::NOT_PRESENT, true, {}};
  }
}

CpsExecuteResult CpsRunEvm::HandleCreateTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  const evm::TrapData_Create& createData = trap_data.create();
  const auto validateResult =
      ValidateCreateTrap(createData, result.remaining_gas());
  if (!validateResult.isSuccess) {
    return validateResult;
  }

  Address contractAddress;
  Address fromAddress;

  const auto& scheme = createData.scheme();
  if (scheme.has_legacy()) {
    const evm::TrapData_Scheme_Legacy& legacy = scheme.legacy();
    fromAddress = ProtoToAddress(legacy.caller());
    contractAddress = mAccountStore.GetAddressForContract(
        fromAddress, TRANSACTION_VERSION_ETH);
  } else if (scheme.has_create2()) {
    const evm::TrapData_Scheme_Create2& create2 = scheme.create2();
    fromAddress = ProtoToAddress(create2.caller());
    contractAddress = ProtoToAddress(create2.create2_address());
  } else if (scheme.has_fixed()) {
    const evm::TrapData_Scheme_Fixed& fixed = scheme.fixed();
    fromAddress = ProtoToAddress(mProtoArgs.origin());
    contractAddress = ProtoToAddress(fixed.addres());
  }

  LOG_GENERAL(WARNING,
              "NONCEE FOR ACC "
                  << fromAddress.hex() << ", IS: "
                  << mAccountStore.GetNonceForAccountAtomic(fromAddress));

  if (!mAccountStore.AddAccountAtomic(contractAddress)) {
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  LOG_GENERAL(WARNING,
              "RUNNING WITH TRAP CONTRACT ADDR: " << contractAddress.hex());
  LOG_GENERAL(WARNING, "RUNNING WITH TRAP FROM ADDR: " << fromAddress.hex());

  mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);
  // mAccountStore.AddAddressToUpdateBufferAtomic(fromAddress);

  LOG_GENERAL(WARNING,
              "NONCEE FOR 2 ACC "
                  << fromAddress.hex() << ", IS: "
                  << mAccountStore.GetNonceForAccountAtomic(fromAddress));

  // InstallCode(contractAddress, createData.call_data());

  // Set continuation (itself) to be resumed when create run is finished
  {
    evm::EvmArgs continuation;
    mProtoArgs.mutable_continuation()->set_feedback_type(
        evm::Continuation_Type_CREATE);
    mProtoArgs.mutable_continuation()->set_id(result.continuation_id());
    mExecutor.PushRun(shared_from_this());
  }

  // Push create job to be run by EVM
  {
    const auto targetGas =
        createData.target_gas() != std::numeric_limits<uint64_t>::max()
            ? createData.target_gas()
            : result.remaining_gas();
    const auto baseFee = Eth::getGasUnitsForContractDeployment(
        {}, DataConversion::StringToCharArray(createData.call_data()));

    const auto inputGas = targetGas - baseFee;
    evm::EvmArgs evmCreateArgs;
    *evmCreateArgs.mutable_address() = AddressToProto(contractAddress);
    *evmCreateArgs.mutable_origin() = AddressToProto(fromAddress);
    *evmCreateArgs.mutable_code() = createData.call_data();
    evmCreateArgs.set_gas_limit(inputGas);
    *evmCreateArgs.mutable_apparent_value() = createData.value();
    evmCreateArgs.set_estimate(mCpsContext.estimate);
    *evmCreateArgs.mutable_context() = "TrapCreate";
    *evmCreateArgs.mutable_extras() = mCpsContext.evmExtras;
    evmCreateArgs.set_enable_cps(ENABLE_CPS);
    auto createRun = std::make_unique<CpsRunEvm>(
        std::move(evmCreateArgs), mExecutor, mCpsContext, CpsRun::TrapCreate);
    mExecutor.PushRun(std::move(createRun));
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunEvm::ValidateCreateTrap(
    const evm::TrapData_Create& createData, uint64_t remainingGas) {
  const evm::Address& protoCaller = createData.caller();
  const Address address = ProtoToAddress(protoCaller);
  // Check Balance, Required Gas

  const auto currentBalance = mAccountStore.GetBalanceForAccountAtomic(address);
  const auto requestedValue = Amount::fromWei(ProtoToUint(createData.value()));
  LOG_GENERAL(WARNING, "Requested balance[Wei]: "
                           << requestedValue.toWei().convert_to<std::string>()
                           << ", current[Wei]: "
                           << currentBalance.toWei().convert_to<std::string>());
  if (requestedValue > currentBalance) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  const auto targetGas =
      createData.target_gas() != std::numeric_limits<uint64_t>::max()
          ? createData.target_gas()
          : remainingGas;

  const auto baseFee = Eth::getGasUnitsForContractDeployment(
      {}, DataConversion::StringToCharArray(createData.call_data()));

  if (targetGas < baseFee || targetGas > remainingGas) {
    return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}

void CpsRunEvm::HandleApply(const evm::EvmResult& result,
                            TransactionReceipt& receipt) {
  if (result.logs_size() > 0) {
    Json::Value entry = Json::arrayValue;

    for (const auto& log : result.logs()) {
      Json::Value logJson;
      logJson["address"] = "0x" + ProtoToAddress(log.address()).hex();
      logJson["data"] = "0x" + boost::algorithm::hex(log.data());
      Json::Value topics_array = Json::arrayValue;
      for (const auto& topic : log.topics()) {
        topics_array.append("0x" + ProtoToH256(topic).hex());
      }
      logJson["topics"] = topics_array;
      entry.append(logJson);
    }
    receipt.AddJsonEntry(entry);
  }

  // parse the return values from the call to evm.
  for (const auto& it : result.apply()) {
    Address address;
    switch (it.apply_case()) {
      case evm::Apply::ApplyCase::kDelete:
        // Set account balance to 0 to avoid any leakage of funds in case
        // selfdestruct is called multiple times
        address = ProtoToAddress(it.delete_().address());
        if (!mAccountStore.AccountExistsAtomic(address)) {
          mAccountStore.AddAccountAtomic(address);
        }
        mAccountStore.SetBalanceAtomic(address, Amount::fromQa(0));
        mAccountStore.AddAddressToUpdateBufferAtomic(address);
        break;
      case evm::Apply::ApplyCase::kModify: {
        // Get the account that this apply instruction applies to
        address = ProtoToAddress(it.modify().address());
        if (!mAccountStore.AccountExistsAtomic(address)) {
          mAccountStore.AddAccountAtomic(address);
        }

        if (it.modify().reset_storage()) {
          LOG_GENERAL(WARNING,
                      "RESETING STORAGE FOR ADDRESS: " << address.hex());
          std::map<std::string, zbytes> states;
          std::vector<std::string> toDeletes;

          mAccountStore.FetchStateDataForContract(states, address, "", {},
                                                  true);
          for (const auto& x : states) {
            toDeletes.emplace_back(x.first);
          }

          if (!mAccountStore.UpdateStates(address, {}, toDeletes, true)) {
            LOG_GENERAL(
                WARNING,
                "Failed to update states hby setting indices for deletion "
                "for "
                    << address);
          }
        }

        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        /*const std::string& code = it.modify().code();
        if (!code.empty()) {
          LOG_GENERAL(INFO, "Saving code from apply: "
                                << address << ", code size: " << code.size());
          mAccountStore.SetImmutableAtomic(
              address, DataConversion::StringToCharArray("EVM" + code), {});
        }*/

        // Actually Update the state for the contract
        for (const auto& sit : it.modify().storage()) {
          LOG_GENERAL(INFO, "Saving storage for Address: " << address);
          if (!mAccountStore.UpdateStateValue(
                  address, DataConversion::StringToCharArray(sit.key()), 0,
                  DataConversion::StringToCharArray(sit.value()), 0)) {
            LOG_GENERAL(WARNING,
                        "Failed to update state value at address " << address);
          }
        }

        if (it.modify().has_balance()) {
          uint256_t balance = ProtoToUint(it.modify().balance());
          if (result.exit_reason().succeed() == evm::ExitReason::SUICIDED) {
            LOG_GENERAL(WARNING, "Balance to be applied for account: "
                                     << address.hex() << ", val: "
                                     << balance.convert_to<std::string>());
            mAccountStore.SetBalanceAtomic(address, Amount::fromQa(balance));
          }
        }
        if (it.modify().has_nonce()) {
          uint256_t nonce = ProtoToUint(it.modify().nonce());
          if ((nonce >> 64) > 0) {
            throw std::runtime_error("Nonce overflow!");
          }
          // account_store.SetNonceForAccountAtomic(address,
          // nonce.convert_to<uint64_t>());
        }
        // Mark the Address as updated
        mAccountStore.AddAddressToUpdateBufferAtomic(address);
      } break;
      case evm::Apply::ApplyCase::APPLY_NOT_SET:
        // do nothing;
        break;
    }
  }
  if (GetType() == CpsRun::Create || GetType() == CpsRun::TrapCreate) {
    InstallCode(ProtoToAddress(mProtoArgs.address()), result.return_value());
  }
}

void CpsRunEvm::ProvideFeedback(const CpsRunEvm& previousRun,
                                const evm::EvmResult& result) {
  LOG_GENERAL(WARNING,
              "PRovide feedback remaining gas: " << result.remaining_gas());
  mProtoArgs.set_gas_limit(result.remaining_gas());

  if (mProtoArgs.continuation().feedback_type() ==
      evm::Continuation_Type_CREATE) {
    *mProtoArgs.mutable_continuation()->mutable_address() =
        previousRun.mProtoArgs.address();
  } else {
    *mProtoArgs.mutable_continuation()->mutable_calldata() =
        result.return_value();
  }
}

void CpsRunEvm::InstallCode(const Address& address, const std::string& code) {
  std::map<std::string, zbytes> t_newmetadata;

  t_newmetadata.emplace(mAccountStore.GenerateContractStorageKey(address),
                        address.asBytes());
  mAccountStore.UpdateStates(address, t_newmetadata, {}, true);

  mAccountStore.SetImmutableAtomic(
      address, DataConversion::StringToCharArray("EVM" + code), {});
}

}  // namespace libCps