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
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <chrono>
#include <functional>
#include <limits>
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
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libPOW/pow.h"
#include "libServer/JSONConversion.h"
#include "libServer/LookupServer.h"
#include "libServer/WebsocketServer.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;
using namespace boost::multiprecision;

bool Node::StoreFinalBlock(const TxBlock& txBlock) {
  LOG_MARKER();

  AddBlock(txBlock);

  // At this point, the transactions in the last Epoch is no longer useful, thus
  // erase. EraseCommittedTransactions(m_mediator.m_currentEpochNum - 2);

  LOG_GENERAL(INFO, "Storing TxBlock:" << endl << txBlock);

  // Store Tx Block to disk
  bytes serializedTxBlock;
  txBlock.Serialize(serializedTxBlock, 0);
  if (!BlockStorage::GetBlockStorage().PutTxBlock(
          txBlock.GetHeader().GetBlockNum(), serializedTxBlock)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
    return false;
  }

  m_mediator.IncreaseEpochNum();

  LOG_STATE(
      "[FINBK]["
      << std::setw(15) << std::left
      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] RECV");

  return true;
}

bool Node::IsMicroBlockTxRootHashInFinalBlock(
    const MBnForwardedTxnEntry& entry, bool& isEveryMicroBlockAvailable) {
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Deleting unavailable microblock: " << entry);
  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
  auto it = m_unavailableMicroBlocks.find(
      entry.m_microBlock.GetHeader().GetEpochNum());
  bool found = (it != m_unavailableMicroBlocks.end() &&
                RemoveTxRootHashFromUnavailableMicroBlock(entry));
  isEveryMicroBlockAvailable = found && it->second.empty();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);
  return found;
}

bool Node::LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                           bool& toSendTxnToLookup,
                                           bool skipShardIDCheck) {
  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);

  uint64_t blockNum = finalBlock.GetHeader().GetBlockNum();
  const auto& microBlockInfos = finalBlock.GetMicroBlockInfos();

  bool foundMB = false;
  // bool doRejoin = false;

  for (const auto& info : microBlockInfos) {
    if (LOOKUP_NODE_MODE) {
      // Add all mbhashes to unavailable list if newlookup/levellookup is
      // syncing. Otherwise respect the check condition.
      if (skipShardIDCheck ||
          !(info.m_shardId == m_mediator.m_ds->m_shards.size() &&
            info.m_txnRootHash == TxnHash())) {
        auto& mbs = m_unavailableMicroBlocks[blockNum];
        if (std::find_if(mbs.begin(), mbs.end(),
                         [info](const std::pair<BlockHash, TxnHash>& e) {
                           return e.first == info.m_microBlockHash;
                         }) == mbs.end()) {
          mbs.push_back({info.m_microBlockHash, info.m_txnRootHash});
          LOG_GENERAL(
              INFO,
              "[TxBlk:" << blockNum << "] Add unavailable block [MbBlockHash] "
                        << info.m_microBlockHash << " [TxnRootHash] "
                        << info.m_txnRootHash << " shardID " << info.m_shardId);
        }
      }
    } else {
      if (info.m_shardId == m_myshardId) {
        if (m_microblock == nullptr) {
          LOG_GENERAL(WARNING,
                      "Found my shard microblock but microblock obj "
                      "not initiated");
          // doRejoin = true;
        } else if (m_lastMicroBlockCoSig.first !=
                   m_mediator.m_currentEpochNum) {
          LOG_GENERAL(WARNING,
                      "Found my shard microblock but Cosig not updated");
          // doRejoin = true;
        } else if (m_microblock->GetBlockHash() == info.m_microBlockHash) {
          // Update transaction processed
          foundMB = true;
          UpdateProcessedTransactions();
          toSendTxnToLookup = true;
        } else {
          LOG_GENERAL(WARNING,
                      "The microblock hashes in finalblock doesn't "
                      "match with the local one"
                          << endl
                          << "expected: " << m_microblock->GetBlockHash()
                          << endl
                          << "received: " << info.m_microBlockHash)
          return false;
        }

        break;
      }
    }
  }

  if (!foundMB && !LOOKUP_NODE_MODE) {
    LOG_GENERAL(INFO, "My MB not in FB");
    PutProcessedInUnconfirmedTxns();
  }

  if (/*doRejoin || */ m_doRejoinAtFinalBlock) {
    LOG_GENERAL(WARNING,
                "Failed the last microblock consensus but "
                "still found my shard microblock, "
                " need to Rejoin");
    RejoinAsNormal();
    return false;
  }

  return true;
}

bool Node::RemoveTxRootHashFromUnavailableMicroBlock(
    const MBnForwardedTxnEntry& entry) {
  for (auto it = m_unavailableMicroBlocks
                     .at(entry.m_microBlock.GetHeader().GetEpochNum())
                     .begin();
       it !=
       m_unavailableMicroBlocks.at(entry.m_microBlock.GetHeader().GetEpochNum())
           .end();
       it++) {
    if (it->first == entry.m_microBlock.GetBlockHash()) {
      TxnHash txnHash = ComputeRoot(entry.m_transactions);
      if (it->second != txnHash) {
        LOG_CHECK_FAIL("Txn root hash", txnHash, it->second);
        return false;
      }

      LOG_GENERAL(INFO, "MB found" << it->first);
      LOG_GENERAL(INFO,
                  "Count before = "
                      << m_unavailableMicroBlocks
                             .at(entry.m_microBlock.GetHeader().GetEpochNum())
                             .size());
      m_unavailableMicroBlocks.at(entry.m_microBlock.GetHeader().GetEpochNum())
          .erase(it);
      LOG_GENERAL(INFO,
                  "Count after  = "
                      << m_unavailableMicroBlocks
                             .at(entry.m_microBlock.GetHeader().GetEpochNum())
                             .size());
      return true;
    }
  }

  LOG_GENERAL(WARNING, "MB not found = " << entry);
  return false;
}

bool Node::VerifyFinalBlockCoSignature(const TxBlock& txblock) {
  LOG_MARKER();

  unsigned int index = 0;
  unsigned int count = 0;

  const vector<bool>& B2 = txblock.GetB2();
  if (m_mediator.m_DSCommittee->size() != B2.size()) {
    LOG_CHECK_FAIL("Cosig size", B2.size(), m_mediator.m_DSCommittee->size());
    return false;
  }

  // Generate the aggregated key
  vector<PubKey> keys;
  for (auto const& kv : *m_mediator.m_DSCommittee) {
    if (B2.at(index)) {
      keys.emplace_back(kv.first);
      count++;
    }
    index++;
  }

  if (count != ConsensusCommon::NumForConsensus(B2.size())) {
    LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
    return false;
  }

  shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
  if (aggregatedKey == nullptr) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    return false;
  }

  // Verify the collective signature
  bytes message;
  if (!txblock.GetHeader().Serialize(message, 0)) {
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed");
    return false;
  }
  txblock.GetCS1().Serialize(message, message.size());
  BitVector::SetBitVector(message, message.size(), txblock.GetB1());
  if (!MultiSig::MultiSigVerify(message, 0, message.size(), txblock.GetCS2(),
                                *aggregatedKey)) {
    LOG_GENERAL(WARNING, "Cosig verification failed");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return false;
  }

  return true;
}

