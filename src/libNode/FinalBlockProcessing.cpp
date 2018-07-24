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
    uint32_t& shard_id)
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

    shard_id = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                 sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

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
    AddBlock(txBlock);
    m_mediator.m_currentEpochNum
        = (uint64_t)m_mediator.m_txBlockChain.GetLastBlock()
              .GetHeader()
              .GetBlockNum()
        + 1;

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

    LOG_STATE(
        "[FINBK]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        << "] RECV");
}

bool Node::IsMicroBlockTxRootHashInFinalBlock(
    TxnHash microBlockTxRootHash, StateHash microBlockStateDeltaHash,
    const uint256_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Deleting unavailable "
                  << "microblock txRootHash " << microBlockTxRootHash
                  << " stateDeltaHash " << microBlockStateDeltaHash
                  << " for blocknum " << blocknum);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    auto it = m_unavailableMicroBlocks.find(blocknum);
    bool found
        = (it != m_unavailableMicroBlocks.end()
           && RemoveTxRootHashFromUnavailableMicroBlock(
                  blocknum, microBlockTxRootHash, microBlockStateDeltaHash));
    isEveryMicroBlockAvailable = found && it->second.empty();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Found txRootHash " << microBlockTxRootHash << " stateDeltaHash "
                                  << microBlockStateDeltaHash << " : "
                                  << isEveryMicroBlockAvailable);
    return found;
}

bool Node::IsMicroBlockStateDeltaHashInFinalBlock(
    StateHash microBlockStateDeltaHash, TxnHash microBlockTxRootHash,
    const boost::multiprecision::uint256_t& blocknum,
    bool& isEveryMicroBlockAvailable)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Deleting unavailable "
                  << "microblock stateDeltaHash: " << microBlockStateDeltaHash
                  << " txRootHash " << microBlockTxRootHash << " for blocknum "
                  << blocknum);
    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    auto it = m_unavailableMicroBlocks.find(blocknum);
    bool found
        = (it != m_unavailableMicroBlocks.end()
           && RemoveStateDeltaHashFromUnavailableMicroBlock(
                  blocknum, microBlockStateDeltaHash, microBlockTxRootHash));
    isEveryMicroBlockAvailable = found && it->second.empty();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Found stateDeltaHash " << microBlockStateDeltaHash << ": "
                                      << isEveryMicroBlockAvailable);
    return found;
}

bool Node::LoadUnavailableMicroBlockHashes(
    const TxBlock& finalBlock, const boost::multiprecision::uint256_t& blocknum)
{
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Unavailable microblock hashes in final block : ")

    lock_guard<mutex> g(m_mutexUnavailableMicroBlocks);
    for (uint i = 0; i < finalBlock.GetMicroBlockHashes().size(); ++i)
    {
        if (!finalBlock.GetIsMicroBlockEmpty()[i]
            || (finalBlock.GetMicroBlockHashes()[i].m_stateDeltaHash
                != StateHash()))
        {
            m_unavailableMicroBlocks[blocknum].insert(
                {{finalBlock.GetMicroBlockHashes()[i],
                  finalBlock.GetShardIDs()[i]},
#ifdef IS_LOOKUP_NODE
                 {!finalBlock.GetIsMicroBlockEmpty()[i], true}});
#else // IS_LOOKUP_NODE
                 {false, true}});
#endif // IS_LOOKUP_NODE
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      finalBlock.GetMicroBlockHashes()[i]);
        }
    }

#ifndef IS_LOOKUP_NODE
    if (m_unavailableMicroBlocks.find(blocknum)
            != m_unavailableMicroBlocks.end()
        && m_unavailableMicroBlocks[blocknum].size() > 0)
    {
        {
            lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
            m_allMicroBlocksRecvd = false;
        }

        bool doRejoin = false;

        if (IsMyShardMicroBlockInFinalBlock(blocknum))
        {
            {
                lock_guard<mutex> g3(m_mutexTempCommitted);
                m_tempStateDeltaCommitted = false;
            }
            if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum)
            {
                LOG_GENERAL(WARNING,
                            "Found my microblock but Cosig not updated");
                doRejoin = true;
            }
        }
        else
        {
            if (IsMyShardIdInFinalBlock(blocknum))
            {
                LOG_GENERAL(WARNING,
                            "Didn't found my micorblock but found shard ID");
                doRejoin = true;
            }
        }

        if (doRejoin || m_doRejoinAtFinalBlock)
        {
            LOG_GENERAL(WARNING,
                        "Failed the last microblock consensus but "
                        "still found my shard microblock, "
                        " need to Rejoin");
            RejoinAsNormal();
            return false;
        }
    }
