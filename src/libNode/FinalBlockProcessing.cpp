/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
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
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libPOW/pow.h"
#include "libServer/Server.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/UpgradeManager.h"

using namespace std;
using namespace boost::multiprecision;

void Node::StoreState() {
  LOG_MARKER();
  AccountStore::GetInstance().MoveUpdatesToDisk();
}

void Node::StoreFinalBlock(const TxBlock& txBlock) {
  LOG_MARKER();

  AddBlock(txBlock);

  m_mediator.IncreaseEpochNum();

  // At this point, the transactions in the last Epoch is no longer useful, thus
  // erase. EraseCommittedTransactions(m_mediator.m_currentEpochNum - 2);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Storing Tx Block Number: "
                << txBlock.GetHeader().GetBlockNum()
                << " with Type: " << to_string(txBlock.GetHeader().GetType())
                << ", Version: " << txBlock.GetHeader().GetVersion()
                << ", Timestamp: " << txBlock.GetTimestamp()
                << ", NumTxs: " << txBlock.GetHeader().GetNumTxs());

  // Store Tx Block to disk
  vector<unsigned char> serializedTxBlock;
  txBlock.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(txBlock.GetHeader().GetBlockNum(),
                                             serializedTxBlock);

  LOG_EPOCH(
      INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
      "Final block "
          << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
          << " received with prevhash 0x"
          << DataConversion::charArrToHexStr(
                 m_mediator.m_txBlockChain.GetLastBlock()
                     .GetHeader()
                     .GetPrevHash()
                     .asArray()));

  LOG_STATE(
      "[FINBK]["
      << std::setw(15) << std::left
      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] RECV");
}

bool Node::IsMicroBlockTxRootHashInFinalBlock(
    const ForwardedTxnEntry& entry, bool& isEveryMicroBlockAvailable) {
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Deleting unavailable microblock: " << entry);
  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
  auto it = m_unavailableMicroBlocks.find(entry.m_blockNum);
  bool found = (it != m_unavailableMicroBlocks.end() &&
                RemoveTxRootHashFromUnavailableMicroBlock(entry));
  isEveryMicroBlockAvailable = found && it->second.empty();
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);
  return found;
}

bool Node::LoadUnavailableMicroBlockHashes(const TxBlock& finalBlock,
                                           const uint64_t& blocknum,
                                           bool& toSendTxnToLookup) {
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Unavailable microblock hashes in final block : ")

  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);

  const auto& microBlockInfos = finalBlock.GetMicroBlockInfos();

  // bool doRejoin = false;

  for (const auto& info : microBlockInfos) {
    if (LOOKUP_NODE_MODE) {
      if (info.m_txnRootHash != TxnHash()) {
        m_unavailableMicroBlocks[blocknum].push_back(
            {info.m_microBlockHash, info.m_txnRootHash});
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
        } else {
          if (m_microblock->GetBlockHash() == info.m_microBlockHash) {
            if (m_microblock->GetHeader().GetTxRootHash() != TxnHash()) {
              if (info.m_txnRootHash != TxnHash()) {
                toSendTxnToLookup = true;
              } else {
                LOG_GENERAL(WARNING,
                            "My MicroBlock txRootHash ("
                                << m_microblock->GetHeader().GetTxRootHash()
                                << ") is not null"
                                   " but isMicroBlockEmpty for me is "
                                << info.m_txnRootHash);
                return false;
              }
            }
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
        }

        break;
      }
    }
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
    const ForwardedTxnEntry& entry) {
  for (auto it = m_unavailableMicroBlocks.at(entry.m_blockNum).begin();
       it != m_unavailableMicroBlocks.at(entry.m_blockNum).end(); it++) {
    if (it->first == entry.m_hash) {
      TxnHash txnHash = ComputeRoot(entry.m_transactions);
      if (it->second != txnHash) {
        LOG_GENERAL(
            WARNING,
            "TxnRootHash computed from forwarded txns doesn't match, expected: "
                << it->second << " received: " << txnHash);
        return false;
      }
      LOG_GENERAL(INFO, "Remove microblock" << it->first);
      LOG_GENERAL(INFO,
                  "Microblocks count before removing: "
                      << m_unavailableMicroBlocks.at(entry.m_blockNum).size());
      m_unavailableMicroBlocks.at(entry.m_blockNum).erase(it);
      LOG_GENERAL(INFO,
                  "Microblocks count after removing: "
                      << m_unavailableMicroBlocks.at(entry.m_blockNum).size());
      return true;
    }
  }
  LOG_GENERAL(WARNING, "Haven't found microblock: " << entry);
  return false;
}

