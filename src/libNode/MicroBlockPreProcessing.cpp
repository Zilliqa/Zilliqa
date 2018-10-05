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
#include "libData/AccountData/TransactionReceipt.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libPOW/pow.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

bool Node::ComposeMicroBlock()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::ComposeMicroBlock not expected to be called from "
                    "LookUp node.");
        return true;
    }
    // To-do: Replace dummy values with the required ones
    LOG_MARKER();

    // TxBlockHeader
    uint8_t type = TXBLOCKTYPE::MICRO;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint32_t shardId = m_myshardId;
    uint256_t gasLimit = MICROBLOCK_GAS_LIMIT;
    uint256_t gasUsed = 1;
    BlockHash prevHash;
    fill(prevHash.asArray().begin(), prevHash.asArray().end(), 0x77);
    uint64_t blockNum = m_mediator.m_currentEpochNum;
    uint256_t timestamp = get_time_as_int();
    TxnHash txRootHash, txReceiptHash;
    uint32_t numTxs = 0;
    const PubKey& minerPubKey = m_mediator.m_selfKey.second;
    uint64_t dsBlockNum
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    BlockHash dsBlockHeader;
    fill(dsBlockHeader.asArray().begin(), dsBlockHeader.asArray().end(), 0x11);
    StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

    // TxBlock
    vector<TxnHash> tranHashes;

    //unsigned int index = 0;
    {

        lock_guard<mutex> g(m_mutexProcessedTransactions);

        auto& processedTransactions = m_processedTransactions[blockNum];

        txRootHash = ComputeTransactionsRoot(m_TxnOrder);

        numTxs = processedTransactions.size();
        if (numTxs != m_TxnOrder.size())
        {
            LOG_GENERAL(FATAL,
                        "Num txns and Order size not same "
                            << " numTxs " << numTxs << " m_TxnOrder "
                            << m_TxnOrder.size());
        }
        tranHashes = m_TxnOrder;
        m_TxnOrder.clear();

        if (!TransactionWithReceipt::ComputeTransactionReceiptsHash(
                tranHashes, processedTransactions, txReceiptHash))
        {
            LOG_GENERAL(WARNING, "Cannot compute transaction receipts hash");
            return false;
        }
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Creating new micro block.")
    m_microblock.reset(new MicroBlock(
        MicroBlockHeader(type, version, shardId, gasLimit, gasUsed, prevHash,
                         blockNum, timestamp, txRootHash, numTxs, minerPubKey,
                         dsBlockNum, dsBlockHeader, stateDeltaHash,
                         txReceiptHash),
        tranHashes, CoSignatures()));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Micro block proposed with "
                  << m_microblock->GetHeader().GetNumTxs()
                  << " transactions for epoch "
                  << m_mediator.m_currentEpochNum);

    return true;
}

bool Node::OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                             const Peer& from)
{
    LOG_MARKER();

    unsigned int offset = 0;

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::OnNodeMissingTxns not expected to be called from "
                    "LookUp node.");
        return true;
    }

    if (errorMsg.size() < sizeof(uint32_t) + sizeof(uint64_t) + offset)
    {
        LOG_GENERAL(WARNING, "Malformed Message");
        return false;
    }

    uint32_t numOfAbsentHashes
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint64_t blockNum
        = Serializable::GetNumber<uint64_t>(errorMsg, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    vector<TxnHash> missingTransactions;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        TxnHash txnHash;
        copy(errorMsg.begin() + offset,
             errorMsg.begin() + offset + TRAN_HASH_SIZE,
             txnHash.asArray().begin());
        offset += TRAN_HASH_SIZE;

        missingTransactions.emplace_back(txnHash);
    }

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock_guard<mutex> g(m_mutexProcessedTransactions);

    auto& processedTransactions = m_processedTransactions[blockNum];

    unsigned int cur_offset = 0;
    vector<unsigned char> tx_message
        = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
    cur_offset += MessageOffset::BODY;
    tx_message.push_back(SUBMITTRANSACTIONTYPE::MISSINGTXN);
    cur_offset += MessageOffset::INST;
    Serializable::SetNumber<uint64_t>(tx_message, cur_offset, blockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        // LOG_GENERAL(INFO, "Peer " << from << " : " << portNo << " missing txn " << missingTransactions[i])

        Transaction t;

        if (processedTransactions.find(missingTransactions[i])
            != processedTransactions.end())
        {
            t = processedTransactions[missingTransactions[i]].GetTransaction();
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Leader unable to find txn proposed in microblock "
                            << missingTransactions[i]);
            // throw exception();
            // return false;
            continue;
        }
        t.Serialize(tx_message, cur_offset);
        cur_offset += t.GetSerializedSize();
    }
    P2PComm::GetInstance().SendMessage(peer, tx_message);

    return true;
}

