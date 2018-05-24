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

#include <boost/multiprecision/cpp_int.hpp>

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

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void Node::SubmitMicroblockToDSCommittee() const
{
    // Message = [32-byte DS blocknum] [4-byte consensusid] [4-byte shard ID] [Tx microblock]
    vector<unsigned char> microblock
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    // 32-byte DS blocknum
    uint256_t DSBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    Serializable::SetNumber<uint256_t>(microblock, cur_offset, DSBlockNum,
                                       sizeof(uint256_t));
    cur_offset += sizeof(uint256_t);

    // 4-byte consensusid
    Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_consensusID,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // 4-byte shard ID
    Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_myShardID,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // Tx microblock
    m_microblock->Serialize(microblock, cur_offset);

#ifdef STAT_TEST
    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] SENT");
#endif // STAT_TEST
    P2PComm::GetInstance().SendBroadcastMessage(
        m_mediator.m_DSCommitteeNetworkInfo, microblock);
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessMicroblockConsensus(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexConsensus);

    // Consensus messages must be processed in correct sequence as they come in
    // It is possible for ANNOUNCE to arrive before correct DS state
    // In that case, state transition will occurs and ANNOUNCE will be processed.

    if ((m_state == TX_SUBMISSION) || (m_state == TX_SUBMISSION_BUFFER)
        || (m_state == MICROBLOCK_CONSENSUS_PREP))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Received microblock announcement from shard leader. I "
                  "will move on to consensus");
        cv_microblockConsensus.notify_all();

        std::unique_lock<std::mutex> cv_lk(m_MutexCVMicroblockConsensusObject);

        if (cv_microblockConsensusObject.wait_for(
                cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
                [this] { return (m_state == MICROBLOCK_CONSENSUS); }))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Time out while waiting for state transition and "
                      "consensus object creation ");
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State transition is completed and consensus object "
                  "creation. (check for timeout)");
    }
    // else if (m_state != MICROBLOCK_CONSENSUS)
    if (!CheckState(PROCESS_MICROBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in MICROBLOCK_CONSENSUS state");
        return false;
    }

    bool result = m_consensusObject->ProcessMessage(message, offset, from);

    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        if (m_isPrimary == true)
        {
#ifdef STAT_TEST
            LOG_STATE("[MICON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_currentEpochNum << "] DONE");
#endif // STAT_TEST

            // Update the micro block with the co-signatures from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        if (m_isMBSender == true)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "I am designated as Microblock sender");

            // Update the micro block with the co-signature from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        SetState(WAITING_FINALBLOCK);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block consensus"
                      << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                      << ")");
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");

        // return false;
        // TODO: Optimize state transition.
        LOG_GENERAL(WARNING,
                    "ConsensusCommon::State::ERROR here, but we move on.");
        SetState(WAITING_FINALBLOCK); // Move on to next Epoch.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "If I received a new Finalblock from DS committee. I will "
                  "still process it");
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << state);
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}

