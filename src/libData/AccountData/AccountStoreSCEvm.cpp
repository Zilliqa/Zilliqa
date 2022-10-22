/*
 * Copyright (C) 2019 Zilliqa
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

#include <chrono>
#include <future>
#include <vector>
#include "AccountStoreSC.h"
#include "EvmClient.h"
#include "common/Constants.h"
#include "libPersistence/ContractStorage.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"

template <class MAP>
void AccountStoreSC<MAP>::EvmCallRunner(
    const INVOKE_TYPE /*invoke_type*/,  //
    EvmCallParameters& params,          //
    const uint32_t version,             //
    bool& ret,                          //
    TransactionReceipt& receipt,        //
    evmproj::CallResponse& evmReturnValues) {
  //
  // create a worker to be executed in the async method
  const auto worker = [&params, &ret, &version, &evmReturnValues]() -> void {
    ret = EvmClient::GetInstance().CallRunner(
        version, EvmUtils::GetEvmCallJson(params), evmReturnValues);
  };

  const auto fut = std::async(std::launch::async, worker);
  // check the future return and when time out log error.
  switch (fut.wait_for(std::chrono::seconds(30))) {
    case std::future_status::ready: {
      LOG_GENERAL(WARNING, "lock released normally");
    } break;
    case std::future_status::timeout: {
      LOG_GENERAL(WARNING, "Txn processing timeout!");
      EvmClient::GetInstance().CheckClient(0, true);
      receipt.AddError(EXECUTE_CMD_TIMEOUT);
      ret = false;
    } break;
    case std::future_status::deferred: {
      LOG_GENERAL(WARNING, "Illegal future return status!");
      ret = false;
    }
  }

  for (auto const &i : evmReturnValues.Trace() ) {
    std::cerr << "ISSSS "  << i << std::endl;
  }
}

