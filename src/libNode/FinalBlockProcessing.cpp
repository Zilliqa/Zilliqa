/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
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
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libServer/Server.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

bool Node::ReadAuxilliaryInfoFromFinalBlockMsg(
    const vector<unsigned char>& message, unsigned int& cur_offset,
    uint8_t& shard_id)
{
    // 32-byte block number
    uint256_t dsBlockNum
        = Serializable::GetNumber<uint256_t>(message, cur_offset, UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    // Check block number
    if (!CheckWhetherDSBlockNumIsLatest(dsBlockNum + 1))
    {
        return false;
    }

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(
        message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    if (consensusID != m_consensusID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus ID is not correct. Expected ID: "
                      << consensusID << " My Consensus ID: " << m_consensusID);
        return false;
    }

    shard_id = Serializable::GetNumber<uint8_t>(message, cur_offset,
                                                sizeof(uint8_t));
    cur_offset += sizeof(uint8_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG shard id is " << (unsigned int)shard_id)

    return true;
}

void Node::StoreState()
{
    LOG_MARKER();
    AccountStore::GetInstance().MoveUpdatesToDisk();
}

void Node::StoreFinalBlock(const TxBlock& txBlock)
{
    m_mediator.m_txBlockChain.AddBlock(txBlock);
    m_mediator.m_currentEpochNum
        = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();

    // At this point, the transactions in the last Epoch is no longer useful, thus erase.
    EraseCommittedTransactions(m_mediator.m_currentEpochNum - 2);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing Tx Block Number: "
                  << txBlock.GetHeader().GetBlockNum()
                  << " with Type: " << txBlock.GetHeader().GetType()
                  << ", Version: " << txBlock.GetHeader().GetVersion()
                  << ", Timestamp: " << txBlock.GetHeader().GetTimestamp()
                  << ", NumTxs: " << txBlock.GetHeader().GetNumTxs());

    // Store Tx Block to disk
    vector<unsigned char> serializedTxBlock;
    txBlock.Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(
        txBlock.GetHeader().GetBlockNum(), serializedTxBlock);

    LOG_GENERAL(
        INFO,
        "View change count:  " << txBlock.GetHeader().GetViewChangeCounter());

    for (unsigned int i = 0; i < txBlock.GetHeader().GetViewChangeCounter();
         i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();
        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block " << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                             << " received with prevhash 0x"
                             << DataConversion::charArrToHexStr(
                                    m_mediator.m_txBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetPrevHash()
                                        .asArray()));

#ifdef STAT_TEST
    LOG_STATE(
        "[FINBK]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        << "] RECV");
#endif // STAT_TEST
}

bool Node::IsMicroBlockTxRootHashInFinalBlock(TxnHash microBlockTxRootHash,
                                              const uint256_t& blocknum,
                                              bool& isEveryMicroBlockAvailable)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Deleting unavailable "
                  << "microblock " << microBlockTxRootHash << " for blocknum "
                  << blocknum);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    auto it = m_unavailableMicroBlocks.find(blocknum);
    bool found = (it != m_unavailableMicroBlocks.end()
                  && it->second.erase(microBlockTxRootHash));
    isEveryMicroBlockAvailable = found && it->second.empty();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Found " << microBlockTxRootHash << ": "
                       << isEveryMicroBlockAvailable);
    return found;
}

void Node::LoadUnavailableMicroBlockTxRootHashes(
    const TxBlock& finalBlock, const boost::multiprecision::uint256_t& blocknum)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unavailable microblock hashes in final block : ")

    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    for (uint i = 0; i < finalBlock.GetMicroBlockHashes().size(); ++i)
    {
        if (!finalBlock.GetIsMicroBlockEmpty()[i])
        {
            auto hash = finalBlock.GetMicroBlockHashes()[i];
            m_unavailableMicroBlocks[blocknum].insert(hash);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      DataConversion::charArrToHexStr(hash.asArray()))
        }
    }

#ifndef IS_LOOKUP_NODE
    if (m_unavailableMicroBlocks.find(blocknum)
            != m_unavailableMicroBlocks.end()
        && m_unavailableMicroBlocks[blocknum].size() > 0)
    {
        lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
        m_allMicroBlocksRecvd = false;
    }
#endif //IS_LOOKUP_NODE
}

bool Node::VerifyFinalBlockCoSignature(const TxBlock& txblock)
{
    LOG_MARKER();

    unsigned int index = 0;
    unsigned int count = 0;

    const vector<bool>& B2 = txblock.GetB2();
    if (m_mediator.m_DSCommitteePubKeys.size() != B2.size())
    {
        LOG_GENERAL(WARNING,
                    "Mismatch: DS committee size = "
                        << m_mediator.m_DSCommitteePubKeys.size()
                        << ", co-sig bitmap size = " << B2.size());
        return false;
    }

    // Generate the aggregated key
    vector<PubKey> keys;
    for (auto& kv : m_mediator.m_DSCommitteePubKeys)
    {
        if (B2.at(index) == true)
        {
            keys.push_back(kv);
            count++;
        }
        index++;
    }

    if (count != ConsensusCommon::NumForConsensus(B2.size()))
    {
        LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
        return false;
    }

    shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
    if (aggregatedKey == nullptr)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
        return false;
    }

    // Verify the collective signature
    vector<unsigned char> message;
    txblock.GetHeader().Serialize(message, 0);
    txblock.GetCS1().Serialize(message, TxBlockHeader::SIZE);
    BitVector::SetBitVector(message, TxBlockHeader::SIZE + BLOCK_SIG_SIZE,
                            txblock.GetB1());
    if (Schnorr::GetInstance().Verify(message, 0, message.size(),
                                      txblock.GetCS2(), *aggregatedKey)
        == false)
    {
        LOG_GENERAL(WARNING, "Cosig verification failed");
        return false;
    }

    return true;
}

