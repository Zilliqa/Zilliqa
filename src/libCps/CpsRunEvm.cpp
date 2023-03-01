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
#include "libCps/CpsMetrics.h"
#include "libCps/CpsRunTransfer.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/evm/EvmClient.h"
#include "libEth/utils/EthUtils.h"
#include "libMetrics/Api.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmUtils.h"

#include <boost/algorithm/hex.hpp>

#include <future>

namespace libCps {
CpsRunEvm::CpsRunEvm(evm::EvmArgs protoArgs, CpsExecutor& executor,
                     const CpsContext& ctx, CpsRun::Type type)
    : CpsRun(executor.GetAccStoreIface(), CpsRun::Domain::Evm, type),
      mProtoArgs(std::move(protoArgs)),
      mExecutor(executor),
      mCpsContext(ctx) {}

bool CpsRunEvm::IsResumable() const {
  return mProtoArgs.has_continuation() && mProtoArgs.continuation().id() > 0;
}

CpsExecuteResult CpsRunEvm::Run(TransactionReceipt& receipt) {
  const auto fromAddress = ProtoToAddress(mProtoArgs.origin());
  const auto contractAddress =
      mAccountStore.GetAddressForContract(fromAddress, TRANSACTION_VERSION_ETH);

  CREATE_SPAN(
      zil::trace::FilterClass::CPS_EVM, fromAddress.hex(),
      contractAddress.hex(), mCpsContext.origSender.hex(),
      ProtoToUint(mProtoArgs.apparent_value()).convert_to<std::string>());

  if (!IsResumable()) {
    // Contract deployment
    if (GetType() == CpsRun::Create) {
      INC_STATUS(GetCPSMetric(), "transaction", "create");
      mAccountStore.AddAccountAtomic(contractAddress);
      mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);
      *mProtoArgs.mutable_address() = AddressToProto(contractAddress);
      const auto baseFee = Eth::getGasUnitsForContractDeployment(
          {}, DataConversion::StringToCharArray(mProtoArgs.code()));
      mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - baseFee);
      if (!mAccountStore.TransferBalanceAtomic(
              ProtoToAddress(mProtoArgs.origin()),
              ProtoToAddress(mProtoArgs.address()),
              Amount::fromWei(ProtoToUint(mProtoArgs.apparent_value())))) {
        TRACE_ERROR("Insufficient Balance");
        return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
      }
      // Contract call (non-trap)
    } else if (GetType() == CpsRun::Call) {
      INC_STATUS(GetCPSMetric(), "transaction", "call");
      const auto code =
          mAccountStore.GetContractCode(ProtoToAddress(mProtoArgs.address()));
      *mProtoArgs.mutable_code() =
          DataConversion::CharArrayToString(StripEVM(code));
      mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);