#endif //IS_LOOKUP_NODE
    return true;
}

bool Node::RemoveTxRootHashFromUnavailableMicroBlock(
    const boost::multiprecision::uint256_t& blocknum,
    const TxnHash& txnRootHash, const StateHash& stateDeltaHash)
{
    for (auto it = m_unavailableMicroBlocks[blocknum].begin();
         it != m_unavailableMicroBlocks[blocknum].end(); it++)
    {
        if (it->first.m_hash.m_txRootHash == txnRootHash
            && it->first.m_hash.m_stateDeltaHash == stateDeltaHash)
        {
            LOG_GENERAL(INFO,
                        "Found microblock txnRootHash: " << txnRootHash
                                                         << " stateDeltaHash: "
                                                         << stateDeltaHash);
            it->second[0] = false;
            if (!it->second[1])
            {
                LOG_GENERAL(INFO,
                            "Remove microblock (txRootHash: "
                                << txnRootHash << " stateDeltaHash: "
                                << it->first.m_hash.m_stateDeltaHash << ")");
                LOG_GENERAL(INFO,
                            "Microblocks count before removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
                m_unavailableMicroBlocks[blocknum].erase(it);
                LOG_GENERAL(INFO,
                            "Microblocks count after removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
            }
            return true;
        }
    }
    LOG_GENERAL(WARNING,
                "Haven't found microblock txnRootHash: " << txnRootHash);
    return false;
}

bool Node::RemoveStateDeltaHashFromUnavailableMicroBlock(
    const boost::multiprecision::uint256_t& blocknum,
    const StateHash& stateDeltaHash, const TxnHash& txnRootHash)
{
    for (auto it = m_unavailableMicroBlocks[blocknum].begin();
         it != m_unavailableMicroBlocks[blocknum].end(); it++)
    {
        if (it->first.m_hash.m_stateDeltaHash == stateDeltaHash
            && it->first.m_hash.m_txRootHash == txnRootHash)
        {
            LOG_GENERAL(INFO,
                        "Found microblock stateDeltaHash: " << stateDeltaHash
                                                            << " txnRootHash: "
                                                            << txnRootHash);
            it->second[1] = false;
            if (!it->second[0])
            {
                LOG_GENERAL(INFO,
                            "Remove microblock (txRootHash: "
                                << it->first.m_hash.m_txRootHash
                                << " stateDeltaHash: " << stateDeltaHash
                                << ")");
                LOG_GENERAL(INFO,
                            "Microblocks count before removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
                m_unavailableMicroBlocks[blocknum].erase(it);
                LOG_GENERAL(INFO,
                            "Microblocks count after removing: "
                                << m_unavailableMicroBlocks[blocknum].size());
            }
            return true;
        }
    }
    LOG_GENERAL(WARNING,
                "Haven't found microblock stateDeltaHash: " << stateDeltaHash);
    return false;
}

bool Node::VerifyFinalBlockCoSignature(const TxBlock& txblock)
{
    LOG_MARKER();

    unsigned int index = 0;
    unsigned int count = 0;

    const vector<bool>& B2 = txblock.GetB2();
    if (m_mediator.m_DSCommittee.size() != B2.size())
    {
        LOG_GENERAL(WARNING,
                    "Mismatch: DS committee size = "
                        << m_mediator.m_DSCommittee.size()
                        << ", co-sig bitmap size = " << B2.size());
        return false;
    }

    // Generate the aggregated key
    vector<PubKey> keys;
    for (auto const& kv : m_mediator.m_DSCommittee)
    {
        if (B2.at(index) == true)
        {
            keys.emplace_back(kv.first);
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
        for (auto& kv : keys)
        {
            LOG_GENERAL(WARNING, kv);
        }
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
    // LOG_MARKER();

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
            txns_to_send.emplace_back(txnIt->second);
        }

        // Move entry from submitted Tx list to committed Tx list
        committedTransactions.emplace_back(txnIt->second);
        submittedTransactions.erase(txnIt);

        // LOG_EPOCH(
        //     INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        //     "[TXN] ["
        //         << blockNum << "] Committed     = 0x"
        //         << DataConversion::charArrToHexStr(
        //                committedTransactions.back().GetTranID().asArray()));

        // Update from and to accounts
        // if (!AccountStore::GetInstance().UpdateAccounts(
        //         m_mediator.m_currentEpochNum - 1, committedTransactions.back()))
        // {
        //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
        //     committedTransactions.pop_back();
        //     return true;
        // }

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

bool Node::FindTxnInReceivedTxnsList(const TxBlock& finalblock,
                                     const uint256_t& blockNum,
                                     uint8_t sharing_mode,
                                     vector<Transaction>& txns_to_send,
                                     const TxnHash& tx_hash)
{
    // LOG_MARKER();

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
            txns_to_send.emplace_back(txnIt->second);
        }

        // Move entry from received Tx list to committed Tx list
        committedTransactions.emplace_back(txnIt->second);
        receivedTransactions.erase(txnIt);

        // LOG_EPOCH(
        //     INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        //     "[TXN] ["
        //         << blockNum << "] Committed     = 0x"
        //         << DataConversion::charArrToHexStr(
        //                committedTransactions.back().GetTranID().asArray()));

        // Update from and to accounts
        // if (!AccountStore::GetInstance().UpdateAccounts(
        //         m_mediator.m_currentEpochNum - 1, committedTransactions.back()))
        // {
        //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
        //     committedTransactions.pop_back();
        //     return true;
        // }

        /**
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "##Storing Transaction##");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr(tx_hash));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), (*entry).GetAmount());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetToAddr()));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), DataConversion::charArrToHexStr((*entry).GetFromAddr()));
        **/

        // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        //           "ReceivedTransaction: Storing Transaction: "
        //               << DataConversion::charArrToHexStr(tx_hash.asArray())
        //               << " with amount: "
        //               << committedTransactions.back().GetAmount() << ", to: "
        //               << committedTransactions.back().GetToAddr() << ", from: "
        //               << Account::GetAddressFromPublicKey(
        //                      committedTransactions.back().GetSenderPubKey()));

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

    LOG_STATE("[TXBOD][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] BEFORE TXN BODIES #" << blocknum);

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

        // microblock tx hash
        copy(microBlockTxHash.asArray().begin(),
             microBlockTxHash.asArray().end(),
             back_inserter(forwardtxn_message));
        cur_offset += TRAN_HASH_SIZE;

        // microblock state delta hash
        StateHash microBlockDeltaHash
            = m_microblock->GetHeader().GetStateDeltaHash();
        copy(microBlockDeltaHash.asArray().begin(),
             microBlockDeltaHash.asArray().end(),
             back_inserter(forwardtxn_message));
        cur_offset += STATE_HASH_SIZE;

        for (unsigned int i = 0; i < txns_to_send.size(); i++)
        {
            // txn body
            txns_to_send.at(i).Serialize(forwardtxn_message, cur_offset);
            cur_offset += txns_to_send.at(i).GetSerializedSize();

            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "[TXN] ["
            //               << blocknum << "] Broadcasted   = 0x"
            //               << DataConversion::charArrToHexStr(
            //                      txns_to_send.at(i).GetTranID().asArray()));
        }

        // P2PComm::GetInstance().SendBroadcastMessage(sendingAssignment,
        //                                             forwardtxn_message);

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

    if (m_microblock->GetHeader().GetStateDeltaHash() != StateHash())
    {

        BroadcastStateDeltaToSendingAssignment(
            blocknum, sendingAssignment,
            m_microblock->GetHeader().GetStateDeltaHash(), microBlockTxHash);
    }

    LOG_STATE("[TXBOD][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] AFTER SENDING TXN BODIES");
}

void Node::BroadcastStateDeltaToSendingAssignment(
    const boost::multiprecision::uint256_t& blocknum,
    const vector<Peer>& sendingAssignment,
    const StateHash& microBlockStateDeltaHash,
    const TxnHash& microBlockTxHash) const
{
    LOG_MARKER();

    unsigned int cur_offset = MessageOffset::BODY;
    vector<unsigned char> forwardstate_message
        = {MessageType::NODE, NodeInstructionType::FORWARDSTATEDELTA};

    // block num
    Serializable::SetNumber<uint256_t>(forwardstate_message, cur_offset,
                                       blocknum, UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    // microblock state delta hash
    copy(microBlockStateDeltaHash.asArray().begin(),
         microBlockStateDeltaHash.asArray().end(),
         back_inserter(forwardstate_message));
    cur_offset += STATE_HASH_SIZE;

    // microblock tx hash
    copy(microBlockTxHash.asArray().begin(), microBlockTxHash.asArray().end(),
         back_inserter(forwardstate_message));
    cur_offset += TRAN_HASH_SIZE;

    // state delta
    vector<unsigned char> stateDel;
    AccountStore::GetInstance().GetSerializedDelta(stateDel);

    copy(stateDel.begin(), stateDel.end(), back_inserter(forwardstate_message));

    P2PComm::GetInstance().SendBroadcastMessage(sendingAssignment,
                                                forwardstate_message);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Broadcasted the state delta! ");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Sending the state delta to the lookup nodes");
    m_mediator.m_lookup->SendMessageToLookupNodes(forwardstate_message);
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

    m_forwardingAssignment.emplace(blocknum, vector<Peer>());
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
        //     peers.emplace_back(m_myShardMembersNetworkInfo.at(i));
        // }
        peers.emplace_back(m_myShardMembersNetworkInfo.at(i));
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

bool Node::IsMyShardMicroBlockTxRootHashInFinalBlock(
    const uint256_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    return m_microblock != nullptr
        && IsMicroBlockTxRootHashInFinalBlock(
               m_microblock->GetHeader().GetTxRootHash(),
               m_microblock->GetHeader().GetStateDeltaHash(), blocknum,
               isEveryMicroBlockAvailable);
}

bool Node::IsMyShardMicroBlockStateDeltaHashInFinalBlock(
    const uint256_t& blocknum, bool& isEveryMicroBlockAvailable)
{
    return m_microblock != nullptr
        && IsMicroBlockStateDeltaHashInFinalBlock(
               m_microblock->GetHeader().GetStateDeltaHash(),
               m_microblock->GetHeader().GetTxRootHash(), blocknum,
               isEveryMicroBlockAvailable);
}

bool Node::IsMyShardMicroBlockInFinalBlock(const uint256_t& blocknum)
{
    if (m_microblock == nullptr)
    {
        return false;
    }

    auto it = m_unavailableMicroBlocks.find(blocknum);
    if (it == m_unavailableMicroBlocks.end())
    {
        return false;
    }

    for (auto it2 = m_unavailableMicroBlocks[blocknum].begin();
         it2 != m_unavailableMicroBlocks[blocknum].end(); it2++)
    {
        if (it2->first.m_hash.m_stateDeltaHash
                == m_microblock->GetHeader().GetStateDeltaHash()
            && it2->first.m_hash.m_txRootHash
                == m_microblock->GetHeader().GetTxRootHash())
        {
            LOG_GENERAL(INFO, "Found my shard microblock in finalblock");
            return true;
        }
    }

    LOG_GENERAL(WARNING, "Didn't find my shard microblock in finalblock");
    return false;
}

bool Node::IsMyShardIdInFinalBlock(const uint256_t& blocknum)
{
    auto it = m_unavailableMicroBlocks.find(blocknum);
    if (it == m_unavailableMicroBlocks.end())
    {
        return false;
    }

    for (auto it2 = m_unavailableMicroBlocks[blocknum].begin();
         it2 != m_unavailableMicroBlocks[blocknum].end(); it2++)
    {
        if (it2->first.m_shardID == m_myShardID)
        {
            LOG_GENERAL(INFO, "Found my shard ID in finalblock");
            return true;
        }
    }

    LOG_GENERAL(WARNING, "Didn't find my shard ID in finalblock");
    return false;
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
        m_forwardingAssignment.emplace(blocknum, nodes);
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
    lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
    bool isEveryMicroBlockAvailable;

    // For now, since each sharding setup only processes one block, then whatever transactions we
    // failed to submit have to be discarded m_createdTransactions.clear();
    if (IsMyShardMicroBlockTxRootHashInFinalBlock(blocknum,
                                                  isEveryMicroBlockAvailable)
        && IsMyShardMicroBlockStateDeltaHashInFinalBlock(
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
        {
            lock_guard<mutex> gt(m_mutexTempCommitted);
            AccountStore::GetInstance().CommitTemp();
            m_tempStateDeltaCommitted = true;

            LOG_GENERAL(INFO, "Temp State Committed");
        }

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
        }
    }
    else if (m_microblock != nullptr
             && m_microblock->GetHeader().GetNumTxs() > 0)
    {
        // TODO
        LOG_GENERAL(WARNING, "Why my shards microblock not in finalblock, one");
        AccountStore::GetInstance().InitTemp();
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
        lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
        bool isEveryMicroBlockAvailable;

        if (IsMyShardMicroBlockTxRootHashInFinalBlock(
                blocknum, isEveryMicroBlockAvailable)
            && IsMyShardMicroBlockStateDeltaHashInFinalBlock(
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
            {
                lock_guard<mutex> gt(m_mutexTempCommitted);
                AccountStore::GetInstance().CommitTemp();
                m_tempStateDeltaCommitted = true;

                LOG_GENERAL(INFO, "Temp State Committed");
            }
            if (isEveryMicroBlockAvailable)
            {
                DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(blocknum);
            }
        }
        else
        {
            // TODO
            LOG_GENERAL(WARNING,
                        "Why my shards microblock not in finalblock, two");
            AccountStore::GetInstance().InitTemp();
        }
    }
    else
    {
        return false;
    }
    // #endif // IS_LOOKUP_NODE
    return true;
}

void Node::InitiatePoW()
{
    // reset consensusID and first consensusLeader is index 0
    m_consensusID = 0;
    m_consensusLeaderID = 0;

    SetState(POW_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        (uint64_t)
            m_mediator.m_dsBlockChain.GetBlockCount()); // FIXME -- typecasting
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Start pow ");
    auto func = [this]() mutable -> void {
        auto epochNumber = m_mediator.m_dsBlockChain.GetBlockCount();
        auto dsBlockRand = m_mediator.m_dsBlockRand;
        auto txBlockRand = m_mediator.m_txBlockRand;
        StartPoW(epochNumber, POW_DIFFICULTY, dsBlockRand, txBlockRand);
    };

    DetachedFunction(1, func);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Soln to pow found ");
}

void Node::UpdateStateForNextConsensusRound()
{
    // Set state to tx submission
    if (m_isPrimary == true)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am no longer the shard leader ");
        m_isPrimary = false;
    }

    m_consensusLeaderID++;
    m_consensusID++;
    m_consensusLeaderID = m_consensusLeaderID % COMM_SIZE;

    if (m_consensusMyID == m_consensusLeaderID)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am the new shard leader ");
        m_isPrimary = true;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "The new shard leader is m_consensusMyID "
                      << m_consensusLeaderID);
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: Next non-ds epoch begins");

    SetState(TX_SUBMISSION);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[No PoW needed] MS: Start submit txn stage again.");
}

void Node::ScheduleTxnSubmission()
{
    auto main_func = [this]() mutable -> void { SubmitTransactions(); };

    DetachedFunction(1, main_func);

    LOG_GENERAL(INFO, "Sleep for " << TXN_SUBMISSION << " seconds");
    this_thread::sleep_for(chrono::seconds(TXN_SUBMISSION));
    LOG_GENERAL(INFO,
                "Woken up from the sleep of " << TXN_SUBMISSION << " seconds");
    auto main_func2
        = [this]() mutable -> void { SetState(TX_SUBMISSION_BUFFER); };

    DetachedFunction(1, main_func2);
}

void Node::ScheduleMicroBlockConsensus()
{
    LOG_GENERAL(INFO,
                "I am going to use conditional variable with timeout of  "
                    << TXN_BROADCAST << " seconds. It is ok to timeout here. ");
    std::unique_lock<std::mutex> cv_lk(m_MutexCVMicroblockConsensus);
    if (cv_microblockConsensus.wait_for(cv_lk,
                                        std::chrono::seconds(TXN_BROADCAST))
        == std::cv_status::timeout)
    {
        LOG_GENERAL(
            INFO, "Woken up from the sleep of " << TXN_BROADCAST << " seconds");
    }
    else
    {
        LOG_GENERAL(INFO,
                    "Received announcement message. Time to run consensus.");
    }
    auto main_func3 = [this]() mutable -> void { RunConsensusOnMicroBlock(); };

    DetachedFunction(1, main_func3);
}

void Node::BeginNextConsensusRound()
{
    LOG_MARKER();

    UpdateStateForNextConsensusRound();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (!isVacuousEpoch)
    {
        {
            unique_lock<mutex> g(m_mutexAllMicroBlocksRecvd);
            if (!m_allMicroBlocksRecvd)
            {
                LOG_GENERAL(INFO, "Wait for allMicroBlocksRecvd");
                if (m_cvAllMicroBlocksRecvd.wait_for(
                        g, std::chrono::seconds(TXN_SUBMISSION + TXN_BROADCAST))
                        == std::cv_status::timeout
                    || m_doRejoinAtNextRound)
                {
                    LOG_EPOCH(WARNING,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "Wake up from "
                                  << TXN_SUBMISSION + TXN_BROADCAST
                                  << "of waiting for all microblock received");
                    if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC)
                    {
                        LOG_EPOCH(
                            WARNING,
                            to_string(m_mediator.m_currentEpochNum).c_str(),
                            "Not in rejoin mode, try rejoining as normal");
                        RejoinAsNormal();
                        return;
                    }
                }
                else
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "All microblocks recvd, moving to "
                              "ScheduleTxnSubmission");
                }
            }
            else
            {
                LOG_GENERAL(INFO, "No need to wait for allMicroBlocksRecvd");
            }

            {
                lock_guard<mutex> g2(m_mutexNewRoundStarted);
                if (!m_newRoundStarted)
                {
                    m_newRoundStarted = true;
                    m_cvNewRoundStarted.notify_all();
                }
            }
        }

        ScheduleTxnSubmission();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping submit transactions");
    }

    ScheduleMicroBlockConsensus();
}

void Node::CallActOnFinalBlockBasedOnSenderForwarderAssgn(uint8_t shard_id)
{
    if ((m_txnSharingIAmSender == false) && (m_txnSharingIAmForwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers
            = m_txnSharingAssignedNodes.at(shard_id + 1);
        ActOnFinalBlock(TxSharingMode::NODE_FORWARD_ONLY, my_shard_receivers);
    }
    else if ((m_txnSharingIAmSender == true)
             && (m_txnSharingIAmForwarder == false))
    {
        vector<Peer> nodes_to_send;

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "iii amam herehere");

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < m_txnSharingAssignedNodes.at(0).size();
             i++)
        {
            nodes_to_send.emplace_back(m_txnSharingAssignedNodes[0][i]);
        }

        for (unsigned int i = 1; i < m_txnSharingAssignedNodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = m_txnSharingAssignedNodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                nodes_to_send.emplace_back(shard[j]);
            }
        }

        ActOnFinalBlock(TxSharingMode::SEND_ONLY, nodes_to_send);
    }
    else if ((m_txnSharingIAmSender == true)
             && (m_txnSharingIAmForwarder == true))
    {
        // Give myself the list of my fellow forwarders
        const vector<Peer>& my_shard_receivers
            = m_txnSharingAssignedNodes.at(shard_id + 1);

        vector<Peer> fellowForwarderNodes;

        // Give myself the list of all receiving nodes in all other committees including DS
        for (unsigned int i = 0; i < m_txnSharingAssignedNodes.at(0).size();
             i++)
        {
            fellowForwarderNodes.emplace_back(m_txnSharingAssignedNodes[0][i]);
        }

        for (unsigned int i = 1; i < m_txnSharingAssignedNodes.size(); i += 2)
        {
            if (((i - 1) / 2) == shard_id)
            {
                continue;
            }

            const vector<Peer>& shard = m_txnSharingAssignedNodes.at(i);
            for (unsigned int j = 0; j < shard.size(); j++)
            {
                fellowForwarderNodes.emplace_back(shard[j]);
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
//     for(auto microBlock : m_microblocks)
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
//     m_microblocks.clear();
// }

bool Node::ProcessFinalBlock(const vector<unsigned char>& message,
                             unsigned int offset, const Peer& from)
{
    // Message = [32-byte DS blocknum] [4-byte consensusid] [1-byte shard id]
    //           [Final block] [Tx body sharing setup]
    LOG_MARKER();

#ifndef IS_LOOKUP_NODE
    if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum)
    {
        std::unique_lock<mutex> cv_lk(m_MutexCVFBWaitMB);
        if (cv_FBWaitMB.wait_for(cv_lk, std::chrono::seconds(TXN_SUBMISSION))
            == std::cv_status::timeout)
        {
            LOG_GENERAL(WARNING,
                        "Timeout, I didn't finish microblock consensus");
        }
    }

    if (m_state == MICROBLOCK_CONSENSUS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I may have missed the micrblock consensus. However, if I "
                  "recently received a valid finalblock, I will accept it");
        // TODO: Optimize state transition.
        AccountStore::GetInstance().InitTemp();
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

    LOG_STATE("[FLBLK][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] RECEIVED FINAL BLOCK");

    unsigned int cur_offset = offset;

    // Initialize it with maximum number
    uint32_t shard_id = std::numeric_limits<uint32_t>::max();

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
        if (!LoadUnavailableMicroBlockHashes(txBlock,
                                             txBlock.GetHeader().GetBlockNum()))
        {
            return false;
        }
    }
    else
    {
        LOG_GENERAL(INFO, "isVacuousEpoch now");

        if (!AccountStore::GetInstance().UpdateStateTrieAll())
        {
            LOG_GENERAL(WARNING, "UpdateStateTrieAll Failed");
            return false;
        }

        if (!CheckStateRoot(txBlock)
#ifndef IS_LOOKUP_NODE
            || m_doRejoinAtStateRoot)
        {
            RejoinAsNormal();
#else // IS_LOOKUP_NODE
        )
        {
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

    // Assumption: New PoW done after every block committed
    // If I am not a DS committee member (and since I got this FinalBlock message,
    // then I know I'm not), I can start doing PoW again
    m_mediator.UpdateDSBlockRand();
    m_mediator.UpdateTxBlockRand();

#ifndef IS_LOOKUP_NODE

    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
    {
        InitiatePoW();
    }
    else
    {
        auto main_func
            = [this]() mutable -> void { BeginNextConsensusRound(); };

        DetachedFunction(1, main_func);
    }

    CallActOnFinalBlockBasedOnSenderForwarderAssgn(shard_id);
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
    TxnHash& microBlockTxHash, StateHash& microBlockStateDeltaHash,
    vector<Transaction>& txnsInForwardedMessage)
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

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + STATE_HASH_SIZE,
         microBlockStateDeltaHash.asArray().begin());
    cur_offset += STATE_HASH_SIZE;

    LOG_GENERAL(INFO,
                "Received MicroBlock StateDelta root : "
                    << DataConversion::charArrToHexStr(
                           microBlockStateDeltaHash.asArray()));

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

        txnsInForwardedMessage.emplace_back(tx);
        txnHashesInForwardedMessage.emplace_back(tx.GetTranID());
    }

    return ComputeTransactionsRoot(txnHashesInForwardedMessage)
        == microBlockTxHash;
}