bool Node::CheckMicroBlockRootHash(
    const TxBlock& finalBlock, const boost::multiprecision::uint256_t& blocknum)
{
    TxnHash microBlocksHash
        = ComputeTransactionsRoot(finalBlock.GetMicroBlockHashes());

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Expected FinalBlock TxRoot hash : "
            << DataConversion::charArrToHexStr(microBlocksHash.asArray()));

    if (finalBlock.GetHeader().GetTxRootHash() != microBlocksHash)
    {
        LOG_GENERAL(INFO,
                    "TxRootHash in Final Block Header doesn't match root of "
                    "microblock hashes");
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "FinalBlock TxRoot hash in final block by DS is correct");

    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::FindTxnInSubmittedTxnsList(const TxBlock& finalblock,
                                      const uint256_t& blockNum,
                                      uint8_t sharing_mode,
                                      vector<Transaction>& txns_to_send,
                                      const TxnHash& tx_hash)
{
    LOG_MARKER();

    // boost::multiprecision::uint256_t blockNum = m_mediator.m_txBlockChain.GetBlockCount();

    lock(m_mutexSubmittedTransactions, m_mutexCommittedTransactions);
    lock_guard<mutex> g(m_mutexSubmittedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexCommittedTransactions, adopt_lock);

    auto& submittedTransactions = m_submittedTransactions[blockNum];
    auto& committedTransactions = m_committedTransactions[blockNum];
    const auto& txnIt = submittedTransactions.find(tx_hash);

    // Check if transaction is part of submitted Tx list
    if (txnIt != submittedTransactions.end())
    {
        if ((sharing_mode == SEND_ONLY) || (sharing_mode == SEND_AND_FORWARD))
        {
            txns_to_send.push_back(txnIt->second);
        }

        // Move entry from submitted Tx list to committed Tx list
        committedTransactions.push_back(txnIt->second);
        submittedTransactions.erase(txnIt);

        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "[TXN] ["
                << blockNum << "] Committed     = 0x"
                << DataConversion::charArrToHexStr(
                       committedTransactions.back().GetTranID().asArray()));

        // Update from and to accounts
        AccountStore::GetInstance().UpdateAccounts(
            committedTransactions.back());

        // DO NOT DELETE. PERISTENT STORAGE
        /**
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "##Storing Transaction##");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr(tx_hash));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), (*entry).GetAmount());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetToAddr()));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetFromAddr()));
        **/

        //LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Storing Transaction: "<< DataConversion::charArrToHexStr(tx_hash) <<
        //    " with amount: "<<(*entry).GetAmount()<<
        //    ", to: "<<DataConversion::charArrToHexStr((*entry).GetToAddr())<<
        //   ", from: "<<DataConversion::charArrToHexStr((*entry).GetFromAddr()));

        // Store TxBody to disk
        vector<unsigned char> serializedTxBody;
        committedTransactions.back().Serialize(serializedTxBody, 0);
        if (BlockStorage::GetBlockStorage().PutTxBody(tx_hash,
                                                      serializedTxBody))
        {
            LOG_GENERAL(INFO, "FAIL: PutTxBody Failed");
        }

        // Move on to next transaction in block
        return true;
    }

    return false;
}

bool Node::FindTxnInReceivedTxnsList(const TxBlock& finalblock,
                                     const uint256_t& blockNum,
                                     uint8_t sharing_mode,
                                     vector<Transaction>& txns_to_send,
                                     const TxnHash& tx_hash)
{
    LOG_MARKER();

    lock(m_mutexReceivedTransactions, m_mutexCommittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexCommittedTransactions, adopt_lock);

    auto& receivedTransactions = m_receivedTransactions[blockNum];
    auto& committedTransactions = m_committedTransactions[blockNum];
    const auto& txnIt = receivedTransactions.find(tx_hash);

    // Check if transaction is part of received Tx list
    if (txnIt != receivedTransactions.end())
    {
        if ((sharing_mode == SEND_ONLY) || (sharing_mode == SEND_AND_FORWARD))
        {
            txns_to_send.push_back(txnIt->second);
        }

        // Move entry from received Tx list to committed Tx list
        committedTransactions.push_back(txnIt->second);
        receivedTransactions.erase(txnIt);

        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "[TXN] ["
                << blockNum << "] Committed     = 0x"
                << DataConversion::charArrToHexStr(
                       committedTransactions.back().GetTranID().asArray()));

        // Update from and to accounts
        AccountStore::GetInstance().UpdateAccounts(
            committedTransactions.back());

        /**
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "##Storing Transaction##");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr(tx_hash));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), (*entry).GetAmount());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetToAddr()));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetFromAddr()));
        **/

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "ReceivedTransaction: Storing Transaction: "
                      << DataConversion::charArrToHexStr(tx_hash.asArray())
                      << " with amount: "
                      << committedTransactions.back().GetAmount() << ", to: "
                      << committedTransactions.back().GetToAddr() << ", from: "
                      << Account::GetAddressFromPublicKey(
                             committedTransactions.back().GetSenderPubKey()));

        // Store TxBody to disk
        vector<unsigned char> serializedTxBody;
        committedTransactions.back().Serialize(serializedTxBody, 0);
        if (!BlockStorage::GetBlockStorage().PutTxBody(tx_hash,
                                                       serializedTxBody))
        {
            LOG_GENERAL(INFO, "FAIL: PutTxBody Failed");
        }

        // Move on to next transaction in block
        return true;
    }

    return false;
}

