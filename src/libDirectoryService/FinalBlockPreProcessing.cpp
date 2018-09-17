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

#include <algorithm>
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::ExtractDataFromMicroblocks(
    TxnHash& microblockTxnTrieRoot, StateHash& microblockDeltaTrieRoot,
    TxnHash& microblockTranReceiptRoot,
    std::vector<MicroBlockHashSet>& microblockHashes,
    std::vector<uint32_t>& shardIDs, uint256_t& allGasLimit,
    uint256_t& allGasUsed, uint32_t& numTxs,
    std::vector<bool>& isMicroBlockEmpty, uint32_t& numMicroBlocks)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ExtractDataFromMicroblocks not expected "
                    "to be called from LookUp node.");
        return;
    }

    LOG_MARKER();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    auto blockNum
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;
    unsigned int i = 1;

    {
        lock_guard<mutex> g(m_mutexMicroBlocks);

        auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
        for (auto& microBlock : microBlocks)
        {
            LOG_STATE("[STATS]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << i << "    ][" << microBlock.GetHeader().GetNumTxs()
                      << "] PROPOSED");

            i++;

            microblockHashes.push_back(
                {microBlock.GetHeader().GetTxRootHash(),
                 microBlock.GetHeader().GetStateDeltaHash(),
                 microBlock.GetHeader().GetTranReceiptHash()});
            shardIDs.push_back(microBlock.GetHeader().GetShardID());
            allGasLimit += microBlock.GetHeader().GetGasLimit();
            allGasUsed += microBlock.GetHeader().GetGasUsed();
            numTxs += microBlock.GetHeader().GetNumTxs();

            ++numMicroBlocks;

            bool isEmpty = microBlock.GetHeader().GetNumTxs() == 0
                && microBlock.GetHeader().GetStateDeltaHash() == StateHash();

            bool isEmptyTxn = (microBlock.GetHeader().GetNumTxs() == 0);

            if (!isVacuousEpoch && !isEmpty)
            {
                m_mediator.m_node->m_unavailableMicroBlocks[blockNum].insert(
                    {{{microBlock.GetHeader().GetTxRootHash(),
                       microBlock.GetHeader().GetStateDeltaHash(),
                       microBlock.GetHeader().GetTranReceiptHash()},
                      microBlock.GetHeader().GetShardID()},
                     {false, true}});

                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Added " << microBlock.GetHeader().GetTxRootHash()
                                   << " "
                                   << microBlock.GetHeader().GetStateDeltaHash()
                                   << " for unavailable"
                                   << " MicroBlock " << blockNum);
            }

            isMicroBlockEmpty.push_back(isEmptyTxn);
        }
    }

    // if (m_mediator.m_node->m_unavailableMicroBlocks.find(blockNum)
    //         != m_mediator.m_node->m_unavailableMicroBlocks.end()
    //     && m_mediator.m_node->m_unavailableMicroBlocks[blockNum].size() > 0)
    // {
    //     unique_lock<mutex> g(m_mediator.m_node->m_mutexAllMicroBlocksRecvd);
    //     m_mediator.m_node->m_allMicroBlocksRecvd = false;
    // }

    microblockTxnTrieRoot = ComputeTransactionsRoot(microblockHashes);
    microblockDeltaTrieRoot = ComputeDeltasRoot(microblockHashes);
    microblockTranReceiptRoot = ComputeTranReceiptsRoot(microblockHashes);

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Proposed FinalBlock TxnTrieRootHash : "
            << microblockTxnTrieRoot.hex() << endl
            << " DeltaTrieRootHash: " << microblockDeltaTrieRoot.hex() << endl
            << " TranReceiptRootHash: " << microblockTranReceiptRoot.hex());
}