#ifndef IS_LOOKUP_NODE
bool Node::ComposeMicroBlock()
{
    // To-do: Replace dummy values with the required ones
    LOG_MARKER();

    // TxBlockHeader
    uint8_t type = TXBLOCKTYPE::MICRO;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint256_t gasLimit = 100;
    uint256_t gasUsed = 1;
    BlockHash prevHash;
    fill(prevHash.asArray().begin(), prevHash.asArray().end(), 0x77);
    uint256_t blockNum = (uint256_t)m_mediator.m_currentEpochNum;
    uint256_t timestamp = get_time_as_int();
    TxnHash txRootHash;
    uint32_t numTxs = 0;
    const PubKey& minerPubKey = m_mediator.m_selfKey.second;
    uint256_t dsBlockNum = (uint256_t)m_mediator.m_currentEpochNum;
    BlockHash dsBlockHeader;
    fill(dsBlockHeader.asArray().begin(), dsBlockHeader.asArray().end(), 0x11);

    // TxBlock
    vector<TxnHash> tranHashes;

    unsigned int index = 0;
    {
        lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
        lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
        lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

        auto& receivedTransactions = m_receivedTransactions[blockNum];
        auto& submittedTransactions = m_submittedTransactions[blockNum];

        txRootHash = ComputeTransactionsRoot(receivedTransactions,
                                             submittedTransactions);

        numTxs = receivedTransactions.size() + submittedTransactions.size();
        tranHashes.resize(numTxs);
        for (const auto& tx : receivedTransactions)
        {
            const auto& txid = tx.first.asArray();
            copy(txid.begin(), txid.end(),
                 tranHashes.at(index).asArray().begin());
            index++;
        }

        for (const auto& tx : submittedTransactions)
        {
            const auto& txid = tx.first.asArray();
            copy(txid.begin(), txid.end(),
                 tranHashes.at(index).asArray().begin());
            index++;
        }
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Creating new micro block.")
    m_microblock.reset(new MicroBlock(
        MicroBlockHeader(type, version, gasLimit, gasUsed, prevHash, blockNum,
                         timestamp, txRootHash, numTxs, minerPubKey, dsBlockNum,
                         dsBlockHeader),
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

    uint32_t numOfAbsentHashes
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint32_t blockNum
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    vector<TxnHash> missingTransactions;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        TxnHash txnHash;
        copy(errorMsg.begin() + offset,
             errorMsg.begin() + offset + TRAN_HASH_SIZE,
             txnHash.asArray().begin());
        offset += TRAN_HASH_SIZE;

        missingTransactions.push_back(txnHash);
    }

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

    auto& receivedTransactions = m_receivedTransactions[blockNum];
    auto& submittedTransactions = m_submittedTransactions[blockNum];

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        // LOG_GENERAL(INFO, "Peer " << from << " : " << portNo << " missing txn " << missingTransactions[i])
        vector<unsigned char> tx_message
            = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
        Transaction t;
        if (submittedTransactions.find(missingTransactions[i])
            != submittedTransactions.end())
        {
            t = submittedTransactions[missingTransactions[i]];
        }
        else if (receivedTransactions.find(missingTransactions[i])
                 != receivedTransactions.end())
        {
            t = receivedTransactions[missingTransactions[i]];
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Leader unable to find txn proposed in microblock "
                            << missingTransactions[i]);
            // throw exception();
            return false;
        }

        Serializable::SetNumber<uint32_t>(tx_message, MessageOffset::BODY,
                                          SUBMITTRANSACTIONTYPE::MISSINGTXN,
                                          sizeof(uint32_t));

        Serializable::SetNumber<uint32_t>(
            tx_message, MessageOffset::BODY + sizeof(uint32_t), blockNum,
            sizeof(uint32_t));

        t.Serialize(tx_message,
                    MessageOffset::BODY + sizeof(uint32_t) + sizeof(uint32_t));
        P2PComm::GetInstance().SendMessage(peer, tx_message);
    }

    return true;
}

bool Node::OnCommitFailure(
    const std::map<unsigned int, std::vector<unsigned char>>& commitFailureMap)
{
    LOG_MARKER();

    // for(auto failureEntry: commitFailureMap)
    // {

    // }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Going to sleep before restarting consensus");

    std::this_thread::sleep_for(30s);
    RunConsensusOnMicroBlockWhenShardLeader();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Woke from sleep after consensus restart");

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am primary. Creating microblock for epoch"
                  << m_mediator.m_currentEpochNum);

    // composed microblock stored in m_microblock
    ComposeMicroBlock();

    vector<unsigned char> microblock;
    m_microblock->Serialize(microblock, 0);

    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: I am shard leader");
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: m_consensusID: " << m_consensusID
                                    << " m_consensusMyID: " << m_consensusMyID);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: m_consensusLeaderID: " << m_consensusLeaderID);

    auto nodeMissingTxnsFunc
        = [this](const vector<unsigned char>& errorMsg, unsigned int offset,
                 const Peer& from) mutable -> bool {
        return OnNodeMissingTxns(errorMsg, offset, from);
    };

    auto commitFailureFunc
        = [this](const map<unsigned int, vector<unsigned char>>& m) mutable
        -> bool { return OnCommitFailure(m); };

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, m_myShardMembersPubKeys,
        m_myShardMembersNetworkInfo, static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), nodeMissingTxnsFunc,
        commitFailureFunc));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