bool Node::VerifyFinalBlockCoSignature(const TxBlock& txblock) {
  LOG_MARKER();

  unsigned int index = 0;
  unsigned int count = 0;

  const vector<bool>& B2 = txblock.GetB2();
  if (m_mediator.m_DSCommittee->size() != B2.size()) {
    LOG_GENERAL(WARNING, "Mismatch: DS committee size = "
                             << m_mediator.m_DSCommittee->size()
                             << ", co-sig bitmap size = " << B2.size());
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
  vector<unsigned char> message;
  if (!txblock.GetHeader().Serialize(message, 0)) {
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed");
    return false;
  }
  txblock.GetCS1().Serialize(message, message.size());
  BitVector::SetBitVector(message, message.size(), txblock.GetB1());
  if (!Schnorr::GetInstance().Verify(message, 0, message.size(),
                                     txblock.GetCS2(), *aggregatedKey)) {
    LOG_GENERAL(WARNING, "Cosig verification failed");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return false;
  }

  return true;
}

void Node::BroadcastTransactionsToLookup(
    const vector<TransactionWithReceipt>& txns_to_send) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::BroadcastTransactionsToLookup not expected to be "
                "called from LookUp node.");
    return;
  }

  LOG_MARKER();

  uint64_t blocknum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  LOG_STATE(
      "[TXBOD]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] BEFORE TXN BODIES #" << blocknum);

  LOG_GENERAL(INFO, "BroadcastTransactionsToLookup for blocknum: " << blocknum);

  // Broadcast Txns to Lookup
  if (txns_to_send.size() > 0) {
    // Transaction body sharing
    vector<unsigned char> forwardtxn_message = {
        MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION};

    if (!Messenger::SetNodeForwardTransaction(
            forwardtxn_message, MessageOffset::BODY, blocknum,
            m_microblock->GetBlockHash(), txns_to_send)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetNodeForwardTransaction failed.");
      return;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I will soon be sending the txn bodies and receipts to the "
              "lookup nodes");
    m_mediator.m_lookup->SendMessageToLookupNodes(forwardtxn_message);
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG I have no txn body and receipt to send")
  }
}

void Node::InitiatePoW() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::InitiatePoW not expected to be called from LookUp node.");
    return;
  }

  SetState(POW_SUBMISSION);
  POW::GetInstance().EthashConfigureClient(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
      FULL_DATASET_MINE);
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Start pow ");
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
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am no longer the shard leader ");
    m_isPrimary = false;
  }

  m_mediator.m_consensusID++;

  uint16_t lastBlockHash = DataConversion::charArrTo16Bits(
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
  m_consensusLeaderID = lastBlockHash % m_myShardMembers->size();

  if (m_consensusMyID == m_consensusLeaderID) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the new shard leader ");
    m_isPrimary = true;
  } else {
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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

void Node::GetMyShardsMicroBlock(const uint64_t& blocknum, uint8_t sharing_mode,
                                 vector<TransactionWithReceipt>& txns_to_send) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::GetMyShardsMicroBlock not expected to be called "
                "from LookUp node.");
    return;
  }

  if (m_microblock != nullptr) {
    const vector<TxnHash>& tx_hashes = m_microblock->GetTranHashes();
    for (const auto& tx_hash : tx_hashes) {
      if (!FindTxnInProcessedTxnsList(blocknum, sharing_mode, txns_to_send,
                                      tx_hash)) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Failed trying to find txn in processed txn list");
      }
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of transactions to broadcast for block "
                  << blocknum << " = " << txns_to_send.size());
  }
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

  auto& processedTransactions = m_processedTransactions[blockNum];
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

  uint64_t blocknum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  std::vector<TransactionWithReceipt> txns_to_send;

  if ((!m_txnSharingIAmSender) && (m_txnSharingIAmForwarder)) {
    GetMyShardsMicroBlock(blocknum, TxSharingMode::NODE_FORWARD_ONLY,
                          txns_to_send);
  } else if (((m_txnSharingIAmSender) && (!m_txnSharingIAmForwarder)) ||
             ((m_txnSharingIAmSender) &&
              (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE))) {
    GetMyShardsMicroBlock(blocknum, TxSharingMode::SEND_ONLY, txns_to_send);
    BroadcastMicroBlockToLookup();
  } else if ((m_txnSharingIAmSender) && (m_txnSharingIAmForwarder)) {
    GetMyShardsMicroBlock(blocknum, TxSharingMode::SEND_AND_FORWARD,
                          txns_to_send);
    BroadcastMicroBlockToLookup();
  } else {
    GetMyShardsMicroBlock(blocknum, TxSharingMode::IDLE, txns_to_send);
  }

  BroadcastTransactionsToLookup(txns_to_send);
}

