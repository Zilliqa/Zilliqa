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

Z_I64METRIC& GetCPSMetric() {
  static Z_I64METRIC counter{Z_FL::CPS, "cps.counter", "Calls into cps",
                             "calls"};
  return counter;
}

namespace libCps {
CpsRunEvm::CpsRunEvm(evm::EvmArgs protoArgs, CpsExecutor& executor,
                     CpsContext& ctx, CpsRun::Type type)
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
      INC_STATUS(GetCPSMetric(), "transaction", "create");
      const auto fromAddress = ProtoToAddress(mProtoArgs.origin());
      const auto contractAddress = mAccountStore.GetAddressForContract(
          fromAddress, TRANSACTION_VERSION_ETH);
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
        INC_STATUS(GetCPSMetric(), "error", "balance to low");
        return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
      }
    }
  }

  mAccountStore.AddAddressToUpdateBufferAtomic(
      ProtoToAddress(mProtoArgs.address()));

  const auto invokeResult = InvokeEvm();
  if (!invokeResult.has_value()) {
    // Timeout
    receipt.AddError(EXECUTE_CMD_TIMEOUT);
    INC_STATUS(GetCPSMetric(), "error", "timeout");
    return {};
  }
  const evm::EvmResult& evmResult = invokeResult.value();

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

  //  const Z_I64METRIC metrics{ Z_FL::ACCOUNTSTORE_EVM, "invocations_count",
  //  "Metrics for AccountStore",  "Blocks"};
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

  const auto validateResult =
      ValidateCallTrap(callData, result.remaining_gas());
  if (!validateResult.isSuccess) {
    return validateResult;
  }

  uint64_t remainingGas = result.remaining_gas();

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation
  if (ProtoToUint(callData.transfer().value()) > 0) {
    mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);
    remainingGas -= MIN_ETH_GAS;
  }

  // Set continuation (itself) to be resumed when create run is finished
  {
    evm::EvmArgs continuation;
    mProtoArgs.mutable_continuation()->set_feedback_type(
        evm::Continuation_Type_CALL);
    mProtoArgs.mutable_continuation()->set_id(result.continuation_id());
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_memory_offset() = callData.memory_offset();
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_offset_len() = callData.offset_len();
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
    const auto& ctx = callData.context();
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
    const auto value =
        Amount::fromWei(ProtoToUint(callData.transfer().value()));
    auto transferRun = std::make_shared<CpsRunTransfer>(
        mExecutor, mCpsContext, fromAccount, toAccount, value);
    mExecutor.PushRun(std::move(transferRun));
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunEvm::ValidateCallTrap(const evm::TrapData_Call& callData,
                                             uint64_t remainingGas) {
  const auto ctxDestAddr = ProtoToAddress(callData.context().destination());
  const auto ctxOriginAddr = ProtoToAddress(callData.context().caller());
  const auto isStatic = callData.is_static();

  const auto tnsfDestAddr = ProtoToAddress(callData.transfer().destination());
  const auto tnsfOriginAddr = ProtoToAddress(callData.transfer().source());
  const auto tnsfVal = ProtoToUint(callData.transfer().value());

  const auto calleeAddr = ProtoToAddress(callData.callee_address());

  if (IsNullAddress(calleeAddr)) {
    INC_STATUS(GetCPSMetric(), "error", "Invalid account");
    return {TxnStatus::INVALID_TO_ACCOUNT, false, {}};
  }

  if (mCpsContext.isStatic && !isStatic) {
    INC_STATUS(GetCPSMetric(), "error", "Incorect txn type");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }

  const bool areTnsfAddressesEmpty =
      IsNullAddress(tnsfDestAddr) && IsNullAddress(tnsfOriginAddr);
  const bool isValZero = (tnsfVal == 0);
  const bool isDelegate = (ctxOriginAddr == mCpsContext.origSender) &&
                          (ctxDestAddr == ProtoToAddress(mProtoArgs.address()));

  if (isStatic || isDelegate) {
    if (!areTnsfAddressesEmpty || !isValZero) {
      INC_STATUS(GetCPSMetric(), "error", "Incorrect txn type");
      return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
    }
  }
  const bool isOrigAddressValid =
      (ctxOriginAddr == mCpsContext.origSender) ||
      (ctxOriginAddr == ProtoToAddress(mProtoArgs.address()));
  const bool isDestAddressValid =
      (ctxDestAddr == calleeAddr) ||
      (ctxDestAddr == ProtoToAddress(mProtoArgs.address()));
  if (!isOrigAddressValid || !isDestAddressValid) {
    INC_STATUS(GetCPSMetric(), "error", "Incorrect txn type");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }

  if (!areTnsfAddressesEmpty) {
    if (tnsfDestAddr != ctxDestAddr || tnsfOriginAddr != ctxOriginAddr) {
      INC_STATUS(GetCPSMetric(), "error", "addressing ??");
      return {TxnStatus::ERROR, false, {}};
    }
    const auto currentBalance =
        mAccountStore.GetBalanceForAccountAtomic(tnsfOriginAddr);
    const auto requestedValue = Amount::fromWei(tnsfVal);
    if (requestedValue > currentBalance) {
      INC_STATUS(GetCPSMetric(), "error", "Insufficient balance");
      return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
    }

    if (remainingGas < MIN_ETH_GAS) {
      INC_STATUS(GetCPSMetric(), "error", "insuffiecient gas");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
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
    fromAddress = ProtoToAddress(mProtoArgs.address());
    contractAddress = ProtoToAddress(fixed.addres());
  }

  if (!mAccountStore.AddAccountAtomic(contractAddress)) {
    INC_STATUS(GetCPSMetric(), "error", "Account creation failed");
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);
  uint64_t remainingGas = result.remaining_gas();

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation
  if (ProtoToUint(createData.value()) > 0) {
    mProtoArgs.set_gas_limit(mProtoArgs.gas_limit() - MIN_ETH_GAS);
    remainingGas -= MIN_ETH_GAS;
  }

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
            : remainingGas;
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

  // Push transfer operation if needed
  {
    if (ProtoToUint(createData.value()) > 0) {
      // Push transfer to be executed first
      const auto value = Amount::fromWei(ProtoToUint(createData.value()));
      auto transferRun = std::make_shared<CpsRunTransfer>(
          mExecutor, mCpsContext, fromAddress, contractAddress, value);
      mExecutor.PushRun(std::move(transferRun));
    }
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunEvm::ValidateCreateTrap(
    const evm::TrapData_Create& createData, uint64_t remainingGas) {
  if (mCpsContext.isStatic) {
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }
  const evm::Address& protoCaller = createData.caller();
  const Address address = ProtoToAddress(protoCaller);

  Address fromAddress;
  const auto& scheme = createData.scheme();
  if (scheme.has_legacy()) {
    const evm::TrapData_Scheme_Legacy& legacy = scheme.legacy();
    fromAddress = ProtoToAddress(legacy.caller());
  } else if (scheme.has_create2()) {
    const evm::TrapData_Scheme_Create2& create2 = scheme.create2();
    fromAddress = ProtoToAddress(create2.caller());
  } else if (scheme.has_fixed()) {
    fromAddress = ProtoToAddress(mProtoArgs.address());
  }

  // Caller should be the same as the contract that triggered trap
  const auto currentAddress = ProtoToAddress(mProtoArgs.address());
  if (address != currentAddress || fromAddress != currentAddress) {
    INC_STATUS(GetCPSMetric(), "error", "Invalid from account");
    return {TxnStatus::INVALID_FROM_ACCOUNT, false, {}};
  }

  // Check Balance, Required Gas

  const auto currentBalance = mAccountStore.GetBalanceForAccountAtomic(address);
  const auto requestedValue = Amount::fromWei(ProtoToUint(createData.value()));
  if (requestedValue > currentBalance) {
    INC_STATUS(GetCPSMetric(), "error", "Insufficient balance");
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  const auto targetGas =
      createData.target_gas() != std::numeric_limits<uint64_t>::max()
          ? createData.target_gas()
          : remainingGas;

  auto baseFee = Eth::getGasUnitsForContractDeployment(
      {}, DataConversion::StringToCharArray(createData.call_data()));

  if (ProtoToUint(createData.value()) > 0) {
    baseFee += MIN_ETH_GAS;
  }
  if (targetGas < baseFee || targetGas > remainingGas) {
    INC_STATUS(GetCPSMetric(), "error", "Insufficient gas");
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
  const CpsRunEvm& prevRunEvm = static_cast<const CpsRunEvm&>(previousRun);
  mProtoArgs.set_gas_limit(results.evmResult.remaining_gas());

  if (mProtoArgs.continuation().feedback_type() ==
      evm::Continuation_Type_CREATE) {
    *mProtoArgs.mutable_continuation()->mutable_address() =
        prevRunEvm.mProtoArgs.address();
  } else {
    *mProtoArgs.mutable_continuation()->mutable_calldata()->mutable_data() =
        results.evmResult.return_value();
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