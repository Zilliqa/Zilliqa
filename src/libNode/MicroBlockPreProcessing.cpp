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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountData/TxnOrderVerifier.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libPOW/pow.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

bool Node::ComposeMicroBlock(const uint64_t& microblock_gas_limit) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeMicroBlock not expected to be called from "
                "LookUp node");
    return true;
  }
  // To-do: Replace dummy values with the required ones
  LOG_MARKER();

  // TxBlockHeader
  const uint32_t version = MICROBLOCK_VERSION;
  const uint32_t shardId = m_myshardId;
  const uint64_t gasLimit = microblock_gas_limit;
  const uint64_t gasUsed = m_gasUsedTotal;
  uint128_t rewards = 0;
  if (m_mediator.GetIsVacuousEpoch() &&
      m_mediator.m_ds->m_mode != DirectoryService::IDLE) {
    rewards =
        (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
         COINBASE_UPDATE_TARGET_DS)
            ? COINBASE_REWARD_PER_DS_NEW
            : COINBASE_REWARD_PER_DS;
  } else {
    rewards = m_txnFees;
  }
  BlockHash prevHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetMyHash();

  TxnHash txRootHash, txReceiptHash;
  uint32_t numTxs = 0;
  const PubKey& minerPubKey = m_mediator.m_selfKey.second;
  StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

  CommitteeHash committeeHash;
  if (m_mediator.m_ds->m_mode == DirectoryService::IDLE) {
    if (!Messenger::GetShardHash(m_mediator.m_ds->m_shards.at(shardId),
                                 committeeHash)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::GetShardHash failed");
      return false;
    }
  } else {
    if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                       committeeHash)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::GetDSCommitteeHash failed");
      return false;
    }
  }

  // TxBlock
  vector<TxnHash> tranHashes;

  // unsigned int index = 0;
  {
    lock_guard<mutex> g(m_mutexProcessedTransactions);

    txRootHash = ComputeRoot(m_TxnOrder);

    numTxs = t_processedTransactions.size();
    if (numTxs != m_TxnOrder.size()) {
      LOG_GENERAL(WARNING, "FATAL Num txns and Order size not same "
                               << " numTxs " << numTxs << " m_TxnOrder "
                               << m_TxnOrder.size());
      return false;
    }
    tranHashes = m_TxnOrder;

    if (!TransactionWithReceipt::ComputeTransactionReceiptsHash(
            tranHashes, t_processedTransactions, txReceiptHash)) {
      LOG_GENERAL(WARNING, "Cannot compute transaction receipts hash");
      return false;
    }
  }

#ifdef DM_TEST_DM_BAD_MB_ANNOUNCE
  if (m_mediator.m_ds->m_viewChangeCounter == 0 &&
      m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Leader compose wrong state root (DM_TEST_DM_BAD_MB_ANNOUNCE)");
    tranHashes.clear();
  }
#endif  // DM_TEST_DM_BAD_MB_ANNOUNCE

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Creating new micro block")
  m_microblock.reset(new MicroBlock(
      MicroBlockHeader(
          shardId, gasLimit, gasUsed, rewards, m_mediator.m_currentEpochNum,
          {txRootHash, stateDeltaHash, txReceiptHash}, numTxs, minerPubKey,
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
          version, committeeHash, prevHash),
      tranHashes, CoSignatures()));

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Micro block proposed with "
                << m_microblock->GetHeader().GetNumTxs()
                << " transactions for epoch " << m_mediator.m_currentEpochNum);

  return true;
}

bool Node::OnNodeMissingTxns(const bytes& errorMsg, const unsigned int offset,
                             const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::OnNodeMissingTxns not expected to be called from "
                "LookUp node");
    return true;
  }

  vector<TxnHash> missingTransactions;
  uint64_t epochNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetNodeMissingTxnsErrorMsg(
          errorMsg, offset, missingTransactions, epochNum, portNo)) {
    LOG_GENERAL(WARNING, "Messenger::GetNodeMissingTxnsErrorMsg failed");
    return false;
  }

  Peer peer(from.m_ipAddress, portNo);

  lock_guard<mutex> g(m_mutexProcessedTransactions);

  unsigned int cur_offset = 0;
  bytes tx_message = {MessageType::NODE,
                      NodeInstructionType::SUBMITTRANSACTION};
  cur_offset += MessageOffset::BODY;
  tx_message.push_back(SUBMITTRANSACTIONTYPE::MISSINGTXN);
  cur_offset += MessageOffset::INST;
  Serializable::SetNumber<uint64_t>(tx_message, cur_offset, epochNum,
                                    sizeof(uint64_t));
  cur_offset += sizeof(uint64_t);

  std::vector<Transaction> txns;

  const std::unordered_map<TxnHash, TransactionWithReceipt>&
      processedTransactions = (epochNum == m_mediator.m_currentEpochNum)
                                  ? t_processedTransactions
                                  : m_processedTransactions[epochNum];

  for (const auto& hash : missingTransactions) {
    // LOG_GENERAL(INFO, "Peer " << from << " : " << portNo << " missing txn "
    // << missingTransactions[i])
    auto found = processedTransactions.find(hash);
    if (found != processedTransactions.end()) {
      txns.emplace_back(found->second.GetTransaction());
    } else {
      LOG_GENERAL(INFO,
                  "Leader unable to find txn proposed in microblock " << hash);
      continue;
    }
  }

  if (!Messenger::SetTransactionArray(tx_message, cur_offset, txns)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransactionArray failed");
    return false;
  }

  P2PComm::GetInstance().SendMessage(peer, tx_message);

  return true;
}

bool Node::OnCommitFailure([
    [gnu::unused]] const std::map<unsigned int, bytes>& commitFailureMap) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::OnCommitFailure not expected to be called from "
                "LookUp node");
    return true;
  }

  LOG_MARKER();

  // for(auto failureEntry: commitFailureMap)
  // {

  // }

  // LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
  //           "Going to sleep before restarting consensus");

  // std::this_thread::sleep_for(30s);
  // RunConsensusOnMicroBlockWhenShardLeader();

  // LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
  //           "Woke from sleep after consensus restart");

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Microblock consensus failed, going to wait for final block "
            "announcement");

  return true;
}