void Node::BroadcastMicroBlockToLookup() {
  if (m_microblock == nullptr) {
    LOG_GENERAL(WARNING, "MicroBlock is null");
    return;
  }

  LOG_GENERAL(INFO, "[SendMB]"
                        << " Sending lookup :"
                        << m_microblock->GetHeader().GetShardId()
                        << " Epoch:" << m_mediator.m_currentEpochNum);
  vector<unsigned char> msg = {MessageType::LOOKUP,
                               LookupInstructionType::SETMICROBLOCKFROMSEED};
  unsigned int curr_offset = MessageOffset::BODY;
  Serializable::SetNumber<uint64_t>(
      msg, curr_offset, m_mediator.m_currentEpochNum, sizeof(uint64_t));
  curr_offset += sizeof(uint64_t);
  m_microblock->Serialize(msg, curr_offset);
  m_mediator.m_lookup->SendMessageToLookupNodes(msg);
}

void Node::LogReceivedFinalBlockDetails([
    [gnu::unused]] const TxBlock& txblock) {
  if (LOOKUP_NODE_MODE) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have deserialized the TxBlock");
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "txblock.GetHeader().GetType(): " << txblock.GetHeader().GetType());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetVersion(): "
                  << txblock.GetHeader().GetVersion());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetGasLimit(): "
                  << txblock.GetHeader().GetGasLimit());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetGasUsed(): "
                  << txblock.GetHeader().GetGasUsed());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetBlockNum(): "
                  << txblock.GetHeader().GetBlockNum());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetMicroBlockInfos().size(): "
                  << txblock.GetMicroBlockInfos().size());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetStateRootHash(): "
                  << txblock.GetHeader().GetStateRootHash());
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "txblock.GetHeader().GetNumTxs(): " << txblock.GetHeader().GetNumTxs());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetMinerPubKey(): "
                  << txblock.GetHeader().GetMinerPubKey());
  }
}

bool Node::CheckStateRoot(const TxBlock& finalBlock) {
  StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

  // AccountStore::GetInstance().PrintAccountState();

  if (stateRoot != finalBlock.GetHeader().GetStateRootHash()) {
    LOG_EPOCH(
        WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
        "State root doesn't match. Expected = "
            << stateRoot << ". "
            << "Received = " << finalBlock.GetHeader().GetStateRootHash());
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "State root matched " << finalBlock.GetHeader().GetStateRootHash());

  return true;
}

// void Node::StoreMicroBlocksToDisk()
// {
//     LOG_MARKER();
//     for(auto microBlock : m_microblocks)
//     {

//         LOG_GENERAL(INFO,  "Storing Micro Block Hash: " <<
//         microBlock.GetHeader().GetTxRootHash() <<
//             " with Type: " << microBlock.GetHeader().GetType() <<
//             ", Version: " << microBlock.GetHeader().GetVersion() <<
//             ", Timestamp: " << microBlock.GetHeader().GetTimestamp() <<
//             ", NumTxs: " << microBlock.GetHeader().GetNumTxs());

//         vector<unsigned char> serializedMicroBlock;
//         microBlock.Serialize(serializedMicroBlock, 0);
//         BlockStorage::GetBlockStorage().PutMicroBlock(microBlock.GetHeader().GetTxRootHash(),
//                                                serializedMicroBlock);
//     }
//     m_microblocks.clear();
// }

void Node::PrepareGoodStateForFinalBlock() {
  if (m_state == MICROBLOCK_CONSENSUS || m_state == MICROBLOCK_CONSENSUS_PREP) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I may have missed the micrblock consensus. However, if I "
              "recently received a valid finalblock, I will accept it");
    // TODO: Optimize state transition.
    SetState(WAITING_FINALBLOCK);
  }
}