void Node::CommitMyShardsMicroBlock(const TxBlock& finalblock,
                                    const uint256_t& blocknum,
                                    uint8_t sharing_mode,
                                    vector<Transaction>& txns_to_send)
{
    LOG_MARKER();

    // Loop through transactions in block
    const vector<TxnHash>& tx_hashes = m_microblock->GetTranHashes();
    for (unsigned int i = 0; i < tx_hashes.size(); i++)
    {
        const TxnHash& tx_hash = tx_hashes.at(i);

        if (FindTxnInSubmittedTxnsList(finalblock, blocknum, sharing_mode,
                                       txns_to_send, tx_hash))
        {
            continue;
        }

        if (!FindTxnInReceivedTxnsList(finalblock, blocknum, sharing_mode,
                                       txns_to_send, tx_hash))
        {
            // TODO
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Cannnot find txn in submitted txn and recv list");
        }
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of transactions to broadcast for block "
                  << blocknum << " = " << txns_to_send.size());

    {
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        m_receivedTransactions.erase(blocknum);
    }
    {
        lock_guard<mutex> g2(m_mutexSubmittedTransactions);
        m_submittedTransactions.erase(blocknum);
    }
}

void Node::BroadcastTransactionsToSendingAssignment(
    const uint256_t& blocknum, const vector<Peer>& sendingAssignment,
    const TxnHash& microBlockTxHash, vector<Transaction>& txns_to_send) const
{
    LOG_MARKER();

    if (txns_to_send.size() > 0)
    {
        // Transaction body sharing
        unsigned int cur_offset = MessageOffset::BODY;
        vector<unsigned char> forwardtxn_message
            = {MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION};

        // block num
        Serializable::SetNumber<uint256_t>(forwardtxn_message, cur_offset,
                                           blocknum, UINT256_SIZE);
        cur_offset += UINT256_SIZE;

        forwardtxn_message.resize(cur_offset + TRAN_HASH_SIZE);

        // microblock tx hash
        copy(microBlockTxHash.asArray().begin(),
             microBlockTxHash.asArray().end(),
             forwardtxn_message.begin() + cur_offset);
        cur_offset += TRAN_HASH_SIZE;

        for (unsigned int i = 0; i < txns_to_send.size(); i++)
        {
            // txn body
            txns_to_send.at(i).Serialize(forwardtxn_message, cur_offset);
            cur_offset += txns_to_send.at(i).GetSerializedSize();

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "[TXN] ["
                          << blocknum << "] Broadcasted   = 0x"
                          << DataConversion::charArrToHexStr(
                                 txns_to_send.at(i).GetTranID().asArray()));
        }

        P2PComm::GetInstance().SendBroadcastMessage(sendingAssignment,
                                                    forwardtxn_message);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG: I have broadcasted the txn body!")

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I will soon be sending the txn bodies to the lookup nodes");
        m_mediator.m_lookup->SendMessageToLookupNodes(forwardtxn_message);
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG I have no txn body to send")
    }
}

void Node::LoadForwardingAssignmentFromFinalBlock(
    const vector<Peer>& fellowForwarderNodes, const uint256_t& blocknum)
{
    // For now, since each sharding setup only processes one block, then whatever transactions we
    // failed to submit have to be discarded m_createdTransactions.clear();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[shard " << m_myShardID
                        << "] I am a forwarder for transactions in block "
                        << blocknum);

    lock_guard<mutex> g2(m_mutexForwardingAssignment);

    m_forwardingAssignment.insert(make_pair(blocknum, vector<Peer>()));
    vector<Peer>& peers = m_forwardingAssignment.at(blocknum);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forward list:");

    for (unsigned int i = 0; i < m_myShardMembersNetworkInfo.size(); i++)
    {
        if (i == m_consensusMyID)
        {
            continue;
        }
        // if (rand() % m_myShardMembersNetworkInfo.size() <= GOSSIP_RATE)
        // {
        //     peers.push_back(m_myShardMembersNetworkInfo.at(i));
        // }
        peers.push_back(m_myShardMembersNetworkInfo.at(i));
    }

    for (unsigned int i = 0; i < fellowForwarderNodes.size(); i++)
    {
        Peer fellowforwarder = fellowForwarderNodes[i];

        for (unsigned int j = 0; j < peers.size(); j++)
        {
            if (peers.at(j) == fellowforwarder)
            {
                peers.at(j) = move(peers.back());
                peers.pop_back();
                break;
            }
        }
    }

    for (unsigned int i = 0; i < peers.size(); i++)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  peers.at(i));
    }
}