void Node::NotifyTimeout(bool& txnProcTimeout) {
  int timeout_time = std::max(
      0,
      ((int)MICROBLOCK_TIMEOUT -
       ((int)TX_DISTRIBUTE_TIME_IN_MS + (int)ANNOUNCEMENT_DELAY_IN_MS) / 1000 -
       (int)CONSENSUS_OBJECT_TIMEOUT));
  LOG_GENERAL(INFO, "The overall timeout for txn processing will be "
                        << timeout_time << " seconds");
  unique_lock<mutex> lock(m_mutexCVTxnProcFinished);
  if (cv_TxnProcFinished.wait_for(lock, chrono::seconds(timeout_time)) ==
      cv_status::timeout) {
    txnProcTimeout = true;
    AccountStore::GetInstance().NotifyTimeout();
  }
}

void Node::ProcessTransactionWhenShardLeader(
    const uint64_t& microblock_gas_limit) {
  LOG_MARKER();

  auto startTime = std::chrono::high_resolution_clock::now();

  if (ENABLE_ACCOUNTS_POPULATING && UPDATE_PREGENED_ACCOUNTS) {
    UpdateBalanceForPreGeneratedAccounts();
  }

  lock_guard<mutex> g(m_mutexCreatedTransactions);

  t_createdTxns = m_createdTxns;
  map<Address, map<uint64_t, Transaction>> t_addrNonceTxnMap;
  t_processedTransactions.clear();
  m_TxnOrder.clear();

  if (LOG_PARAMETERS) {
    LOG_STATE("[TXNPROC-BEG][" << m_mediator.m_currentEpochNum
                               << "] Shard=" << m_myshardId
                               << " NumTx=" << t_createdTxns.size());
  }

  bool txnProcTimeout = false;

  auto txnProcTimer = [this, &txnProcTimeout]() -> void {
    NotifyTimeout(txnProcTimeout);
  };

  DetachedFunction(1, txnProcTimer);

  this_thread::sleep_for(chrono::milliseconds(100));

  auto findOneFromAddrNonceTxnMap =
      [](Transaction& t,
         map<Address, map<uint64_t, Transaction>>& t_addrNonceTxnMap) -> bool {
    for (auto it = t_addrNonceTxnMap.begin(); it != t_addrNonceTxnMap.end();
         it++) {
      if (it->second.begin()->first ==
          AccountStore::GetInstance().GetNonceTemp(it->first) + 1) {
        t = move(it->second.begin()->second);
        it->second.erase(it->second.begin());

        if (it->second.empty()) {
          t_addrNonceTxnMap.erase(it);
        }
        return true;
      }
    }
    return false;
  };

  auto appendOne = [this](const Transaction& t, const TransactionReceipt& tr) {
    t_processedTransactions.insert(
        make_pair(t.GetTranID(), TransactionWithReceipt(t, tr)));
    m_TxnOrder.push_back(t.GetTranID());
  };

  m_gasUsedTotal = 0;
  m_txnFees = 0;

  vector<Transaction> gasLimitExceededTxnBuffer;
  vector<pair<TxnHash, TxnStatus>> droppedTxns;

  AccountStore::GetInstance().CleanStorageRootUpdateBufferTemp();

  while (m_gasUsedTotal < microblock_gas_limit) {
    if (txnProcTimeout) {
      break;
    }

    Transaction t;
    TransactionReceipt tr;

    // check m_addrNonceTxnMap contains any txn meets right nonce,
    // if contains, process it
    if (findOneFromAddrNonceTxnMap(t, t_addrNonceTxnMap)) {
      // check whether m_createdTransaction have transaction with same Addr and
      // nonce if has and with larger gasPrice then replace with that one.
      // (*optional step)
      t_createdTxns.findSameNonceButHigherGas(t);

      if (m_gasUsedTotal + t.GetGasLimit() > microblock_gas_limit) {
        gasLimitExceededTxnBuffer.emplace_back(t);
        continue;
      }
      TxnStatus error_code;
      if (m_mediator.m_validator->CheckCreatedTransaction(t, tr, error_code)) {
        if (!SafeMath<uint64_t>::add(m_gasUsedTotal, tr.GetCumGas(),
                                     m_gasUsedTotal)) {
          LOG_GENERAL(WARNING, "m_gasUsedTotal addition unsafe!");
          break;
        }
        uint128_t txnFee;
        if (!SafeMath<uint128_t>::mul(tr.GetCumGas(), t.GetGasPrice(),
                                      txnFee)) {
          LOG_GENERAL(WARNING, "txnFee multiplication unsafe!");
          continue;
        }
        if (!SafeMath<uint128_t>::add(m_txnFees, txnFee, m_txnFees)) {
          LOG_GENERAL(WARNING, "m_txnFees addition unsafe!");
          break;
        }
        appendOne(t, tr);

        continue;
      } else {
        droppedTxns.emplace_back(t.GetTranID(), error_code);
      }
    }
    // if no txn in u_map meet right nonce process new come-in transactions
    else if (t_createdTxns.findOne(t)) {
      // LOG_GENERAL(INFO, "findOneFromCreated");

      Address senderAddr = t.GetSenderAddr();
      // check nonce, if nonce larger than expected, put it into
      // m_addrNonceTxnMap
      if (t.GetNonce() >
          AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1) {
        // LOG_GENERAL(INFO, "High nonce: "
        //                     << t.GetNonce() << " cur sender " <<
        //                     senderAddr.hex()
        //                     << " nonce: "
        //                     <<
        //                     AccountStore::GetInstance().GetNonceTemp(senderAddr));
        auto it1 = t_addrNonceTxnMap.find(senderAddr);
        if (it1 != t_addrNonceTxnMap.end()) {
          auto it2 = it1->second.find(t.GetNonce());
          if (it2 != it1->second.end()) {
            // found the txn with same addr and same nonce
            // then compare the gasprice and remains the higher one
            if (t.GetGasPrice() > it2->second.GetGasPrice()) {
              it2->second = t;
            }
            continue;
          }
        }
        t_addrNonceTxnMap[senderAddr].insert({t.GetNonce(), t});
      }
      // if nonce too small, ignore it
      else if (t.GetNonce() <
               AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1) {
        // LOG_GENERAL(INFO,
        //             "Nonce too small"
        //                 << " Expected "
        //                 <<
        //                 AccountStore::GetInstance().GetNonceTemp(senderAddr)
        //                 << " Found " << t.GetNonce());
      }
      // if nonce correct, process it
      else {
        if (m_gasUsedTotal + t.GetGasLimit() > microblock_gas_limit) {
          gasLimitExceededTxnBuffer.emplace_back(t);
          continue;
        }
        TxnStatus error_code;
        if (m_mediator.m_validator->CheckCreatedTransaction(t, tr,
                                                            error_code)) {
          if (!SafeMath<uint64_t>::add(m_gasUsedTotal, tr.GetCumGas(),
                                       m_gasUsedTotal)) {
            LOG_GENERAL(WARNING, "m_gasUsedTotal addition unsafe!");
            break;
          }
          uint128_t txnFee;
          if (!SafeMath<uint128_t>::mul(tr.GetCumGas(), t.GetGasPrice(),
                                        txnFee)) {
            LOG_GENERAL(WARNING, "txnFee multiplication unsafe!");
            continue;
          }
          if (!SafeMath<uint128_t>::add(m_txnFees, txnFee, m_txnFees)) {
            LOG_GENERAL(WARNING, "m_txnFees addition unsafe!");
            break;
          }
          appendOne(t, tr);
        } else {
          droppedTxns.emplace_back(t.GetTranID(), error_code);
        }
      }
    } else {
      break;
    }
  }

  AccountStore::GetInstance().ProcessStorageRootUpdateBufferTemp();
  AccountStore::GetInstance().CleanNewLibrariesCacheTemp();

  cv_TxnProcFinished.notify_all();
  PutTxnsInTempDataBase(t_processedTransactions);
  if (ENABLE_TXNS_BACKUP) {
    SaveTxnsToS3(t_processedTransactions);
  }

  if (LOG_PARAMETERS) {
    double elaspedTimeMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - startTime)
            .count();
    LOG_STATE("[TXNPROC-END][" << m_mediator.m_currentEpochNum
                               << "] Shard=" << m_myshardId
                               << " NumTx=" << t_createdTxns.size()
                               << " Time=" << elaspedTimeMs);
  }
  // Put txns in map back into pool
  ReinstateMemPool(t_addrNonceTxnMap, gasLimitExceededTxnBuffer, droppedTxns);
}