bool Node::OnCommitFailure([
    [gnu::unused]] const std::map<unsigned int, std::vector<unsigned char>>&
                               commitFailureMap)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::OnCommitFailure not expected to be called from "
                    "LookUp node.");
        return true;
    }

    LOG_MARKER();

    // for(auto failureEntry: commitFailureMap)
    // {

    // }

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Going to sleep before restarting consensus");

    // std::this_thread::sleep_for(30s);
    // RunConsensusOnMicroBlockWhenShardLeader();

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Woke from sleep after consensus restart");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Microblock consensus failed, going to wait for final block "
              "announcement");

    return true;
}

void Node::ProcessTransactionWhenShardLeader()
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexCreatedTransactions);

    unsigned int txn_sent_count = 0;

    auto findOneFromAddrNonceTxnMap = [this](Transaction& t) -> bool {
        for (auto it = m_addrNonceTxnMap.begin(); it != m_addrNonceTxnMap.end();
             it++)
        {
            if (it->second.begin()->first
                == AccountStore::GetInstance().GetNonceTemp(it->first) + 1)
            {
                t = std::move(it->second.begin()->second);
                it->second.erase(it->second.begin());

                if (it->second.empty())
                {
                    m_addrNonceTxnMap.erase(it);
                }
                return true;
            }
        }
        return false;
    };

    auto findSameNonceButHigherGasPrice = [this](Transaction& t) -> void {
        auto& compIdx
            = m_createdTransactions.get<MULTI_INDEX_KEY::PUBKEY_NONCE>();
        auto it = compIdx.find(make_tuple(t.GetSenderPubKey(), t.GetNonce()));
        if (it != compIdx.end())
        {
            if (it->GetGasPrice() > t.GetGasPrice())
            {
                t = std::move(*it);
                compIdx.erase(it);
            }
        }
    };

    auto findOneFromCreated = [this](Transaction& t) -> bool {
        auto& listIdx = m_createdTransactions.get<MULTI_INDEX_KEY::GAS_PRICE>();

        //LOG_GENERAL(INFO, "Size List Idx " << listIdx.size());
        if (listIdx.size() == 0)
        {
            return false;
        }

        auto it = listIdx.begin();
        t = std::move(*it);
        listIdx.erase(it);
        return true;
    };

    auto appendOne
        = [this](const Transaction& t, const TransactionReceipt& tr) {
              // LOG_GENERAL(INFO, "appendOne: " << t.GetTranID().hex());
              lock_guard<mutex> g(m_mutexProcessedTransactions);
              auto& processedTransactions
                  = m_processedTransactions[m_mediator.m_currentEpochNum];
              processedTransactions.insert(
                  make_pair(t.GetTranID(), TransactionWithReceipt(t, tr)));
              m_TxnOrder.push_back(t.GetTranID());
          };

    uint256_t gasUsedTotal = 0;

    while (txn_sent_count < MAXSUBMITTXNPERNODE * m_myShardMembers->size()
           && gasUsedTotal < MICROBLOCK_GAS_LIMIT)
    {
        Transaction t;
        TransactionReceipt tr;

        // check m_addrNonceTxnMap contains any txn meets right nonce,
        // if contains, process it withou increment the txn_sent_count as it's already
        // incremented when inserting
        if (findOneFromAddrNonceTxnMap(t))
        {
            // check whether m_createdTransaction have transaction with same Addr and nonce
            // if has and with larger gasPrice then replace with that one. (*optional step)
            findSameNonceButHigherGasPrice(t);

            if (m_mediator.m_validator->CheckCreatedTransaction(t, tr)
                || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                    && t.GetCode().empty()))
            {
                appendOne(t, tr);
                gasUsedTotal += tr.GetCumGas();
                continue;
            }
        }
        // if no txn in u_map meet right nonce process new come-in transactions
        else if (findOneFromCreated(t))
        {
            // LOG_GENERAL(INFO, "findOneFromCreated");

            Address senderAddr = t.GetSenderAddr();
            // check nonce, if nonce larger than expected, put it into m_addrNonceTxnMap
            if (t.GetNonce()
                > AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1)
            {
                // LOG_GENERAL(INFO,
                //             "High nonce: "
                //                 << t.GetNonce() << " cur sender nonce: "
                //                 << AccountStore::GetInstance().GetNonceTemp(
                //                        senderAddr));
                auto it1 = m_addrNonceTxnMap.find(senderAddr);
                if (it1 != m_addrNonceTxnMap.end())
                {
                    auto it2 = it1->second.find(t.GetNonce());
                    if (it2 != it1->second.end())
                    {
                        // found the txn with same addr and same nonce
                        // then compare the gasprice and remains the higher one
                        if (t.GetGasPrice() > it2->second.GetGasPrice())
                        {
                            it2->second = t;
                        }
                        txn_sent_count++;
                        continue;
                    }
                }
                m_addrNonceTxnMap[senderAddr].insert({t.GetNonce(), t});
            }
            // if nonce too small, ignore it
            else if (t.GetNonce()
                     < AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1)
            {
                // LOG_GENERAL(INFO,
                //             "Nonce too small"
                //                 << " Expected "
                //                 << AccountStore::GetInstance().GetNonceTemp(
                //                        senderAddr)
                //                 << " Found " << t.GetNonce());
            }
            // if nonce correct, process it
            else if (m_mediator.m_validator->CheckCreatedTransaction(t, tr)
                     || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                     || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                         && t.GetCode().empty()))
            {

                appendOne(t, tr);
                gasUsedTotal += tr.GetCumGas();
            }
            else
            {
                // LOG_GENERAL(WARNING, "CheckCreatedTransaction failed");
            }
        }
        else
        {
            break;
        }
        txn_sent_count++;
    }
}