bool Node::IsMyShardsMicroBlockTxRootHashInFinalBlock(
    const uint256_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    return m_microblock != nullptr
        && IsMicroBlockTxRootHashInFinalBlock(
               m_microblock->GetHeader().GetTxRootHash(), blocknum,
               isEveryMicroBlockAvailable);
}

bool Node::ActOnFinalBlock(uint8_t tx_sharing_mode, const vector<Peer>& nodes)
{
    // #ifndef IS_LOOKUP_NODE
    // If tx_sharing_mode=IDLE              ==> Body = [ignored]
    // If tx_sharing_mode=SEND_ONLY         ==> Body = [num receivers in other shards] [IP and node] ... [IP and node]
    // If tx_sharing_mode=DS_FORWARD_ONLY   ==> Body = [num receivers in DS comm] [IP and node] ... [IP and node]
    // If tx_sharing_mode=NODE_FORWARD_ONLY ==> Body = [num fellow forwarders] [IP and node] ... [IP and node]
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexMicroBlock);
    const TxBlock finalblock = m_mediator.m_txBlockChain.GetLastBlock();
    const uint256_t& blocknum = finalblock.GetHeader().GetBlockNum();

    vector<Peer> sendingAssignment;

    switch (tx_sharing_mode)
    {
    case SEND_ONLY:
    {
        sendingAssignment = nodes;
        break;
    }
    case DS_FORWARD_ONLY:
    {
        lock_guard<mutex> g2(m_mutexForwardingAssignment);
        m_forwardingAssignment.insert(make_pair(blocknum, nodes));
        break;
    }
    case NODE_FORWARD_ONLY:
    {
        LoadForwardingAssignmentFromFinalBlock(nodes, blocknum);
        break;
    }
    case IDLE:
    default:
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am idle for transactions in block " << blocknum);
        break;
    }
    }

    // LoadUnavailableMicroBlockTxRootHashes(finalblock, blocknum);
    bool isEveryMicroBlockAvailable;

    // For now, since each sharding setup only processes one block, then whatever transactions we
    // failed to submit have to be discarded m_createdTransactions.clear();
    if (IsMyShardsMicroBlockTxRootHashInFinalBlock(blocknum,
                                                   isEveryMicroBlockAvailable))
    {
        vector<Transaction> txns_to_send;

        CommitMyShardsMicroBlock(finalblock, blocknum, tx_sharing_mode,
                                 txns_to_send);

        if (sendingAssignment.size() > 0)
        {
            BroadcastTransactionsToSendingAssignment(
                blocknum, sendingAssignment,
                m_microblock->GetHeader().GetTxRootHash(), txns_to_send);
        }

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
        }
    }
    else
    {
        // TODO
    }
    // #endif // IS_LOOKUP_NODE
    return true;
}

bool Node::ActOnFinalBlock(uint8_t tx_sharing_mode,
                           vector<Peer> sendingAssignment,
                           const vector<Peer>& fellowForwarderNodes)
{
    // #ifndef IS_LOOKUP_NODE
    // Body = [num receivers in  other shards] [IP and node] ... [IP and node]
    //        [num fellow forwarders] [IP and node] ... [IP and node]

    LOG_MARKER();

    lock_guard<mutex> g(m_mutexMicroBlock);
    if (tx_sharing_mode == SEND_AND_FORWARD)
    {
        const TxBlock finalblock = m_mediator.m_txBlockChain.GetLastBlock();
        const uint256_t& blocknum = finalblock.GetHeader().GetBlockNum();

        LoadForwardingAssignmentFromFinalBlock(fellowForwarderNodes, blocknum);

        // LoadUnavailableMicroBlockTxRootHashes(finalblock, blocknum);
        bool isEveryMicroBlockAvailable;

        if (IsMyShardsMicroBlockTxRootHashInFinalBlock(
                blocknum, isEveryMicroBlockAvailable))
        {
            vector<Transaction> txns_to_send;

            CommitMyShardsMicroBlock(finalblock, blocknum, tx_sharing_mode,
                                     txns_to_send);

            if (sendingAssignment.size() > 0)
            {
                BroadcastTransactionsToSendingAssignment(
                    blocknum, sendingAssignment,
                    m_microblock->GetHeader().GetTxRootHash(), txns_to_send);
            }

            if (isEveryMicroBlockAvailable)
            {
                DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
            }
        }
        else
        {
            // TODO
        }
    }
    else
    {
        return false;
    }
    // #endif // IS_LOOKUP_NODE
    return true;
}

void Node::InitiatePoW1()
{
    // reset consensusID and first consensusLeader is index 0
    m_consensusID = 0;
    m_consensusLeaderID = 0;

    SetState(POW1_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        (uint64_t)m_mediator.m_dsBlockChain
            .GetBlockCount()); // hack hack hack -- typecasting
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Start pow1 ");
    auto func = [this]() mutable -> void {
        auto epochNumber = m_mediator.m_dsBlockChain.GetBlockCount();
        auto dsBlockRand = m_mediator.m_dsBlockRand;
        auto txBlockRand = m_mediator.m_txBlockRand;
        StartPoW1(epochNumber, POW1_DIFFICULTY, dsBlockRand, txBlockRand);
    };

    DetachedFunction(1, func);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Soln to pow1 found ");
}