bool Node::VerifyTxnsOrdering(const vector<TxnHash>& tranHashes,
                              vector<TxnHash>& missingtranHashes) {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexCreatedTransactions);

    for (const auto& tranHash : tranHashes) {
      if (!m_createdTxns.exist(tranHash)) {
        missingtranHashes.emplace_back(tranHash);
      }
    }
  }

  if (!missingtranHashes.empty()) {
    return true;
  }

  if (!VerifyTxnOrderWTolerance(m_expectedTranOrdering, tranHashes,
                                TXN_MISORDER_TOLERANCE_IN_PERCENT)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Failed to Verify due to bad txn ordering");

    for (const auto& th : m_expectedTranOrdering) {
      Transaction t;
      if (m_createdTxns.get(th, t)) {
        LOG_GENERAL(INFO, "Expected txn: "
                              << t.GetTranID() << " " << t.GetSenderAddr()
                              << " " << t.GetNonce() << " " << t.GetGasPrice());
      }
    }
    for (const auto& th : tranHashes) {
      Transaction t;
      if (m_createdTxns.get(th, t)) {
        LOG_GENERAL(INFO, "Received txn: "
                              << t.GetTranID() << " " << t.GetSenderAddr()
                              << " " << t.GetNonce() << " " << t.GetGasPrice());
      }
    }

    return false;
  }

  return true;
}

void Node::UpdateProcessedTransactions() {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexCreatedTransactions);
    m_createdTxns = move(t_createdTxns);
    t_createdTxns.clear();
  }
  if (m_mediator.m_currentEpochNum % NUM_STORE_TX_BODIES_INTERVAL == 0) {
    BlockStorage::GetBlockStorage().ResetDB(
        BlockStorage::DBTYPE::PROCESSED_TEMP);
  }

  {
    lock_guard<mutex> g(m_mutexProcessedTransactions);
    m_processedTransactions[m_mediator.m_currentEpochNum] =
        move(t_processedTransactions);
    t_processedTransactions.clear();
  }
}