bool Node::ProcessFinalBlock(const vector<unsigned char>& message,
                             unsigned int offset,
                             [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  uint32_t shardId = std::numeric_limits<uint32_t>::max();
  uint64_t dsBlockNumber = 0;
  uint32_t consensusID = 0;
  TxBlock txBlock;
  vector<unsigned char> stateDelta;

  if (!Messenger::GetNodeFinalBlock(message, offset, shardId, dsBlockNumber,
                                    consensusID, txBlock, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetNodeFinalBlock failed.");
    return false;
  }

  lock_guard<mutex> g(m_mutexFinalBlock);

  BlockHash temp_blockHash = txBlock.GetHeader().GetMyHash();
  if (temp_blockHash != txBlock.GetBlockHash()) {
    LOG_GENERAL(WARNING,
                "Block Hash in Newly received Tx Block doesn't match. "
                "Calculated: "
                    << temp_blockHash
                    << " Received: " << txBlock.GetBlockHash().hex());
    return false;
  }

  // Check block number
  if (!m_mediator.CheckWhetherBlockIsLatest(
          dsBlockNumber + 1, txBlock.GetHeader().GetBlockNum())) {
    LOG_GENERAL(WARNING, "ProcessFinalBlock CheckWhetherBlockIsLatest failed");
    return false;
  }

  if (consensusID != m_mediator.m_consensusID) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Consensus ID is not correct. Expected ID: "
                  << consensusID
                  << " My Consensus ID: " << m_mediator.m_consensusID);
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  if (committeeHash != txBlock.GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING,
                "DS committee hash in newly received Tx Block doesn't match. "
                "Calculated: "
                    << committeeHash
                    << " Received: " << txBlock.GetHeader().GetCommitteeHash());
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DEBUG shard id is " << (unsigned int)shardId)

  LogReceivedFinalBlockDetails(txBlock);

  LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                       << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                       << txBlock.GetHeader().GetBlockNum() << "] FRST");

  // Verify the co-signature
  if (!VerifyFinalBlockCoSignature(txBlock)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "TxBlock co-sig verification failed");
    return false;
  }

  // Compute the MBInfoHash of the extra MicroBlock information
  MBInfoHash mbInfoHash;
  if (!Messenger::GetMbInfoHash(txBlock.GetMicroBlockInfos(), mbInfoHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetMbInfoHash failed.");
    return false;
  }

  if (mbInfoHash != txBlock.GetHeader().GetMbInfoHash()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "TxBlock MbInfo verification failed");
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
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Too late - current state is " << m_state << ".");
      return false;
    }
  }

  LOG_STATE(
      "[FLBLK]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] RECEIVED FINAL BLOCK");

  bool toSendTxnToLookup = false;

  bool isVacuousEpoch = m_mediator.GetIsVacuousEpoch();
  m_isVacuousEpochBuffer = isVacuousEpoch;

  ProcessStateDeltaFromFinalBlock(stateDelta,
                                  txBlock.GetHeader().GetStateDeltaHash());

  BlockStorage::GetBlockStorage().PutStateDelta(
      txBlock.GetHeader().GetBlockNum(), stateDelta);

  if (!LOOKUP_NODE_MODE &&
      (!CheckStateRoot(txBlock) || m_doRejoinAtStateRoot)) {
    RejoinAsNormal();
    return false;
  } else if (LOOKUP_NODE_MODE && !CheckStateRoot(txBlock)) {
    return false;
  }

  if (!isVacuousEpoch) {
    if (!LoadUnavailableMicroBlockHashes(
            txBlock, txBlock.GetHeader().GetBlockNum(), toSendTxnToLookup)) {
      return false;
    }
    StoreFinalBlock(txBlock);
  } else {
    LOG_GENERAL(INFO, "isVacuousEpoch now");

    // Remove because shard nodes will be shuffled in next epoch.
    CleanMicroblockConsensusBuffer();

    StoreState();
    StoreFinalBlock(txBlock);
    BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED, {'0'});
  }

  m_mediator.HeartBeatPulse();

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

  {
    lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
    if (isVacuousEpoch && m_mediator.m_curSWInfo.GetUpgradeDS() - 1 ==
                              m_mediator.m_dsBlockChain.GetLastBlock()
                                  .GetHeader()
                                  .GetBlockNum()) {
      auto func = [this]() mutable -> void {
        UpgradeManager::GetInstance().ReplaceNode(m_mediator);
      };

      DetachedFunction(1, func);
    }
  }

  if (!LOOKUP_NODE_MODE) {
    if (toSendTxnToLookup) {
      CallActOnFinalblock();
    }

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

    // Now only forwarded txn are left, so only call in lookup
    CommitForwardedTransactionBuffer();
    m_mediator.m_lookup->CommitMicroBlockStorage();
    if (m_mediator.m_lookup->GetIsServer() && !isVacuousEpoch) {
      m_mediator.m_lookup->SenderTxnBatchThread();
    }
  }

  FallbackTimerPulse();

  return true;
}