void Node::InitiatePoW() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::InitiatePoW not expected to be called from LookUp node.");
    return;
  }

  SetState(POW_SUBMISSION);

  if (m_mediator.m_disablePoW) {
    LOG_GENERAL(INFO, "Skipping PoW");
    return;
  }

  POW::GetInstance().EthashConfigureClient(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
      FULL_DATASET_MINE);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Start pow ");
  auto func = [this]() mutable -> void {
    auto epochNumber =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
    auto dsBlockRand = m_mediator.m_dsBlockRand;
    auto txBlockRand = m_mediator.m_txBlockRand;
    StartPoW(
        epochNumber,
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty(),
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty(),
        dsBlockRand, txBlockRand);
  };

  DetachedFunction(1, func);
}

void Node::UpdateStateForNextConsensusRound() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::UpdateStateForNextConsensusRound not expected to be "
                "called from LookUp node.");
    return;
  }

  // Set state to tx submission
  if (m_isPrimary) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I am no longer the shard leader ");
    m_isPrimary = false;
  }

  m_mediator.m_consensusID++;

  uint16_t lastBlockHash = DataConversion::charArrTo16Bits(
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
  {
    lock_guard<mutex> g(m_mutexShardMember);

    if (m_mediator.m_ds->m_mode != DirectoryService::IDLE && GUARD_MODE) {
      m_consensusLeaderID =
          lastBlockHash % Guard::GetInstance().GetNumOfDSGuard();
    } else {
      m_consensusLeaderID = CalculateShardLeaderFromDequeOfNode(
          lastBlockHash, m_myShardMembers->size(), *m_myShardMembers);
    }
  }

  if (m_consensusMyID == m_consensusLeaderID) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I am the new shard leader of shard " << m_myshardId);
    LOG_STATE("[IDENT][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "]["
                         << m_myshardId << "][  0] SCLD");
    m_isPrimary = true;
  } else {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "The new shard leader is m_consensusLeaderID " << m_consensusLeaderID);
  }
}

void Node::ScheduleMicroBlockConsensus() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ScheduleMicroBlockConsensus not expected to be "
                "called from LookUp node.");
    return;
  }

  auto main_func = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

  DetachedFunction(1, main_func);
}

void Node::BeginNextConsensusRound() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::BeginNextConsensusRound not expected to be called "
                "from LookUp node.");
    return;
  }

  LOG_MARKER();

  UpdateStateForNextConsensusRound();

  ScheduleMicroBlockConsensus();

  CommitTxnPacketBuffer();
}

bool Node::FindTxnInProcessedTxnsList(
    const uint64_t& blockNum, uint8_t sharing_mode,
    vector<TransactionWithReceipt>& txns_to_send, const TxnHash& tx_hash) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::FindTxnInSubmittedTxnsList not expected to be "
                "called from LookUp node.");
    return true;
  }

  lock_guard<mutex> g(m_mutexProcessedTransactions);

  const auto& processedTransactions = m_processedTransactions[blockNum];
  // auto& committedTransactions = m_committedTransactions[blockNum];
  const auto& txnIt = processedTransactions.find(tx_hash);

  // Check if transaction is part of submitted Tx list
  if (txnIt != processedTransactions.end()) {
    if ((sharing_mode == SEND_ONLY) || (sharing_mode == SEND_AND_FORWARD)) {
      txns_to_send.emplace_back(txnIt->second);
    }

    // Move entry from submitted Tx list to committed Tx list
    // committedTransactions.push_back(txnIt->second);
    // processedTransactions.erase(txnIt);

    // Move on to next transaction in block
    return true;
  }

  return false;
}

void Node::CallActOnFinalblock() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CallActOnFinalblock not expected to be called from "
                "LookUp node.");
    return;
  }

  LOG_MARKER();

  auto composeMBnForwardTxnMessageForSender =
      [this](bytes& forwardtxn_message) -> bool {
    return ComposeMBnForwardTxnMessageForSender(forwardtxn_message);
  };

  auto sendMbnFowardTxnToShardNodes =
      []([[gnu::unused]] const bytes& message,
         [[gnu::unused]] const DequeOfShard& shards,
         [[gnu::unused]] const unsigned int& my_shards_lo,
         [[gnu::unused]] const unsigned int& my_shards_hi) -> void {};

  lock_guard<mutex> g(m_mutexShardMember);

  DataSender::GetInstance().SendDataToOthers(
      *m_microblock, *m_myShardMembers, {}, {},
      m_mediator.m_lookup->GetLookupNodes(),
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash(), m_consensusMyID,
      composeMBnForwardTxnMessageForSender, false, SendDataToLookupFuncDefault,
      sendMbnFowardTxnToShardNodes);
}

bool Node::ComposeMBnForwardTxnMessageForSender(bytes& mb_txns_message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeMBnForwardTxnMessageForSender not expected to be "
                "called from LookUp node.");
    return false;
  }

  std::vector<TransactionWithReceipt> txns_to_send;

  if (m_microblock == nullptr) {
    return false;
  }

  const auto& blocknum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  {
    const vector<TxnHash>& tx_hashes = m_microblock->GetTranHashes();
    lock_guard<mutex> g(m_mutexProcessedTransactions);
    auto& processedTransactions =
        m_mediator.m_ds->m_mode == DirectoryService::IDLE
            ? t_processedTransactions
            : m_processedTransactions[blocknum];
    for (const auto& tx_hash : tx_hashes) {
      const auto& txnIt = processedTransactions.find(tx_hash);
      if (txnIt != processedTransactions.end()) {
        txns_to_send.emplace_back(txnIt->second);
      } else {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Failed trying to find txn " << tx_hash
                                               << " in processed txn list");
      }
    }
  }
  // Transaction body sharing
  mb_txns_message = {MessageType::NODE,
                     NodeInstructionType::MBNFORWARDTRANSACTION};

  if (!Messenger::SetNodeMBnForwardTransaction(
          mb_txns_message, MessageOffset::BODY, *m_microblock, txns_to_send)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeMBnForwardTransaction failed.");
    return false;
  }

  LOG_STATE(
      "[TXBOD]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] BEFORE SENDING MB & FORWARDING TXN BODIES #" << blocknum);

  LOG_GENERAL(INFO, "[SendMBnTxn]"
                        << " Sending lookup :"
                        << m_microblock->GetHeader().GetShardId()
                        << " Epoch:" << m_mediator.m_currentEpochNum);

  return true;
}