void Node::ProcessTransactionWhenShardBackup(
    const uint64_t& microblock_gas_limit) {
  LOG_MARKER();

  auto startTime = std::chrono::high_resolution_clock::now();

  if (ENABLE_ACCOUNTS_POPULATING && UPDATE_PREGENED_ACCOUNTS) {
    UpdateBalanceForPreGeneratedAccounts();
  }

  lock_guard<mutex> g(m_mutexCreatedTransactions);

  t_createdTxns = m_createdTxns;
  m_expectedTranOrdering.clear();
  map<Address, map<uint64_t, Transaction>> t_addrNonceTxnMap;
  t_processedTransactions.clear();

  if (LOG_PARAMETERS) {
    LOG_STATE("[TXNPROC-BEG][" << m_mediator.m_currentEpochNum
                               << "] Shard=" << m_myshardId
                               << " NumTx=" << t_createdTxns.size());
  }

  bool txnProcTimeout = false;

  auto txnProcTimer = [this, &txnProcTimeout]() -> void {
    NotifyTimeout(txnProcTimeout);
  };

  DetachedFunction(1, txnProcTimer);

  this_thread::sleep_for(chrono::milliseconds(100));

  auto findOneFromAddrNonceTxnMap =
      [](Transaction& t,
         std::map<Address, map<uint64_t, Transaction>>& t_addrNonceTxnMap)
      -> bool {
    for (auto it = t_addrNonceTxnMap.begin(); it != t_addrNonceTxnMap.end();
         it++) {
      if (it->second.begin()->first ==
          AccountStore::GetInstance().GetNonceTemp(it->first) + 1) {
        t = move(it->second.begin()->second);
        it->second.erase(it->second.begin());

        if (it->second.empty()) {
          t_addrNonceTxnMap.erase(it);
        }
        return true;
      }
    }
    return false;
  };

  auto appendOne = [this](const Transaction& t, const TransactionReceipt& tr) {
    m_expectedTranOrdering.emplace_back(t.GetTranID());
    t_processedTransactions.insert(
        make_pair(t.GetTranID(), TransactionWithReceipt(t, tr)));
  };

  m_gasUsedTotal = 0;
  m_txnFees = 0;

  vector<Transaction> gasLimitExceededTxnBuffer;
  vector<pair<TxnHash, TxnStatus>> droppedTxns;

  AccountStore::GetInstance().CleanStorageRootUpdateBufferTemp();

  while (m_gasUsedTotal < microblock_gas_limit) {
    if (txnProcTimeout) {
      break;
    }

    Transaction t;
    TransactionReceipt tr;

    // check t_addrNonceTxnMap contains any txn meets right nonce,
    // if contains, process it
    if (findOneFromAddrNonceTxnMap(t, t_addrNonceTxnMap)) {
      // check whether m_createdTransaction have transaction with same Addr and
      // nonce if has and with larger gasPrice then replace with that one.
      // (*optional step)
      t_createdTxns.findSameNonceButHigherGas(t);

      if (m_gasUsedTotal + t.GetGasLimit() > microblock_gas_limit) {
        gasLimitExceededTxnBuffer.emplace_back(t);
        continue;
      }
      TxnStatus error_code;
      if (m_mediator.m_validator->CheckCreatedTransaction(t, tr, error_code)) {
        if (!SafeMath<uint64_t>::add(m_gasUsedTotal, tr.GetCumGas(),
                                     m_gasUsedTotal)) {
          LOG_GENERAL(WARNING, "m_gasUsedTotal addition unsafe!");
          break;
        }
        uint128_t txnFee;
        if (!SafeMath<uint128_t>::mul(tr.GetCumGas(), t.GetGasPrice(),
                                      txnFee)) {
          LOG_GENERAL(WARNING, "txnFee multiplication unsafe!");
          continue;
        }
        if (!SafeMath<uint128_t>::add(m_txnFees, txnFee, m_txnFees)) {
          LOG_GENERAL(WARNING, "m_txnFees addition unsafe!");
          break;
        }
        appendOne(t, tr);
        continue;
      }

      else {
        droppedTxns.emplace_back(t.GetTranID(), error_code);
      }

    }
    // if no txn in u_map meet right nonce process new come-in transactions
    else if (t_createdTxns.findOne(t)) {
      Address senderAddr = t.GetSenderAddr();
      // check nonce, if nonce larger than expected, put it into
      // t_addrNonceTxnMap
      if (t.GetNonce() >
          AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1) {
        auto it1 = t_addrNonceTxnMap.find(senderAddr);
        if (it1 != t_addrNonceTxnMap.end()) {
          auto it2 = it1->second.find(t.GetNonce());
          if (it2 != it1->second.end()) {
            // found the txn with same addr and same nonce
            // then compare the gasprice and remains the higher one
            if (t.GetGasPrice() > it2->second.GetGasPrice()) {
              it2->second = t;
            }
            continue;
          }
        }
        t_addrNonceTxnMap[senderAddr].insert({t.GetNonce(), t});
      }
      // if nonce too small, ignore it
      else if (t.GetNonce() <
               AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1) {
      }
      // if nonce correct, process it
      else {
        if (m_gasUsedTotal + t.GetGasLimit() > microblock_gas_limit) {
          gasLimitExceededTxnBuffer.emplace_back(t);
          continue;
        }
        TxnStatus error_code;
        if (m_mediator.m_validator->CheckCreatedTransaction(t, tr,
                                                            error_code)) {
          if (!SafeMath<uint64_t>::add(m_gasUsedTotal, tr.GetCumGas(),
                                       m_gasUsedTotal)) {
            LOG_GENERAL(WARNING, "m_gasUsedTotal addition overflow!");
            break;
          }
          uint128_t txnFee;
          if (!SafeMath<uint128_t>::mul(tr.GetCumGas(), t.GetGasPrice(),
                                        txnFee)) {
            LOG_GENERAL(WARNING, "txnFee multiplication overflow!");
            continue;
          }
          if (!SafeMath<uint128_t>::add(m_txnFees, txnFee, m_txnFees)) {
            LOG_GENERAL(WARNING, "m_txnFees addition overflow!");
            break;
          }
          appendOne(t, tr);
        } else {
          droppedTxns.emplace_back(t.GetTranID(), error_code);
        }
      }
    } else {
      break;
    }
  }

  AccountStore::GetInstance().ProcessStorageRootUpdateBufferTemp();
  AccountStore::GetInstance().CleanNewLibrariesCacheTemp();

  cv_TxnProcFinished.notify_all();

  PutTxnsInTempDataBase(t_processedTransactions);

  if (LOG_PARAMETERS) {
    double elaspedTimeMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - startTime)
            .count();
    LOG_STATE("[TXNPROC-END][" << m_mediator.m_currentEpochNum
                               << "] Shard=" << m_myshardId
                               << " NumTx=" << t_createdTxns.size()
                               << " Time=" << elaspedTimeMs);
  }

  ReinstateMemPool(t_addrNonceTxnMap, gasLimitExceededTxnBuffer, droppedTxns);
}

void Node::PutTxnsInTempDataBase(
    const std::unordered_map<TxnHash, TransactionWithReceipt>&
        processedTransactions) {
  for (const auto& hashTxnPair : processedTransactions) {
    bytes serializedTxn;
    hashTxnPair.second.Serialize(serializedTxn, 0);
    BlockStorage::GetBlockStorage().PutProcessedTxBodyTmp(hashTxnPair.first,
                                                          serializedTxn);
  }
}