void DirectoryService::ComposeFinalBlock()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ComposeFinalBlock not expected to "
                    "be called from LookUp node.");
        return;
    }

    TxnHash microblockTxnTrieRoot;
    StateHash microblockDeltaTrieRoot;
    TxnHash microblockTranReceiptRoot;
    std::vector<MicroBlockHashSet> microBlockHashes;
    std::vector<uint32_t> shardIDs;
    uint8_t type = TXBLOCKTYPE::FINAL;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint256_t allGasLimit = 0;
    uint256_t allGasUsed = 0;
    uint32_t numTxs = 0;
    std::vector<bool> isMicroBlockEmpty;
    uint32_t numMicroBlocks = 0;
    StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

    ExtractDataFromMicroblocks(microblockTxnTrieRoot, microblockDeltaTrieRoot,
                               microblockTranReceiptRoot, microBlockHashes,
                               shardIDs, allGasLimit, allGasUsed, numTxs,
                               isMicroBlockEmpty, numMicroBlocks);

    BlockHash prevHash;
    uint256_t timestamp = get_time_as_int();

    uint64_t blockNum = 0;
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        TxBlock lastBlock = m_mediator.m_txBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        lastBlock.GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> hashVec = sha2.Finalize();
        copy(hashVec.begin(), hashVec.end(), prevHash.asArray().begin());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Prev block hash as per leader "
                      << prevHash.hex() << endl
                      << "TxBlockHeader: " << lastBlock.GetHeader());
        blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
    }

    if (m_mediator.m_dsBlockChain.GetBlockCount() <= 0)
    {
        LOG_GENERAL(WARNING,
                    "assertion failed (" << __FILE__ << ":" << __LINE__ << ": "
                                         << __FUNCTION__ << ")");
    }

    DSBlock lastDSBlock = m_mediator.m_dsBlockChain.GetLastBlock();
    uint64_t lastDSBlockNum = lastDSBlock.GetHeader().GetBlockNum();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    vector<unsigned char> vec;
    lastDSBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    vector<unsigned char> hashVec = sha2.Finalize();
    BlockHash dsBlockHeader;
    copy(hashVec.begin(), hashVec.end(), dsBlockHeader.asArray().begin());

    StateHash stateRoot = StateHash();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        AccountStore::GetInstance().UpdateStateTrieAll();
        stateRoot = AccountStore::GetInstance().GetStateRootHash();
    }

    m_finalBlock.reset(new TxBlock(
        TxBlockHeader(type, version, allGasLimit, allGasUsed, prevHash,
                      blockNum, timestamp, microblockTxnTrieRoot, stateRoot,
                      microblockDeltaTrieRoot, stateDeltaHash,
                      microblockTranReceiptRoot, numTxs, numMicroBlocks,
                      m_mediator.m_selfKey.second, lastDSBlockNum,
                      dsBlockHeader),
        vector<bool>(isMicroBlockEmpty),
        vector<MicroBlockHashSet>(microBlockHashes), vector<uint32_t>(shardIDs),
        CoSignatures(m_mediator.m_DSCommittee->size())));

    LOG_STATE(
        "[STATS]["
        << std::setw(15) << std::left
        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1
        << "][" << m_finalBlock->GetHeader().GetNumTxs() << "] FINAL");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block proposed with "
                  << m_finalBlock->GetHeader().GetNumTxs() << " transactions.");
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary "
                    "not expected to be called from LookUp node.");
        return true;
    }

    // Compose the final block from all the microblocks
    // I guess only the leader has to do this
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am the leader DS node. Creating final block.");

    ComposeFinalBlock(); // stores it in m_finalBlock

    // kill first ds leader (used for view change testing)
    /**
    if (m_consensusMyID == 0 && m_viewChangeCounter < 1)
    {
        LOG_GENERAL(INFO, "I am killing/suspending myself to test view change");
        // throw exception();
        return false;
    }
    **/

    // Create new consensus object
    // Dummy values for now
    //uint32_t consensusID = 0x0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto nodeMissingMicroBlocksFunc
        = [this](const vector<unsigned char>& errorMsg,
                 const Peer& from) mutable -> bool {
        return OnNodeMissingMicroBlocks(errorMsg, from);
    };

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
        static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(FINALBLOCKCONSENSUS),
        nodeMissingMicroBlocksFunc, ShardCommitFailureHandlerFunc()));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

    if (m_mode == PRIMARY_DS)
    {
        LOG_STATE("[FBCON][" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "]["
                             << m_mediator.m_txBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                      + 1 << "] BGIN");
    }

    auto announcementGeneratorFunc =
        [this](vector<unsigned char>& dst, unsigned int offset,
               const uint32_t consensusID,
               const vector<unsigned char>& blockHash, const uint16_t leaderID,
               const pair<PrivKey, PubKey>& leaderKey,
               vector<unsigned char>& messageToCosign) mutable -> bool {
        return Messenger::SetDSFinalBlockAnnouncement(
            dst, offset, consensusID, blockHash, leaderID, leaderKey,
            *m_finalBlock, messageToCosign);
    };

    cl->StartConsensus(announcementGeneratorFunc);

    return true;
}