bool Node::ProcessTransactionWhenShardBackup(const vector<TxnHash>& tranHashes,
                                             vector<TxnHash>& missingtranHashes)
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexCreatedTransactions);

    auto findFromCreated = [this](const TxnHash& th) -> bool {
        auto& hashIdx = m_createdTransactions.get<MULTI_INDEX_KEY::TXN_ID>();
        if (!hashIdx.size())
        {
            return false;
        }

        auto it = hashIdx.find(th);

        if (hashIdx.end() == it)
        {
            LOG_GENERAL(WARNING, "txn is not found");
            return false;
        }

        return true;
    };

    for (const auto& tranHash : tranHashes)
    {
        if (!findFromCreated(tranHash))
        {
            missingtranHashes.emplace_back(tranHash);
        }
    }

    if (!missingtranHashes.empty())
    {
        return true;
    }

    std::list<Transaction> curTxns;

    if (!VerifyTxnsOrdering(tranHashes, curTxns))
    {
        return false;
    }

    auto appendOne
        = [this](const Transaction& t, const TransactionReceipt& tr) {
              lock_guard<mutex> g(m_mutexProcessedTransactions);
              auto& processedTransactions
                  = m_processedTransactions[m_mediator.m_currentEpochNum];
              processedTransactions.insert(
                  make_pair(t.GetTranID(), TransactionWithReceipt(t, tr)));
          };

    AccountStore::GetInstance().InitTemp();
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        AccountStore::GetInstance().DeserializeDeltaTemp(
            m_mediator.m_ds->m_stateDeltaWhenRunDSMB, 0);
    }

    for (const auto& t : curTxns)
    {
        TransactionReceipt tr;
        if (m_mediator.m_validator->CheckCreatedTransaction(t, tr)
            || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
            || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                && t.GetCode().empty()))
        {
            appendOne(t, tr);
        }
    }

    return true;
}

