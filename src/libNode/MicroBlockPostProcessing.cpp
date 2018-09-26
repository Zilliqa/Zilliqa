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

void Node::SubmitMicroblockToDSCommittee() const
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::SubmitMicroblockToDSCommittee not expected to be "
                    "called from LookUp node.");
        return;
    }

    // Message = [8-byte DS blocknum] [4-byte consensusid] [4-byte shard ID] [Tx microblock]
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        return;
    }

    // Message = [8-byte Tx blocknum] [Tx microblock core] [State Delta]
    vector<unsigned char> microblock
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    microblock.push_back(
        m_mediator.m_ds->SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK);
    cur_offset += MessageOffset::INST;

    // 8-byte tx blocknum
    uint64_t txBlockNum
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    Serializable::SetNumber<uint64_t>(microblock, cur_offset, txBlockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    // Tx microblock
    m_microblock->SerializeCore(microblock, cur_offset);
    cur_offset += m_microblock->GetSerializedCoreSize();

    // Append State Delta
    AccountStore::GetInstance().GetSerializedDelta(microblock);

    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] SENT");
    deque<Peer> peerList;

    for (auto const& i : *m_mediator.m_DSCommittee)
    {
        peerList.push_back(i.second);
    }

    P2PComm::GetInstance().SendBroadcastMessage(peerList, microblock);
}

bool Node::ProcessMicroblockConsensus(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Node::ProcessMicroblockConsensus not expected to be "
                    "called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    // check size
    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(unsigned char) + sizeof(uint32_t)
                                       + BLOCK_HASH_SIZE + sizeof(uint16_t)))
    {
        return false;
    }

    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        message, offset + sizeof(unsigned char), sizeof(uint32_t));

    if (m_state != MICROBLOCK_CONSENSUS)
    {
        lock_guard<mutex> h(m_mutexMicroBlockConsensusBuffer);

        m_microBlockConsensusBuffer[consensus_id].push_back(
            make_pair(from, message));

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Process micro block arrived earlier, saved to buffer");
    }
    else
    {
        if (consensus_id < m_consensusID)
        {
            LOG_GENERAL(WARNING,
                        "Consensus ID in message ("
                            << consensus_id << ") is smaller than current ("
                            << m_consensusID << ")");
            return false;
        }
        else if (consensus_id > m_consensusID)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Buffer micro block with larger consensus ID ("
                          << consensus_id << "), current (" << m_consensusID
                          << ")");

            lock_guard<mutex> h(m_mutexMicroBlockConsensusBuffer);

            m_microBlockConsensusBuffer[consensus_id].push_back(
                make_pair(from, message));
        }
        else
        {
            return ProcessMicroblockConsensusCore(message, offset, from);
        }
    }

    return true;
}

void Node::CommitMicroBlockConsensusBuffer()
{
    lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);

    for (const auto& i : m_microBlockConsensusBuffer[m_consensusID])
    {
        auto runconsensus = [this, i]() {
            ProcessMicroblockConsensusCore(i.second, MessageOffset::BODY,
                                           i.first);
        };
        DetachedFunction(1, runconsensus);
    }
}

void Node::CleanMicroblockConsensusBuffer()
{
    lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);
    m_microBlockConsensusBuffer.clear();
}

