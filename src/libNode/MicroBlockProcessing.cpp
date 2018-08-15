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

#ifndef IS_LOOKUP_NODE
void Node::SubmitMicroblockToDSCommittee() const
{
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        return;
    }

    // Message = [8-byte Tx blocknum] [Tx microblock core] [State Delta]
    vector<unsigned char> microblock
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    // 8-byte tx blocknum
    uint64_t txBlockNum
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    Serializable::SetNumber<uint64_t>(microblock, cur_offset, txBlockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    // // 4-byte consensusid
    // Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_consensusID,
    //                                   sizeof(uint32_t));
    // cur_offset += sizeof(uint32_t);

    // // 4-byte shard ID
    // Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_myShardID,
    //                                   sizeof(uint32_t));
    // cur_offset += sizeof(uint32_t);

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
#endif // IS_LOOKUP_NODE

bool Node::ProcessMicroblockConsensus(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    {
        unique_lock<mutex> g(m_mutexNewRoundStarted);
        if (!m_newRoundStarted)
        {
            // LOG_GENERAL(INFO, "Wait for new consensus round started");
            if (m_cvNewRoundStarted.wait_for(
                    g, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for new round started timeout, ignore");
                return false;
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "New consensus round started, moving to "
                      "ProcessSubmitTxnSharing");
            if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
            {
                LOG_GENERAL(WARNING, "The node started rejoin, ignore");
                return false;
            }
        }
        else
        {
            // LOG_GENERAL(INFO, "No need to wait for newRoundStarted");
        }
    }

    {
        lock_guard<mutex> g(m_mutexConsensus);

        // Consensus messages must be processed in correct sequence as they come in
        // It is possible for ANNOUNCE to arrive before correct DS state
        // In that case, state transition will occurs and ANNOUNCE will be processed.

        if (m_state == MICROBLOCK_CONSENSUS_PREP)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Received microblock announcement from shard leader. I "
                      "will move on to consensus");
            cv_microblockConsensus.notify_all();

            std::unique_lock<std::mutex> cv_lk(
                m_MutexCVMicroblockConsensusObject);

            if (cv_microblockConsensusObject.wait_for(
                    cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
                    [this] { return (m_state == MICROBLOCK_CONSENSUS); }))
            {
                // condition passed without timeout
            }
            else
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Time out while waiting for state transition and "
                          "consensus object creation ");
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "State transition is completed and consensus object "
                      "creation.");
        }
    }

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
        {
            lock_guard<mutex> g2(m_mutexNewRoundStarted);
            m_newRoundStarted = false;
        }

        if (m_isPrimary == true)
        {
            LOG_STATE("[MICON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_currentEpochNum << "] DONE");

            // Update the micro block with the co-signatures from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        if (m_isMBSender == true)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Designated as Microblock sender");

            // Update the micro block with the co-signature from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block consensus "
                      << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                      << ")");
        m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
        m_lastMicroBlockCoSig.second.SetCoSignatures(*m_consensusObject);

        if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
        {
            SetState(WAITING_FINALBLOCK);

            lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
            cv_FBWaitMB.notify_all();
        }
        else
        {
            m_mediator.m_ds->cv_scheduleFinalBlockConsensus.notify_all();
            {
                lock_guard<mutex> g(m_mediator.m_ds->m_mutexMicroBlocks);
                m_mediator.m_ds->m_microBlocks.emplace(*m_microblock);
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

        {
            lock_guard<mutex> g2(m_mutexNewRoundStarted);
            m_newRoundStarted = false;
        }

        if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
        {
            // SetState(WAITING_FINALBLOCK); // Move on to next Epoch.

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
#endif // IS_LOOKUP_NODE
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::ComposeMicroBlock()
{
    // To-do: Replace dummy values with the required ones
    LOG_MARKER();

    // TxBlockHeader
    uint8_t type = TXBLOCKTYPE::MICRO;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint32_t shardID = m_myShardID;
    uint256_t gasLimit = MICROBLOCK_GAS_LIMIT;
    uint256_t gasUsed = 1;
    BlockHash prevHash;
    fill(prevHash.asArray().begin(), prevHash.asArray().end(), 0x77);
    uint64_t blockNum = m_mediator.m_currentEpochNum;
    uint256_t timestamp = get_time_as_int();
    TxnHash txRootHash;
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
            LOG_GENERAL(FATAL, "Num txns and Order size not same");
        }
        tranHashes = m_TxnOrder;
        m_TxnOrder.clear();
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Creating new micro block.")
    m_microblock.reset(new MicroBlock(
        MicroBlockHeader(type, version, shardID, gasLimit, gasUsed, prevHash,
                         blockNum, timestamp, txRootHash, numTxs, minerPubKey,
                         dsBlockNum, dsBlockHeader, stateDeltaHash),
        tranHashes, CoSignatures()));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Micro block proposed with "
                  << m_microblock->GetHeader().GetNumTxs()
                  << " transactions for epoch "
                  << m_mediator.m_currentEpochNum);

    return true;
}

bool Node::OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                             unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (errorMsg.size() < 2 * sizeof(uint32_t) + offset)
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
        // if (submittedTransactions.find(missingTransactions[i])
        //     != submittedTransactions.end())
        // {
        //     t = submittedTransactions[missingTransactions[i]];
        // }
        // else if (receivedTransactions.find(missingTransactions[i])
        //          != receivedTransactions.end())
        // {
        //     t = receivedTransactions[missingTransactions[i]];
        // }
        if (processedTransactions.find(missingTransactions[i])
            != processedTransactions.end())
        {
            t = processedTransactions[missingTransactions[i]];
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

    unsigned int txn_sent_count = 0;

    std::list<Transaction> curTxns;

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
        lock_guard<mutex> g(m_mutexCreatedTransactions);

        auto& compIdx
            = m_createdTransactions.get<MULTI_INDEX_KEY::ADDR_NONCE>();
        auto it = compIdx.find(make_tuple(t.GetSenderAddr(), t.GetNonce()));
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
        lock_guard<mutex> g(m_mutexCreatedTransactions);

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

    uint64_t blockNum = m_mediator.m_currentEpochNum;

    auto appendOne = [this, &blockNum](const Transaction& t) {
        lock_guard<mutex> g(m_mutexProcessedTransactions);
        auto& processedTransactions = m_processedTransactions[blockNum];
        processedTransactions.insert(make_pair(t.GetTranID(), t));
        m_TxnOrder.push_back(t.GetTranID());
    };

    uint256_t gasUsedTotal = 0;

    while (txn_sent_count < MAXSUBMITTXNPERNODE * m_myShardMembers->size()
           && gasUsedTotal < MICROBLOCK_GAS_LIMIT)
    {
        Transaction t;

        // check m_addrNonceTxnMap contains any txn meets right nonce,
        // if contains, process it withou increment the txn_sent_count as it's already
        // incremented when inserting
        if (findOneFromAddrNonceTxnMap(t))
        {
            // check whether m_createdTransaction have transaction with same Addr and nonce
            // if has and with larger gasPrice then replace with that one. (*optional step)
            findSameNonceButHigherGasPrice(t);

            uint256_t gasUsed = 0;
            if (m_mediator.m_validator->CheckCreatedTransaction(t, gasUsed)
                || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                    && t.GetCode().empty()))
            {
                appendOne(t);
                gasUsedTotal += gasUsed;
                continue;
            }
        }
        // if no txn in u_map meet right nonce process new come-in transactions
        else if (findOneFromCreated(t))
        {
            uint256_t gasUsed = 0;

            // check nonce, if nonce larger than expected, put it into m_addrNonceTxnMap
            if (t.GetNonce()
                > AccountStore::GetInstance().GetNonceTemp(t.GetSenderAddr())
                    + 1)
            {
                auto it1 = m_addrNonceTxnMap.find(t.GetSenderAddr());
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
                m_addrNonceTxnMap[t.GetSenderAddr()].insert({t.GetNonce(), t});
            }
            // if nonce too small, ignore it
            else if (t.GetNonce() < AccountStore::GetInstance().GetNonceTemp(
                                        t.GetSenderAddr())
                         + 1)
            {
                LOG_GENERAL(INFO,
                            "Nonce too small"
                                << " Expected "
                                << AccountStore::GetInstance().GetNonceTemp(
                                       t.GetSenderAddr())
                                << " Found " << t.GetNonce());
            }
            // if nonce correct, process it
            else if (m_mediator.m_validator->CheckCreatedTransaction(t, gasUsed)
                     || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
                     || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                         && t.GetCode().empty()))
            {

                appendOne(t);
                gasUsedTotal += gasUsed;
            }
        }
        else
        {
            break;
        }
        txn_sent_count++;
    }
}