bool Node::VerifyTxnsOrdering(const vector<TxnHash>& tranHashes,
                              list<Transaction>& curTxns)
{
    LOG_MARKER();

    std::unordered_map<Address,
                       std::map<boost::multiprecision::uint256_t, Transaction>>
        t_addrNonceTxnMap = m_addrNonceTxnMap;
    gas_txnid_comp_txns t_createdTransactions = m_createdTransactions;
    vector<TxnHash> t_tranHashes;
    unsigned int txn_sent_count = 0;

    auto findOneFromAddrNonceTxnMap
        = [&t_addrNonceTxnMap](Transaction& t) -> bool {
        for (auto it = t_addrNonceTxnMap.begin(); it != t_addrNonceTxnMap.end();
             it++)
        {
            if (it->second.begin()->first
                == AccountStore::GetInstance().GetNonceTemp(it->first) + 1)
            {
                t = std::move(it->second.begin()->second);
                it->second.erase(it->second.begin());

                if (it->second.empty())
                {
                    t_addrNonceTxnMap.erase(it);
                }
                return true;
            }
        }
        return false;
    };

    auto findSameNonceButHigherGasPrice
        = [&t_createdTransactions](Transaction& t) -> void {
        auto& compIdx
            = t_createdTransactions.get<MULTI_INDEX_KEY::PUBKEY_NONCE>();
        auto it = compIdx.find(make_tuple(t.GetSenderPubKey(), t.GetNonce()));
        if (it != compIdx.end())
        {
            if (it->GetGasPrice() > t.GetGasPrice())
            {
                t = std::move(*it);
                compIdx.erase(it);
            }
        }
    };

    auto findOneFromCreated = [&t_createdTransactions](Transaction& t) -> bool {
        auto& listIdx = t_createdTransactions.get<MULTI_INDEX_KEY::GAS_PRICE>();
        if (!listIdx.size())
        {
            return false;
        }

        auto it = listIdx.begin();
        t = std::move(*it);
        listIdx.erase(it);
        return true;
    };

    auto appendOne = [&t_tranHashes, &curTxns](const Transaction& t) {
        t_tranHashes.emplace_back(t.GetTranID());
        curTxns.emplace_back(t);
    };

    uint256_t gasUsedTotal = 0;

    while (txn_sent_count < MAXSUBMITTXNPERNODE * m_myShardMembers->size()
           && gasUsedTotal < MICROBLOCK_GAS_LIMIT)
    {
        Transaction t;
        TransactionReceipt tr;

        // check t_addrNonceTxnMap contains any txn meets right nonce,
        // if contains, process it withou increment the txn_sent_count as it's already
        // incremented when inserting
        if (findOneFromAddrNonceTxnMap(t))
        {
            // check whether m_createdTransaction have transaction with same Addr and nonce
            // if has and with larger gasPrice then replace with that one. (*optional step)
            findSameNonceButHigherGasPrice(t);

            if (m_mediator.m_validator->CheckCreatedTransaction(t, tr)
                || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                    && t.GetCode().empty()))
            {
                appendOne(t);
                gasUsedTotal += tr.GetCumGas();
                continue;
            }
        }
        // if no txn in u_map meet right nonce process new come-in transactions
        else if (findOneFromCreated(t))
        {
            Address senderAddr = t.GetSenderAddr();
            // check nonce, if nonce larger than expected, put it into t_addrNonceTxnMap
            if (t.GetNonce()
                > AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1)
            {
                auto it1 = t_addrNonceTxnMap.find(senderAddr);
                if (it1 != t_addrNonceTxnMap.end())
                {
                    auto it2 = it1->second.find(t.GetNonce());
                    if (it2 != it1->second.end())
                    {
                        // found the txn with same addr and same nonce
                        // then compare the gasprice and remains the higher one
                        if (t.GetGasPrice() > it2->second.GetGasPrice())
                        {
                            it2->second = t;
                        }
                        txn_sent_count++;
                        continue;
                    }
                }
                t_addrNonceTxnMap[senderAddr].insert({t.GetNonce(), t});
            }
            // if nonce too small, ignore it
            else if (t.GetNonce()
                     < AccountStore::GetInstance().GetNonceTemp(senderAddr) + 1)
            {
            }
            // if nonce correct, process it
            else if (m_mediator.m_validator->CheckCreatedTransaction(t, tr)
                     || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                     || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                         && t.GetCode().empty()))
            {
                appendOne(t);
                gasUsedTotal += tr.GetCumGas();
            }
        }
        else
        {
            break;
        }
        txn_sent_count++;
    }

    if (t_tranHashes == tranHashes)
    {
        m_addrNonceTxnMap = std::move(t_addrNonceTxnMap);
        m_createdTransactions = std::move(t_createdTransactions);
        return true;
    }

    return false;
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::RunConsensusOnMicroBlockWhenShardLeader not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. Creating microblock for epoch "
                  << m_mediator.m_currentEpochNum);

    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE
        && !m_mediator.GetIsVacuousEpoch())
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Non-vacuous epoch, going to sleep for "
                      << TX_DISTRIBUTE_TIME_IN_MS << " milliseconds");
        std::this_thread::sleep_for(
            chrono::milliseconds(TX_DISTRIBUTE_TIME_IN_MS));
    }

    if (!m_mediator.GetIsVacuousEpoch())
    {
        ProcessTransactionWhenShardLeader();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping submit transactions");
    }

    AccountStore::GetInstance().SerializeDelta();

    // composed microblock stored in m_microblock
    if (!ComposeMicroBlock())
    {
        LOG_GENERAL(WARNING, "Unable to create microblock");
        return false;
    }

    //m_consensusID = 0;
    m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                               .GetHeader()
                               .GetMyHash()
                               .asBytes();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. "
                  << "m_consensusID: " << m_mediator.m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << (*m_myShardMembers)[m_consensusLeaderID].second);

    auto nodeMissingTxnsFunc = [this](const vector<unsigned char>& errorMsg,
                                      const Peer& from) mutable -> bool {
        return OnNodeMissingTxns(errorMsg, from);
    };

    auto commitFailureFunc
        = [this](const map<unsigned int, vector<unsigned char>>& m) mutable
        -> bool { return OnCommitFailure(m); };

    m_consensusObject.reset(new ConsensusLeader(
        m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
        m_consensusBlockHash, m_consensusMyID, m_mediator.m_selfKey.first,
        *m_myShardMembers, static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), nodeMissingTxnsFunc,
        commitFailureFunc));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    LOG_STATE("[MICON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] BGIN");

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    auto announcementGeneratorFunc =
        [this](vector<unsigned char>& dst, unsigned int offset,
               const uint32_t consensusID, const uint64_t blockNumber,
               const vector<unsigned char>& blockHash, const uint16_t leaderID,
               const pair<PrivKey, PubKey>& leaderKey,
               vector<unsigned char>& messageToCosign) mutable -> bool {
        return Messenger::SetNodeMicroBlockAnnouncement(
            dst, offset, consensusID, blockNumber, blockHash, leaderID,
            leaderKey, *m_microblock, messageToCosign);
    };

    cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardBackup()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::RunConsensusOnMicroBlockWhenShardBackup not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a backup node. Waiting for microblock announcement for epoch "
            << m_mediator.m_currentEpochNum);
    //m_consensusID = 0;
    m_consensusBlockHash = m_mediator.m_txBlockChain.GetLastBlock()
                               .GetHeader()
                               .GetMyHash()
                               .asBytes();

    auto func = [this](const vector<unsigned char>& input, unsigned int offset,
                       vector<unsigned char>& errorMsg,
                       const uint32_t consensusID, const uint64_t blockNumber,
                       const vector<unsigned char>& blockHash,
                       const uint16_t leaderID, const PubKey& leaderKey,
                       vector<unsigned char>& messageToCosign) mutable -> bool {
        return MicroBlockValidator(input, offset, errorMsg, consensusID,
                                   blockNumber, blockHash, leaderID, leaderKey,
                                   messageToCosign);
    };

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard backup. "
                  << " m_mediator.m_consensusID: " << m_mediator.m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << (*m_myShardMembers)[m_consensusLeaderID].second);

    deque<pair<PubKey, Peer>> peerList;

    for (auto& it : *m_myShardMembers)
    {
        peerList.emplace_back(it);
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Leader is at index  "
                  << m_consensusLeaderID << " "
                  << peerList.at(m_consensusLeaderID).second);

    m_consensusObject.reset(new ConsensusBackup(
        m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
        m_consensusBlockHash, m_consensusMyID, m_consensusLeaderID,
        m_mediator.m_selfKey.first, peerList, static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

bool Node::RunConsensusOnMicroBlock()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::RunConsensusOnMicroBlock not expected to be called "
                    "from LookUp node.");
        return true;
    }

    LOG_MARKER();

    SetState(MICROBLOCK_CONSENSUS_PREP);

    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        m_mediator.m_ds->m_toSendTxnToLookup = false;
        m_mediator.m_ds->m_startedRunFinalblockConsensus = false;
        m_mediator.m_ds->m_stateDeltaWhenRunDSMB
            = m_mediator.m_ds->m_stateDeltaFromShards;

        if (m_mediator.GetIsVacuousEpoch())
        {
            //Coinbase
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "[CNBSE]");

            m_mediator.m_ds->InitCoinbase();
            m_mediator.m_ds->m_stateDeltaWhenRunDSMB.clear();
            AccountStore::GetInstance().SerializeDelta();
            AccountStore::GetInstance().GetSerializedDelta(
                m_mediator.m_ds->m_stateDeltaWhenRunDSMB);
        }
    }
    if (m_isPrimary)
    {
        if (!RunConsensusOnMicroBlockWhenShardLeader())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardLeader");
            // throw exception();
            return false;
        }
    }
    else
    {
        if (!RunConsensusOnMicroBlockWhenShardBackup())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardBackup");
            // throw exception();
            return false;
        }
    }

    SetState(MICROBLOCK_CONSENSUS);

    CommitMicroBlockConsensusBuffer();

    return true;
}