bool Node::CheckStateRoot(const TxBlock& finalBlock) {
  StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

  // AccountStore::GetInstance().PrintAccountState();

  if (stateRoot != finalBlock.GetHeader().GetStateRootHash()) {
    LOG_CHECK_FAIL("State root hash", finalBlock.GetHeader().GetStateRootHash(),
                   stateRoot);
    return false;
  }

  LOG_GENERAL(INFO, "State root hash = " << stateRoot);

  return true;
}

void Node::PrepareGoodStateForFinalBlock() {
  if (m_state == MICROBLOCK_CONSENSUS || m_state == MICROBLOCK_CONSENSUS_PREP) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I may have missed the micrblock consensus. However, if I "
              "recently received a valid finalblock, I will accept it");
    // TODO: Optimize state transition.
    SetState(WAITING_FINALBLOCK);
  }
}

bool Node::ProcessVCFinalBlock(const bytes& message, unsigned int offset,
                               [[gnu::unused]] const Peer& from) {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP || MULTIPLIER_SYNC_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::ProcessVCFinalBlock not expected to be "
        "called by other than seed node without multiplier syncing mode.");
    return false;
  }
  return ProcessVCFinalBlockCore(message, offset, from);
}

bool Node::ProcessVCFinalBlockCore(const bytes& message, unsigned int offset,
                                   [[gnu::unused]] const Peer& from) {
  LOG_MARKER();
  uint64_t dsBlockNumber = 0;
  uint32_t consensusID = 0;
  TxBlock txBlock;
  bytes stateDelta;
  std::vector<VCBlock> vcBlocks;

  if (!Messenger::GetNodeVCFinalBlock(message, offset, dsBlockNumber,
                                      consensusID, txBlock, stateDelta,
                                      vcBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeVCFinalBlock failed.");
    return false;
  }

  for (const auto& vcBlock : vcBlocks) {
    if (!ProcessVCBlockCore(vcBlock)) {
      LOG_GENERAL(WARNING, "view change failed for vc blocknum "
                               << vcBlock.GetHeader().GetViewChangeCounter());
      return false;
    }
  }

  if (ProcessFinalBlockCore(dsBlockNumber, consensusID, txBlock, stateDelta,
                            message.size())) {
    if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
      {
        unique_lock<mutex> lock(
            m_mediator.m_lookup->m_mutexVCFinalBlockProcessed);
        m_mediator.m_lookup->m_vcFinalBlockProcessed = true;
      }
      m_mediator.m_lookup->cv_vcFinalBlockProcessed.notify_all();
    }
    return true;
  }

  return false;
}

bool Node::ProcessFinalBlock(const bytes& message, unsigned int offset,
                             [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  uint64_t dsBlockNumber = 0;
  uint32_t consensusID = 0;
  TxBlock txBlock;
  bytes stateDelta;

  if (LOOKUP_NODE_MODE) {
    if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
      // Buffer the Final Block
      lock_guard<mutex> g(m_mutexSeedTxnBlksBuffer);
      m_seedTxnBlksBuffer.push_back(message);
      LOG_GENERAL(INFO, "Seed not synced, buffered this FBLK");
      return false;
    } else {
      // If seed node is synced and have buffered txn blocks
      lock_guard<mutex> g(m_mutexSeedTxnBlksBuffer);
      if (!m_seedTxnBlksBuffer.empty()) {
        LOG_GENERAL(INFO, "Seed synced, processing buffered FBLKS");
        for (const auto& message : m_seedTxnBlksBuffer) {
          if (!Messenger::GetNodeFinalBlock(message, offset, dsBlockNumber,
                                            consensusID, txBlock, stateDelta)) {
            LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                      "Messenger::GetNodeFinalBlock failed.");
            return false;
          }
          if (!ProcessFinalBlockCore(dsBlockNumber, consensusID, txBlock,
                                     stateDelta, message.size())) {
            // ignore bufferred final blocks because rejoin must have been
            // already
            break;
          }
        }
        // clear the buffer since all buffered ones are checked and processed
        m_seedTxnBlksBuffer.clear();
      }
    }
  }

  if (!Messenger::GetNodeFinalBlock(message, offset, dsBlockNumber, consensusID,
                                    txBlock, stateDelta)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeFinalBlock failed.");
    return false;
  }

  if (ProcessFinalBlockCore(dsBlockNumber, consensusID, txBlock, stateDelta,
                            message.size())) {
    if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && MULTIPLIER_SYNC_MODE) {
      // Reached here. Final block was processed successfully.
      // Avoid using the original message in case it contains
      // excess data beyond the FINALBLOCK
      bytes vc_fb_message = {MessageType::NODE,
                             NodeInstructionType::VCFINALBLOCK};
      /*
        Check if the VCBlock exist in local store for key:
        txBlock.GetHeader().GetBlockNum()
      */
      std::lock_guard<mutex> g1(m_mutexvcBlocksStore);
      if (!Messenger::SetNodeVCFinalBlock(vc_fb_message, MessageOffset::BODY,
                                          dsBlockNumber, consensusID, txBlock,
                                          stateDelta, m_vcBlockStore)) {
        LOG_GENERAL(WARNING, "Messenger::SetNodeVCFinalBlock failed");
      } else {
        // Store to local map for VCFINALBLOCK
        m_vcFinalBlockStore[txBlock.GetHeader().GetBlockNum()] = vc_fb_message;
      }
      // Clear the vc blocks store
      m_vcBlockStore.clear();
    }
    return true;
  }

  return false;
}