// Check type (must be final block type)
bool DirectoryService::CheckBlockTypeIsFinal()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckBlockTypeIsFinal not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    if (m_finalBlock->GetHeader().GetType() != TXBLOCKTYPE::FINAL)
    {
        LOG_GENERAL(WARNING,
                    "Type check failed. Expected: "
                        << (unsigned int)TXBLOCKTYPE::FINAL << " Actual: "
                        << (unsigned int)m_finalBlock->GetHeader().GetType());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_FINALBLOCK);

        return false;
    }

    return true;
}

// Check version (must be most current version)
bool DirectoryService::CheckFinalBlockVersion()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckFinalBlockVersion not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    if (m_finalBlock->GetHeader().GetVersion() != BLOCKVERSION::VERSION1)
    {
        LOG_GENERAL(
            WARNING,
            "Version check failed. Expected: "
                << (unsigned int)BLOCKVERSION::VERSION1 << " Actual: "
                << (unsigned int)m_finalBlock->GetHeader().GetVersion());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_FINALBLOCK_VERSION);

        return false;
    }

    return true;
}

// Check block number (must be = 1 + block number of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckFinalBlockNumber()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckFinalBlockNumber not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    const uint64_t& finalblockBlocknum
        = m_finalBlock->GetHeader().GetBlockNum();
    uint64_t expectedBlocknum = 0;
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        expectedBlocknum
            = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1;
    }
    if (finalblockBlocknum != expectedBlocknum)
    {
        LOG_GENERAL(WARNING,
                    "Block number check failed. Expected: "
                        << expectedBlocknum
                        << " Actual: " << finalblockBlocknum);

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_FINALBLOCK_NUMBER);

        return false;
    }
    else
    {
        LOG_GENERAL(
            INFO,
            "finalblockBlocknum = expectedBlocknum = " << expectedBlocknum);
    }

    return true;
}

// Check previous hash (must be = sha2-256 digest of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckPreviousFinalBlockHash()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckPreviousFinalBlockHash not "
                    "expected to be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    const BlockHash& finalblockPrevHash
        = m_finalBlock->GetHeader().GetPrevHash();
    BlockHash expectedPrevHash;

    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> hashVec = sha2.Finalize();
        copy(hashVec.begin(), hashVec.end(),
             expectedPrevHash.asArray().begin());
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "m_mediator.m_txBlockChain.GetLastBlock().GetHeader():"
                      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader());
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Prev block hash recvd: "
                  << finalblockPrevHash.hex() << endl
                  << "Prev block hash expected: " << expectedPrevHash.hex()
                  << endl
                  << "TxBlockHeader: "
                  << m_mediator.m_txBlockChain.GetLastBlock().GetHeader());

    if (finalblockPrevHash != expectedPrevHash)
    {
        LOG_GENERAL(WARNING, "Previous hash check failed.");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_PREV_FINALBLOCK_HASH);

        return false;
    }

    return true;
}