bool Node::CheckBlockTypeIsMicro()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckBlockTypeIsMicro not expected to be called "
                    "from LookUp node.");
        return true;
    }

    // Check type (must be micro block type)
    if (m_microblock->GetHeader().GetType() != TXBLOCKTYPE::MICRO)
    {
        LOG_GENERAL(WARNING,
                    "Type check failed. Expected: "
                        << (unsigned int)TXBLOCKTYPE::MICRO << " Actual: "
                        << (unsigned int)m_microblock->GetHeader().GetType());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK);

        return false;
    }

    LOG_GENERAL(INFO, "Type check passed");

    return true;
}

bool Node::CheckMicroBlockVersion()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockVersion not expected to be called "
                    "from LookUp node.");
        return true;
    }

    // Check version (must be most current version)
    if (m_microblock->GetHeader().GetVersion() != BLOCKVERSION::VERSION1)
    {
        LOG_GENERAL(
            WARNING,
            "Version check failed. Expected: "
                << (unsigned int)BLOCKVERSION::VERSION1 << " Actual: "
                << (unsigned int)m_microblock->GetHeader().GetVersion());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_VERSION);

        return false;
    }

    LOG_GENERAL(INFO, "Version check passed");

    return true;
}

bool Node::CheckMicroBlockshardId()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockshardId not expected to be called "
                    "from LookUp node.");
        return true;
    }

    // Check version (must be most current version)
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        return true;
    }
    if (m_microblock->GetHeader().GetShardId() != m_myshardId)
    {
        LOG_GENERAL(WARNING,
                    "shardId check failed. Expected: "
                        << m_myshardId << " Actual: "
                        << m_microblock->GetHeader().GetShardId());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_SHARD_ID);

        return false;
    }

    LOG_GENERAL(INFO, "shardId check passed");

    return true;
}

