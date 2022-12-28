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
#include "libData/AccountData/EvmClient.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/Metrics.h"

#include <boost/algorithm/hex.hpp>

#include <future>

namespace libCps {
CpsRunEvm::CpsRunEvm(evm::EvmArgs proto_args)
    : m_proto_args(std::move(proto_args)) {}

CpsExecuteResult CpsRunEvm::Run(CpsAccountStoreInterface& account_store,
                                TransactionReceipt& receipt) {
  if (account_store.AccountExists(dev::h160{})) {
    return {};
  }

  if (zil::metrics::Filter::GetInstance().Enabled(
          zil::metrics::FilterClass::ACCOUNTSTORE_EVM)) {
    const auto metrics = Metrics::GetInstance().CreateInt64Metric(
        "zilliqa_accountstroe", "invocations_count", "Metrics for AccountStore",
        "Blocks");
    metrics->Add(1, {{"method", "EvmCallRunner"}});
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
    return HandleTrap(evm_result, receipt);
  } else if (exit_reason_case == evm::ExitReason::ExitReasonCase::kSucceed) {
    HandleApply(evm_result, receipt, account_store);
    return {TxnStatus::NOT_PRESENT, true};
  }
  // Revert or Abort
  return {};
}

std::optional<evm::EvmResult> CpsRunEvm::InvokeEvm() {
  evm::EvmResult result;
  const auto worker = [args = std::cref(m_proto_args), &result]() -> void {
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
    const evm::TrapData_Create& create_data = trap_data.create();
    const auto validateResult = ValidateCreateTrap(create_data);
    if (!validateResult.is_success) {
      return validateResult;
    }

    Address contractAddress;
    Address fromAddress;

    if (scheme.has_legacy()) {
      const evm::TrapData_Scheme_Legacy& legacy = scheme.legacy();
      fromAddress = ProtoToAddress(legacy.caller());
      contractAddress = account_store.GetAddressForContract(
          fromAddress, TRANSACTION_VERSION_ETH);
    } else if (scheme.has_create2()) {
      const evm::TrapData_Scheme_Create2& create2 = scheme.create2();
      fromAddress = ProtoToAddress(create2.caller());
      contractAddress = account_store.GetAddressForContract(
          fromAddress, TRANSACTION_VERSION_ETH);
    }

    if (!accountStore.AddAccountAtomic(contractAddress)) {
      return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false};
    }

    if (!accountStore.TransferBalanceAtomic(
            fromAddress, contractAddress,
            Amount::fromWei(ProtoToUint(createData.value())))) {
      return {TxnStatus::INSUFFICIENT_BALANCE, false};
    }

    // Check if account is atomic or already existed and set nonce respectively
    if (accountStore.AccountExistsAtomic(fromAddress)) {
      const auto currentNonce =
          accountStore.GetNonceForAccountAtomic(fromAddress);
      accountStore.SetNonceAtomic(currentNonce + 1);
    } else {
      const auto currentNonce = accountStore.GetNonceForAccount(fromAddress);
      accountStore.SetNonceForAccount(fromAddress);
    }
  }

  (void)evm_result;
  (void)receipt;
}

CpsExecuteResult CpsRunEvm::ValidateCreateTrap(
    const evm::TrapData_Create& createData) {
  const evm::Address& proto_caller = createData.caller();
  const Address address = ProtoToAddress(proto_caller);
  const evm::TrapData_Scheme& scheme = createData.scheme();
  // Check Balance, Required Gas

    const auto current_balance = Amount::fromQa(account_store.GetBalanceForAccount(address);
    const auto requested_value = Amount::fromWei(ProtoToUint(createData.value()));

    if(current_balance < requested_value) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false};
    }

    const auto targetGas = createData.target_gas();

    const auto baseFee = Eth::getGasUnitsForContractDeployment(
          {}, DataConversion::StringToCharArray(createData.call_data()));


    if(targetGas < baseFee) {
    return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false};
    }

    return {TxnStatus::NOT_PRESENT, true};
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
          continue;
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
          account_store.SetNonceAtomic(address, nonce.convert_to<uint64_t>());
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

}  // namespace libCps