// Check timestamp (must be greater than timestamp of last Tx block header in the Tx blockchain)
bool DirectoryService::CheckFinalBlockTimestamp()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckFinalBlockTimestamp not expected "
                    "to be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
        uint256_t finalblockTimestamp
            = m_finalBlock->GetHeader().GetTimestamp();
        uint256_t lastTxBlockTimestamp = lastTxBlock.GetHeader().GetTimestamp();
        if (finalblockTimestamp <= lastTxBlockTimestamp)
        {
            LOG_GENERAL(WARNING,
                        "Timestamp check failed. Last Tx Block: "
                            << lastTxBlockTimestamp
                            << " Final block: " << finalblockTimestamp);

            m_consensusObject->SetConsensusErrorCode(
                ConsensusCommon::INVALID_TIMESTAMP);

            return false;
        }
    }

    return true;
}

// Check microblock hashes
bool DirectoryService::CheckMicroBlockHashes(
    std::vector<unsigned char>& errorMsg)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckMicroBlockHashes not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    auto& hashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();

    std::vector<MicroBlockHashSet> missingMicroBlocks;

    {
        lock_guard<mutex> g(m_mutexMicroBlocks);
        // O(n^2) might be fine since number of shards is low
        // If its slow on benchmarking, may be first populate an unordered_set and then std::find
        for (auto& microBlockHash : hashesInMicroBlocks)
        {
            bool found = false;
            auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
            for (auto& microBlock : microBlocks)
            {
                if (microBlock.GetHeader().GetTxRootHash()
                        == microBlockHash.m_txRootHash
                    && microBlock.GetHeader().GetStateDeltaHash()
                        == microBlockHash.m_stateDeltaHash)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                LOG_GENERAL(WARNING, "cannot find hashes. " << microBlockHash)
                missingMicroBlocks.emplace_back(microBlockHash);
            }
        }
    }

    m_numOfAbsentMicroBlockHashes = 0;
    int offset = 0;

    if (!missingMicroBlocks.empty())
    {
        for (auto const& hash : missingMicroBlocks)
        {
            if (errorMsg.empty())
            {
                errorMsg.resize(sizeof(uint32_t) + sizeof(uint64_t)
                                + hash.size());
                offset += (sizeof(uint32_t) + sizeof(uint64_t));
            }
            else
            {
                errorMsg.resize(offset + hash.size());
            }
            offset = hash.Serialize(errorMsg, offset);

            m_numOfAbsentMicroBlockHashes++;
        }

        if (m_numOfAbsentMicroBlockHashes > 0)
        {
            Serializable::SetNumber<uint32_t>(
                errorMsg, 0, m_numOfAbsentMicroBlockHashes, sizeof(uint32_t));
            Serializable::SetNumber<uint64_t>(errorMsg, sizeof(uint32_t),
                                              m_mediator.m_currentEpochNum,
                                              sizeof(uint64_t));
        }

        // AccountStore::GetInstance().InitTemp();
        // LOG_GENERAL(WARNING, "Got missing microblocks, revert state delta");
        // AccountStore::GetInstance().DeserializeDeltaTemp(
        //     m_mediator.m_ds->m_stateDeltaFromShards, 0);

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::FINALBLOCK_MISSING_HASH);

        return false;
    }

    return true;
}