void Node::SaveTxnsToS3(
    const std::unordered_map<TxnHash, TransactionWithReceipt>&
        processedTransactions) {
  ostringstream oss;
  oss << "/tmp/txns_shard_" << m_myshardId << "_txblk_"
      << m_mediator.m_currentEpochNum;
  string txns_filename = oss.str();
  ofstream txns_file(txns_filename, std::fstream::binary);

  for (const auto& hashTxnPair : processedTransactions) {
    bytes serializedTxn;
    hashTxnPair.second.Serialize(serializedTxn, 0);

    // write HASH LEN and HASH
    size_t size = hashTxnPair.first.size;
    txns_file.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
    txns_file.write(reinterpret_cast<const char*>(hashTxnPair.first.data()),
                    size);

    // write TXN LEN AND TXN
    size = serializedTxn.size();
    txns_file.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
    txns_file.write(reinterpret_cast<const char*>(serializedTxn.data()), size);
  }

  txns_file.close();

  // upload to S3
  if (!SysCommand::ExecuteCmd(SysCommand::WITHOUT_OUTPUT,
                              GetAwsS3CpString(txns_filename))) {
    LOG_GENERAL(WARNING, "Failed to upload txns file :" << txns_filename);
  } else {
    LOG_GENERAL(DEBUG,
                "upload txns file : " << txns_filename << " successfully");
  }

  !SHARDLDR_SAVE_TXN_LOCALLY && std::remove(txns_filename.c_str());
}

std::string Node::GetAwsS3CpString(const std::string& uploadFilePath) {
  std::ostringstream ossS3Cmd;
  ossS3Cmd << "aws s3 cp " << uploadFilePath << " s3://" << BUCKET_NAME << "/"
           << TXN_PERSISTENCE_NAME << "/";
  return ossS3Cmd.str();
}

void Node::ReinstateMemPool(
    const map<Address, map<uint64_t, Transaction>>& addrNonceTxnMap,
    const vector<Transaction>& gasLimitExceededTxnBuffer,
    const vector<pair<TxnHash, TxnStatus>>& droppedTxns) {
  unique_lock<shared_timed_mutex> g(m_unconfirmedTxnsMutex);

  MempoolInsertionStatus status;
  // Put remaining txns back in pool
  for (const auto& kv : addrNonceTxnMap) {
    for (const auto& nonceTxn : kv.second) {
      t_createdTxns.insert(nonceTxn.second, status);
      LOG_GENERAL(INFO, "Txn " << nonceTxn.second.GetTranID() << ", Status: "
                               << status.first << "  " << status.second);
      m_unconfirmedTxns.emplace(nonceTxn.second.GetTranID(),
                                TxnStatus::PRESENT_NONCE_HIGH);
    }
  }

  for (const auto& t : gasLimitExceededTxnBuffer) {
    t_createdTxns.insert(t, status);
    LOG_GENERAL(INFO, "Txn " << t.GetTranID() << ", Status: " << status.first
                             << "  " << status.second);
    m_unconfirmedTxns.emplace(t.GetTranID(), TxnStatus::PRESENT_GAS_EXCEEDED);
  }

  for (const auto& txnHashStatus : droppedTxns) {
    LOG_GENERAL(INFO,
                "[DTXN]" << txnHashStatus.first << " " << txnHashStatus.second);
    m_unconfirmedTxns.emplace(txnHashStatus);
  }
}

void Node::PutProcessedInUnconfirmedTxns() {
  unique_lock<shared_timed_mutex> g(m_unconfirmedTxnsMutex);

  uint count = 0;

  for (const auto& t : t_processedTransactions) {
    m_unconfirmedTxns.emplace(t.first,
                              TxnStatus::PRESENT_VALID_CONSENSUS_NOT_REACHED);
    count++;
  }
  LOG_GENERAL(INFO, "Count of txns " << count);
}

TxnStatus Node::IsTxnInMemPool(const TxnHash& txhash) const {
  auto findTxnHashStatus = [txhash](
                               shared_timed_mutex& mut,
                               const HashCodeMap& t_hashCodeMap) -> TxnStatus {
    shared_lock<shared_timed_mutex> g(mut, defer_lock);
    // Try to lock for 100 ms
    if (!g.try_lock_for(chrono::milliseconds(100))) {
      return TxnStatus::ERROR;
    }
    const auto res = t_hashCodeMap.find(txhash);
    if (res != t_hashCodeMap.end()) {
      return res->second;
    }

    return TxnStatus::NOT_PRESENT;
  };

  if (LOOKUP_NODE_MODE) {
    const auto& unconfirmStatus =
        findTxnHashStatus(m_pendingTxnsMutex, m_pendingTxns.GetHashCodeMap());

    if ((unconfirmStatus == TxnStatus::NOT_PRESENT)) {
      return findTxnHashStatus(m_droppedTxnsMutex,
                               m_droppedTxns.GetHashCodeMap());
    }
    return unconfirmStatus;
  } else {
    const auto& unconfirmStatus =
        findTxnHashStatus(m_unconfirmedTxnsMutex, m_unconfirmedTxns);

    return unconfirmStatus;
  }
}

unordered_map<TxnHash, TxnStatus> Node::GetUnconfirmedTxns() const {
  shared_lock<shared_timed_mutex> g(m_unconfirmedTxnsMutex);

  return m_unconfirmedTxns;
}

HashCodeMap Node::GetPendingTxns() const {
  shared_lock<shared_timed_mutex> g(m_pendingTxnsMutex);
  return m_pendingTxns.GetHashCodeMap();
}

HashCodeMap Node::GetDroppedTxns() const {
  shared_lock<shared_timed_mutex> g(m_droppedTxnsMutex);
  return m_droppedTxns.GetHashCodeMap();
}

bool Node::IsUnconfirmedTxnEmpty() const {
  shared_lock<shared_timed_mutex> g(m_unconfirmedTxnsMutex);

  return m_unconfirmedTxns.empty();
}