bool Node::ProcessFinalBlockCore(uint64_t& dsBlockNumber,
                                 [[gnu::unused]] uint32_t& consensusID,
                                 TxBlock& txBlock, bytes& stateDelta,
                                 const uint64_t& messageSize) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexFinalBlock);
  if (txBlock.GetHeader().GetVersion() != TXBLOCK_VERSION) {
    LOG_CHECK_FAIL("TxBlock version", txBlock.GetHeader().GetVersion(),
                   TXBLOCK_VERSION);
    return false;
  }

  BlockHash temp_blockHash = txBlock.GetHeader().GetMyHash();
  if (temp_blockHash != txBlock.GetBlockHash()) {
    LOG_CHECK_FAIL("Block Hash", txBlock.GetBlockHash(), temp_blockHash);
    return false;
  }

  // Check timestamp
  if (!VerifyTimestamp(
          txBlock.GetTimestamp(),
          CONSENSUS_OBJECT_TIMEOUT + MICROBLOCK_TIMEOUT +
              (TX_DISTRIBUTE_TIME_IN_MS + ANNOUNCEMENT_DELAY_IN_MS) / 1000)) {
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  if (committeeHash != txBlock.GetHeader().GetCommitteeHash()) {
    LOG_CHECK_FAIL("DS committee hash", txBlock.GetHeader().GetCommitteeHash(),
                   committeeHash);
    return false;
  }

  if (LOOKUP_NODE_MODE) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Deserialized TxBlock" << endl
                                     << txBlock);
  }

  LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                       << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                       << txBlock.GetHeader().GetBlockNum() << "] FRST");

  if (LOOKUP_NODE_MODE && LOG_PARAMETERS) {
    uint64_t timeDiff = txBlock.GetTimestamp() -
                        m_mediator.m_txBlockChain.GetLastBlock().GetTimestamp();

    const double oneMillion = 1000000.0;

    cpp_dec_float_50 td_float(timeDiff);
    cpp_dec_float_50 numTxns(txBlock.GetHeader().GetNumTxs());
    td_float = td_float / 1000;

    LOG_STATE("[FBSTAT][" << m_mediator.m_currentEpochNum
                          << "] Size=" << messageSize << " Time=" << td_float
                          << " TPS=" << numTxns * oneMillion / timeDiff
                          << " Gas=" << txBlock.GetHeader().GetGasUsed())
  }

  // Verify the co-signature
  if (!VerifyFinalBlockCoSignature(txBlock)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "TxBlock co-sig verification failed");
    return false;
  }

  // Check block number. Now put after verify co-sig to prevent malicious Tx
  // block message to make the node rejoin.
  if (!m_mediator.CheckWhetherBlockIsLatest(
          dsBlockNumber + 1, txBlock.GetHeader().GetBlockNum())) {
    LOG_GENERAL(WARNING, "ProcessFinalBlock CheckWhetherBlockIsLatest failed");

    // Missed some ds block, rejoin
    if (dsBlockNumber >
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()) {
      if (!LOOKUP_NODE_MODE) {
        RejoinAsNormal();
      } else if (ARCHIVAL_LOOKUP) {
        // Sync from S3
        m_mediator.m_lookup->RejoinAsNewLookup(false);
      } else  // Lookup
      {
        m_mediator.m_lookup->RejoinAsLookup();
      }
    }
    // Missed some final block, rejoin
    else if (txBlock.GetHeader().GetBlockNum() > m_mediator.m_currentEpochNum) {
      if (!LOOKUP_NODE_MODE) {
        if (txBlock.GetHeader().GetBlockNum() - m_mediator.m_currentEpochNum <=
            NUM_FINAL_BLOCK_PER_POW) {
          LOG_GENERAL(INFO, "Syncing as normal node from seeds ...");
          m_mediator.m_lookup->SetSyncType(SyncType::NORMAL_SYNC);
          auto func = [this]() mutable -> void { StartSynchronization(); };
          DetachedFunction(1, func);
        } else {
          RejoinAsNormal();
        }
      } else if (ARCHIVAL_LOOKUP) {
        // Too many txblks ( and corresponding mb/txns) to be fetch from lookup.
        // so sync from S3 instead
        if (txBlock.GetHeader().GetBlockNum() - m_mediator.m_currentEpochNum >
            NUM_FINAL_BLOCK_PER_POW) {
          m_mediator.m_lookup->RejoinAsNewLookup(false);
        } else {
          // Sync from lookup
          m_mediator.m_lookup->RejoinAsNewLookup(true);
        }
      } else  // Lookup
      {
        m_mediator.m_lookup->RejoinAsLookup();
      }
    }
    return false;
  }

  // Compute the MBInfoHash of the extra MicroBlock information
  MBInfoHash mbInfoHash;
  if (!Messenger::GetMbInfoHash(txBlock.GetMicroBlockInfos(), mbInfoHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetMbInfoHash failed.");
    return false;
  }

  if (mbInfoHash != txBlock.GetHeader().GetMbInfoHash()) {
    LOG_CHECK_FAIL("MBInfo hash", txBlock.GetHeader().GetMbInfoHash(),
                   mbInfoHash);
    return false;
  }

  if (!LOOKUP_NODE_MODE) {
    if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum) {
      std::unique_lock<mutex> cv_lk(m_MutexCVFBWaitMB);
      if (cv_FBWaitMB.wait_for(
              cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW)) ==
          std::cv_status::timeout) {
        LOG_GENERAL(WARNING, "Timeout, I didn't finish microblock consensus");
      }
    }

    PrepareGoodStateForFinalBlock();

    if (!CheckState(PROCESS_FINALBLOCK)) {
      return false;
    }
  }

  if (LOG_PARAMETERS) {
    LOG_STATE("[FLBLKRECV][" << m_mediator.m_currentEpochNum
                             << "] Shard=" << m_myshardId);
  } else {
    LOG_STATE("[FLBLK][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum
                         << "] RECVD FLBLK");
  }

  bool toSendTxnToLookup = false;

  const bool& toSendPendingTxn = !(IsUnconfirmedTxnEmpty());

  bool isVacuousEpoch = m_mediator.GetIsVacuousEpoch();
  m_isVacuousEpochBuffer = isVacuousEpoch;

  if (!ProcessStateDeltaFromFinalBlock(
          stateDelta, txBlock.GetHeader().GetStateDeltaHash())) {
    return false;
  }

  if (isVacuousEpoch) {
    unordered_map<Address, int256_t> addressMap;
    if (!Messenger::StateDeltaToAddressMap(stateDelta, 0, addressMap)) {
      LOG_GENERAL(WARNING, "Messenger::StateDeltaToAccountMap failed");
    } else {
      auto it = addressMap.find(
          Account::GetAddressFromPublicKey(m_mediator.m_selfKey.second));
      if (it != addressMap.end()) {
        auto reward = it->second;
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "[REWARD]"
                      << " Got " << reward << " as reward");
        LOG_STATE("[REWARD][" << setw(15) << left
                              << m_mediator.m_selfPeer.GetPrintableIPAddress()
                              << "][" << m_mediator.m_currentEpochNum << "]["
                              << reward << "] FLBLK");
      } else {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "[REWARD]"
                      << "Got no reward this ds epoch");
      }
    }
  }

  if (!BlockStorage::GetBlockStorage().PutStateDelta(
          txBlock.GetHeader().GetBlockNum(), stateDelta)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutStateDelta failed");
    return false;
  }

  if (!LOOKUP_NODE_MODE &&
      (!CheckStateRoot(txBlock) || m_doRejoinAtStateRoot)) {
    RejoinAsNormal();
    return false;
  } else if (LOOKUP_NODE_MODE && !CheckStateRoot(txBlock)) {
    return false;
  }

  auto resumeBlackList = []() mutable -> void {
    this_thread::sleep_for(chrono::seconds(RESUME_BLACKLIST_DELAY_IN_SECONDS));
    Blacklist::GetInstance().Enable(true);
  };

  DetachedFunction(1, resumeBlackList);

  if (!LoadUnavailableMicroBlockHashes(txBlock, toSendTxnToLookup)) {
    return false;
  }

  if (!isVacuousEpoch) {
    if (!StoreFinalBlock(txBlock)) {
      LOG_GENERAL(WARNING, "StoreFinalBlock failed!");
      return false;
    }

    // if lookup and loaded microblocks, then skip
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    if (!(LOOKUP_NODE_MODE &&
          m_unavailableMicroBlocks.find(txBlock.GetHeader().GetBlockNum()) !=
              m_unavailableMicroBlocks.end())) {
      if (!BlockStorage::GetBlockStorage().PutEpochFin(
              m_mediator.m_currentEpochNum)) {
        LOG_GENERAL(WARNING, "BlockStorage::PutEpochFin failed "
                                 << m_mediator.m_currentEpochNum);
        return false;
      }
    }
  } else {
    LOG_GENERAL(INFO, "isVacuousEpoch now");

    // Check whether any ds guard change network info
    if (!LOOKUP_NODE_MODE) {
      QueryLookupForDSGuardNetworkInfoUpdate();
    }

    // Remove because shard nodes will be shuffled in next epoch.
    CleanMicroblockConsensusBuffer();

    if (!StoreFinalBlock(txBlock)) {
      LOG_GENERAL(WARNING, "StoreFinalBlock failed!");
      return false;
    }
    auto writeStateToDisk = [this]() -> void {
      if (!AccountStore::GetInstance().MoveUpdatesToDisk()) {
        LOG_GENERAL(WARNING, "MoveUpdatesToDisk failed, what to do?");
        // return false;
      } else {
        if (!BlockStorage::GetBlockStorage().PutLatestEpochStatesUpdated(
                m_mediator.m_currentEpochNum)) {
          LOG_GENERAL(WARNING, "BlockStorage::PutLatestEpochStatesUpdated "
                                   << m_mediator.m_currentEpochNum
                                   << " failed");
          return;
        }
        if (!LOOKUP_NODE_MODE) {
          if (!BlockStorage::GetBlockStorage().PutMetadata(
                  MetaType::DSINCOMPLETED, {'0'})) {
            LOG_GENERAL(WARNING,
                        "BlockStorage::PutMetadata (DSINCOMPLETED) '0' failed");
            return;
          }
          if (!BlockStorage::GetBlockStorage().PutEpochFin(
                  m_mediator.m_currentEpochNum)) {
            LOG_GENERAL(WARNING, "BlockStorage::PutEpochFin failed "
                                     << m_mediator.m_currentEpochNum);
            return;
          }
        } else {
          // change if all microblock received from shards
          lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
          if (m_unavailableMicroBlocks.find(
                  m_mediator.m_txBlockChain.GetLastBlock()
                      .GetHeader()
                      .GetBlockNum()) == m_unavailableMicroBlocks.end()) {
            if (!BlockStorage::GetBlockStorage().PutMetadata(
                    MetaType::DSINCOMPLETED, {'0'})) {
              LOG_GENERAL(WARNING,
                          "BlockStorage::PutMetadata DSINCOMPLETED '0' failed");
            }
            if (!BlockStorage::GetBlockStorage().PutEpochFin(
                    m_mediator.m_currentEpochNum)) {
              LOG_GENERAL(WARNING, "BlockStorage::PutEpochFin failed "
                                       << m_mediator.m_currentEpochNum);
              return;
            }
          }
        }
        LOG_STATE("[FLBLK][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetBlockNum() +
                                    1
                             << "] FINISH WRITE STATE TO DISK");
        if (ENABLE_ACCOUNTS_POPULATING &&
            m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
                PREGEN_ACCOUNT_TIMES) {
          PopulateAccounts();
        }
      }
    };
    DetachedFunction(1, writeStateToDisk);
  }

  // m_mediator.HeartBeatPulse();

  if (txBlock.GetMicroBlockInfos().size() == 1) {
    LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << txBlock.GetHeader().GetBlockNum()
                         << "] LAST");
  }

  // Assumption: New PoW done after every block committed
  // If I am not a DS committee member (and since I got this FinalBlock message,
  // then I know I'm not), I can start doing PoW again
  m_mediator.UpdateDSBlockRand();
  m_mediator.UpdateTxBlockRand();

  LOG_GENERAL(INFO, "toSendPendingTxn " << toSendPendingTxn);

  if (!LOOKUP_NODE_MODE) {
    if (toSendPendingTxn) {
      SendPendingTxnToLookup();
    }
    ClearUnconfirmedTxn();
    if (isVacuousEpoch) {
      InitiatePoW();
    } else {
      auto main_func = [this]() mutable -> void { BeginNextConsensusRound(); };

      DetachedFunction(1, main_func);
    }
  } else {
    if (!isVacuousEpoch) {
      m_mediator.m_consensusID++;
      m_consensusLeaderID++;
      m_consensusLeaderID = m_consensusLeaderID % m_mediator.GetShardSize(true);
    }
    ClearPendingAndDroppedTxn();
    // Now only forwarded txn are left, so only call in lookup

    uint32_t numShards = m_mediator.m_ds->GetNumShards();

    CommitMBnForwardedTransactionBuffer();
    CommitPendingTxnBuffer();
    if (!ARCHIVAL_LOOKUP && m_mediator.m_lookup->GetIsServer() &&
        !isVacuousEpoch && !m_mediator.GetIsVacuousEpoch() &&
        ((m_mediator.m_currentEpochNum + NUM_VACUOUS_EPOCHS + 1) %
         NUM_FINAL_BLOCK_PER_POW) != 0) {
      m_mediator.m_lookup->SenderTxnBatchThread(numShards);
    }

    m_mediator.m_lookup->CheckAndFetchUnavailableMBs(
        true);  // except last block
  }

  FallbackTimerPulse();

  return true;
}