bool DirectoryService::OnNodeMissingMicroBlocks(
    const std::vector<unsigned char>& errorMsg, const Peer& from)
{
    LOG_MARKER();

    unsigned int offset = 0;

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

    vector<MicroBlockHashSet> missingMicroBlockHashes;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        MicroBlockHashSet mbHash;
        if (mbHash.Deserialize(errorMsg, offset) != 0)
        {
            LOG_GENERAL(WARNING, "Unable to deserialize MicroBlockHashSet");
            return false;
        }
        offset += mbHash.size();

        missingMicroBlockHashes.emplace_back(mbHash);
    }

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock_guard<mutex> g(m_mutexMicroBlocks);

    auto& microBlocks = m_microBlocks[blockNum];

    unsigned int cur_offset = 0;
    vector<unsigned char> mb_message
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    cur_offset += MessageOffset::BODY;
    mb_message.push_back(SUBMITMICROBLOCKTYPE::MISSINGMICROBLOCK);
    cur_offset += MessageOffset::INST;
    Serializable::SetNumber<uint64_t>(mb_message, cur_offset, blockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);
    mb_message.resize(cur_offset + sizeof(uint32_t));
    unsigned int mb_num_offset = cur_offset;
    cur_offset += sizeof(uint32_t);

    uint32_t numOfMicroblocksSent = 0;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        MicroBlock mb;

        bool found = false;
        // O(n^2) might be fine since number of shards is low
        // If its slow on benchmarking, may be first populate an unordered_set and then std::find
        for (const auto& microBlock : microBlocks)
        {
            if (microBlock.GetHeader().GetTxRootHash()
                    == missingMicroBlockHashes[i].m_txRootHash
                && microBlock.GetHeader().GetStateDeltaHash()
                    == missingMicroBlockHashes[i].m_stateDeltaHash)
            {
                mb = microBlock;
                found = true;
                break;
            }
        }
        if (!found)
        {
            LOG_GENERAL(WARNING,
                        "cannot find missing microblock: "
                            << missingMicroBlockHashes[i]);
            continue;
        }
        numOfMicroblocksSent++;
        cur_offset += mb.SerializeCore(mb_message, cur_offset);
    }
    Serializable::SetNumber<uint32_t>(mb_message, mb_num_offset,
                                      numOfMicroblocksSent, sizeof(uint32_t));

    // Final state delta
    if (m_finalBlock->GetHeader().GetStateDeltaHash() != StateHash())
    {
        vector<unsigned char> stateDelta;
        AccountStore::GetInstance().GetSerializedDelta(stateDelta);

        copy(stateDelta.begin(), stateDelta.end(), back_inserter(mb_message));
        cur_offset += stateDelta.size();
    }
    else
    {
        LOG_GENERAL(
            INFO, "State Delta Hash is empty, skip sharing final state delta");
    }

    P2PComm::GetInstance().SendMessage(peer, mb_message);

    return true;
}

// Check microblock hashes root
bool DirectoryService::CheckMicroBlockHashRoot()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckMicroBlockHashRoot not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    LOG_GENERAL(INFO, "Microblock hashes: ")

    for (const auto& i : m_finalBlock->GetMicroBlockHashes())
    {
        LOG_GENERAL(INFO, i);
    }

    TxnHash microBlocksTxnRoot
        = ComputeTransactionsRoot(m_finalBlock->GetMicroBlockHashes());

    StateHash microBlocksDeltaRoot
        = ComputeDeltasRoot(m_finalBlock->GetMicroBlockHashes());

    TxnHash microBlockTranReceiptsRoot
        = ComputeTranReceiptsRoot(m_finalBlock->GetMicroBlockHashes());

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Expected FinalBlock txnRoot : "
                  << microBlocksTxnRoot.hex() << endl
                  << "stateDeltaRoot : " << microBlocksDeltaRoot.hex() << endl
                  << "tranReceiptRoot : " << microBlockTranReceiptsRoot.hex());

    if (m_finalBlock->GetHeader().GetTxRootHash() != microBlocksTxnRoot
        || m_finalBlock->GetHeader().GetDeltaRootHash() != microBlocksDeltaRoot
        || m_finalBlock->GetHeader().GetTranReceiptRootHash()
            != microBlockTranReceiptsRoot)
    {
        LOG_GENERAL(WARNING,
                    "Microblock root hash in proposed final block by "
                    "leader is incorrect");

        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Received FinalBlock txnRoot : "
                << m_finalBlock->GetHeader().GetTxRootHash().hex() << endl
                << "stateDeltaRoot : "
                << m_finalBlock->GetHeader().GetDeltaRootHash().hex() << endl
                << "tranReceiptRoot : "
                << m_finalBlock->GetHeader().GetTranReceiptRootHash().hex());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::FINALBLOCK_INVALID_MICROBLOCK_ROOT_HASH);

        return false;
    }

    return true;
}