bool Node::CheckMicroBlockTimestamp()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockTimestamp not expected to be called "
                    "from LookUp node.");
        return true;
    }

    // Check timestamp (must be greater than timestamp of last Tx block header in the Tx blockchain)
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
        uint256_t thisMicroblockTimestamp
            = m_microblock->GetHeader().GetTimestamp();
        uint256_t lastTxBlockTimestamp = lastTxBlock.GetHeader().GetTimestamp();
        if (thisMicroblockTimestamp <= lastTxBlockTimestamp)
        {
            LOG_GENERAL(WARNING,
                        "Timestamp check failed. Last Tx Block: "
                            << lastTxBlockTimestamp
                            << " Microblock: " << thisMicroblockTimestamp);

            m_consensusObject->SetConsensusErrorCode(
                ConsensusCommon::INVALID_TIMESTAMP);

            return false;
        }
    }

    LOG_GENERAL(INFO, "Timestamp check passed");

    return true;
}

unsigned char Node::CheckLegitimacyOfTxnHashes(vector<unsigned char>& errorMsg)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckLegitimacyOfTxnHashes not expected to be "
                    "called from LookUp node.");
        return true;
    }

    if (!m_mediator.GetIsVacuousEpoch())
    {
        vector<TxnHash> missingTxnHashes;
        if (!ProcessTransactionWhenShardBackup(m_microblock->GetTranHashes(),
                                               missingTxnHashes))
        {
            LOG_GENERAL(WARNING, "The leader may have composed wrong order");
            return LEGITIMACYRESULT::WRONGORDER;
        }

        m_numOfAbsentTxnHashes = 0;

        int offset = 0;

        for (auto const& hash : missingTxnHashes)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Missing txn: " << hash)
            if (errorMsg.size() == 0)
            {
                errorMsg.resize(sizeof(uint32_t) + sizeof(uint64_t)
                                + TRAN_HASH_SIZE);
                offset += (sizeof(uint32_t) + sizeof(uint64_t));
            }
            else
            {
                errorMsg.resize(offset + TRAN_HASH_SIZE);
            }
            copy(hash.asArray().begin(), hash.asArray().end(),
                 errorMsg.begin() + offset);
            offset += TRAN_HASH_SIZE;
            m_numOfAbsentTxnHashes++;
        }

        if (m_numOfAbsentTxnHashes > 0)
        {
            Serializable::SetNumber<uint32_t>(
                errorMsg, 0, m_numOfAbsentTxnHashes, sizeof(uint32_t));
            Serializable::SetNumber<uint64_t>(errorMsg, sizeof(uint32_t),
                                              m_mediator.m_currentEpochNum,
                                              sizeof(uint64_t));

            m_txnsOrdering = m_microblock->GetTranHashes();

            AccountStore::GetInstance().InitTemp();
            if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
            {
                LOG_GENERAL(WARNING, "Got missing txns, revert state delta");
                AccountStore::GetInstance().DeserializeDeltaTemp(
                    m_mediator.m_ds->m_stateDeltaWhenRunDSMB, 0);
            }

            return LEGITIMACYRESULT::MISSEDTXN;
        }

        AccountStore::GetInstance().SerializeDelta();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping processing transactions");
    }

    return LEGITIMACYRESULT::SUCCESS;
}