      if (!mAccountStore.TransferBalanceAtomic(
              ProtoToAddress(mProtoArgs.origin()),
              ProtoToAddress(mProtoArgs.address()),
              Amount::fromWei(ProtoToUint(mProtoArgs.apparent_value())))) {
        INC_STATUS(GetCPSMetric(), "error", "balance too low");
        TRACE_ERROR("balance tool low");
        return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
      }
    }
  }

  mAccountStore.AddAddressToUpdateBufferAtomic(
      ProtoToAddress(mProtoArgs.address()));

  mProtoArgs.set_tx_trace_enabled(TX_TRACES);
  mProtoArgs.set_tx_trace(mExecutor.CurrentTrace());

  const auto invokeResult = InvokeEvm();

  if (!invokeResult.has_value()) {
    // Timeout
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    span.SetError("Evm-ds Invoke Error");
    INC_STATUS(GetCPSMetric(), "error", "timeout");
    return {};
  }
  const evm::EvmResult& evmResult = invokeResult.value();

  mExecutor.CurrentTrace() = evmResult.tx_trace();
  mProtoArgs.set_gas_limit(evmResult.remaining_gas());

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
    span.SetError("Unknown trap type");
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
      INC_STATUS(GetCPSMetric(), "error", "Rpc exception");
      LOG_GENERAL(WARNING, "Exception from underlying RPC call " << e.what());
    } catch (...) {
      INC_STATUS(GetCPSMetric(), "error",
                 "unhandled RPC exception underlying call");
      LOG_GENERAL(WARNING, "UnHandled Exception from underlying RPC call ");
    }
  };

  const auto fut = std::async(std::launch::async, worker);
  // check the future return and when time out log error.
  switch (fut.wait_for(std::chrono::seconds(EVM_RPC_TIMEOUT_SECONDS))) {
    case std::future_status::ready: {
      LOG_GENERAL(WARNING, "lock released normally");
      INC_STATUS(GetCPSMetric(), "unlock", "ok");
      return result;
    } break;
    case std::future_status::timeout: {
      LOG_GENERAL(WARNING, "Txn processing timeout!");
      if (LAUNCH_EVM_DAEMON) {
        EvmClient::GetInstance().Reset();
      }
      INC_STATUS(GetCPSMetric(), "unlock", "timeout");
      return std::nullopt;
    } break;
    case std::future_status::deferred: {
      LOG_GENERAL(WARNING, "Illegal future return status!");
      INC_STATUS(GetCPSMetric(), "unlock", "illegal");
      return std::nullopt;
    }
  }
  return {};
}

CpsExecuteResult CpsRunEvm::HandleTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  if (trap_data.has_create()) {
    return HandleCreateTrap(result);
  } else {
    return HandleCallTrap(result);
  }
}