void Node::UpdateStateForNextConsensusRound()
{
    // Set state to tx submission
    if (m_isPrimary == true)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "MS: I am no longer the shard leader ");
        m_isPrimary = false;
    }

    m_consensusLeaderID++;
    m_consensusID++;
    m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;

    if (m_consensusMyID == m_consensusLeaderID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "MS: I am the new shard leader ");
        m_isPrimary = true;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "MS: The new shard leader is m_consensusMyID "
                      << m_consensusLeaderID);
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: Next non-ds epoch begins");

    SetState(TX_SUBMISSION);
    cv_txSubmission.notify_all();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[No PoW needed] MS: Start submit txn stage again.");
}

void Node::ScheduleTxnSubmission()
{
    auto main_func = [this]() mutable -> void { SubmitTransactions(); };

    DetachedFunction(1, main_func);

    LOG_GENERAL(INFO,
                "I am going to sleep for " << SUBMIT_TX_WINDOW << " seconds");
    this_thread::sleep_for(chrono::seconds(SUBMIT_TX_WINDOW));
    LOG_GENERAL(INFO,
                "I have woken up from the sleep of " << SUBMIT_TX_WINDOW
                                                     << " seconds");

    auto main_func2 = [this]() mutable -> void {
        SetState(TX_SUBMISSION_BUFFER);
        cv_txSubmission.notify_all();
    };

    DetachedFunction(1, main_func2);
}

void Node::ScheduleMicroBlockConsensus()
{
    LOG_GENERAL(INFO,
                "I am going to use conditional variable with timeout of  "
                    << SUBMIT_TX_WINDOW_EXTENDED
                    << " seconds. It is ok to timeout here. ");
    std::unique_lock<std::mutex> cv_lk(m_MutexCVMicroblockConsensus);
    if (cv_microblockConsensus.wait_for(
            cv_lk, std::chrono::seconds(SUBMIT_TX_WINDOW_EXTENDED))
        == std::cv_status::timeout)
    {
        LOG_GENERAL(INFO,
                    "I have woken up from the sleep of "
                        << SUBMIT_TX_WINDOW_EXTENDED << " seconds");
    }
    else
    {
        LOG_GENERAL(
            INFO,
            "I have received announcement message. Time to run consensus.");
    }
    auto main_func3 = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

    DetachedFunction(1, main_func3);
}

void Node::BeginNextConsensusRound()
{
    UpdateStateForNextConsensusRound();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (!isVacuousEpoch)
    {
        ScheduleTxnSubmission();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping submit transactions");
    }

    ScheduleMicroBlockConsensus();
}

void Node::LoadTxnSharingInfo(const vector<unsigned char>& message,
                              unsigned int& cur_offset, uint8_t shard_id,
                              bool& i_am_sender, bool& i_am_forwarder,
                              vector<vector<Peer>>& nodes)
{
    // Transaction body sharing setup
    // Everyone (DS and non-DS) needs to remember their sharing assignments for this particular block

    // Transaction body sharing assignments:
    // PART 1. Select X random nodes from DS committee for receiving Tx bodies and broadcasting to other DS nodes
    // PART 2. Select X random nodes per shard for receiving Tx bodies and broadcasting to other nodes in the shard
    // PART 3. Select X random nodes per shard for sending Tx bodies to the receiving nodes in other committees (DS and shards)

    // Message format:
    // [4-byte num of DS nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committees]
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // ...
    LOG_MARKER();

    uint32_t num_ds_nodes = Serializable::GetNumber<uint32_t>(
        message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Forwarders inside the DS committee (" << num_ds_nodes << "):");

    nodes.push_back(vector<Peer>());

    for (unsigned int i = 0; i < num_ds_nodes; i++)
    {
        nodes.back().push_back(Peer(message, cur_offset));
        cur_offset += IP_SIZE + PORT_SIZE;

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  nodes.back().back());
    }

    uint32_t num_shards = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                            sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Number of shards: " << num_shards);

    for (unsigned int i = 0; i < num_shards; i++)
    {
        if (i == shard_id)
        {
            nodes.push_back(vector<Peer>());

            uint32_t num_recv = Serializable::GetNumber<uint32_t>(
                message, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "  Shard " << i << " forwarders:");

            for (unsigned int j = 0; j < num_recv; j++)
            {
                nodes.back().push_back(Peer(message, cur_offset));
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          nodes.back().back());

                if (nodes.back().back() == m_mediator.m_selfPeer)
                {
                    i_am_forwarder = true;
                }
            }

            nodes.push_back(vector<Peer>());

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "  Shard " << i << " senders:");

            uint32_t num_send = Serializable::GetNumber<uint32_t>(
                message, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            for (unsigned int j = 0; j < num_send; j++)
            {
                nodes.back().push_back(Peer(message, cur_offset));
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          nodes.back().back());

                if (nodes.back().back() == m_mediator.m_selfPeer)
                {
                    i_am_sender = true;
                }
            }
        }
        else
        {
            nodes.push_back(vector<Peer>());

            uint32_t num_recv = Serializable::GetNumber<uint32_t>(
                message, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "  Shard " << i << " forwarders:");

            for (unsigned int j = 0; j < num_recv; j++)
            {
                nodes.back().push_back(Peer(message, cur_offset));
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          nodes.back().back());
            }

            nodes.push_back(vector<Peer>());

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "  Shard " << i << " senders:");

            uint32_t num_send = Serializable::GetNumber<uint32_t>(
                message, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            for (unsigned int j = 0; j < num_send; j++)
            {
                nodes.back().push_back(Peer(message, cur_offset));
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          nodes.back().back());
            }
        }
    }
}