bool DirectoryService::CheckIsMicroBlockEmpty()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckIsMicroBlockEmpty not expected to "
                    "be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    auto& hashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();

    for (unsigned int i = 0; i < hashesInMicroBlocks.size(); i++)
    {
        LOG_GENERAL(INFO,
                    "Microblock"
                        << i << ";" << hashesInMicroBlocks[i]
                        << ";IsMicroBlockEmpty:"
                        << m_finalBlock->GetIsMicroBlockEmpty().size());
        auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
        for (auto& microBlock : microBlocks)
        {
            LOG_GENERAL(INFO,
                        "Checking " << microBlock.GetHeader().GetTxRootHash());
            if (microBlock.GetHeader().GetTxRootHash()
                == hashesInMicroBlocks[i].m_txRootHash)
            {
                if (m_finalBlock->GetIsMicroBlockEmpty()[i]
                    != (microBlock.GetHeader().GetNumTxs() == 0))

                {
                    LOG_GENERAL(WARNING,
                                "IsMicroBlockEmpty in proposed final "
                                "block is incorrect "
                                    << i << " Expected: "
                                    << (microBlock.GetHeader().GetNumTxs() == 0)
                                    << " Received: "
                                    << m_finalBlock->GetIsMicroBlockEmpty()[i]);

                    m_consensusObject->SetConsensusErrorCode(
                        ConsensusCommon::FINALBLOCK_MICROBLOCK_EMPTY_ERROR);

                    return false;
                }
                break;
            }
        }
    }

    return true;
}

// Check state root
bool DirectoryService::CheckStateRoot()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckStateRoot not expected to be "
                    "called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    StateHash stateRoot = StateHash();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        // AccountStore::GetInstance().PrintAccountState();
        stateRoot = AccountStore::GetInstance().GetStateRootHash();
    }

    if (stateRoot != m_finalBlock->GetHeader().GetStateRootHash())
    {
        LOG_GENERAL(WARNING,
                    "State root doesn't match. Expected = "
                        << stateRoot << ". "
                        << "Received = "
                        << m_finalBlock->GetHeader().GetStateRootHash());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_FINALBLOCK_STATE_ROOT);

        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State root matched "
                  << m_finalBlock->GetHeader().GetStateRootHash());

    return true;
}

bool DirectoryService::CheckStateDeltaHash()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckStateDeltaHash not expected to be "
                    "called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    StateHash stateRootHash = AccountStore::GetInstance().GetStateDeltaHash();

    if (stateRootHash != m_finalBlock->GetHeader().GetStateDeltaHash())
    {
        LOG_GENERAL(WARNING,
                    "State delta hash doesn't match. Expected = "
                        << stateRootHash << ". "
                        << "Received = "
                        << m_finalBlock->GetHeader().GetStateDeltaHash());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_FINALBLOCK_STATE_DELTA_HASH);

        return false;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State delta hash matched "
                  << m_finalBlock->GetHeader().GetStateDeltaHash());

    return true;
}