CpsExecuteResult CpsRunEvm::HandleCallTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  const evm::TrapData_Call callData = trap_data.call();
  const auto& ctx = callData.context();

  CREATE_SPAN(
      zil::trace::FilterClass::CPS_EVM,
      ProtoToAddress(mProtoArgs.origin()).hex(),
      ProtoToAddress(ctx.destination()).hex(), mCpsContext.origSender.hex(),
      ProtoToUint(callData.transfer().value()).convert_to<std::string>());

  uint64_t remainingGas = result.remaining_gas();

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation
  const auto transferValue = ProtoToUint(callData.transfer().value());
  if (transferValue > 0) {
    if (remainingGas < MIN_ETH_GAS) {
      LOG_GENERAL(WARNING, "Insufficient gas in call-trap, remaining: "
                               << remainingGas
                               << ", required: " << MIN_ETH_GAS);
      TRACE_ERROR("Insufficient gas in call-trap");
      INC_STATUS(GetCPSMetric(), "error", "Insufficient gas in call-trap");
      span.SetError("Insufficient gas, given: " + std::to_string(remainingGas) +
                    ", required: " + std::to_string(MIN_ETH_GAS) +
                    " in call-trap");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
    mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);
    remainingGas -= MIN_ETH_GAS;
  }

  // Set continuation (itself) to be resumed when create run is finished
  {
    mProtoArgs.mutable_continuation()->set_feedback_type(
        evm::Continuation_Type_CALL);
    mProtoArgs.mutable_continuation()->set_id(result.continuation_id());
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_memory_offset() = callData.memory_offset();
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_offset_len() = callData.offset_len();
    *mProtoArgs.mutable_continuation()->mutable_logs() = result.logs();
    mExecutor.PushRun(shared_from_this());
  }

  {
    const auto targetGas =
        callData.target_gas() != std::numeric_limits<uint64_t>::max()
            ? callData.target_gas()
            : remainingGas;
    auto inputGas = std::min(targetGas, remainingGas);
    inputGas = std::max(remainingGas, inputGas);
    evm::EvmArgs evmCallArgs;
    *evmCallArgs.mutable_address() = ctx.destination();
    *evmCallArgs.mutable_origin() = ctx.caller();
    const auto code = mAccountStore.GetContractCode(
        ProtoToAddress(callData.callee_address()));
    *evmCallArgs.mutable_code() =
        DataConversion::CharArrayToString(StripEVM(code));
    *evmCallArgs.mutable_data() = callData.call_data();
    evmCallArgs.set_gas_limit(inputGas);
    *evmCallArgs.mutable_apparent_value() = ctx.apparent_value();
    evmCallArgs.set_estimate(mCpsContext.estimate);
    *evmCallArgs.mutable_context() = "TrapCall";
    *evmCallArgs.mutable_extras() = mCpsContext.evmExtras;
    evmCallArgs.set_enable_cps(ENABLE_CPS);
    auto callRun = std::make_unique<CpsRunEvm>(
        std::move(evmCallArgs), mExecutor, mCpsContext, CpsRun::TrapCall);
    mExecutor.PushRun(std::move(callRun));
  }

  // Push transfer to be executed first
  if (ProtoToUint(callData.transfer().value()) > 0) {
    const auto fromAccount = ProtoToAddress(callData.transfer().source());
    const auto toAccount = ProtoToAddress(callData.transfer().destination());

    if (fromAccount != mCpsContext.origSender &&
        fromAccount != ProtoToAddress(mProtoArgs.address())) {
      LOG_GENERAL(
          WARNING,
          "Source is incorrect for value transfer in call-trap, source addr: "
              << fromAccount.hex());
      INC_STATUS(GetCPSMetric(), "error",
                 "Source addr is incorrect for value transfer in call-trap");
      span.SetError("Addr(val: " + fromAccount.hex() +
                    ") is invalid for value transfer in call-trap");
      return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
    }

    const auto currentBalance =
        mAccountStore.GetBalanceForAccountAtomic(fromAccount);
    const auto requestedValue = Amount::fromWei(transferValue);
    if (requestedValue > currentBalance) {
      LOG_GENERAL(WARNING,
                  "From account has insufficient balance in call-trap");
      TRACE_ERROR("Insufficient balance");
      INC_STATUS(GetCPSMetric(), "error", "Insufficient balance in call-trap");
      span.SetError(
          "Insufficient balance, requested: " +
          requestedValue.toWei().convert_to<std::string>() + ", current: " +
          currentBalance.toWei().convert_to<std::string>() + " in call-trap");
      return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
    }

    const auto value =
        Amount::fromWei(ProtoToUint(callData.transfer().value()));
    auto transferRun = std::make_shared<CpsRunTransfer>(
        mExecutor, mCpsContext, evm::EvmResult{}, fromAccount, toAccount,
        value);
    mExecutor.PushRun(std::move(transferRun));
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunEvm::HandleCreateTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  const evm::TrapData_Create& createData = trap_data.create();

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
    fromAddress = ProtoToAddress(mProtoArgs.address());
    contractAddress = ProtoToAddress(fixed.addres());
  }

  const auto transferValue = ProtoToUint(createData.value());

  CREATE_SPAN(zil::trace::FilterClass::CPS_EVM,
              ProtoToAddress(mProtoArgs.origin()).hex(), contractAddress.hex(),
              mCpsContext.origSender.hex(),
              transferValue.convert_to<std::string>());

  if (!mAccountStore.AddAccountAtomic(contractAddress)) {
    INC_STATUS(GetCPSMetric(), "error", "Account creation failed");
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);
  uint64_t remainingGas = result.remaining_gas();

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation

  if (transferValue > 0) {
    if (remainingGas < MIN_ETH_GAS) {
      LOG_GENERAL(WARNING, "Insufficient gas in create-trap, remaining: "
                               << remainingGas
                               << ", required: " << MIN_ETH_GAS);
      TRACE_ERROR("Insufficient gas in create-trap");
      INC_STATUS(GetCPSMetric(), "error", "Insufficient gas in call-trap");
      span.SetError("Insufficient gas, given: " + std::to_string(remainingGas) +
                    ", required: " + std::to_string(MIN_ETH_GAS) +
                    " in create-trap");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
    mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);
    remainingGas -= MIN_ETH_GAS;
  }

  // Set continuation (itself) to be resumed when create run is finished
  {
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
            : remainingGas;
    const auto baseFee = Eth::getGasUnitsForContractDeployment(
        {}, DataConversion::StringToCharArray(createData.call_data()));

    if (baseFee > targetGas) {
      LOG_GENERAL(WARNING, "Insufficient gas in create-trap, fee: "
                               << baseFee << ", targetGas: " << targetGas);
      TRACE_ERROR("Insufficient target gas in create-trap");
      INC_STATUS(GetCPSMetric(), "error",
                 "Insufficient target gas in call-trap");
      span.SetError(
          "Insufficient target gas, given: " + std::to_string(targetGas) +
          ", required: " + std::to_string(baseFee) + " in create-trap");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
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

  // Push transfer operation if needed
  {
    if (transferValue > 0) {
      const auto currentAddress = ProtoToAddress(mProtoArgs.address());
      if (fromAddress != currentAddress &&
          fromAddress != mCpsContext.origSender) {
        LOG_GENERAL(WARNING,
                    "Incorrect from address in create-trap, fromAddress: "
                        << fromAddress.hex());
        INC_STATUS(GetCPSMetric(), "error",
                   "Invalid from account in create-trap");
        span.SetError("Invalid from account. fromAddress: " +
                      fromAddress.hex() + " in create-trap");
        return {TxnStatus::INVALID_FROM_ACCOUNT, false, {}};
      }
      // Check Balance
      const auto currentBalance =
          mAccountStore.GetBalanceForAccountAtomic(fromAddress);
      const auto requestedValue =
          Amount::fromWei(ProtoToUint(createData.value()));
      if (requestedValue > currentBalance) {
        LOG_GENERAL(WARNING, "Insufficient balance in create-trap");
        INC_STATUS(GetCPSMetric(), "error",
                   "Insufficient balance in create-trap");
        span.SetError(
            "Insufficient balance, requested: " +
            requestedValue.toWei().convert_to<std::string>() +
            ", current: " + currentBalance.toWei().convert_to<std::string>() +
            " in create-trap");
        return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
      }
      // Push transfer to be executed first
      const auto value = Amount::fromWei(ProtoToUint(createData.value()));
      auto transferRun = std::make_shared<CpsRunTransfer>(
          mExecutor, mCpsContext, evm::EvmResult{}, fromAddress,
          contractAddress, value);
      mExecutor.PushRun(std::move(transferRun));
    }
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}
void CpsRunEvm::HandleApply(const evm::EvmResult& result,
                            TransactionReceipt& receipt) {
  CREATE_SPAN(
      zil::trace::FilterClass::CPS_EVM,
      ProtoToAddress(mProtoArgs.origin()).hex(),
      ProtoToAddress(mProtoArgs.address()).hex(), mCpsContext.origSender.hex(),
      ProtoToUint(mProtoArgs.apparent_value()).convert_to<std::string>());

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

  Address thisContractAddress = ProtoToAddress(mProtoArgs.address());
  Address accountToRemove;
  Address fundsRecipient;
  Amount funds;

  // parse the return values from the call to evm.
  // we should expect no more that 2 apply instuctions (in case of selfdestruct:
  // fund recipiend and deleted account)
  for (const auto& it : result.apply()) {
    switch (it.apply_case()) {
      case evm::Apply::ApplyCase::kDelete:
        accountToRemove = ProtoToAddress(it.delete_().address());
        break;
      case evm::Apply::ApplyCase::kModify: {
        const auto iterAddress = ProtoToAddress(it.modify().address());
        // Get the account that this apply instruction applies to
        if (!mAccountStore.AccountExistsAtomic(thisContractAddress)) {
          mAccountStore.AddAccountAtomic(thisContractAddress);
        }

        // only allowed for thisContractAddress!
        if (it.modify().reset_storage() && iterAddress == thisContractAddress) {
          std::map<std::string, zbytes> states;
          std::vector<std::string> toDeletes;

          mAccountStore.FetchStateDataForContract(states, thisContractAddress,
                                                  "", {}, true);
          for (const auto& x : states) {
            toDeletes.emplace_back(x.first);
          }

          mAccountStore.UpdateStates(thisContractAddress, {}, toDeletes, true);
        }
        // Actually Update the state for the contract (only allowed for
        // thisContractAddress!)
        for (const auto& sit : it.modify().storage()) {
          if (iterAddress != thisContractAddress) {
            break;
          }
          LOG_GENERAL(INFO,
                      "Saving storage for Address: " << thisContractAddress);
          if (!mAccountStore.UpdateStateValue(
                  thisContractAddress,
                  DataConversion::StringToCharArray(sit.key()), 0,
                  DataConversion::StringToCharArray(sit.value()), 0)) {
          }
        }

        if (it.modify().has_balance()) {
          fundsRecipient = ProtoToAddress(it.modify().address());
          funds = Amount::fromQa(ProtoToUint(it.modify().balance()));
        }
        // Mark the Address as updated
        mAccountStore.AddAddressToUpdateBufferAtomic(thisContractAddress);
      } break;
      case evm::Apply::ApplyCase::APPLY_NOT_SET:
        // do nothing;
        break;
    }
  }

  // Allow only removal of self
  if (accountToRemove == thisContractAddress) {
    const auto currentFunds =
        mAccountStore.GetBalanceForAccountAtomic(accountToRemove);
    const auto zero = Amount::fromQa(0);
    if (funds > zero && funds <= currentFunds) {
      mAccountStore.TransferBalanceAtomic(accountToRemove, fundsRecipient,
                                          funds);
    } else if (funds > zero) {
      span.SetError("Possible zil mint. Funds in destroyed account: " +
                    currentFunds.toWei().convert_to<std::string>() +
                    ", requested: " + funds.toWei().convert_to<std::string>());
    }
    mAccountStore.SetBalanceAtomic(accountToRemove, zero);
    mAccountStore.AddAddressToUpdateBufferAtomic(accountToRemove);
    mAccountStore.AddAddressToUpdateBufferAtomic(fundsRecipient);
  }

  if (GetType() == CpsRun::Create || GetType() == CpsRun::TrapCreate) {
    InstallCode(ProtoToAddress(mProtoArgs.address()), result.return_value());
  }
}

bool CpsRunEvm::HasFeedback() const {
  return GetType() == CpsRun::TrapCall || GetType() == CpsRun::TrapCreate;
}

void CpsRunEvm::ProvideFeedback(const CpsRun& previousRun,
                                const CpsExecuteResult& results) {
  if (!previousRun.HasFeedback()) {
    return;
  }

  // For now only Evm is supported!
  if (std::holds_alternative<evm::EvmResult>(results.result)) {
    const auto& evmResult = std::get<evm::EvmResult>(results.result);
    mProtoArgs.set_gas_limit(evmResult.remaining_gas());
    *mProtoArgs.mutable_continuation()->mutable_logs() = evmResult.logs();

    if (previousRun.GetDomain() == CpsRun::Evm) {
      const CpsRunEvm& prevRunEvm = static_cast<const CpsRunEvm&>(previousRun);
      if (mProtoArgs.continuation().feedback_type() ==
          evm::Continuation_Type_CREATE) {
        *mProtoArgs.mutable_continuation()->mutable_address() =
            prevRunEvm.mProtoArgs.address();
      } else {
        *mProtoArgs.mutable_continuation()->mutable_calldata()->mutable_data() =
            evmResult.return_value();
      }
    }
  } else {
    // TODO: allow scilla runner to provide feedback too
  }
}

void CpsRunEvm::InstallCode(const Address& address, const std::string& code) {
  std::map<std::string, zbytes> t_newmetadata;

  t_newmetadata.emplace(mAccountStore.GenerateContractStorageKey(
                            address, CONTRACT_ADDR_INDICATOR, {}),
                        address.asBytes());
  mAccountStore.UpdateStates(address, t_newmetadata, {}, true);

  mAccountStore.SetImmutableAtomic(
      address, DataConversion::StringToCharArray("EVM" + code), {});
}

}  // namespace libCps