bool Node::LoadForwardedStateDeltaAndCheckRoot(
    const vector<unsigned char>& message, unsigned int cur_offset,
    StateHash& microBlockStateDeltaHash, TxnHash& microBlockTxHash)
{
    LOG_MARKER();

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + STATE_HASH_SIZE,
         microBlockStateDeltaHash.asArray().begin());
    cur_offset += STATE_HASH_SIZE;

    LOG_GENERAL(INFO,
                "Received MicroBlock State Delta root : "
                    << DataConversion::charArrToHexStr(
                           microBlockStateDeltaHash.asArray()));

    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + TRAN_HASH_SIZE,
         microBlockTxHash.asArray().begin());
    cur_offset += TRAN_HASH_SIZE;

    LOG_GENERAL(
        INFO,
        "Received MicroBlock TxHash root : "
            << DataConversion::charArrToHexStr(microBlockTxHash.asArray()));

    vector<unsigned char> vec;
    copy(message.begin() + cur_offset, message.end(), back_inserter(vec));

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    StateHash delta(sha2.Finalize());

    LOG_GENERAL(INFO, "Calculated StateHash: " << delta);

    return delta == microBlockStateDeltaHash;
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
            m_committedTransactions[blocknum].emplace_back(tx);
            // if (!AccountStore::GetInstance().UpdateAccounts(
            //         m_mediator.m_currentEpochNum - 1, tx))
            // {
            //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
            //     m_committedTransactions[blocknum].pop_back();
            //     continue;
            // }
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
                      it2.first);
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
        lock_guard<mutex> gt(m_mutexTempCommitted);
        if (m_unavailableMicroBlocks.empty() && m_tempStateDeltaCommitted)
        {
            {
                lock_guard<mutex> g2(m_mutexAllMicroBlocksRecvd);
                m_allMicroBlocksRecvd = true;
            }
            LOG_GENERAL(INFO, "Notify All MicroBlocks Received");
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
    // Message = [block number] [microblocktxhash] [microblockdeltahash] [Transaction] [Transaction] [Transaction] ....
    // Received from other shards

    LOG_MARKER();

    // reading [block number] from received msg
    m_latestForwardBlockNum = (uint64_t)Serializable::GetNumber<uint256_t>(
        message, cur_offset, UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    LOG_STATE("[TXBOD][" << setw(15) << left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_txBlockChain.GetBlockCount()
                         << "] RECEIVED TXN BODIES #"
                         << m_latestForwardBlockNum);

    LOG_GENERAL(INFO,
                "Received forwarded txns for block number "
                    << m_latestForwardBlockNum);

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        < m_latestForwardBlockNum)
    {
        std::unique_lock<std::mutex> cv_lk(m_mutexForwardBlockNumSync);

        if (m_cvForwardBlockNumSync.wait_for(
                cv_lk, std::chrono::seconds(TXN_SUBMISSION + WAITING_FORWARD))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Blocknum "
                          << m_latestForwardBlockNum
                          << " waiting for state change from "
                             "WAITING_FINALBLOCK to TX_SUBMISSION too long!");
            return false;
        }
    }

    TxnHash microBlockTxRootHash;
    StateHash microBlockStateDeltaHash;
    vector<Transaction> txnsInForwardedMessage;
    // vector<TxnHash> txnHashesInForwardedMessage;

    if (!LoadForwardedTxnsAndCheckRoot(
            message, cur_offset, microBlockTxRootHash, microBlockStateDeltaHash,
            txnsInForwardedMessage /*, txnHashesInForwardedMessage*/))
    {
        return false;
    }

    {
        lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
        bool isEveryMicroBlockAvailable;

        if (!IsMicroBlockTxRootHashInFinalBlock(
                microBlockTxRootHash, microBlockStateDeltaHash,
                m_latestForwardBlockNum, isEveryMicroBlockAvailable))
        {
            LOG_GENERAL(WARNING,
                        "The forwarded data is not in finalblock, why?");
            return false;
        }
        // StoreTxInMicroBlock(microBlockTxRootHash, txnHashesInForwardedMessage)

        CommitForwardedTransactions(txnsInForwardedMessage,
                                    m_latestForwardBlockNum);

#ifndef IS_LOOKUP_NODE
        vector<Peer> forward_list;
        LoadFwdingAssgnForThisBlockNum(m_latestForwardBlockNum, forward_list);
#endif // IS_LOOKUP_NODE

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
                m_latestForwardBlockNum);
        }