void Node::CallActOnFinalBlockBasedOnSenderForwarderAssgn(
    bool i_am_sender, bool i_am_forwarder, const vector<vector<Peer>>& nodes,
    uint8_t shard_id)
{
    if ((i_am_sender == false) && (i_am_forwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers = nodes.at(shard_id + 1);
        ActOnFinalBlock(TxSharingMode::NODE_FORWARD_ONLY, my_shard_receivers);
    }
    else if ((i_am_sender == true) && (i_am_forwarder == false))
    {
        vector<Peer> nodes_to_send;

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "iii amam herehere");

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < nodes.at(0).size(); i++)
        {
            nodes_to_send.push_back(nodes[0][i]);
        }

        for (unsigned int i = 1; i < nodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = nodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                nodes_to_send.push_back(shard[j]);
            }
        }

        ActOnFinalBlock(TxSharingMode::SEND_ONLY, nodes_to_send);
    }
    else if ((i_am_sender == true) && (i_am_forwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers = nodes.at(shard_id + 1);

        vector<Peer> fellowForwarderNodes;

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < nodes.at(0).size(); i++)
        {
            fellowForwarderNodes.push_back(nodes[0][i]);
        }

        for (unsigned int i = 1; i < nodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = nodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                fellowForwarderNodes.push_back(shard[j]);
            }
        }

        ActOnFinalBlock(TxSharingMode::SEND_AND_FORWARD, fellowForwarderNodes,
                        my_shard_receivers);
    }
    else
    {
        ActOnFinalBlock(TxSharingMode::IDLE, vector<Peer>());
    }
}
#endif // IS_LOOKUP_NODE

void Node::LogReceivedFinalBlockDetails(const TxBlock& txblock)
{
#ifdef IS_LOOKUP_NODE
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
              "txblock.GetHeader().GetNumMicroBlockHashes(): "
                  << txblock.GetHeader().GetNumMicroBlockHashes());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetStateRootHash(): "
                  << txblock.GetHeader().GetStateRootHash());
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "txblock.GetHeader().GetNumTxs(): " << txblock.GetHeader().GetNumTxs());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "txblock.GetHeader().GetMinerPubKey(): "
                  << txblock.GetHeader().GetMinerPubKey());
#endif // IS_LOOKUP_NODE
}

bool Node::CheckStateRoot(const TxBlock& finalBlock)
{
    StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

    AccountStore::GetInstance().PrintAccountState();

    if (stateRoot != finalBlock.GetHeader().GetStateRootHash())
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State root doesn't match. Expected = "
                      << stateRoot << ". "
                      << "Received = "
                      << finalBlock.GetHeader().GetStateRootHash());
        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State root matched "
                  << finalBlock.GetHeader().GetStateRootHash());

    return true;
}

// void Node::StoreMicroBlocksToDisk()
// {
//     LOG_MARKER();
//     for(auto microBlock : m_microBlocks)
//     {

//         LOG_GENERAL(INFO,  "Storing Micro Block Hash: " << microBlock.GetHeader().GetTxRootHash() <<
//             " with Type: " << microBlock.GetHeader().GetType() <<
//             ", Version: " << microBlock.GetHeader().GetVersion() <<
//             ", Timestamp: " << microBlock.GetHeader().GetTimestamp() <<
//             ", NumTxs: " << microBlock.GetHeader().GetNumTxs());

//         vector<unsigned char> serializedMicroBlock;
//         microBlock.Serialize(serializedMicroBlock, 0);
//         BlockStorage::GetBlockStorage().PutMicroBlock(microBlock.GetHeader().GetTxRootHash(),
//                                                serializedMicroBlock);
//     }
//     m_microBlocks.clear();
// }

bool Node::ProcessFinalBlock(const vector<unsigned char>& message,
                             unsigned int offset, const Peer& from)
{
    // Message = [32-byte DS blocknum] [4-byte consensusid] [1-byte shard id]
    //           [Final block] [Tx body sharing setup]
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    if (m_state == MICROBLOCK_CONSENSUS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I may have missed the micrblock consensus. However, if I "
                  "recent a valid finalblock. I will accept it");
        // TODO: Optimize state transition.
        SetState(WAITING_FINALBLOCK);
    }

    if (!CheckState(PROCESS_FINALBLOCK))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Too late - current state is " << m_state << ".");
        return false;
    }

        /*
    unsigned int sleep_time_while_waiting = 100;
    if (m_state == MICROBLOCK_CONSENSUS)
    {
        for (unsigned int i = 0; i < 50; i++)
        {
            if (m_state == WAITING_FINALBLOCK)
            {
                break;
            }

            if (i % 10 == 0)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                             "Waiting for MICROBLOCK_CONSENSUS before "
                             "proceeding to process finalblock");
            }
            this_thread::sleep_for(
                chrono::milliseconds(sleep_time_while_waiting));
        }
        LOG_GENERAL(INFO,
            "I got stuck at process final block but move on. Current state is "
            "MICROBLOCK_CONSENSUS, ")
        // return false;
        SetState(WAITING_FINALBLOCK);
    }
    */