bool Node::ProcessStateDeltaFromFinalBlock(
    const bytes& stateDeltaBytes, const StateHash& finalBlockStateDeltaHash) {
  LOG_MARKER();

  // Init local AccountStoreTemp first
  AccountStore::GetInstance().InitTemp();

  LOG_GENERAL(INFO,
              "State delta root hash = " << finalBlockStateDeltaHash.hex());

  if (finalBlockStateDeltaHash == StateHash()) {
    LOG_GENERAL(INFO,
                "State Delta hash received from finalblock is null, "
                "skip processing state delta");
    AccountStore::GetInstance().CommitTemp();
    return true;
  }

  if (stateDeltaBytes.empty()) {
    LOG_GENERAL(WARNING, "Cannot get state delta from message");
    return false;
  }

  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(stateDeltaBytes);
  StateHash stateDeltaHash(sha2.Finalize());

  if (stateDeltaHash != finalBlockStateDeltaHash) {
    LOG_CHECK_FAIL("State delta hash", finalBlockStateDeltaHash,
                   stateDeltaHash);
    return false;
  }

  LOG_GENERAL(INFO, "State delta hash = " << stateDeltaHash);

  // Deserialize State Delta
  if (finalBlockStateDeltaHash == StateHash()) {
    LOG_GENERAL(INFO, "State Delta from finalblock is empty");
    return false;
  }

  if (!AccountStore::GetInstance().DeserializeDelta(stateDeltaBytes, 0)) {
    LOG_GENERAL(WARNING, "AccountStore::GetInstance().DeserializeDelta failed");
    return false;
  }

  return true;
}