void Node::UpdateBalanceForPreGeneratedAccounts() {
  LOG_MARKER();
  int counter = 0;
  for (unsigned int i = 0; i < m_populatedAddresses.size(); i++) {
    if ((i % (m_mediator.m_ds->m_shards.size() + 1) == m_myshardId) &&
        (i % NUM_FINAL_BLOCK_PER_POW ==
         (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW))) {
      AccountStore::GetInstance().IncreaseBalanceTemp(
          m_populatedAddresses.at(i), 1);
      counter++;
    }
  }
  LOG_GENERAL(INFO, "Number of pre-generated accounts get balance changed: "
                        << counter);
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::RunConsensusOnMicroBlockWhenShardLeader not "
                "expected to be called from LookUp node");
    return true;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am shard leader. Creating microblock for epoch "
                << m_mediator.m_currentEpochNum);

  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE &&
      !m_mediator.GetIsVacuousEpoch()) {
    std::this_thread::sleep_for(chrono::milliseconds(TX_DISTRIBUTE_TIME_IN_MS +
                                                     ANNOUNCEMENT_DELAY_IN_MS));
  }

  m_txn_distribute_window_open = false;

  if (m_mediator.ToProcessTransaction()) {
    ProcessTransactionWhenShardLeader(SHARD_MICROBLOCK_GAS_LIMIT);
    if (!AccountStore::GetInstance().SerializeDelta()) {
      LOG_GENERAL(WARNING, "AccountStore::SerializeDelta failed");
      return false;
    }
  }

  // composed microblock stored in m_microblock
  if (!ComposeMicroBlock(SHARD_MICROBLOCK_GAS_LIMIT)) {
    LOG_GENERAL(WARNING, "Unable to create microblock");
    return false;
  }

  // m_consensusID = 0;
  m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetMyHash()
                             .asBytes();

  {
    lock_guard<mutex> g(m_mutexShardMember);

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I am shard leader. "
                  << "m_consensusID: " << m_mediator.m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << (*m_myShardMembers)[m_consensusLeaderID].second);

    auto nodeMissingTxnsFunc = [this](const bytes& errorMsg,
                                      const Peer& from) mutable -> bool {
      return OnNodeMissingTxns(errorMsg, 0, from);
    };

    auto commitFailureFunc =
        [this](const map<unsigned int, bytes>& m) mutable -> bool {
      return OnCommitFailure(m);
    };

    m_consensusObject.reset(new ConsensusLeader(
        m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
        m_consensusBlockHash, m_consensusMyID, m_mediator.m_selfKey.first,
        *m_myShardMembers, static_cast<uint8_t>(NODE),
        static_cast<uint8_t>(MICROBLOCKCONSENSUS), nodeMissingTxnsFunc,
        commitFailureFunc, m_mediator.m_ds->m_mode != DirectoryService::IDLE));
  }

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  auto announcementGeneratorFunc =
      [this](bytes& dst, unsigned int offset, const uint32_t consensusID,
             const uint64_t blockNumber, const bytes& blockHash,
             const uint16_t leaderID, const PairOfKey& leaderKey,
             bytes& messageToCosign) mutable -> bool {
    return Messenger::SetNodeMicroBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_microblock, messageToCosign);
  };

  LOG_STATE(
      "[MICON]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "][" << m_myshardId << "] BEGIN");

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);

  return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardBackup() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::RunConsensusOnMicroBlockWhenShardBackup not "
                "expected to be called from LookUp node");
    return true;
  }

  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE &&
      !m_mediator.GetIsVacuousEpoch() &&
      ((m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty() >=
            TXN_SHARD_TARGET_DIFFICULTY &&
        m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetDSDifficulty() >= TXN_DS_TARGET_DIFFICULTY) ||
       m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
           TXN_DS_TARGET_NUM)) {
    std::this_thread::sleep_for(chrono::milliseconds(TX_DISTRIBUTE_TIME_IN_MS));
    ProcessTransactionWhenShardBackup(SHARD_MICROBLOCK_GAS_LIMIT);
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am a backup node. Waiting for microblock announcement for epoch "
                << m_mediator.m_currentEpochNum);
  // m_consensusID = 0;
  m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetMyHash()
                             .asBytes();

  auto func = [this](const bytes& input, unsigned int offset, bytes& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const bytes& blockHash, const uint16_t leaderID,
                     const PubKey& leaderKey,
                     bytes& messageToCosign) mutable -> bool {
    return MicroBlockValidator(input, offset, errorMsg, consensusID,
                               blockNumber, blockHash, leaderID, leaderKey,
                               messageToCosign);
  };

  DequeOfNode peerList;

  {
    lock_guard<mutex> g(m_mutexShardMember);
    LOG_GENERAL(INFO, "I am shard backup");
    LOG_GENERAL(INFO, "Leader IP    = "
                          << (*m_myShardMembers)[m_consensusLeaderID].second);

    for (const auto& it : *m_myShardMembers) {
      peerList.emplace_back(it);
    }
  }

  m_consensusObject.reset(new ConsensusBackup(
      m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
      m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
      m_mediator.m_selfKey.first, peerList, static_cast<uint8_t>(NODE),
      static_cast<uint8_t>(MICROBLOCKCONSENSUS), func));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unable to create consensus object");
    return false;
  }

  return true;
}

bool Node::RunConsensusOnMicroBlock() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::RunConsensusOnMicroBlock not expected to be called "
                "from LookUp node");
    return true;
  }

  LOG_MARKER();

  SetState(MICROBLOCK_CONSENSUS_PREP);
  m_txn_distribute_window_open = true;

  if (m_mediator.GetIsVacuousEpoch()) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Vacuous epoch: Skipping submit transactions");
    CleanCreatedTransaction();
  }

  if (m_isPrimary) {
    if (!RunConsensusOnMicroBlockWhenShardLeader()) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Error at RunConsensusOnMicroBlockWhenShardLeader");
      // throw exception();
      return false;
    }
  } else {
    if (!RunConsensusOnMicroBlockWhenShardBackup()) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Error at RunConsensusOnMicroBlockWhenShardBackup");
      // throw exception();
      return false;
    }
  }

  SetState(MICROBLOCK_CONSENSUS);

  CommitMicroBlockConsensusBuffer();

  return true;
}

bool Node::CheckMicroBlockVersion() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockVersion not expected to be called "
                "from LookUp node");
    return true;
  }

  // Check version (must be most current version)
  if (m_microblock->GetHeader().GetVersion() != MICROBLOCK_VERSION) {
    LOG_CHECK_FAIL("MicroBlock version", m_microblock->GetHeader().GetVersion(),
                   MICROBLOCK_VERSION);
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_MICROBLOCK_VERSION);

    return false;
  }

  LOG_GENERAL(INFO, "Version check passed");

  return true;
}