#endif // IS_LOOKUP_NODE

    unsigned int cur_offset = offset;

    uint8_t shard_id = (uint8_t)-1;

    // Reads and checks DS Block number, consensus ID and Shard ID
    if (!ReadAuxilliaryInfoFromFinalBlockMsg(message, cur_offset, shard_id))
    {
        return false;
    }

    // TxBlock txBlock(message, cur_offset);
    TxBlock txBlock;
    if (txBlock.Deserialize(message, cur_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize TxBlock.");
        return false;
    }
    cur_offset += txBlock.GetSerializedSize();

    LogReceivedFinalBlockDetails(txBlock);

    LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << txBlock.GetHeader().GetBlockNum()
                         << "] FRST");

    // Verify the co-signature
    if (!VerifyFinalBlockCoSignature(txBlock))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "TxBlock co-sig verification failed");
        return false;
    }

    // #ifdef IS_LOOKUP_NODE
    if (!CheckMicroBlockRootHash(txBlock, txBlock.GetHeader().GetBlockNum()))
    {
        return false;
    }

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (!isVacuousEpoch)
    {
        LoadUnavailableMicroBlockTxRootHashes(
            txBlock, txBlock.GetHeader().GetBlockNum());
    }
    else
    {
        LOG_GENERAL(INFO, "isVacuousEpoch now");

        if (!AccountStore::GetInstance().UpdateStateTrieAll())
        {
            LOG_GENERAL(WARNING, "UpdateStateTrieAll Failed");
            return false;
        }

        if (!CheckStateRoot(txBlock))
        {
#ifndef IS_LOOKUP_NODE
            RejoinAsNormal();
#endif // IS_LOOKUP_NODE
            return false;
        }
        StoreState();
        BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                    {'0'});
#ifndef IS_LOOKUP_NODE
        BlockStorage::GetBlockStorage().PopFrontTxBodyDB();
#else // IS_LOOKUP_NODE
        BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TX_BODY_TMP);
#endif // IS_LOOKUP_NODE
    }
    // #endif // IS_LOOKUP_NODE

    StoreFinalBlock(txBlock);

    if (txBlock.GetHeader().GetNumMicroBlockHashes() == 1)
    {
        LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << txBlock.GetHeader().GetBlockNum()
                             << "] LAST");
    }

    // Assumption: New PoW1 done after every block committed
    // If I am not a DS committee member (and since I got this FinalBlock message,
    // then I know I'm not), I can start doing PoW1 again
    m_mediator.UpdateDSBlockRand();
    m_mediator.UpdateTxBlockRand();

#ifndef IS_LOOKUP_NODE

    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
    {
        InitiatePoW1();
    }
    else
    {
        auto main_func
            = [this]() mutable -> void { BeginNextConsensusRound(); };

        DetachedFunction(1, main_func);
    }

    bool i_am_sender = false;
    bool i_am_forwarder = false;
    vector<vector<Peer>> nodes;

    LoadTxnSharingInfo(message, cur_offset, shard_id, i_am_sender,
                       i_am_forwarder, nodes);

    CallActOnFinalBlockBasedOnSenderForwarderAssgn(i_am_sender, i_am_forwarder,
                                                   nodes, shard_id);
#else // IS_LOOKUP_NODE
    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
    {
        m_consensusID = 0;
        m_consensusLeaderID = 0;
    }
    else
    {
        m_consensusID++;
        m_consensusLeaderID++;
        m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Node::LoadForwardedTxnsAndCheckRoot(
    const vector<unsigned char>& message, unsigned int cur_offset,
    TxnHash& microBlockTxHash, vector<Transaction>& txnsInForwardedMessage)
// vector<TxnHash> & txnHashesInForwardedMessage)
{
    LOG_MARKER();

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + TRAN_HASH_SIZE,
         microBlockTxHash.asArray().begin());
    cur_offset += TRAN_HASH_SIZE;
    LOG_GENERAL(
        INFO,
        "Received MicroBlock TxHash root : "
            << DataConversion::charArrToHexStr(microBlockTxHash.asArray()));

    vector<TxnHash> txnHashesInForwardedMessage;

    while (cur_offset < message.size())
    {
        // reading [Transaction] from received msg
        // Transaction tx(message, cur_offset);
        Transaction tx;
        if (tx.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize Transaction.");
            return false;
        }
        cur_offset += tx.GetSerializedSize();

        txnsInForwardedMessage.push_back(tx);
        txnHashesInForwardedMessage.push_back(tx.GetTranID());
    }

    return ComputeTransactionsRoot(txnHashesInForwardedMessage)
        == microBlockTxHash;
}