bool DirectoryService::CheckFinalBlockValidity(vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::CheckFinalBlockValidity not expected to "
                    "be called from LookUp node.");
        return true;
    }

    if (!CheckBlockTypeIsFinal() || !CheckFinalBlockVersion()
        || !CheckFinalBlockNumber() || !CheckPreviousFinalBlockHash()
        || !CheckFinalBlockTimestamp() || !CheckMicroBlockHashes(errorMsg)
        || !CheckMicroBlockHashRoot() || !CheckIsMicroBlockEmpty()
        || !CheckStateRoot() || !CheckStateDeltaHash())
    {
        Serializable::SetNumber<uint32_t>(
            errorMsg, errorMsg.size(), m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        return false;
    }

    // TODO: Check gas limit (must satisfy some equations)
    // TODO: Check gas used (must be <= gas limit)
    // TODO: Check pubkey (must be valid and = shard leader)
    // TODO: Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
    // TODO: Check parent DS block number (must be = block number of last DS block header in the DS blockchain)

    return true;
}

/** To remove. Redundant code. 

bool DirectoryService::WaitForTxnBodies()
{
    LOG_MARKER();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    {
        unique_lock<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks,
                             defer_lock);
        unique_lock<mutex> g2(m_mediator.m_node->m_mutexAllMicroBlocksRecvd,
                              defer_lock);
        lock(g, g2);

        if (isVacuousEpoch && !m_mediator.m_node->m_allMicroBlocksRecvd)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Waiting for microblocks before verifying final block. Count: "
                    << m_mediator.m_node->m_unavailableMicroBlocks.size());
            for (auto it : m_mediator.m_node->m_unavailableMicroBlocks)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for finalblock " << it.first << ". Count "
                                                    << it.second.size());
                for (auto it2 : it.second)
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              it2.first);
                }
            }

            m_mediator.m_node->m_cvAllMicroBlocksRecvd.wait(
                g, [this] { return m_mediator.m_node->m_allMicroBlocksRecvd; });
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "All microblocks recvd, moving to verify final block");
        }
    }

    return true;
}
**/

// void DirectoryService::LoadUnavailableMicroBlocks()
// {
//     LOG_MARKER();

//     auto blockNum = m_finalBlock->GetHeader().GetBlockNum();
//     auto& hashesInMicroBlocks = m_finalBlock->GetMicroBlockHashes();
//     lock_guard<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks);
//     for (auto& microBlockHash : hashesInMicroBlocks)
//     {
//         for (auto& microBlock : m_microBlocks)
//         {
//             if (microBlock.GetHeader().GetTxRootHash()
//                     == microBlockHash.m_txRootHash
//                 && microBlock.GetHeader().GetStateDeltaHash()
//                     == microBlockHash.m_stateDeltaHash
//                 && (microBlock.GetHeader().GetNumTxs() > 0
//                     || microBlock.GetHeader().GetStateDeltaHash()
//                         != StateHash()))
//             {
//                 // bool b = microBlock.GetHeader().GetNumTxs() > 0;
//                 m_mediator.m_node->m_unavailableMicroBlocks[blockNum].insert(
//                     // {microBlockHash, {b, true}});
//                     {{microBlockHash, microBlock.GetHeader().GetShardID()},
//                      {false, true}});
//                 break;
//             }
//         }
//     }

//     // if (m_mediator.m_node->m_unavailableMicroBlocks.find(blockNum)
//     //         != m_mediator.m_node->m_unavailableMicroBlocks.end()
//     //     && m_mediator.m_node->m_unavailableMicroBlocks[blockNum].size() > 0)
//     // {
//     //     LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
//     //               "setting false for unavailable microblock " << m_consensusID);
//     //     unique_lock<mutex> g(m_mediator.m_node->m_mutexAllMicroBlocksRecvd);
//     //     m_mediator.m_node->m_allMicroBlocksRecvd = false;
//     // }
// }

