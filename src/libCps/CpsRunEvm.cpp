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
#include "libCps/CpsRunScilla.h"
#include "libCps/CpsRunTransfer.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/evm/EvmClient.h"
#include "libEth/utils/EthUtils.h"
#include "libMetrics/Api.h"
#include "libMetrics/Tracing.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"
#include "libUtils/JsonUtils.h"

#include <boost/algorithm/hex.hpp>

#include <future>

namespace libCps {
CpsRunEvm::CpsRunEvm(evm::EvmArgs protoArgs, CpsExecutor& executor,
                     CpsContext& ctx, CpsRun::Type type)
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
      zil::trace::FilterClass::TXN, fromAddress.hex(), contractAddress.hex(),
      mCpsContext.origSender.hex(),
      ProtoToUint(mProtoArgs.apparent_value()).convert_to<std::string>());

  if (!IsResumable()) {
    // Contract deployment
    if (GetType() == CpsRun::Create) {
      INC_STATUS(GetCPSMetric(), "transaction", "create");
      mAccountStore.AddAccountAtomic(contractAddress);
      *mProtoArgs.mutable_address() = AddressToProto(contractAddress);
      const auto baseFee = Eth::getGasUnitsForContractDeployment(
          {}, DataConversion::StringToCharArray(mProtoArgs.code()));
      mCpsContext.gasTracker.DecreaseByEth(baseFee);

      if (!mAccountStore.TransferBalanceAtomic(
              ProtoToAddress(mProtoArgs.origin()),
              ProtoToAddress(mProtoArgs.address()),
              Amount::fromWei(ProtoToUint(mProtoArgs.apparent_value())))) {
        TRACE_ERROR("Insufficient Balance");
        return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
      }
      if (!BlockStorage::GetBlockStorage().PutContractCreator(
              contractAddress, mCpsContext.scillaExtras.txnHash)) {
        LOG_GENERAL(WARNING, "Failed to save contract creator");
      }
      // Contract call (non-trap)
    } else if (GetType() == CpsRun::Call) {
      INC_STATUS(GetCPSMetric(), "transaction", "call");
      const auto code =
          mAccountStore.GetContractCode(ProtoToAddress(mProtoArgs.address()));
      *mProtoArgs.mutable_code() =
          DataConversion::CharArrayToString(StripEVM(code));
      mCpsContext.gasTracker.DecreaseByEth(MIN_ETH_GAS);

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

  mProtoArgs.set_gas_limit(mCpsContext.gasTracker.GetEthGas());

  mAccountStore.AddAddressToUpdateBufferAtomic(
      ProtoToAddress(mProtoArgs.address()));

  mProtoArgs.set_tx_trace_enabled(TX_TRACES);
  mProtoArgs.set_tx_trace(mExecutor.CurrentTrace());

  LOG_GENERAL(INFO, "Running EVM with gasLimit: " << mProtoArgs.gas_limit());

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
  mCpsContext.gasTracker.DecreaseByEth(mProtoArgs.gas_limit() -
                                       evmResult.remaining_gas());

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
  using namespace zil::trace;

  evm::EvmResult result;
  const auto worker = [args = std::cref(mProtoArgs), &result,
                       trace_info =
                           Tracing::GetActiveSpan().GetIds()]() -> void {
    try {
      auto span = Tracing::CreateChildSpanOfRemoteTrace(
          FilterClass::FILTER_CLASS_ALL, "InvokeEvm", trace_info);
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
    const bool is_precompile = trap_data.call().is_precompile();
    if (is_precompile) {
      return HandlePrecompileTrap(result);
    }
    return HandleCallTrap(result);
  }
}

CpsExecuteResult CpsRunEvm::HandleCallTrap(const evm::EvmResult& result) {
  const evm::TrapData& trap_data = result.trap_data();
  const evm::TrapData_Call callData = trap_data.call();
  const auto& ctx = callData.context();

  CREATE_SPAN(
      zil::trace::FilterClass::TXN, ProtoToAddress(mProtoArgs.origin()).hex(),
      ProtoToAddress(ctx.destination()).hex(), mCpsContext.origSender.hex(),
      ProtoToUint(callData.transfer().value()).convert_to<std::string>());

  Address thisContractAddress = ProtoToAddress(mProtoArgs.address());
  Address fundsRecipient;
  Amount funds;

  // Apply the evm state changes made so far so subsequent contract calls
  // can see the changes (delegatecall)
  for (const auto& it : result.apply()) {
    switch (it.apply_case()) {
      case evm::Apply::ApplyCase::kDelete:
        break;
      case evm::Apply::ApplyCase::kModify: {
        const auto iterAddress = ProtoToAddress(it.modify().address());
        // Get the account that this apply instruction applies to
        if (!mAccountStore.AccountExistsAtomic(thisContractAddress)) {
          mAccountStore.AddAccountAtomic(thisContractAddress);
        }

        // only allowed for thisContractAddress in non-static context!
        if (it.modify().reset_storage() && iterAddress == thisContractAddress &&
            !mProtoArgs.is_static_call()) {
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
        // thisContractAddress in non-static context!)
        for (const auto& sit : it.modify().storage()) {
          if (iterAddress != thisContractAddress ||
              mProtoArgs.is_static_call()) {
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

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation
  const auto targetGas =
      (callData.target_gas() != std::numeric_limits<uint64_t>::max() &&
       callData.target_gas() != 0)
          ? callData.target_gas()
          : mCpsContext.gasTracker.GetEthGas();
  auto inputGas = std::min(targetGas, mCpsContext.gasTracker.GetEthGas());
  const auto transferValue = ProtoToUint(callData.transfer().value());
  const bool isStatic = callData.is_static();

  // Don't allow for non-static calls when its parent is already static
  if (mProtoArgs.is_static_call() && !isStatic) {
    INC_STATUS(GetCPSMetric(), "error",
               "Context change from static to non-static");
    TRACE_ERROR(
        "Attempt to change context from static to non-static in call-trap");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }

  span.SetAttribute("IsStatic", isStatic);

  if (!isStatic && transferValue > 0) {
    if (inputGas < MIN_ETH_GAS) {
      LOG_GENERAL(WARNING, "Insufficient gas in call-trap, remaining: "
                               << inputGas << ", required: " << MIN_ETH_GAS);
      TRACE_ERROR("Insufficient gas in call-trap");
      INC_STATUS(GetCPSMetric(), "error", "Insufficient gas in call-trap");
      span.SetError("Insufficient gas, given: " + std::to_string(inputGas) +
                    ", required: " + std::to_string(MIN_ETH_GAS) +
                    " in call-trap");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
    mCpsContext.gasTracker.DecreaseByEth(MIN_ETH_GAS);
    inputGas -= MIN_ETH_GAS;
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
    evm::EvmArgs evmCallArgs;
    *evmCallArgs.mutable_address() = ctx.destination();
    *evmCallArgs.mutable_origin() = mProtoArgs.origin();
    *evmCallArgs.mutable_caller() = ctx.caller();
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

    evmCallArgs.set_is_static_call(isStatic);
    auto callRun = std::make_unique<CpsRunEvm>(
        std::move(evmCallArgs), mExecutor, mCpsContext, CpsRun::TrapCall);
    mExecutor.PushRun(std::move(callRun));
  }

  // Push transfer to be executed first
  if (!isStatic && transferValue > 0) {
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

CpsExecuteResult CpsRunEvm::HandlePrecompileTrap(
    const evm::EvmResult& evm_result) {
  const evm::TrapData& trap_data = evm_result.trap_data();
  const evm::TrapData_Call callData = trap_data.call();

  const auto strJsonData = callData.call_data();
  Json::Value jsonData;
  if (!JSONUtils::GetInstance().convertStrtoJson(strJsonData, jsonData)) {
    LOG_GENERAL(
        WARNING,
        "Error with convert precompile call_data string to json object");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }

  const auto sender = (jsonData["keep_origin"].isBool() &&
                       jsonData["keep_origin"].asBool() == true)
                          ? ProtoToAddress(mProtoArgs.caller()).hex()
                          : ProtoToAddress(mProtoArgs.address()).hex();

  jsonData.removeMember("keep_origin");

  jsonData["_origin"] = "0x" + mCpsContext.origSender.hex();
  jsonData["_sender"] = "0x" + sender;
  jsonData["_amount"] = "0";

  CREATE_SPAN(
      zil::trace::FilterClass::TXN, ProtoToAddress(mProtoArgs.origin()).hex(),
      jsonData["_address"].asString(), mCpsContext.origSender.hex(),
      ProtoToUint(callData.transfer().value()).convert_to<std::string>());

  mCpsContext.gasTracker.DecreaseByCore(SCILLA_RUNNER_INVOKE_GAS +
                                        CONTRACT_INVOKE_GAS);
  const bool isStatic = callData.is_static();

  // Don't allow for non-static calls when its parent is already static
  if (mProtoArgs.is_static_call() && !isStatic) {
    INC_STATUS(GetCPSMetric(), "error",
               "Context change from static to non-static");
    TRACE_ERROR(
        "Atempt to change context from static to non-static in "
        "precompile-trap");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }
  span.SetAttribute("IsStatic", isStatic);
  span.SetAttribute("IsPrecompile", true);

  // Set continuation (itself) to be resumed when create run is finished
  {
    mProtoArgs.mutable_continuation()->set_feedback_type(
        evm::Continuation_Type_CALL);
    mProtoArgs.mutable_continuation()->set_id(evm_result.continuation_id());
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_memory_offset() = callData.memory_offset();
    *mProtoArgs.mutable_continuation()
         ->mutable_calldata()
         ->mutable_offset_len() = callData.offset_len();
    *mProtoArgs.mutable_continuation()->mutable_logs() = evm_result.logs();
    mExecutor.PushRun(shared_from_this());
  }

  const auto destAddress = jsonData["_address"].asString();

  ScillaArgs scillaArgs = {
      .from = ProtoToAddress(mProtoArgs.address()),
      .dest = Address{destAddress},
      .origin = mCpsContext.origSender,
      .value = Amount{},
      .calldata = jsonData,
      .edge = 0,
      .depth = 0,
      .extras = ScillaArgExtras{.scillaReceiverAddress = Address{}}};

  auto nextRun = std::make_shared<CpsRunScilla>(
      std::move(scillaArgs), mExecutor, mCpsContext, CpsRun::TrapScillaCall);

  mExecutor.PushRun(nextRun);
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

  CREATE_SPAN(zil::trace::FilterClass::TXN,
              ProtoToAddress(mProtoArgs.origin()).hex(), contractAddress.hex(),
              mCpsContext.origSender.hex(),
              transferValue.convert_to<std::string>());

  if (mProtoArgs.is_static_call()) {
    INC_STATUS(GetCPSMetric(), "error",
               "Account creation cannot be created in static call");
    TRACE_ERROR("Account creation attempt by static call in create-trap");
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }

  if (!mAccountStore.AddAccountAtomic(contractAddress)) {
    INC_STATUS(GetCPSMetric(), "error", "Account creation failed");
    TRACE_ERROR("Account creation failed gas in create-trap");
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  mAccountStore.IncreaseNonceForAccountAtomic(fromAddress);

  // Adjust remainingGas and recalculate gas for resume operation
  // Charge MIN_ETH_GAS for transfer operation
  const uint64_t targetGas =
      (createData.target_gas() != std::numeric_limits<uint64_t>::max() &&
       createData.target_gas() != 0)
          ? createData.target_gas()
          : mCpsContext.gasTracker.GetEthGas();
  auto inputGas = std::min(createData.target_gas(), targetGas);

  if (transferValue > 0) {
    if (inputGas < MIN_ETH_GAS) {
      LOG_GENERAL(WARNING, "Insufficient gas in create-trap, remaining: "
                               << inputGas << ", required: " << MIN_ETH_GAS);
      TRACE_ERROR("Insufficient gas in create-trap");
      INC_STATUS(GetCPSMetric(), "error", "Insufficient gas in call-trap");
      span.SetError("Insufficient gas, given: " + std::to_string(inputGas) +
                    ", required: " + std::to_string(MIN_ETH_GAS) +
                    " in create-trap");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
    inputGas -= MIN_ETH_GAS;
    mCpsContext.gasTracker.DecreaseByEth(MIN_ETH_GAS);
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
    const uint64_t baseFee = Eth::getGasUnitsForContractDeployment(
        {}, DataConversion::StringToCharArray(createData.call_data()));

    if (baseFee > inputGas) {
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
    inputGas -= targetGas - baseFee;
    mCpsContext.gasTracker.DecreaseByEth(baseFee);
    evm::EvmArgs evmCreateArgs;
    *evmCreateArgs.mutable_address() = AddressToProto(contractAddress);
    *evmCreateArgs.mutable_origin() = mProtoArgs.origin();
    *evmCreateArgs.mutable_caller() = AddressToProto(fromAddress);
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
      zil::trace::FilterClass::TXN, ProtoToAddress(mProtoArgs.origin()).hex(),
      ProtoToAddress(mProtoArgs.address()).hex(), mCpsContext.origSender.hex(),
      ProtoToUint(mProtoArgs.apparent_value()).convert_to<std::string>());

  if (result.logs_size() > 0) {
    for (const auto& log : result.logs()) {
      Json::Value logJson;
      logJson["address"] = "0x" + ProtoToAddress(log.address()).hex();
      logJson["data"] = "0x" + boost::algorithm::hex(log.data());
      Json::Value topics_array = Json::arrayValue;
      for (const auto& topic : log.topics()) {
        topics_array.append("0x" + ProtoToH256(topic).hex());
      }
      logJson["topics"] = topics_array;
      receipt.AppendJsonEntry(logJson);
    }
  }

  Address thisContractAddress = ProtoToAddress(mProtoArgs.address());
  Address accountToRemove;
  Address fundsRecipient;
  Amount funds;

  // parse the return values from the call to evm.
  // we should expect no more that 2 apply instuctions (in case of selfdestruct:
  // fund recipient and deleted account)
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

        // only allowed for thisContractAddress in non-static context!
        if (it.modify().reset_storage() && iterAddress == thisContractAddress &&
            !mProtoArgs.is_static_call()) {
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
        // thisContractAddress in non-static context!)
        for (const auto& sit : it.modify().storage()) {
          if (iterAddress != thisContractAddress ||
              mProtoArgs.is_static_call()) {
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

  // Allow only removal of self in non-static calls
  if (accountToRemove == thisContractAddress && !mProtoArgs.is_static_call()) {
    const auto currentContractFunds =
        mAccountStore.GetBalanceForAccountAtomic(accountToRemove);

    // Funds for recipient
    const auto recipientPreFunds =
        mAccountStore.GetBalanceForAccountAtomic(fundsRecipient);

    const auto zero = Amount::fromQa(0);

    // Funds is what we want our contract to become/be modified to.
    // Check that the contract funds plus the current funds in our account
    // is equal to this value
    if (funds != recipientPreFunds + currentContractFunds) {
      std::string error =
          "Possible zil mint. Funds in destroyed account: " +
          currentContractFunds.toWei().convert_to<std::string>() +
          ", requested: " +
          (funds - recipientPreFunds).toWei().convert_to<std::string>();

      LOG_GENERAL(WARNING, "ERROR IN DESTUCT! " << error);
      span.SetError(error);
    }

    mAccountStore.TransferBalanceAtomic(accountToRemove, fundsRecipient,
                                        currentContractFunds);
    mAccountStore.SetBalanceAtomic(accountToRemove, zero);
    mAccountStore.AddAddressToUpdateBufferAtomic(accountToRemove);
    mAccountStore.AddAddressToUpdateBufferAtomic(fundsRecipient);
  }

  if (GetType() == CpsRun::Create || GetType() == CpsRun::TrapCreate) {
    InstallCode(ProtoToAddress(mProtoArgs.address()), result.return_value());
  }
}

bool CpsRunEvm::ProbeERC165Interface(CpsAccountStoreInterface& accStore,
                                     CpsContext& ctx, const Address& caller,
                                     const Address& destinationAddress) {
  constexpr auto ERC165METHOD =
      "0x01ffc9a701ffc9a7000000000000000000000000000000000000000000000000000000"
      "00";

  // Check if destination is ERC-165 compatible
  evm::EvmArgs args;
  *args.mutable_address() = AddressToProto(destinationAddress);
  const auto code = accStore.GetContractCode(destinationAddress);
  *args.mutable_code() = DataConversion::CharArrayToString(StripEVM(code));

  *args.mutable_data() = DataConversion::CharArrayToString(
      DataConversion::HexStrToUint8VecRet(ERC165METHOD));
  *args.mutable_caller() = AddressToProto(caller);
  *args.mutable_origin() = AddressToProto(ctx.origSender);
  // Set gas limit as per EIP-165
  args.set_gas_limit(30000);
  args.set_estimate(false);
  *args.mutable_context() = "ScillaCall";
  *args.mutable_extras() = ctx.evmExtras;
  args.set_enable_cps(ENABLE_CPS);

  TransactionReceipt unusedReceipt;
  CpsExecutor unusedExecutor{accStore, unusedReceipt};

  {
    CpsRunEvm evmRun{args, unusedExecutor, ctx, CpsRun::Type::Call};

    const auto result = evmRun.InvokeEvm();
    if (!result.has_value()) {
      return false;
    }

    const evm::EvmResult& evmResult = result.value();

    // Bool encoded return value = expect last digit to be 1
    if (std::empty(evmResult.return_value()) ||
        static_cast<uint8_t>(evmResult.return_value().back()) != 0x01) {
      return false;
    }
  }

  {
    // Second probe - with different calldata (see EIP-165)
    constexpr auto ERC165INVALID =
        "0x01ffc9a7ffffffff0000000000000000000000000000000000000000000000000000"
        "0000";
    *args.mutable_data() = DataConversion::CharArrayToString(
        DataConversion::HexStrToUint8VecRet(ERC165INVALID));
    CpsRunEvm evmRun{args, unusedExecutor, ctx, CpsRun::Type::Call};

    const auto result = evmRun.InvokeEvm();

    if (!result.has_value()) {
      return false;
    }

    const evm::EvmResult& evmResult = result.value();

    if (!std::empty(evmResult.return_value()) &&
        static_cast<uint8_t>(evmResult.return_value().back()) == 0x01) {
      return false;
    }
  }
  // Eventually check support for scilla interface in evm contract
  {
    // Check if destination supports 'function
    // handle_scilla_message(string,bytes)'
    // it's a 0x01ffc9a7 (ERC-165) +
    // bytes4(keccak(hadle_scilla_message(string,bytes))
    constexpr auto SUPPORT_SCILLA_IFACE =
        "0x01ffc9a742ede2780000000000000000000000000000000000000000000000000000"
        "0000";
    *args.mutable_data() = DataConversion::CharArrayToString(
        DataConversion::HexStrToUint8VecRet(SUPPORT_SCILLA_IFACE));

    CpsRunEvm evmRun{args, unusedExecutor, ctx, CpsRun::Type::Call};

    const auto result = evmRun.InvokeEvm();
    const evm::EvmResult& evmResult = result.value();

    if (!std::empty(evmResult.return_value()) ||
        static_cast<uint8_t>(evmResult.return_value().back()) == 0x01) {
      return true;
    }
  }
  return false;
}

bool CpsRunEvm::HasFeedback() const {
  return GetType() == CpsRun::TrapCall || GetType() == CpsRun::TrapCreate;
}

void CpsRunEvm::ProvideFeedback(const CpsRun& previousRun,
                                const CpsExecuteResult& results) {
  if (!previousRun.HasFeedback()) {
    // If there's no feedback from previous run we assume it was successful
    mProtoArgs.mutable_continuation()->set_succeeded(true);
    return;
  }

  // For now only Evm is supported!
  if (std::holds_alternative<evm::EvmResult>(results.result)) {
    const auto& evmResult = std::get<evm::EvmResult>(results.result);
    const auto evmSucceeded = evmResult.exit_reason().exit_reason_case() ==
                              evm::ExitReason::ExitReasonCase::kSucceed;
    mProtoArgs.mutable_continuation()->set_succeeded(evmSucceeded);

    *mProtoArgs.mutable_continuation()->mutable_logs() = evmResult.logs();

    if (previousRun.GetDomain() == CpsRun::Evm) {
      const CpsRunEvm& prevRunEvm = static_cast<const CpsRunEvm&>(previousRun);
      if (mProtoArgs.continuation().feedback_type() ==
          evm::Continuation_Type_CREATE) {
        *mProtoArgs.mutable_continuation()->mutable_address() =
            (results.isSuccess ? prevRunEvm.mProtoArgs.address()
                               : AddressToProto(libCps::CpsRunEvm::Address{}));
      } else {
        *mProtoArgs.mutable_continuation()->mutable_calldata()->mutable_data() =
            evmResult.return_value();
      }
    }
  } else {
    const auto& scillaResult = std::get<ScillaResult>(results.result);
    if (mProtoArgs.continuation().feedback_type() ==
        evm::Continuation_Type_CALL) {
      mProtoArgs.mutable_continuation()->set_succeeded(scillaResult.isSuccess);
    }
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