bool Node::ProcessMicroblockConsensusCore(const vector<unsigned char>& message,
                                          unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (!CheckState(PROCESS_MICROBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in MICROBLOCK_CONSENSUS state");
        return false;
    }

    // Consensus message must be processed in order. The following will block till it is the right order.
    std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
    if (cv_processConsensusMessage.wait_for(
            cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
            [this, message, offset]() -> bool {
                lock_guard<mutex> g(m_mutexConsensus);
                if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
                {
                    LOG_GENERAL(WARNING,
                                "The node started the process of rejoining, "
                                "Ignore rest of "
                                "consensus msg.")
                    return false;
                }

                if (m_consensusObject == nullptr)
                {
                    LOG_GENERAL(WARNING,
                                "m_consensusObject should have been created "
                                "but it is not")
                    return false;
                }
                return m_consensusObject->CanProcessMessage(message, offset);
            }))
    {
        // Correct order preserved
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Timeout while waiting for correct order of consensus "
                    "messages");
        return false;
    }

    lock_guard<mutex> g(m_mutexConsensus);

    if (!m_consensusObject->ProcessMessage(message, offset, from))
    {
        return false;
    }

    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        // Update the micro block with the co-signatures from the consensus
        m_microblock->SetCoSignatures(*m_consensusObject);

        if (m_isPrimary == true)
        {
            LOG_STATE("[MICON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_currentEpochNum << "] DONE");

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        if (m_isMBSender == true)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Designated as Microblock sender");

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block consensus "
                      << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                      << ")");
        m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
        m_lastMicroBlockCoSig.second.SetCoSignatures(*m_consensusObject);

        SetState(WAITING_FINALBLOCK);

        if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
        {
            lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
            cv_FBWaitMB.notify_all();
        }
        else
        {
            m_mediator.m_ds->m_stateDeltaFromShards.clear();
            AccountStore::GetInstance().SerializeDelta();
            AccountStore::GetInstance().GetSerializedDelta(
                m_mediator.m_ds->m_stateDeltaFromShards);
            m_mediator.m_ds->SaveCoinbase(
                m_microblock->GetB1(), m_microblock->GetB2(),
                m_microblock->GetHeader().GetShardID());
            m_mediator.m_ds->cv_scheduleFinalBlockConsensus.notify_all();
            {
                lock_guard<mutex> g(m_mediator.m_ds->m_mutexMicroBlocks);
                m_mediator.m_ds->m_microBlocks[m_mediator.m_currentEpochNum]
                    .emplace(*m_microblock);
            }
            m_mediator.m_ds->m_toSendTxnToLookup = true;
            m_mediator.m_ds->RunConsensusOnFinalBlock();
        }
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");

        if (m_consensusObject->GetConsensusErrorCode()
            == ConsensusCommon::MISSING_TXN)
        {
            // Missing txns in microblock proposed by leader. Will attempt to fetch
            // missing txns from leader, set to a valid state to accept cosig1 and cosig2
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << (m_consensusObject->GetConsensusErrorMsg()));

            // Block till txn is fetched
            unique_lock<mutex> lock(m_mutexCVMicroBlockMissingTxn);
            if (cv_MicroBlockMissingTxn.wait_for(
                    lock, chrono::seconds(FETCHING_MISSING_TXNS_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "fetching missing txn timeout");
            }
            else
            {
                // Re-run consensus
                m_consensusObject->RecoveryAndProcessFromANewState(
                    ConsensusCommon::INITIAL);

                auto rerunconsensus = [this, message, offset, from]() {
                    ProcessMicroblockConsensus(message, offset, from);
                };
                DetachedFunction(1, rerunconsensus);
                return true;
            }
        }
        else
        {
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - unhandled consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << m_consensusObject->GetConsensusErrorMsg());
        }

        // return false;
        // TODO: Optimize state transition.
        LOG_GENERAL(WARNING,
                    "ConsensusCommon::State::ERROR here, but we move on.");

        SetState(WAITING_FINALBLOCK); // Move on to next Epoch.
        if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "If I received a new Finalblock from DS committee. I will "
                "still process it");

            lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
            cv_FBWaitMB.notify_all();
        }
        else
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DS Microblock failed, discard changes on microblock and "
                      "proceed to finalblock consensus");
            m_mediator.m_ds->cv_scheduleFinalBlockConsensus.notify_all();
            m_mediator.m_ds->RunConsensusOnFinalBlock(true);
        }
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << m_consensusObject->GetStateString());

        cv_processConsensusMessage.notify_all();
    }
    return true;
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