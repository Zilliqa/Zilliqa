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
                     const CpsContext& ctx)
    : mProtoArgs(std::move(protoArgs)), mExecutor(executor), mCpsContext(ctx) {}

bool CpsRunEvm::IsResumable() const {
  return mProtoArgs.has_continuation() && mProtoArgs.continuation().id() > 0;
}

CpsExecuteResult CpsRunEvm::Run(CpsAccountStoreInterface& accountStore,
                                TransactionReceipt& receipt) {
  if (zil::metrics::Filter::GetInstance().Enabled(
          zil::metrics::FilterClass::ACCOUNTSTORE_EVM)) {
    const auto metrics = Metrics::GetInstance().CreateInt64Metric(
        "zilliqa_accountstroe", "invocations_count", "Metrics for AccountStore",
        "Blocks");
    metrics->Add(1, {{"method", "EvmCallRunner"}});
  }

  if (!accountStore.TransferBalanceAtomic(
          ProtoToAddress(mProtoArgs.origin()),
          ProtoToAddress(mProtoArgs.address()),
          Amount::fromWei(ProtoToUint(mProtoArgs.apparent_value())))) {
    return {};
  }

  const auto invoke_result = InvokeEvm();
  if (!invoke_result.has_value()) {
    // Timeout
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    return {};
  }

  const evm::EvmResult& evm_result = invoke_result.value();
  LOG_GENERAL(WARNING, EvmUtils::ExitReasonString(evm_result.exit_reason()));

  const auto& exit_reason_case = evm_result.exit_reason().exit_reason_case();

  if (exit_reason_case == evm::ExitReason::ExitReasonCase::kTrap) {
    return HandleTrap(evm_result, accountStore);
  } else if (exit_reason_case == evm::ExitReason::ExitReasonCase::kSucceed) {
    HandleApply(evm_result, receipt, accountStore);
    return {TxnStatus::NOT_PRESENT, true, {}};
  }
  // Revert or Abort
  return {};
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

CpsExecuteResult CpsRunEvm::HandleTrap(const evm::EvmResult& result,
                                       CpsAccountStoreInterface& accountStore) {
  const evm::TrapData& trap_data = result.trap_data();
  if (trap_data.has_create()) {
    return HandleCreateTrap(result, accountStore);
  }
  // Call trap
  else {
    return {TxnStatus::NOT_PRESENT, true, {}};
  }
}

CpsExecuteResult CpsRunEvm::HandleCreateTrap(
    const evm::EvmResult& result, CpsAccountStoreInterface& accountStore) {
  const evm::TrapData& trap_data = result.trap_data();
  const evm::TrapData_Create& createData = trap_data.create();
  const auto validateResult =
      ValidateCreateTrap(createData, accountStore, result.remaining_gas());
  if (!validateResult.isSuccess) {
    return validateResult;
  }

  Address contractAddress;
  Address fromAddress;

  const auto& scheme = createData.scheme();
  if (scheme.has_legacy()) {
    const evm::TrapData_Scheme_Legacy& legacy = scheme.legacy();
    fromAddress = ProtoToAddress(legacy.caller());
    contractAddress = accountStore.GetAddressForContract(
        fromAddress, TRANSACTION_VERSION_ETH);
  } else if (scheme.has_create2()) {
    const evm::TrapData_Scheme_Create2& create2 = scheme.create2();
    fromAddress = ProtoToAddress(create2.caller());
    contractAddress = accountStore.GetAddressForContract(
        fromAddress, TRANSACTION_VERSION_ETH);
  }

  if (!accountStore.AddAccountAtomic(contractAddress)) {
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  if (!accountStore.TransferBalanceAtomic(
          fromAddress, contractAddress,
          Amount::fromWei(ProtoToUint(createData.value())))) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  const auto currentNonce = accountStore.GetNonceForAccountAtomic(fromAddress);
  accountStore.SetNonceForAccountAtomic(fromAddress, currentNonce + 1);

  // Set continuation to be resumed when create run is finished
  {
    evm::EvmArgs continuation;
    continuation.mutable_continuation()->set_feedback_type(
        evm::Continuation_Type_CREATE);
    continuation.mutable_continuation()->set_id(result.continuation_id());
    auto continuationRun = std::make_unique<CpsRunEvm>(std::move(continuation),
                                                       mExecutor, mCpsContext);
    mExecutor.PushRun(std::move(continuationRun));
  }

  // Push create job to be run by EVM
  {
    const auto targetGas =
        createData.target_gas() != std::numeric_limits<uint64_t>::max()
            ? createData.target_gas()
            : result.remaining_gas();
    evm::EvmArgs evmCreateArgs;
    *evmCreateArgs.mutable_address() = AddressToProto(contractAddress);
    *evmCreateArgs.mutable_origin() = AddressToProto(fromAddress);
    *evmCreateArgs.mutable_code() = createData.call_data();
    evmCreateArgs.set_gas_limit(targetGas);
    *evmCreateArgs.mutable_apparent_value() = createData.value();
    evmCreateArgs.set_estimate(mCpsContext.estimate);
    *evmCreateArgs.mutable_context() = "TrapCreate";
    *evmCreateArgs.mutable_extras() = mCpsContext.evmExtras;
    auto createRun = std::make_unique<CpsRunEvm>(std::move(evmCreateArgs),
                                                 mExecutor, mCpsContext);
    mExecutor.PushRun(std::move(createRun));
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunEvm::ValidateCreateTrap(
    const evm::TrapData_Create& createData,
    CpsAccountStoreInterface& accountStore, uint64_t remainingGas) {
  const evm::Address& protoCaller = createData.caller();
  const Address address = ProtoToAddress(protoCaller);
  // Check Balance, Required Gas

  const auto currentBalance = accountStore.GetBalanceForAccountAtomic(address);
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
                            TransactionReceipt& receipt,
                            CpsAccountStoreInterface& account_store) {
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
        if (!account_store.AccountExistsAtomic(address)) {
          account_store.AddAccountAtomic(address);
        }
        account_store.SetBalanceAtomic(address, Amount::fromQa(0));
        account_store.AddAddressToUpdateBufferAtomic(address);
        break;
      case evm::Apply::ApplyCase::kModify: {
        // Get the account that this apply instruction applies to
        address = ProtoToAddress(it.modify().address());
        if (!account_store.AccountExistsAtomic(address)) {
          account_store.AddAccountAtomic(address);
        }

        if (it.modify().reset_storage()) {
          std::map<std::string, zbytes> states;
          std::vector<std::string> toDeletes;

          account_store.FetchStateDataForContract(states, address, "", {},
                                                  true);
          for (const auto& x : states) {
            toDeletes.emplace_back(x.first);
          }

          if (!account_store.UpdateStates(address, {}, toDeletes, true)) {
            LOG_GENERAL(
                WARNING,
                "Failed to update states hby setting indices for deletion "
                "for "
                    << address);
          }
        }

        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        const std::string& code = it.modify().code();
        if (!code.empty()) {
          account_store.SetImmutableAtoimic(
              address, DataConversion::StringToCharArray("EVM" + code), {});
        }

        // Actually Update the state for the contract
        for (const auto& sit : it.modify().storage()) {
          LOG_GENERAL(INFO, "Saving storage for Address: " << address);
          if (!account_store.UpdateStateValue(
                  address, DataConversion::StringToCharArray(sit.key()), 0,
                  DataConversion::StringToCharArray(sit.value()), 0)) {
            LOG_GENERAL(WARNING,
                        "Failed to update state value at address " << address);
          }
        }

        if (it.modify().has_balance()) {
          uint256_t balance = ProtoToUint(it.modify().balance());
          if ((balance >> 128) > 0) {
            throw std::runtime_error("Balance overflow!");
          }
          account_store.SetBalanceAtomic(
              address, Amount::fromQa(balance.convert_to<uint128_t>()));
        }
        if (it.modify().has_nonce()) {
          uint256_t nonce = ProtoToUint(it.modify().nonce());
          if ((nonce >> 64) > 0) {
            throw std::runtime_error("Nonce overflow!");
          }
          account_store.SetNonceForAccountAtomic(address,
                                                 nonce.convert_to<uint64_t>());
        }
        // Mark the Address as updated
        account_store.AddAddressToUpdateBufferAtomic(address);
      } break;
      case evm::Apply::ApplyCase::APPLY_NOT_SET:
        // do nothing;
        break;
    }
  }
}

void CpsRunEvm::ProvideFeedback(const CpsRunEvm& previousRun,
                                const evm::EvmResult& result) {
  if (mProtoArgs.continuation().feedback_type() ==
      evm::Continuation_Type_CREATE) {
    *mProtoArgs.mutable_continuation()->mutable_address() =
        previousRun.mProtoArgs.address();
  } else {
    *mProtoArgs.mutable_continuation()->mutable_calldata() =
        result.return_value();
  }
}

}  // namespace libCps