#ifdef STAT_TEST
    LOG_STATE("[MICON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] BGIN");
#endif // STAT_TEST
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
              "MS: I am shard backup");
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: m_consensusID: " << m_consensusID
                                    << " m_consensusMyID: " << m_consensusMyID);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "MS: m_consensusLeaderID: " << m_consensusLeaderID);

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first,
        m_myShardMembersPubKeys, m_myShardMembersNetworkInfo,
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

    // set state first and then take writer lock so that SubmitTransactions
    // if it takes reader lock later breaks out of loop
    SetState(MICROBLOCK_CONSENSUS_PREP);
    // unique_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);

    if (m_isPrimary == true)
    {
        if (!RunConsensusOnMicroBlockWhenShardLeader())
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardLeader");
            // throw exception();
            return false;
        }
    }
    else
    {
        if (!RunConsensusOnMicroBlockWhenShardBackup())
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardBackup");
            // throw exception();
            return false;
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
            return false;
        }
    }

    LOG_GENERAL(INFO, "Timestamp check passed");

    return true;
}

bool Node::CheckLegitimacyOfTxnHashes(vector<unsigned char>& errorMsg)
{
    lock(m_mutexReceivedTransactions, m_mutexSubmittedTransactions);
    lock_guard<mutex> g(m_mutexReceivedTransactions, adopt_lock);
    lock_guard<mutex> g2(m_mutexSubmittedTransactions, adopt_lock);

    auto const& receivedTransactions
        = m_receivedTransactions[m_mediator.m_currentEpochNum];
    auto const& submittedTransactions
        = m_submittedTransactions[m_mediator.m_currentEpochNum];

    uint32_t numOfAbsentHashes = 0;

    int offset = 0;

    for (auto const& hash : m_microblock->GetTranHashes())
    {
        // Check if transaction is part of submitted Tx list
        if (submittedTransactions.find(hash) != submittedTransactions.end())
        {
            continue;
        }

        // Check if transaction is part of received Tx list
        if (receivedTransactions.find(hash) == receivedTransactions.end())
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Missing txn: " << hash)
            if (errorMsg.size() == 0)
            {
                errorMsg.resize(2 * sizeof(uint32_t) + TRAN_HASH_SIZE);
                offset += (2 * sizeof(uint32_t));
            }
            else
            {
                errorMsg.resize(offset + TRAN_HASH_SIZE);
            }
            copy(hash.asArray().begin(), hash.asArray().end(),
                 errorMsg.begin() + offset);
            offset += TRAN_HASH_SIZE;
            numOfAbsentHashes++;
        }
    }

    if (numOfAbsentHashes)
    {
        Serializable::SetNumber<uint32_t>(errorMsg, 0, numOfAbsentHashes,
                                          sizeof(uint32_t));
        Serializable::SetNumber<uint32_t>(errorMsg, sizeof(uint32_t),
                                          (uint)m_mediator.m_currentEpochNum,
                                          sizeof(uint32_t));
        return false;
    }

    return true;
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
        return false;
    }

    LOG_GENERAL(INFO, "Hash count check passed");

    // Check if I have the txn bodies corresponding to the hashes included in the microblock
    if (!CheckLegitimacyOfTxnHashes(errorMsg))
    {
        LOG_GENERAL(WARNING,
                    "Missing a txn hash included in proposed microblock");
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
        return false;
    }

    LOG_GENERAL(INFO, "Root check passed");

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
            || !CheckMicroBlockTxnRootHash())
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