bool Node::CheckMicroBlockHashes(vector<unsigned char>& errorMsg)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockHashes not expected to be called "
                    "from LookUp node.");
        return true;
    }

    // Check transaction hashes (number of hashes must be = Tx count field)
    uint32_t txhashessize = m_microblock->GetTranHashes().size();
    uint32_t numtxs = m_microblock->GetHeader().GetNumTxs();
    if (txhashessize != numtxs)
    {
        LOG_GENERAL(WARNING,
                    "Tx hashes check failed. Tx hashes size: "
                        << txhashessize << " Num txs: " << numtxs);

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_BLOCK_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Hash count check passed");

    switch (CheckLegitimacyOfTxnHashes(errorMsg))
    {
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

    LOG_GENERAL(INFO, "Hash legitimacy check passed");

    return true;
}

bool Node::CheckMicroBlockTxnRootHash()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockTxnRootHash not expected to be "
                    "called from LookUp node.");
        return true;
    }

    // Check transaction root
    TxnHash expectedTxRootHash
        = ComputeTransactionsRoot(m_microblock->GetTranHashes());

    LOG_GENERAL(
        INFO,
        "Microblock root computation done "
            << DataConversion::charArrToHexStr(expectedTxRootHash.asArray()));
    LOG_GENERAL(
        INFO,
        "Expected root: " << m_microblock->GetHeader().GetTxRootHash().hex());

    if (expectedTxRootHash != m_microblock->GetHeader().GetTxRootHash())
    {
        LOG_GENERAL(WARNING, "Txn root does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_ROOT_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Root check passed");

    return true;
}