bool Node::VerifyTxnsOrdering([[gnu::unused]] const list<Transaction>& txns)
{

    LOG_MARKER();
    unordered_map<Address, uint256_t> nonceMap;

    for (const auto& t : txns)
    {
        auto it = nonceMap.find(t.GetSenderAddr());
        if (it == nonceMap.end())
        {

            nonceMap.insert({t.GetSenderAddr(), t.GetNonce()});
        }
        else
        {
            if (t.GetNonce() != nonceMap[t.GetSenderAddr()] + 1)
            {
                LOG_GENERAL(INFO,
                            "Nonce of txn: "
                                << t.GetNonce() << " Nonce Map: "
                                << nonceMap[t.GetSenderAddr()] + 1);
                return false;
            }
            nonceMap[t.GetSenderAddr()] = t.GetNonce();
        }
    }

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. Creating microblock for epoch "
                  << m_mediator.m_currentEpochNum);

    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "going to sleep for " << TX_DISTRIBUTE_TIME_IN_MS
                                        << " milliseconds");
        std::this_thread::sleep_for(
            chrono::milliseconds(TX_DISTRIBUTE_TIME_IN_MS));
    }

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (!isVacuousEpoch)
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
    ComposeMicroBlock();

    vector<unsigned char> microblock;
    m_microblock->Serialize(microblock, 0);

    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << (*m_myShardMembers)[m_consensusLeaderID].second);

    auto nodeMissingTxnsFunc
        = [this](const vector<unsigned char>& errorMsg, unsigned int offset,
                 const Peer& from) mutable -> bool {
        return OnNodeMissingTxns(errorMsg, offset, from);
    };

    auto commitFailureFunc
        = [this](const map<unsigned int, vector<unsigned char>>& m) mutable
        -> bool { return OnCommitFailure(m); };

    deque<pair<PubKey, Peer>> peerList;

    for (auto it = m_myShardMembers->begin(); it != m_myShardMembers->end();
         ++it)
    {
        peerList.emplace_back(*it);
    }

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, peerList, static_cast<unsigned char>(NODE),
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
    cl->StartConsensus(microblock, MicroBlockHeader::SIZE);

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardBackup()
{
    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a backup node. Waiting for microblock announcement for epoch "
            << m_mediator.m_currentEpochNum);
    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return MicroBlockValidator(message, errorMsg);
    };

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard backup. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << (*m_myShardMembers)[m_consensusLeaderID].second);

    deque<pair<PubKey, Peer>> peerList;

    for (auto it = m_myShardMembers->begin(); it != m_myShardMembers->end();
         ++it)
    {
        peerList.emplace_back(*it);
    }

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first, peerList,
        static_cast<unsigned char>(NODE),
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
    LOG_MARKER();

    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
    {
        InitCoinbase();
    }
    else
    {
        m_mediator.m_ds->m_toSendTxnToLookup = false;
    }

    if (m_isPrimary == true)
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

    {
        lock_guard<mutex> g2(m_mutexNewRoundStarted);
        if (!m_newRoundStarted)
        {
            m_newRoundStarted = true;
            m_cvNewRoundStarted.notify_all();
        }
    }

    SetState(MICROBLOCK_CONSENSUS);
    cv_microblockConsensusObject.notify_all();
    return true;
}