bool Node::ProcessStateDeltaFromFinalBlock(
    const vector<unsigned char>& stateDeltaBytes,
    const StateHash& finalBlockStateDeltaHash) {
  LOG_MARKER();

  // Init local AccountStoreTemp first
  AccountStore::GetInstance().InitTemp();

  LOG_GENERAL(INFO, "Received FinalBlock State Delta root : "
                        << finalBlockStateDeltaHash.hex());

  if (finalBlockStateDeltaHash == StateHash()) {
    LOG_GENERAL(INFO,
                "State Delta hash received from finalblock is null, "
                "skip processing state delta");
    AccountStore::GetInstance().CommitTempReversible();
    return true;
  }

  if (stateDeltaBytes.empty()) {
    LOG_GENERAL(WARNING, "Cannot get state delta from message");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(stateDeltaBytes);
  StateHash stateDeltaHash(sha2.Finalize());

  LOG_GENERAL(INFO, "Calculated StateDeltaHash: " << stateDeltaHash);

  if (stateDeltaHash != finalBlockStateDeltaHash) {
    LOG_GENERAL(WARNING,
                "State delta hash calculated does not match finalblock");
    return false;
  }

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

void Node::CommitForwardedTransactions(const ForwardedTxnEntry& entry) {
  LOG_MARKER();

  unsigned int txn_counter = 0;
  for (const auto& twr : entry.m_transactions) {
    if (LOOKUP_NODE_MODE) {
      Server::AddToRecentTransactions(twr.GetTransaction().GetTranID());
    }

    // Store TxBody to disk
    vector<unsigned char> serializedTxBody;
    twr.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(twr.GetTransaction().GetTranID(),
                                              serializedTxBody);

    txn_counter++;
    if (txn_counter % 10000 == 0) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Proceessed " << txn_counter << " of txns.");
    }
  }
}

void Node::DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
    const uint64_t& blocknum) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);

  auto it = m_unavailableMicroBlocks.find(blocknum);

  for (auto it : m_unavailableMicroBlocks) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unavailable"
              " microblock bodies in finalblock "
                  << it.first << ": " << it.second.size());
    for (auto it2 : it.second) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                it2.first);
    }
  }

  if (it != m_unavailableMicroBlocks.end() && it->second.empty()) {
    m_unavailableMicroBlocks.erase(it);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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

bool Node::ProcessForwardTransaction(const vector<unsigned char>& message,
                                     unsigned int cur_offset,
                                     [[gnu::unused]] const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessForwardTransaction not expected to be "
                "called from Normal node.");
    return true;
  }

  LOG_MARKER();

  ForwardedTxnEntry entry;

  if (!Messenger::GetNodeForwardTransaction(message, cur_offset, entry)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetNodeForwardTransaction failed.");
    return false;
  }

  LOG_STATE(
      "[TXBOD]["
      << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
      << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] RECEIVED TXN BODIES #" << entry.m_blockNum);

  LOG_GENERAL(INFO,
              "Received forwarded txns for block number " << entry.m_blockNum);

  if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
      entry.m_blockNum) {
    lock_guard<mutex> g(m_mutexForwardedTxnBuffer);
    m_forwardedTxnBuffer[entry.m_blockNum].push_back(entry);

    return true;
  }

  return ProcessForwardTransactionCore(entry);
}

bool Node::ProcessForwardTransactionCore(const ForwardedTxnEntry& entry) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessForwardTransactionCore not expected to be "
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

    CommitForwardedTransactions(entry);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

    if (isEveryMicroBlockAvailable) {
      DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(entry.m_blockNum);

      if (LOOKUP_NODE_MODE && m_isVacuousEpochBuffer &&
          entry.m_blockNum == m_mediator.m_txBlockChain.GetLastBlock()
                                  .GetHeader()
                                  .GetBlockNum()) {
        BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                    {'0'});
        BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TX_BODY_TMP);
      }
    }
  }

  return true;
}

void Node::CommitForwardedTransactionBuffer() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CommitForwardedTransactionBuffer not expected to be "
                "called from Normal node.");
    return;
  }

  LOG_MARKER();

  lock_guard<mutex> g(m_mutexForwardedTxnBuffer);

  for (auto it = m_forwardedTxnBuffer.begin();
       it != m_forwardedTxnBuffer.end();) {
    if (it->first >=
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()) {
      for (const auto& entry : it->second) {
        ProcessForwardTransactionCore(entry);
      }
    }
    it = m_forwardedTxnBuffer.erase(it);
  }
}