void Node::CommitForwardedTransactions(
    const vector<Transaction>& txnsInForwardedMessage,
    const uint256_t& blocknum)
{
    LOG_MARKER();

    unsigned int txn_counter = 0;
    for (const auto& tx : txnsInForwardedMessage)
    {
        {
            lock_guard<mutex> g(m_mutexCommittedTransactions);
            m_committedTransactions[blocknum].push_back(tx);
            AccountStore::GetInstance().UpdateAccounts(tx);
        }

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //              "[TXN] [" << blocknum << "] Body received = 0x" << tx.GetTranID());

            // Update from and to accounts
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Account store updated");

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //              "Storing Transaction: " << tx.GetTranID() <<
            //              " with amount: " << tx.GetAmount() <<
            //              ", to: " << tx.GetToAddr() <<
            //              ", from: " << tx.GetFromAddr());
#ifdef IS_LOOKUP_NODE
        Server::AddToRecentTransactions(tx.GetTranID());
#endif //IS_LOOKUP_NODE

        // Store TxBody to disk
        vector<unsigned char> serializedTxBody;
        tx.Serialize(serializedTxBody, 0);
        BlockStorage::GetBlockStorage().PutTxBody(tx.GetTranID(),
                                                  serializedTxBody);

        txn_counter++;
        if (txn_counter % 10000 == 0)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Proceessed " << txn_counter << " of txns.");
        }
    }
}

#ifndef IS_LOOKUP_NODE
void Node::LoadFwdingAssgnForThisBlockNum(const uint256_t& blocknum,
                                          vector<Peer>& forward_list)
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexForwardingAssignment);
    auto f = m_forwardingAssignment.find(blocknum);
    if (f != m_forwardingAssignment.end())
    {
        forward_list = f->second;
    }
}
#endif // IS_LOOKUP_NODE

void Node::DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
    const uint256_t& blocknum)
{
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    lock(m_mutexForwardingAssignment, m_mutexUnavailableMicroBlocks);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks, adopt_lock);
    lock_guard<mutex> g2(m_mutexForwardingAssignment, adopt_lock);
#else // IS_LOOKUP_NODE
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
#endif // IS_LOOKUP_NODE

    auto it = m_unavailableMicroBlocks.find(blocknum);

    for (auto it : m_unavailableMicroBlocks)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unavailable"
                  " microblock bodies in finalblock "
                      << it.first << ": " << it.second.size());
        for (auto it2 : it.second)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      it2);
        }
    }

    if (it != m_unavailableMicroBlocks.end() && it->second.empty())
    {
        m_unavailableMicroBlocks.erase(it);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Deleting blocknum "
                      << blocknum << " from unavailable microblocks list.");

#ifndef IS_LOOKUP_NODE
        m_forwardingAssignment.erase(blocknum);
        if (m_unavailableMicroBlocks.empty())
        {
            {
                lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
                m_allMicroBlocksRecvd = true;
            }
            m_cvAllMicroBlocksRecvd.notify_all();
        }
#endif // IS_LOOKUP_NODE

        LOG_STATE("[TXBOD][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << blocknum << "] LAST");
    }
}

bool Node::ProcessForwardTransaction(const vector<unsigned char>& message,
                                     unsigned int cur_offset, const Peer& from)
{
    // Message = [block number] [microblockhash] [Transaction] [Transaction] [Transaction] ....
    // Received from other shards

    LOG_MARKER();

    // reading [block number] from received msg
    uint256_t blocknum
        = Serializable::GetNumber<uint256_t>(message, cur_offset, UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    LOG_GENERAL(INFO, "Received forwarded txns for block number " << blocknum);

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        < blocknum)
    {
        unsigned int time_pass = 0;
        while (
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            < blocknum)
        {
            if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
            {
                return false;
            }

            if (time_pass % 600 == 0)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Blocknum " + blocknum.convert_to<string>()
                              + " waiting "
                              + "for state change from WAITING_FINALBLOCK "
                                "to TX_SUBMISSION");
            }
            time_pass++;

            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    TxnHash microBlockTxRootHash;
    vector<Transaction> txnsInForwardedMessage;
    // vector<TxnHash> txnHashesInForwardedMessage;

    if (!LoadForwardedTxnsAndCheckRoot(
            message, cur_offset, microBlockTxRootHash,
            txnsInForwardedMessage /*, txnHashesInForwardedMessage*/))
    {
        return false;
    }

    bool isEveryMicroBlockAvailable;

    if (!IsMicroBlockTxRootHashInFinalBlock(microBlockTxRootHash, blocknum,
                                            isEveryMicroBlockAvailable))
    {
        return false;
    }

    // StoreTxInMicroBlock(microBlockTxRootHash, txnHashesInForwardedMessage)

    CommitForwardedTransactions(txnsInForwardedMessage, blocknum);

#ifndef IS_LOOKUP_NODE
    vector<Peer> forward_list;
    LoadFwdingAssgnForThisBlockNum(blocknum, forward_list);
#endif // IS_LOOKUP_NODE

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

    if (isEveryMicroBlockAvailable)
    {
        DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
    }

#ifndef IS_LOOKUP_NODE
    if (forward_list.size() > 0)
    {
        P2PComm::GetInstance().SendBroadcastMessage(forward_list, message);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG I have broadcasted the txn body!")
    }
#endif // IS_LOOKUP_NODE

    return true;
}