void Node::CommitForwardedTransactions(const MBnForwardedTxnEntry& entry) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CommitForwardedTransactions not expected to be "
                "called from Normal node.");
    return;
  }

  LOG_MARKER();
  if (LOG_PARAMETERS) {
    LOG_STATE("[TXNPUT]"
              << "BGN")
  }

  for (const auto& twr : entry.m_transactions) {
    LOG_GENERAL(INFO, "Commit txn " << twr.GetTransaction().GetTranID().hex());
    if (LOOKUP_NODE_MODE) {
      LookupServer::AddToRecentTransactions(twr.GetTransaction().GetTranID());
    }

    // feed the event log holder
    if (ENABLE_WEBSOCKET) {
      WebsocketServer::GetInstance().ParseTxn(twr);
    }

    // Store TxBody to disk
    bytes serializedTxBody;
    twr.Serialize(serializedTxBody, 0);
    if (!BlockStorage::GetBlockStorage().PutTxBody(
            twr.GetTransaction().GetTranID(), serializedTxBody)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutTxBody failed "
                               << twr.GetTransaction().GetTranID());
      return;
    }
  }
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Proceessed " << entry.m_transactions.size() << " of txns.");
  if (LOG_PARAMETERS) {
    LOG_STATE("[TXNPUT]"
              << "DONE [" << entry.m_transactions.size() << "]");
  }
}

void Node::SoftConfirmForwardedTransactions(const MBnForwardedTxnEntry& entry) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::SoftConfirmForwardedTransactions not expected to be "
                "called from Normal node.");
    return;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexSoftConfirmedTxns);

  for (const auto& twr : entry.m_transactions) {
    m_softConfirmedTxns.emplace(twr.GetTransaction().GetTranID(), twr);
  }
}

bool Node::GetSoftConfirmedTransaction(const TxnHash& txnHash,
                                       TxBodySharedPtr& tptr) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::GetSoftConfirmedTransaction not expected to be "
                "called from Normal node.");
    return false;
  }

  lock_guard<mutex> g(m_mutexSoftConfirmedTxns);

  auto find = m_softConfirmedTxns.find(txnHash);
  if (find != m_softConfirmedTxns.end()) {
    tptr = TxBodySharedPtr(new TransactionWithReceipt(find->second));
    return true;
  }
  return false;
}

void Node::ClearSoftConfirmedTransactions() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ClearSoftConfirmedTransactions not expected to be "
                "called from Normal node.");
    return;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexSoftConfirmedTxns);

  m_softConfirmedTxns.clear();
}

void Node::DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
    const uint64_t& blocknum) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);

  auto it = m_unavailableMicroBlocks.find(blocknum);

  for (const auto& it : m_unavailableMicroBlocks) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Unavailable"
              " microblock bodies in finalblock "
                  << it.first << ": " << it.second.size());
    for (auto it2 : it.second) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, it2.first);
    }
  }

  if (it != m_unavailableMicroBlocks.end() && it->second.empty()) {
    m_unavailableMicroBlocks.erase(it);
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Deleting blocknum " << blocknum
                                   << " from unavailable microblocks list.");

    // #ifndef IS_LOOKUP_NODE
    //         m_forwardingAssignment.erase(blocknum);
    //         lock_guard<mutex> gt(m_mutexTempCommitted);
    //         if (m_unavailableMicroBlocks.empty() &&
    //         m_tempStateDeltaCommitted)
    //         {
    //             {
    //                 lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
    //                 m_allMicroBlocksRecvd = true;
    //             }
    //             LOG_GENERAL(INFO, "Notify All MicroBlocks Received");
    //             m_cvAllMicroBlocksRecvd.notify_all();
    //         }
    // #endif // IS_LOOKUP_NODE
    LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << blocknum << "] LAST");
  }
}

bool Node::ProcessMBnForwardTransaction(const bytes& message,
                                        unsigned int cur_offset,
                                        const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessMBnForwardTransaction not expected to be "
                "called from Normal node.");
    return true;
  }

  LOG_MARKER();

#ifdef SJ_TEST_SJ_MISSING_MBTXNS
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP) {
    LOG_GENERAL(
        INFO,
        "Stimulating missing mb/txns so ignoring received mb/txns message "
        "(SJ_TEST_SJ_MISSING_MBTXNS)");
    return false;
  }