#ifndef IS_LOOKUP_NODE
        if (forward_list.size() > 0)
        {
            P2PComm::GetInstance().SendBroadcastMessage(forward_list, message);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DEBUG I have broadcasted the txn body!");
        }
#endif // IS_LOOKUP_NODE
    }

    return true;
}

bool Node::ProcessForwardStateDelta(const vector<unsigned char>& message,
                                    unsigned int cur_offset, const Peer& from)
{
    // Message = [block number] [microblockdeltahash] [microblocktxhash] [AccountStateDelta]
    // Received from other shards

    LOG_MARKER();

    // reading [block number] from received msg
    m_latestForwardBlockNum = (uint64_t)Serializable::GetNumber<uint256_t>(
        message, cur_offset, UINT256_SIZE);

    cur_offset += UINT256_SIZE;

    LOG_GENERAL(INFO,
                "Received state delta for block number "
                    << m_latestForwardBlockNum);

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        < m_latestForwardBlockNum)
    {
        std::unique_lock<std::mutex> cv_lk(m_mutexForwardBlockNumSync);

        if (m_cvForwardBlockNumSync.wait_for(
                cv_lk, std::chrono::seconds(TXN_SUBMISSION + WAITING_FORWARD))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Blocknum "
                          << m_latestForwardBlockNum
                          << " waiting for state change from "
                             "WAITING_FINALBLOCK to TX_SUBMISSION too long!");
            return false;
        }
    }

    StateHash microBlockStateDeltaHash;
    TxnHash microBlockTxRootHash;

    if (!LoadForwardedStateDeltaAndCheckRoot(message, cur_offset,
                                             microBlockStateDeltaHash,
                                             microBlockTxRootHash))
    {
        return false;
    }

    LOG_GENERAL(INFO, "LoadedForwardedStateDelta!");

    cur_offset += STATE_HASH_SIZE + TRAN_HASH_SIZE;

    {
        lock_guard<mutex> gi(m_mutexIsEveryMicroBlockAvailable);
        bool isEveryMicroBlockAvailable;

        if (!IsMicroBlockStateDeltaHashInFinalBlock(
                microBlockStateDeltaHash, microBlockTxRootHash,
                m_latestForwardBlockNum, isEveryMicroBlockAvailable))
        {
            LOG_GENERAL(WARNING,
                        "The forwarded data is not in finalblock, why?");
            return false;
        }

        AccountStore::GetInstance().DeserializeDelta(message, cur_offset);

#ifndef IS_LOOKUP_NODE
        vector<Peer> forward_list;
        LoadFwdingAssgnForThisBlockNum(m_latestForwardBlockNum, forward_list);
#endif // IS_LOOKUP_NODE

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "isEveryMicroBlockAvailable: " << isEveryMicroBlockAvailable);

        if (isEveryMicroBlockAvailable)
        {
            DeleteEntryFromFwdingAssgnAndMissingBodyCountMap(
                m_latestForwardBlockNum);
        }

#ifndef IS_LOOKUP_NODE
        if (forward_list.size() > 0)
        {
            P2PComm::GetInstance().SendBroadcastMessage(forward_list, message);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DEBUG I have broadcasted the state delta!");
        }
#endif // IS_LOOKUP_NODE
    }

    return true;
}