bool DirectoryService::FinalBlockValidator(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] vector<unsigned char>& errorMsg, const uint32_t consensusID,
    const vector<unsigned char>& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, vector<unsigned char>& messageToCosign)
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::FinalBlockValidator not expected to be "
                    "called from LookUp node.");
        return true;
    }

    m_finalBlock.reset(new TxBlock);

    if (!Messenger::GetDSFinalBlockAnnouncement(message, offset, consensusID,
                                                blockHash, leaderID, leaderKey,
                                                *m_finalBlock, messageToCosign))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Messenger::GetDSFinalBlockAnnouncement failed.");
        return false;
    }

    // WaitForTxnBodies();

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (isVacuousEpoch)
    {
        AccountStore::GetInstance().UpdateStateTrieAll();
    }

    if (!CheckFinalBlockValidity(errorMsg))
    {
        LOG_GENERAL(WARNING,
                    "To-do: What to do if proposed finalblock is not valid?");
        // throw exception();
        // TODO: microblock is invalid
        return false;
    }

    // if (!isVacuousEpoch)
    // {
    //     LoadUnavailableMicroBlocks();
    // }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Final block "
                  << m_finalBlock->GetHeader().GetBlockNum()
                  << " received with prevhash 0x"
                  << DataConversion::charArrToHexStr(
                         m_finalBlock->GetHeader().GetPrevHash().asArray()));

    return true;
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSBackup()
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnFinalBlockWhenDSBackup "
                    "not expected to be called from LookUp node.");
        return true;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a backup DS node. Waiting for final block announcement.");

    // Create new consensus object

    // Dummy values for now
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);

    auto func
        = [this](const vector<unsigned char>& input, unsigned int offset,
                 vector<unsigned char>& errorMsg, const uint32_t consensusID,
                 const vector<unsigned char>& blockHash,
                 const uint16_t leaderID, const PubKey& leaderKey,
                 vector<unsigned char>& messageToCosign) mutable -> bool {
        return FinalBlockValidator(input, offset, errorMsg, consensusID,
                                   blockHash, leaderID, leaderKey,
                                   messageToCosign);
    };

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first,
        *m_mediator.m_DSCommittee, static_cast<unsigned char>(DIRECTORY),
        static_cast<unsigned char>(FINALBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

void DirectoryService::RunConsensusOnFinalBlock(bool revertStateDelta)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::RunConsensusOnFinalBlock not expected "
                    "to be called from LookUp node.");
        return;
    }

    LOG_MARKER();

    SetState(FINALBLOCK_CONSENSUS_PREP);

    if (revertStateDelta)
    {
        LOG_GENERAL(WARNING,
                    "Failed DS microblock consensus, revert state delta");
        AccountStore::GetInstance().InitTemp();
        AccountStore::GetInstance().DeserializeDeltaTemp(m_stateDeltaFromShards,
                                                         0);
    }

    AccountStore::GetInstance().SerializeDelta();

    // Upon consensus object creation failure, one should not return from the function, but rather wait for view change.
    bool ConsensusObjCreation = true;
    if (m_mode == PRIMARY_DS)
    {
        ConsensusObjCreation = RunConsensusOnFinalBlockWhenDSPrimary();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Consensus failed at "
                        "RunConsensusOnFinalBlockWhenDSPrimary");
        }
    }
    else
    {
        ConsensusObjCreation = RunConsensusOnFinalBlockWhenDSBackup();
        if (!ConsensusObjCreation)
        {
            LOG_GENERAL(WARNING,
                        "Consensus failed at "
                        "RunConsensusOnFinalBlockWhenDSBackup");
        }
    }

    if (ConsensusObjCreation)
    {
        SetState(FINALBLOCK_CONSENSUS);
    }

    auto func1 = [this]() -> void { CommitFinalBlockConsensusBuffer(); };

    DetachedFunction(1, func1);

    // View change will wait for timeout. If conditional variable is notified before timeout, the thread will return
    // without triggering view change.
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeFinalBlock);
    if (cv_viewChangeFinalBlock.wait_for(cv_lk,
                                         std::chrono::seconds(VIEWCHANGE_TIME))
        == std::cv_status::timeout)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Initiated final block view change. ");
        auto func = [this]() -> void { RunConsensusOnViewChange(); };
        DetachedFunction(1, func);
    }
}