bool Node::CheckMicroBlockStateDeltaHash()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::CheckMicroBlockStateDeltaHash not expected to be "
                    "called from LookUp node.");
        return true;
    }

    StateHash expectedStateDeltaHash
        = AccountStore::GetInstance().GetStateDeltaHash();

    LOG_GENERAL(INFO,
                "Microblock state delta generation done "
                    << expectedStateDeltaHash.hex());
    LOG_GENERAL(INFO,
                "Received root: "
                    << m_microblock->GetHeader().GetStateDeltaHash().hex());

    if (expectedStateDeltaHash != m_microblock->GetHeader().GetStateDeltaHash())
    {
        LOG_GENERAL(WARNING, "State delta hash does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_STATE_DELTA_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "State delta hash check passed");

    return true;
}

bool Node::CheckMicroBlockTranReceiptHash()
{
    uint64_t blockNum = m_mediator.m_currentEpochNum;
    auto& processedTransactions = m_processedTransactions[blockNum];
    TxnHash expectedTranHash;
    if (!TransactionWithReceipt::ComputeTransactionReceiptsHash(
            m_microblock->GetTranHashes(), processedTransactions,
            expectedTranHash))
    {
        LOG_GENERAL(WARNING, "Cannot compute transaction receipts hash");
        return false;
    }
    LOG_GENERAL(INFO,
                "Microblock transaction receipt hash generation done "
                    << expectedTranHash.hex());
    LOG_GENERAL(INFO,
                "Received hash: "
                    << m_microblock->GetHeader().GetTranReceiptHash().hex());

    if (expectedTranHash != m_microblock->GetHeader().GetTranReceiptHash())
    {
        LOG_GENERAL(WARNING, "Transaction receipt hash does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_TRAN_RECEIPT_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Transaction receipt hash check passed");

    return true;
}

bool Node::MicroBlockValidator(const vector<unsigned char>& message,
                               unsigned int offset,
                               vector<unsigned char>& errorMsg,
                               const uint32_t consensusID,
                               const uint64_t blockNumber,
                               const vector<unsigned char>& blockHash,
                               const uint16_t leaderID, const PubKey& leaderKey,
                               vector<unsigned char>& messageToCosign)
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::MicroBlockValidator not expected to be called from "
                    "LookUp node.");
        return true;
    }

    m_microblock.reset(new MicroBlock);

    if (!Messenger::GetNodeMicroBlockAnnouncement(
            message, offset, consensusID, blockNumber, blockHash, leaderID,
            leaderKey, *m_microblock, messageToCosign))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Messenger::GetNodeMicroBlockAnnouncement failed.");
        return false;
    }

    if (!CheckBlockTypeIsMicro() || !CheckMicroBlockVersion()
        || !CheckMicroBlockshardId() || !CheckMicroBlockTimestamp()
        || !CheckMicroBlockHashes(errorMsg) || !CheckMicroBlockTxnRootHash()
        || !CheckMicroBlockStateDeltaHash()
        || !CheckMicroBlockTranReceiptHash())
    {
        m_microblock = nullptr;
        Serializable::SetNumber<uint32_t>(
            errorMsg, errorMsg.size(), m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        // LOG_GENERAL(INFO, "To-do: What to do if proposed microblock is not valid?");
        return false;
    }

    // Check gas limit (must satisfy some equations)
    // Check gas used (must be <= gas limit)
    // Check state root (TBD)
    // Check pubkey (must be valid and = shard leader)
    // Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
    // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)
    // Check parent DS block number (must be = block number of last DS block header in the DS blockchain)
    // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)

    return true;
}