bool Node::CheckMicroBlockshardId() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockshardId not expected to be called "
                "from LookUp node");
    return true;
  }

  if (m_microblock->GetHeader().GetShardId() != m_myshardId) {
    LOG_CHECK_FAIL("Shard ID", m_microblock->GetHeader().GetShardId(),
                   m_myshardId);
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_MICROBLOCK_SHARD_ID);

    return false;
  }

  LOG_GENERAL(INFO, "shardId check passed");

  // Verify the shard committee hash
  CommitteeHash committeeHash;
  if (m_mediator.m_ds->m_mode == DirectoryService::IDLE) {
    if (!Messenger::GetShardHash(m_mediator.m_ds->m_shards.at(m_myshardId),
                                 committeeHash)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::GetShardHash failed");
      return false;
    }
  } else {
    if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                       committeeHash)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::GetDSCommitteeHash failed");
      return false;
    }
  }
  if (committeeHash != m_microblock->GetHeader().GetCommitteeHash()) {
    LOG_CHECK_FAIL("Committee hash",
                   m_microblock->GetHeader().GetCommitteeHash(), committeeHash);
    m_consensusObject->SetConsensusErrorCode(ConsensusCommon::INVALID_COMMHASH);
    return false;
  }

  return true;
}

bool Node::CheckMicroBlockTimestamp() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockTimestamp not expected to be called "
                "from LookUp node");
    return true;
  }

  LOG_MARKER();

  return VerifyTimestamp(m_microblock->GetTimestamp(),
                         CONSENSUS_OBJECT_TIMEOUT);
}

unsigned char Node::CheckLegitimacyOfTxnHashes(bytes& errorMsg) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckLegitimacyOfTxnHashes not expected to be "
                "called from LookUp node");
    return true;
  }

  if (!m_mediator.GetIsVacuousEpoch() &&
      ((m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty() >=
            TXN_SHARD_TARGET_DIFFICULTY &&
        m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetDSDifficulty() >= TXN_DS_TARGET_DIFFICULTY) ||
       m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
           TXN_DS_TARGET_NUM)) {
    vector<TxnHash> missingTxnHashes;
    if (!VerifyTxnsOrdering(m_microblock->GetTranHashes(), missingTxnHashes)) {
      LOG_GENERAL(WARNING, "The leader may have composed wrong order");
      return LEGITIMACYRESULT::WRONGORDER;
    }

    if (missingTxnHashes.size() > 0) {
      if (!Messenger::SetNodeMissingTxnsErrorMsg(
              errorMsg, 0, missingTxnHashes, m_mediator.m_currentEpochNum,
              m_mediator.m_selfPeer.m_listenPortHost)) {
        LOG_GENERAL(WARNING, "Messenger::SetNodeMissingTxnsErrorMsg failed");
        return false;
      }

      {
        lock_guard<mutex> g(m_mutexCreatedTransactions);
        LOG_GENERAL(WARNING, m_createdTxns);
      }

      AccountStore::GetInstance().InitTemp();
      if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE) {
        LOG_GENERAL(WARNING, "Got missing txns, revert state delta");
        if (!AccountStore::GetInstance().DeserializeDeltaTemp(
                m_mediator.m_ds->m_stateDeltaFromShards, 0)) {
          LOG_GENERAL(WARNING, "AccountStore::DeserializeDeltaTemp failed");
          return LEGITIMACYRESULT::DESERIALIZATIONERROR;
        } else {
          AccountStore::GetInstance().SerializeDelta();
        }
      }

      return LEGITIMACYRESULT::MISSEDTXN;
    }

    if (!AccountStore::GetInstance().SerializeDelta()) {
      LOG_GENERAL(WARNING, "AccountStore::SerializeDelta failed");
      return LEGITIMACYRESULT::SERIALIZATIONERROR;
    }
  } else {
    if (m_mediator.GetIsVacuousEpoch()) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Vacuous epoch: Skipping processing txns");
    } else {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Target diff or DS num not met: Skipping processing txns");
    }
  }

  return LEGITIMACYRESULT::SUCCESS;
}

bool Node::CheckMicroBlockGasLimit(const uint64_t& microblock_gas_limit) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockGasLimit not expected to be called "
                "from LookUp node");
    return true;
  }

  if (microblock_gas_limit != m_microblock->GetHeader().GetGasLimit()) {
    LOG_CHECK_FAIL("Gas limit", m_microblock->GetHeader().GetGasLimit(),
                   microblock_gas_limit);
    m_consensusObject->SetConsensusErrorCode(ConsensusCommon::WRONG_GASLIMIT);
    return false;
  }

  return true;
}