#endif  // SJ_TEST_SJ_MISSING_MBTXNS

  MBnForwardedTxnEntry entry;

  if (!Messenger::GetNodeMBnForwardTransaction(message, cur_offset, entry)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::ProcessMBnForwardTransaction failed.");
    return false;
  }

  if (entry.m_microBlock.GetHeader().GetVersion() != MICROBLOCK_VERSION) {
    LOG_CHECK_FAIL("MicroBlock version",
                   entry.m_microBlock.GetHeader().GetVersion(),
                   MICROBLOCK_VERSION);
    return false;
  }

  // Verify the co-signature if not DS MB
  if (entry.m_microBlock.GetHeader().GetShardId() !=
          m_mediator.m_ds->m_shards.size() &&
      !m_mediator.m_ds->VerifyMicroBlockCoSignature(
          entry.m_microBlock, entry.m_microBlock.GetHeader().GetShardId())) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Microblock co-sig verification failed");
    return false;
  }

  // Verify Microblock agains forwarded txns
  // BlockHash
  BlockHash temp_blockHash = entry.m_microBlock.GetHeader().GetMyHash();
  if (temp_blockHash != entry.m_microBlock.GetBlockHash()) {
    LOG_CHECK_FAIL("Block hash", entry.m_microBlock.GetBlockHash(),
                   temp_blockHash);
    return false;
  }

  // Verify txnhash
  TxnHash txnHash = ComputeRoot(entry.m_transactions);
  if (txnHash != entry.m_microBlock.GetHeader().GetTxRootHash()) {
    LOG_CHECK_FAIL("Txn root hash",
                   entry.m_microBlock.GetHeader().GetTxRootHash(), txnHash);
    return false;
  }

  // Verify txreceipt
  TxnHash txReceiptHash =
      TransactionWithReceipt::ComputeTransactionReceiptsHash(
          entry.m_transactions);
  if (txReceiptHash != entry.m_microBlock.GetHeader().GetTranReceiptHash()) {
    LOG_CHECK_FAIL("Txn receipt hash",
                   entry.m_microBlock.GetHeader().GetTranReceiptHash(),
                   txReceiptHash);
    return false;
  }

  LOG_GENERAL(INFO, "[SendMBnTXBOD] Recvd from " << from);
  LOG_GENERAL(INFO,
              " EpochNum = " << entry.m_microBlock.GetHeader().GetEpochNum());
  LOG_GENERAL(INFO,
              " ShardID  = " << entry.m_microBlock.GetHeader().GetShardId());

  LOG_STATE(
      "[TXBOD]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] RECVD MB & TXN BODIES #"
      << entry.m_microBlock.GetHeader().GetEpochNum() << " shard "
      << entry.m_microBlock.GetHeader().GetShardId());

  if (LOOKUP_NODE_MODE && LOG_PARAMETERS) {
    LOG_STATE("[MBPCKT] Size:"
              << message.size()
              << " Epoch:" << entry.m_microBlock.GetHeader().GetEpochNum()
              << " Shard:" << entry.m_microBlock.GetHeader().GetShardId()
              << " Txns:" << entry.m_microBlock.GetHeader().GetNumTxs());
  }

  if ((m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
       entry.m_microBlock.GetHeader()
           .GetEpochNum()) || /* Buffer for syncing seed node */
      (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP &&
       m_mediator.m_lookup->GetSyncType() == SyncType::NEW_LOOKUP_SYNC) ||
      (LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP &&
       m_mediator.m_lookup->GetSyncType() == SyncType::LOOKUP_SYNC)) {
    lock_guard<mutex> g(m_mutexMBnForwardedTxnBuffer);
    m_mbnForwardedTxnBuffer[entry.m_microBlock.GetHeader().GetEpochNum()]
        .push_back(entry);
    LOG_GENERAL(INFO, "Buffered MB & TXN BODIES #"
                          << entry.m_microBlock.GetHeader().GetEpochNum()
                          << " shard "
                          << entry.m_microBlock.GetHeader().GetShardId());

    {
      // skip for DS microblock submission
      std::lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);
      if (entry.m_microBlock.GetHeader().GetShardId() ==
          m_mediator.m_ds->m_shards.size()) {
        return true;
      }
    }

    // shard microblock only:
    // pre-process of early MBnForwardTxn submission
    // soft confirmation
    SoftConfirmForwardedTransactions(entry);
    // [TODO] invoke txn distribution

    return true;
  }

  return ProcessMBnForwardTransactionCore(entry);
}

bool Node::AddPendingTxn(const HashCodeMap& pendingTxns, const PubKey& pubkey,
                         uint32_t shardId) {
  uint size;
  {
    lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);
    size = m_mediator.m_ds->m_shards.size();
    if (shardId > size) {
      LOG_GENERAL(WARNING, "Shard id exceeds shards: " << shardId);
      return false;
    } else if (shardId < size) {
      if (!Lookup::VerifySenderNode(m_mediator.m_ds->m_shards.at(shardId),
                                    pubkey)) {
        LOG_GENERAL(WARNING, "Could not find PubKey in shard " << shardId);
        return false;
      }
    }
  }
  if (shardId == size) {
    // DS Committee
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (!Lookup::VerifySenderNode(*m_mediator.m_DSCommittee, pubkey)) {
      LOG_GENERAL(WARNING, "Could not find pubkey in ds committee");
      return false;
    }
  }

  const auto& currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  lock(m_pendingTxnsMutex, m_droppedTxnsMutex);

  unique_lock<shared_timed_mutex> g1(m_pendingTxnsMutex, adopt_lock);
  unique_lock<shared_timed_mutex> g2(m_droppedTxnsMutex, adopt_lock);
  for (const auto& entry : pendingTxns) {
    LOG_GENERAL(INFO, " " << entry.first << " " << entry.second);

    if (BlockStorage::GetBlockStorage().CheckTxBody(entry.first)) {
      LOG_GENERAL(INFO, "TranHash: " << entry.first << " sent by pubkey "
                                     << pubkey << " of shard " << shardId
                                     << " is already confirmed");
      continue;
    }

    if (!IsTxnDropped(entry.second)) {
      m_pendingTxns.insert(entry.first, entry.second, currentEpochNum);
    } else {
      LOG_GENERAL(INFO, "[DTXN]" << entry.first << " " << currentEpochNum);
      m_droppedTxns.insert(entry.first, entry.second, currentEpochNum);
    }
  }
  return true;
}

bool Node::SendPendingTxnToLookup() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "SendPendingTxnToLookup called from lookup");
    return false;
  }

  if (m_consensusMyID > NUM_SHARE_PENDING_TXNS && !m_isPrimary) {
    return false;
  }

  const auto pendingTxns = GetUnconfirmedTxns();
  const auto& blocknum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  bytes pend_txns_message = {MessageType::NODE,
                             NodeInstructionType::PENDINGTXN};
  if (!Messenger::SetNodePendingTxn(pend_txns_message, MessageOffset::BODY,
                                    blocknum, pendingTxns, m_myshardId,
                                    m_mediator.m_selfKey)) {
    LOG_GENERAL(WARNING, "Failed to set SetNodePendingTxn");
    return false;
  }

  LOG_GENERAL(INFO, "Sent lookup Pending txns");
  m_mediator.m_lookup->SendMessageToLookupNodes(pend_txns_message);

  return true;
}

bool Node::ProcessPendingTxn(const bytes& message, unsigned int cur_offset,
                             [[gnu::unused]] const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Node::ProcessPendingTxn called from Normal node");
    return false;
  }
  uint64_t epochNum;
  unordered_map<TxnHash, ErrTxnStatus> hashCodeMap;
  uint32_t shardId;
  PubKey pubkey;

  if (!Messenger::GetNodePendingTxn(message, cur_offset, epochNum, hashCodeMap,
                                    shardId, pubkey)) {
    LOG_GENERAL(WARNING, "Failed to set GetNodePendingTxn");
    return false;
  }
  const auto& currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (currentEpochNum > epochNum + 1) {
    LOG_GENERAL(WARNING,
                "PENDINGTXN sent of an two epoches older epoch " << epochNum);
    return false;
  } else if (currentEpochNum < epochNum || /* Buffer for syncing seed node */
             (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP &&
              m_mediator.m_lookup->GetSyncType() ==
                  SyncType::NEW_LOOKUP_SYNC) ||
             (LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP &&
              m_mediator.m_lookup->GetSyncType() == SyncType::LOOKUP_SYNC)) {
    lock_guard<mutex> g(m_mutexPendingTxnBuffer);
    m_pendingTxnBuffer[epochNum].emplace_back(hashCodeMap, pubkey, shardId);
    LOG_GENERAL(INFO, "Buffer PENDINGTXN for epoch " << epochNum);
    return true;
  }
  LOG_GENERAL(INFO, "Received message for epoch " << epochNum << " and shard "
                                                  << shardId);
  // Store to local map for PENDINGTXN
  // map -> key : epochnum value : map {key: shardid, value: vector<bytes>
  // pend_txns_message}
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && MULTIPLIER_SYNC_MODE) {
    std::lock_guard<mutex> g1(m_mutexPendingTxnStore);
    auto it = m_pendingTxnStore.find(epochNum);
    if (it == m_pendingTxnStore.end() ||
        (it->second.find(shardId) == it->second.end())) {
      m_pendingTxnStore[epochNum][shardId] = message;
    }
  }

  AddPendingTxn(hashCodeMap, pubkey, shardId);

  return true;
}