bool Node::CheckBlockTypeIsMicro()
{
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

bool Node::CheckMicroBlockTimestamp()
{
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

bool Node::ProcessTransactionWhenShardBackup(const vector<TxnHash>& tranHashes,
                                             vector<TxnHash>& missingtranHashes)
{
    LOG_MARKER();

    auto findFromCreated = [this](Transaction& t, const TxnHash& th) -> bool {
        lock_guard<mutex> g(m_mutexCreatedTransactions);

        auto& hashIdx = m_createdTransactions.get<MULTI_INDEX_KEY::TXN_ID>();

        if (hashIdx.size() == 0)
        {
            return false;
        }

        // auto it = find_if(
        //     begin(m_createdTransactions), end(m_createdTransactions),
        //     [&th](const Transaction& t) { return t.GetTranID() == th; });

        // if (m_createdTransactions.end() == it)
        // {
        //     LOG_GENERAL(WARNING, "txn is not found");
        //     return false;
        // }

        // t = move(*it);
        // m_createdTransactions.erase(it);

        auto it = hashIdx.find(th);

        if (hashIdx.end() == it)
        {
            LOG_GENERAL(WARNING, "txn is not found");
            return false;
        }

        t = move(*it);
        hashIdx.erase(it);

        return true;
    };

    std::list<Transaction> curTxns;

    for (const auto& tranHash : tranHashes)
    {
        Transaction t;

        if (findFromCreated(t, tranHash))
        {

            curTxns.emplace_back(t);
        }
        else
        {
            missingtranHashes.emplace_back(tranHash);
        }
    }

    if (!missingtranHashes.empty())
    {
        return true;
    }

    if (!VerifyTxnsOrdering(curTxns))
    {
        return false;
    }

    uint64_t blockNum = m_mediator.m_currentEpochNum;

    auto appendOne = [this, &blockNum](const Transaction& t) {
        lock_guard<mutex> g(m_mutexProcessedTransactions);
        auto& processedTransactions = m_processedTransactions[blockNum];
        processedTransactions.insert(make_pair(t.GetTranID(), t));
    };

    for (const auto& t : curTxns)
    {
        uint256_t gasUsed = 0;
        if (m_mediator.m_validator->CheckCreatedTransaction(t, gasUsed)
            || (!t.GetCode().empty() && t.GetToAddr() == NullAddress)
            || (!t.GetData().empty() && t.GetToAddr() != NullAddress
                && t.GetCode().empty()))
        {
            appendOne(t);
        }
    }

    AccountStore::GetInstance().SerializeDelta();

    return true;
}

unsigned char Node::CheckLegitimacyOfTxnHashes(vector<unsigned char>& errorMsg)
{

    vector<TxnHash> missingTxnHashes;
    if (!ProcessTransactionWhenShardBackup(m_microblock->GetTranHashes(),
                                           missingTxnHashes))
    {
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
        Serializable::SetNumber<uint32_t>(errorMsg, 0, m_numOfAbsentTxnHashes,
                                          sizeof(uint32_t));
        Serializable::SetNumber<uint64_t>(errorMsg, sizeof(uint32_t),
                                          m_mediator.m_currentEpochNum,
                                          sizeof(uint64_t));

        m_txnsOrdering = m_microblock->GetTranHashes();
        return LEGITIMACYRESULT::MISSEDTXN;
    }

    return LEGITIMACYRESULT::SUCCESS;
}

bool Node::CheckMicroBlockHashes(vector<unsigned char>& errorMsg)
{
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
    // Check transaction root
    TxnHash expectedTxRootHash
        = ComputeTransactionsRoot(m_microblock->GetTranHashes());

    LOG_GENERAL(
        INFO,
        "Microblock root computation done "
            << DataConversion::charArrToHexStr(expectedTxRootHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetTxRootHash().asArray()));

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
    StateHash expectedStateDeltaHash
        = AccountStore::GetInstance().GetStateDeltaHash();

    LOG_GENERAL(INFO,
                "Microblock state delta generation done "
                    << DataConversion::charArrToHexStr(
                           expectedStateDeltaHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetStateDeltaHash().asArray()));

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

bool Node::CheckMicroBlockShardID()
{
    // Check version (must be most current version)
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        return true;
    }
    if (m_microblock->GetHeader().GetShardID() != m_myShardID)
    {
        LOG_GENERAL(WARNING,
                    "ShardID check failed. Expected: "
                        << m_myShardID << " Actual: "
                        << m_microblock->GetHeader().GetShardID());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_SHARD_ID);

        return false;
    }

    LOG_GENERAL(INFO, "ShardID check passed");

    return true;
}

bool Node::MicroBlockValidator(const vector<unsigned char>& microblock,
                               vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // [TODO] To put in the logic
    m_microblock = make_shared<MicroBlock>(MicroBlock(microblock, 0));

    bool valid = false;

    do
    {
        if (!CheckBlockTypeIsMicro() || !CheckMicroBlockVersion()
            || !CheckMicroBlockTimestamp() || !CheckMicroBlockHashes(errorMsg)
            || !CheckMicroBlockTxnRootHash() || !CheckMicroBlockStateDeltaHash()
            || !CheckMicroBlockShardID())
        {
            break;
        }

        // Check gas limit (must satisfy some equations)
        // Check gas used (must be <= gas limit)
        // Check state root (TBD)
        // Check pubkey (must be valid and = shard leader)
        // Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)
        // Check parent DS block number (must be = block number of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)

        valid = true;
    } while (false);

    if (!valid)
    {
        m_microblock = nullptr;
        Serializable::SetNumber<uint32_t>(
            errorMsg, errorMsg.size(), m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        // LOG_GENERAL(INFO, "To-do: What to do if proposed microblock is not valid?");
        return false;
    }

    return valid;

    // #else // IS_LOOKUP_NODE

    // return true;
}
#endif // IS_LOOKUP_NODE