bool Node::CheckMicroBlockHashes(bytes& errorMsg) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockHashes not expected to be called "
                "from LookUp node");
    return true;
  }

  // Check transaction hashes (number of hashes must be = Tx count field)
  uint32_t txhashessize = m_microblock->GetTranHashes().size();
  uint32_t numtxs = m_microblock->GetHeader().GetNumTxs();
  if (txhashessize != numtxs) {
    LOG_GENERAL(WARNING, "Tx hashes check failed. Tx hashes size: "
                             << txhashessize << " Num txs: " << numtxs);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_BLOCK_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "Hash count check passed");

  switch (CheckLegitimacyOfTxnHashes(errorMsg)) {
    case LEGITIMACYRESULT::SUCCESS:
      break;
    case LEGITIMACYRESULT::MISSEDTXN:
      LOG_GENERAL(WARNING,
                  "Missing a txn hash included in proposed microblock");
      m_consensusObject->SetConsensusErrorCode(ConsensusCommon::MISSING_TXN);
      return false;
    case LEGITIMACYRESULT::WRONGORDER:
      LOG_GENERAL(WARNING, "Order of txns proposed by leader is wrong");
      m_consensusObject->SetConsensusErrorCode(
          ConsensusCommon::WRONG_TXN_ORDER);
      return false;
    default:
      return false;
  }

  // Check Gas Used
  if (m_gasUsedTotal != m_microblock->GetHeader().GetGasUsed()) {
    LOG_GENERAL(WARNING, "The total gas used mismatched, local: "
                             << m_gasUsedTotal << " received: "
                             << m_microblock->GetHeader().GetGasUsed());
    m_consensusObject->SetConsensusErrorCode(ConsensusCommon::WRONG_GASUSED);
    return false;
  }

  // Check Rewards
  if (m_mediator.GetIsVacuousEpoch() &&
      m_mediator.m_ds->m_mode != DirectoryService::IDLE) {
    // Check COINBASE_REWARD_PER_DS

    uint128_t rewards =
        (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
         COINBASE_UPDATE_TARGET_DS)
            ? COINBASE_REWARD_PER_DS_NEW
            : COINBASE_REWARD_PER_DS;

    if (rewards != m_microblock->GetHeader().GetRewards()) {
      LOG_CHECK_FAIL("Total rewards", m_microblock->GetHeader().GetRewards(),
                     rewards);
      m_consensusObject->SetConsensusErrorCode(ConsensusCommon::WRONG_REWARDS);
      return false;
    }
  } else {
    // Check TxnFees
    if (m_txnFees != m_microblock->GetHeader().GetRewards()) {
      LOG_CHECK_FAIL("Txn fees", m_microblock->GetHeader().GetRewards(),
                     m_txnFees);
      m_consensusObject->SetConsensusErrorCode(ConsensusCommon::WRONG_REWARDS);
      return false;
    }
  }

  LOG_GENERAL(INFO, "Hash legitimacy check passed");

  return true;
}

bool Node::CheckMicroBlockTxnRootHash() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockTxnRootHash not expected to be "
                "called from LookUp node");
    return true;
  }

  // Check transaction root
  TxnHash expectedTxRootHash = ComputeRoot(m_microblock->GetTranHashes());

  if (expectedTxRootHash != m_microblock->GetHeader().GetTxRootHash()) {
    LOG_CHECK_FAIL("Txn root hash", m_microblock->GetHeader().GetTxRootHash(),
                   expectedTxRootHash);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_MICROBLOCK_ROOT_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "Txn root hash    = " << expectedTxRootHash);

  return true;
}

bool Node::CheckMicroBlockStateDeltaHash() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockStateDeltaHash not expected to be "
                "called from LookUp node");
    return true;
  }

  StateHash expectedStateDeltaHash =
      AccountStore::GetInstance().GetStateDeltaHash();

  if (expectedStateDeltaHash != m_microblock->GetHeader().GetStateDeltaHash()) {
    LOG_CHECK_FAIL("State delta hash",
                   m_microblock->GetHeader().GetStateDeltaHash(),
                   expectedStateDeltaHash);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_MICROBLOCK_STATE_DELTA_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "State delta hash = " << expectedStateDeltaHash);

  return true;
}

bool Node::CheckMicroBlockTranReceiptHash() {
  TxnHash expectedTranHash;
  if (!TransactionWithReceipt::ComputeTransactionReceiptsHash(
          m_microblock->GetTranHashes(), t_processedTransactions,
          expectedTranHash)) {
    LOG_GENERAL(WARNING, "Cannot compute transaction receipts hash");
    return false;
  }

  if (expectedTranHash != m_microblock->GetHeader().GetTranReceiptHash()) {
    LOG_CHECK_FAIL("Txn receipt hash",
                   m_microblock->GetHeader().GetTranReceiptHash(),
                   expectedTranHash);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_MICROBLOCK_TRAN_RECEIPT_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "Txn receipt hash = " << expectedTranHash);

  return true;
}

bool Node::CheckMicroBlockValidity(bytes& errorMsg,
                                   const uint64_t& microblock_gas_limit) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CheckMicroBlockValidity not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  return CheckMicroBlockVersion() && CheckMicroBlockshardId() &&
         CheckMicroBlockTimestamp() &&
         CheckMicroBlockGasLimit(microblock_gas_limit) &&
         CheckMicroBlockHashes(errorMsg) && CheckMicroBlockTxnRootHash() &&
         CheckMicroBlockStateDeltaHash() && CheckMicroBlockTranReceiptHash();

  // Check gas limit (must satisfy some equations)
  // Check gas used (must be <= gas limit)
  // Check state root (TBD)
  // Check pubkey (must be valid and = shard leader)
  // Check parent DS hash (must be = digest of last DS block header in the DS
  // blockchain) Need some rework to be able to access DS blockchain (or we
  // switch to using the persistent storage lib) Check parent DS block number
  // (must be = block number of last DS block header in the DS blockchain) Need
  // some rework to be able to access DS blockchain (or we switch to using the
  // persistent storage lib)
}

bool Node::MicroBlockValidator(const bytes& message, unsigned int offset,
                               bytes& errorMsg, const uint32_t consensusID,
                               const uint64_t blockNumber,
                               const bytes& blockHash, const uint16_t leaderID,
                               const PubKey& leaderKey,
                               bytes& messageToCosign) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::MicroBlockValidator not expected to be called from "
                "LookUp node");
    return true;
  }

  LOG_MARKER();

  m_microblock.reset(new MicroBlock);

  if (!Messenger::GetNodeMicroBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_microblock, messageToCosign)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeMicroBlockAnnouncement failed");
    return false;
  }

  m_txn_distribute_window_open = false;

  if (!m_mediator.CheckWhetherBlockIsLatest(
          m_microblock->GetHeader().GetDSBlockNum() + 1,
          m_microblock->GetHeader().GetEpochNum())) {
    LOG_GENERAL(WARNING,
                "MicroBlockValidator CheckWhetherBlockIsLatest failed");
    return false;
  }

  BlockHash temp_blockHash = m_microblock->GetHeader().GetMyHash();
  if (temp_blockHash != m_microblock->GetBlockHash()) {
    LOG_CHECK_FAIL("Block hash", m_microblock->GetBlockHash().hex(),
                   temp_blockHash);
    return false;
  }

  if (!CheckMicroBlockValidity(errorMsg, SHARD_MICROBLOCK_GAS_LIMIT)) {
    m_microblock = nullptr;
    LOG_GENERAL(WARNING, "CheckMicroBlockValidity failed");
    return false;
  }

  return true;
}