bool Node::ProcessMBnForwardTransactionCore(const MBnForwardedTxnEntry& entry) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessMBnForwardTransactionCore not expected to be "
                "called from Normal node.");
    return true;
  }

  LOG_MARKER();

  LOG_GENERAL(INFO, entry);

  {
    lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
    bool isEveryMicroBlockAvailable;

    if (!IsMicroBlockTxRootHashInFinalBlock(entry,
                                            isEveryMicroBlockAvailable)) {
      LOG_GENERAL(WARNING, "The forwarded data is not in finalblock, why?");
      return false;
    }

    m_mediator.m_lookup->AddMicroBlockToStorage(entry.m_microBlock);

    CommitForwardedTransactions(entry);

    // Microblock and Transaction body sharing
    bytes mb_txns_message = {MessageType::NODE,
                             NodeInstructionType::MBNFORWARDTRANSACTION};

    if (!Messenger::SetNodeMBnForwardTransaction(
            mb_txns_message, MessageOffset::BODY, entry.m_microBlock,
            entry.m_transactions)) {
      LOG_GENERAL(WARNING, "Messenger::SetNodeMBnForwardTransaction failed.");
    } else if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && MULTIPLIER_SYNC_MODE) {
      // Store to local map for MBNFORWARDTRANSACTION
      std::lock_guard<mutex> g1(m_mutexMBnForwardedTxnStore);
      m_mbnForwardedTxnStore[entry.m_microBlock.GetHeader().GetEpochNum()]
                            [entry.m_microBlock.GetHeader().GetShardId()] =
                                mb_txns_message;
    }

    if (isEveryMicroBlockAvailable) {
      DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
          entry.m_microBlock.GetHeader().GetEpochNum());

      ClearSoftConfirmedTransactions();

      if (m_isVacuousEpochBuffer) {
        // Check is states updated
        uint64_t epochNum;
        if (m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetBlockNum() == 1) {
          epochNum = 1;
        } else {
          if (!BlockStorage::GetBlockStorage().GetLatestEpochStatesUpdated(
                  epochNum)) {
            LOG_GENERAL(WARNING,
                        "BlockStorage::GetLatestEpochStateusUpdated failed");
            return false;
          }
        }
        if (AccountStore::GetInstance().GetPrevRootHash() ==
            m_mediator.m_txBlockChain.GetLastBlock()
                .GetHeader()
                .GetStateRootHash()) {
          if (!BlockStorage::GetBlockStorage().PutMetadata(
                  MetaType::DSINCOMPLETED, {'0'})) {
            LOG_GENERAL(WARNING,
                        "BlockStorage::PutMetadata (DSINCOMPLETED) '0' failed");
            return false;
          }
          if (!BlockStorage::GetBlockStorage().PutEpochFin(
                  m_mediator.m_currentEpochNum)) {
            LOG_GENERAL(WARNING,
                        "BlockStorage::PutEpochFin failed "
                            << entry.m_microBlock.GetHeader().GetEpochNum());
            return false;
          }
          if (!BlockStorage::GetBlockStorage().ResetDB(
                  BlockStorage::TX_BODY_TMP)) {
            LOG_GENERAL(WARNING, "BlockStorage::ResetDB (TX_BODY_TMP) failed");
          }
        } else if (epochNum == m_mediator.m_currentEpochNum) {
          if (!BlockStorage::GetBlockStorage().PutMetadata(
                  MetaType::DSINCOMPLETED, {'0'})) {
            LOG_GENERAL(WARNING,
                        "BlockStorage::PutMetadata (DSINCOMPLETED) '0' failed");
            return false;
          }
          if (!BlockStorage::GetBlockStorage().ResetDB(
                  BlockStorage::TX_BODY_TMP)) {
            LOG_GENERAL(WARNING, "BlockStorage::ResetDB (TX_BODY_TMP) failed");
          }
        }
      } else {
        if (!BlockStorage::GetBlockStorage().PutEpochFin(
                m_mediator.m_currentEpochNum)) {
          LOG_GENERAL(WARNING,
                      "BlockStorage::PutEpochFin failed "
                          << entry.m_microBlock.GetHeader().GetEpochNum());
          return false;
        }
      }

      if (ENABLE_WEBSOCKET) {
        // send tx block and attach txhashes
        const TxBlock& txBlock = m_mediator.m_txBlockChain.GetLastBlock();
        Json::Value j_txnhashes;
        try {
          j_txnhashes = LookupServer::GetTransactionsForTxBlock(
              txBlock, m_mediator.m_lookup->m_historicalDB);
        } catch (...) {
          j_txnhashes = Json::arrayValue;
        }
        WebsocketServer::GetInstance().PrepareTxBlockAndTxHashes(
            JSONConversion::convertTxBlocktoJson(txBlock), j_txnhashes);

        // send event logs
        WebsocketServer::GetInstance().SendOutMessages();
      }
    }
  }

  return true;
}

void Node::CommitMBnForwardedTransactionBuffer() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CommitMBnForwardedTransactionBuffer not expected to be "
                "called from Normal node.");
    return;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMBnForwardedTxnBuffer);

  for (auto it = m_mbnForwardedTxnBuffer.begin();
       it != m_mbnForwardedTxnBuffer.end();) {
    if (it->first <=
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()) {
      for (const auto& entry : it->second) {
        ProcessMBnForwardTransactionCore(entry);
      }
    }
    it = m_mbnForwardedTxnBuffer.erase(it);
  }
}

void Node::CommitPendingTxnBuffer() {
  // Clear Pending txn
  lock_guard<mutex> g(m_mutexPendingTxnBuffer);

  const auto& epochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  auto itr = m_pendingTxnBuffer.find(epochNum);

  if (itr != m_pendingTxnBuffer.end()) {
    for (const auto& entry : itr->second) {
      AddPendingTxn(get<PendingData::HASH_CODE_MAP>(entry),
                    get<PendingData::PUBKEY>(entry),
                    get<PendingData::SHARD_ID>(entry));
    }
  }

  m_pendingTxnBuffer.clear();
}