template <class MAP>
uint64_t AccountStoreSC<MAP>::InvokeEvmInterpreter(
    Account* contractAccount, INVOKE_TYPE invoke_type,
    EvmCallParameters& params, const uint32_t& version, bool& ret,
    TransactionReceipt& receipt, evmproj::CallResponse& evmReturnValues) {
  // call evm-ds
  EvmCallRunner(invoke_type, params, version, ret, receipt, evmReturnValues);

  for (auto const&i : evmReturnValues.Trace()) {
    std::cerr << "sigh RESP is : " << i << std::endl;
  }
  //std::cerr << "hash is : " << transaction.GetTranID() << std::endl;

  //if (!BlockStorage::GetBlockStorage().PutTxTrace(transaction.GetTranID(), "wheee")) {
  //  LOG_GENERAL(INFO, "FAIL: Put TX trace failed " << transaction.GetTranID());
  //}

  if (not evmReturnValues.Success()) {
    LOG_GENERAL(WARNING, evmReturnValues.ExitReason());
  }

  // switch ret to reflect our overall success
  ret = evmReturnValues.Success() ? ret : false;

  if (!evmReturnValues.Logs().empty()) {
    Json::Value _json = Json::arrayValue;

    for (const auto& logJsonString : evmReturnValues.Logs()) {
      LOG_GENERAL(INFO, "Evm return value logs: " << logJsonString);

      try {
        Json::Value tmp;
        Json::Reader _reader;
        if (_reader.parse(logJsonString, tmp)) {
          _json.append(tmp);
        } else {
          LOG_GENERAL(WARNING, "Parsing json unsuccessful " << logJsonString);
        }
      } catch (std::exception& e) {
        LOG_GENERAL(WARNING, "Exception: " << e.what());
      }
    }
    receipt.AddJsonEntry(_json);
  }

  auto gas = evmReturnValues.Gas();

  std::map<std::string, zbytes> states;
  std::vector<std::string> toDeletes;
  // parse the return values from the call to evm.
  for (const auto& it : evmReturnValues.GetApplyInstructions()) {
    if (it->OperationType() == "delete") {
      // Set account balance to 0 to avoid any leakage of funds in case
      // selfdestruct is called multiple times
      Account* targetAccount = this->GetAccountAtomic(Address(it->Address()));
      targetAccount->SetBalance(uint128_t(0));
      m_storageRootUpdateBufferAtomic.emplace(it->Address());

    } else {
      // Get the account that this apply instruction applies to
      Account* targetAccount = this->GetAccountAtomic(Address(it->Address()));
      if (targetAccount == nullptr) {
        if (!this->AddAccountAtomic(Address(it->Address()), {0, 0})) {
          LOG_GENERAL(WARNING, "AddAccount failed for address "
                                   << Address(it->Address()).hex());
          continue;
        }
        targetAccount = this->GetAccountAtomic(Address(it->Address()));
        if (targetAccount == nullptr) {
          LOG_GENERAL(WARNING, "failed to retrieve new account for address "
                                   << Address(it->Address()).hex());
          continue;
        }
      }

      if (it->OperationType() == "modify") {
        try {
          if (it->isResetStorage()) {
            states.clear();
            toDeletes.clear();

            Contract::ContractStorage::GetContractStorage()
                .FetchStateDataForContract(states, Address(it->Address()), "",
                                           {}, true);
            for (const auto& x : states) {
              toDeletes.emplace_back(x.first);
            }

            if (!targetAccount->UpdateStates(Address(it->Address()), {},
                                             toDeletes, true)) {
              LOG_GENERAL(
                  WARNING,
                  "Failed to update states hby setting indices for deletion "
                  "for "
                      << it->Address());
            }
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to reset storage " << e.what());
        }

        // If Instructed to reset the Code do so and call SetImmutable to reset
        // the hash
        try {
          if (it->hasCode() && it->Code().size() > 0) {
            targetAccount->SetImmutable(
                DataConversion::StringToCharArray("EVM" + it->Code()), {});
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(
              WARNING,
              "Exception thrown trying to update Contract code " << e.what());
        }

        // Actually Update the state for the contract
        try {
          for (const auto& sit : it->Storage()) {
            if (not Contract::ContractStorage::GetContractStorage()
                        .UpdateStateValue(
                            Address(it->Address()),
                            DataConversion::StringToCharArray(sit.Key()), 0,
                            DataConversion::StringToCharArray(sit.Value()),
                            0)) {
              LOG_GENERAL(WARNING,
                          "Exception thrown trying to update state in Contract "
                          "storage "
                              << it->Address());
            }
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to update state on the contract "
                          << e.what());
        }

        try {
          if (it->hasBalance() && it->Balance().size()) {
            targetAccount->SetBalance(uint128_t(it->Balance()));
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(
              WARNING,
              "Exception thrown trying to update balance on target Account "
                  << e.what());
        }

        try {
          if (it->hasNonce() && it->Nonce().size()) {
            targetAccount->SetNonce(std::stoull(it->Nonce(), nullptr, 0));
          }
        } catch (std::exception& e) {
          // for now catch any generic exceptions and report them
          // will examine exact possibilities and catch specific exceptions.
          LOG_GENERAL(WARNING,
                      "Exception thrown trying to set Nonce on target Account "
                          << e.what());
        }
        // Mark the Address as updated
        m_storageRootUpdateBufferAtomic.emplace(it->Address());
      }
    }
  }

  // send code to be executed
  if (invoke_type == RUNNER_CREATE) {
    contractAccount->SetImmutable(DataConversion::StringToCharArray(
                                      "EVM" + evmReturnValues.ReturnedBytes()),
                                  contractAccount->GetInitData());
  }

  for (auto const&i : evmReturnValues.Trace()) {
    std::cerr << "???? sigh RESP is : " << i << std::endl;
  }

  return gas;
}

template <class MAP>
bool AccountStoreSC<MAP>::ViewAccounts(const EvmCallParameters& params,
                                       evmproj::CallResponse& response) {
  uint32_t evm_version{0};

  return EvmClient::GetInstance().CallRunner(
      evm_version, EvmUtils::GetEvmCallJson(params), response);
}

template <class MAP>
bool AccountStoreSC<MAP>::UpdateAccountsEvm(const uint64_t& blockNum,
                                            const unsigned int& numShards,
                                            const bool& isDS,
                                            const Transaction& transaction,
                                            TransactionReceipt& receipt,
                                            TxnStatus& error_code) {
  LOG_MARKER();

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Process txn: " << transaction.GetTranID());
  }

  std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
  m_curIsDS = isDS;
  m_txnProcessTimeout = false;
  error_code = TxnStatus::NOT_PRESENT;
  const Address fromAddr = transaction.GetSenderAddr();

  uint64_t gasRemained = transaction.GetGasLimitEth();

  // Get the amount of deposit for running this txn
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(transaction.GetGasLimitZil(),
                                transaction.GetGasPriceWei(), gasDepositWei)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  switch (Transaction::GetTransactionType(transaction)) {
    case Transaction::NON_CONTRACT: {
      LOG_GENERAL(WARNING, "Non Contracts are handled by Scilla processor");
      return false;
    }

    case Transaction::CONTRACT_CREATION: {
      LOG_GENERAL(INFO, "Create contract");
      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      // Check if gaslimit meets the minimum requirement for contract deployment
      if (transaction.GetGasLimitEth() < MIN_ETH_GAS) {
        LOG_GENERAL(WARNING, "Gas limit " << transaction.GetGasLimitEth()
                                          << " less than " << MIN_ETH_GAS);
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      // Check if the sender has enough balance to pay gasDeposit
      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
      if (fromAccountBalance < gasDepositWei + transaction.GetAmountWei()) {
        LOG_GENERAL(WARNING,
                    "The account doesn't have enough gas to create a contract");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      // generate address for new contract account
      Address contractAddress =
          Account::GetAddressForContract(fromAddr, fromAccount->GetNonce(),
                                         transaction.GetVersionIdentifier());
      // instantiate the object for contract account
      // ** Remember to call RemoveAccount if deployment failed halfway
      Account* contractAccount;
      // For the EVM, we create the temporary account in Atomics, so that we
      // can remove it easily if something doesn't work out.
      DiscardAtomics();  // We start by making sure our new state is clean.
      if (!this->AddAccountAtomic(contractAddress, {0, 0})) {
        LOG_GENERAL(WARNING, "AddAccount failed for contract address "
                                 << contractAddress.hex());
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      contractAccount = this->GetAccountAtomic(contractAddress);
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "contractAccount is null ptr");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }
      if (transaction.GetCode().empty()) {
        LOG_GENERAL(WARNING,
                    "Creating a contract with empty code is not feasible.");
        error_code = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        return false;
      }

      uint32_t scilla_version{0};
      uint32_t evm_version{0};

      try {
        // TODO verify this line is needed, suspect it is a scilla thing
        m_curBlockNum = blockNum;
        const uint128_t decreaseAmount =
            uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
        if (!this->DecreaseBalance(fromAddr, decreaseAmount)) {
          LOG_GENERAL(WARNING, "Evm Decrease Balance has failed");
          error_code = TxnStatus::FAIL_CONTRACT_INIT;
          return false;
        }
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING,
                    "Evm Exception caught in Decrease Balance " << e.what());
        error_code = TxnStatus::FAIL_CONTRACT_INIT;
        return false;
      }

      // prepare IPC with current blockchain info provider.

      m_scillaIPCServer->setBCInfoProvider(
          {m_curBlockNum, m_curDSBlockNum, m_originAddr, contractAddress,
           contractAccount->GetStorageRoot(), scilla_version});

      std::map<std::string, zbytes> t_metadata;
      t_metadata.emplace(
          Contract::ContractStorage::GetContractStorage().GenerateStorageKey(
              contractAddress, SCILLA_VERSION_INDICATOR, {}),
          DataConversion::StringToCharArray(std::to_string(scilla_version)));

      // *************************************************************************
      // Undergo a runner
      bool evm_call_run_succeeded{true};

      LOG_GENERAL(INFO, "Invoking EVM with Cumulative Gas "
                            << gasRemained << " alleged "
                            << transaction.GetAmountQa() << " limit "
                            << transaction.GetGasLimitEth());

      if (!TransferBalanceAtomic(fromAddr, contractAddress,
                                 transaction.GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
        return false;
      }

      EvmCallParameters params = {
          contractAddress.hex(),
          fromAddr.hex(),
          DataConversion::CharArrayToString(transaction.GetCode()),
          DataConversion::CharArrayToString(transaction.GetData()),
          transaction.GetGasLimitEth(),
          transaction.GetAmountWei()};

      std::map<std::string, zbytes> t_newmetadata;

      t_newmetadata.emplace(Contract::ContractStorage::GenerateStorageKey(
                                contractAddress, CONTRACT_ADDR_INDICATOR, {}),
                            contractAddress.asBytes());

      if (!contractAccount->UpdateStates(contractAddress, t_newmetadata, {},
                                         true)) {
        LOG_GENERAL(WARNING, "Account::UpdateStates failed");
        return false;
      }
      evmproj::CallResponse response;
      const auto gasRemained = InvokeEvmInterpreter(
          contractAccount, RUNNER_CREATE, params, evm_version,
          evm_call_run_succeeded, receipt, response);


      for (auto const&i : response.Trace()) {
        std::cout << "RESP is : " << i << std::endl;
      }

      if(response.Trace().size() > 0) {
        if (!BlockStorage::GetBlockStorage().PutTxTrace(transaction.GetTranID(), response.Trace()[0])) {
          LOG_GENERAL(INFO, "FAIL: Put TX trace failed " << transaction.GetTranID());
        }
      }

      std::cerr << "hash is : " << transaction.GetTranID() << std::endl;

      const auto gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);
      // *************************************************************************
      // Summary
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                    transaction.GetGasPriceWei(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }
      if (!this->IncreaseBalance(fromAddr,
                                 gasRefund / EVM_ZIL_SCALING_FACTOR)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
      }
      if (evm_call_run_succeeded) {
        CommitAtomics();
      } else {
        DiscardAtomics();

        receipt.SetResult(false);
        receipt.AddError(RUNNER_FAILED);
        receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);
        receipt.update();
        // TODO : confirm we increase nonce on failure
        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
        }

        LOG_GENERAL(INFO,
                    "Executing contract Creation transaction finished "
                    "unsuccessfully");
        return false;
      }

      if (transaction.GetGasLimitZil() < gasRemainedCore) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        return false;
      }

      /// calculate total gas in receipt
      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);

      break;
    }

    case Transaction::CONTRACT_CALL: {
      // reset the storageroot update buffer atomic per transaction
      m_storageRootUpdateBufferAtomic.clear();

      m_originAddr = fromAddr;

      Account* fromAccount = this->GetAccount(fromAddr);
      if (fromAccount == nullptr) {
        LOG_GENERAL(WARNING, "Sender has no balance, reject");
        error_code = TxnStatus::INVALID_FROM_ACCOUNT;
        return false;
      }

      Account* contractAccount = this->GetAccount(transaction.GetToAddr());
      if (contractAccount == nullptr) {
        LOG_GENERAL(WARNING, "The target contract account doesn't exist");
        error_code = TxnStatus::INVALID_TO_ACCOUNT;
        return false;
      }

      LOG_GENERAL(INFO, "Call contract");

      const uint256_t fromAccountBalance =
          uint256_t{fromAccount->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
      if (fromAccountBalance < gasDepositWei + transaction.GetAmountWei()) {
        LOG_GENERAL(WARNING, "The account (balance: "
                                 << fromAccountBalance
                                 << ") "
                                    "has not enough balance to deposit the gas "
                                    "price to deposit ("
                                 << gasDepositWei
                                 << ") "
                                    "and transfer the amount ("
                                 << transaction.GetAmountWei()
                                 << ") in the txn, "
                                    "rejected");
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        return false;
      }

      m_curSenderAddr = fromAddr;
      m_curEdges = 0;

      if (contractAccount->GetCode().empty()) {
        LOG_GENERAL(
            WARNING,
            "Trying to call a smart contract that has no code will fail");
        error_code = TxnStatus::NOT_PRESENT;
        return false;
      }

      m_curBlockNum = blockNum;
      uint32_t scilla_version{0};
      uint32_t evm_version{0};

      DiscardAtomics();
      const uint128_t amountToDecrease =
          uint128_t{gasDepositWei / EVM_ZIL_SCALING_FACTOR};
      if (!this->DecreaseBalance(fromAddr, amountToDecrease)) {
        LOG_GENERAL(WARNING, "DecreaseBalance failed");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      m_curGasLimit = transaction.GetGasLimitZil();
      m_curGasPrice = transaction.GetGasPriceWei();
      m_curContractAddr = transaction.GetToAddr();
      m_curAmount = transaction.GetAmountQa();
      m_curNumShards = numShards;

      std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        tpStart = r_timer_start();
      }

      // prepare IPC with current blockchain info provider.
      m_scillaIPCServer->setBCInfoProvider(
          {m_curBlockNum, m_curDSBlockNum, m_originAddr, m_curContractAddr,
           contractAccount->GetStorageRoot(), scilla_version});

      Contract::ContractStorage::GetContractStorage().BufferCurrentState();

      std::string runnerPrint;
      bool evm_call_succeeded{true};

      if (!TransferBalanceAtomic(fromAddr, m_curContractAddr,
                                 transaction.GetAmountQa())) {
        error_code = TxnStatus::INSUFFICIENT_BALANCE;
        LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
        return false;
      }

      EvmCallParameters params = {
          m_curContractAddr.hex(),
          fromAddr.hex(),
          DataConversion::CharArrayToString(contractAccount->GetCode()),
          DataConversion::CharArrayToString(transaction.GetData()),
          transaction.GetGasLimitEth(),
          transaction.GetAmountWei()};

      LOG_GENERAL(WARNING, "contract address is " << params.m_contract
                                                  << " caller account is "
                                                  << params.m_caller);
      evmproj::CallResponse response;
      const uint64_t gasRemained = InvokeEvmInterpreter(
          contractAccount, RUNNER_CALL, params, evm_version, evm_call_succeeded,
          receipt, response);


      for (auto const&i : response.Trace()) {
        std::cout << "KLJLKJLKJLKJ **** RESP is : " << i << std::endl;
      }
      std::cerr << "hash is : " << transaction.GetTranID() << std::endl;

      if(response.Trace().size() > 0) {
        if (!BlockStorage::GetBlockStorage().PutTxTrace(transaction.GetTranID(), response.Trace()[0])) {
          LOG_GENERAL(INFO, "FAIL: Put TX trace failed " << transaction.GetTranID());
        }
      }

      uint64_t gasRemainedCore = GasConv::GasUnitsFromEthToCore(gasRemained);

      if (!evm_call_succeeded) {
        Contract::ContractStorage::GetContractStorage().RevertPrevState();
        DiscardAtomics();
        gasRemainedCore =
            std::min(transaction.GetGasLimitZil(), gasRemainedCore);
      } else {
        CommitAtomics();
      }
      uint128_t gasRefund;
      if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                    transaction.GetGasPriceWei(), gasRefund)) {
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      if (!this->IncreaseBalance(
              fromAddr,

              gasRefund / EVM_ZIL_SCALING_FACTOR / EVM_ZIL_SCALING_FACTOR)) {
        LOG_GENERAL(WARNING, "IncreaseBalance failed for gasRefund");
      }

      if (transaction.GetGasLimitZil() < gasRemainedCore) {
        LOG_GENERAL(WARNING, "Cumulative Gas calculated Underflow, gasLimit: "
                                 << transaction.GetGasLimitZil()
                                 << " gasRemained: " << gasRemained
                                 << ". Must be something wrong!");
        error_code = TxnStatus::MATH_ERROR;
        return false;
      }

      receipt.SetCumGas(transaction.GetGasLimitZil() - gasRemainedCore);
      if (!evm_call_succeeded) {
        receipt.SetResult(false);
        receipt.CleanEntry();
        receipt.update();

        if (!this->IncreaseNonce(fromAddr)) {
          error_code = TxnStatus::MATH_ERROR;
          LOG_GENERAL(WARNING, "Increase Nonce failed on bad txn");
        }
        return false;
      }
    } break;

    default: {
      LOG_GENERAL(WARNING, "Txn is not typed correctly")
      error_code = TxnStatus::INCORRECT_TXN_TYPE;
      return false;
    }
    case Transaction::ERROR:
      // TODO
      // maybe we should treat this error properly we have just fallen through.
      break;
  }

  if (!this->IncreaseNonce(fromAddr)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  receipt.SetResult(true);
  receipt.update();

  // since txn succeeded, commit the atomic buffer. If no updates, it is a noop.
  m_storageRootUpdateBuffer.insert(m_storageRootUpdateBufferAtomic.begin(),
                                   m_storageRootUpdateBufferAtomic.end());

  if (LOG_SC) {
    LOG_GENERAL(INFO, "Executing contract transaction finished");
    LOG_GENERAL(INFO, "receipt: " << receipt.GetString());
  }

  return true;
}

template <class MAP>
bool AccountStoreSC<MAP>::AddAccountAtomic(const Address& address,
                                           const Account& account) {
  return m_accountStoreAtomic->AddAccount(address, account);
}

template class AccountStoreSC<std::map<Address, Account>>;
template class AccountStoreSC<std::unordered_map<Address, Account>>;
