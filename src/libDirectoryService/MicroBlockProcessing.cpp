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
#include <thread>
#include <chrono>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "depends/libDatabase/MemoryDB.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

bool DirectoryService::ProcessMicroblockSubmission(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [32-byte DS blocknum] [4-byte consensusid] [4-byte shard ID] [Tx microblock]

    LOG_MARKER();

    // if (m_state != MICROBLOCK_SUBMISSION)
    if (!CheckState(PROCESS_MICROBLOCKSUBMISSION))
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Not at MICROBLOCK_SUBMISSION. Current state is " << m_state);
        return false;
    }

    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint256_t) + sizeof(uint32_t) +
            sizeof(uint32_t) + TxBlock::GetMinSize()))
    {
        return false;
    }

    unsigned int curr_offset = offset;

    // 32-byte block number
    uint256_t DSBlockNum = Serializable::GetNumber<uint256_t>(message, curr_offset, sizeof(uint256_t));
    curr_offset += sizeof(uint256_t);

    // Check block number
    if (!CheckWhetherDSBlockIsFresh(DSBlockNum + 1))
    {
        return false;
    }

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(message, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    if (consensusID != m_consensusID)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Consensus ID is not correct. Expected ID: " << consensusID <<
                     " My Consensus ID: " << m_consensusID);
        return false;
    }

    // 4-byte shard ID
    uint32_t shardId = Serializable::GetNumber<uint32_t>(message, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "shard_id " << shardId); 

    // Tx microblock
    MicroBlock microBlock(message, curr_offset);

    const PubKey & pubKey = microBlock.GetHeader().GetMinerPubKey();

    // Check public key - shard ID mapping
    const auto & minerEntry = m_publicKeyToShardIdMap.find(pubKey);
    if (minerEntry == m_publicKeyToShardIdMap.end())
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: Cannot find the miner key: " << 
                     DataConversion::SerializableToHexStr(pubKey));
        return false;
    }
    if (minerEntry->second != shardId)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: Microblock shard ID mismatch");
        return false;
    }

    // [TODO] verify the co-signature, if pass

    lock_guard<mutex> g(m_mutexMicroBlocks);
    m_microBlocks.insert(microBlock);

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), m_microBlocks.size() << " of " << 
                 m_shards.size() << " microblocks received");

    if (m_microBlocks.size() == m_shards.size())
    {
#ifdef STAT_TEST
        if (m_mode == PRIMARY_DS)
        {
            LOG_STATE("[MICRO][" << std::setw(15) << std::left << 
                      m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" <<
                      m_mediator.m_txBlockChain.GetBlockCount() << "] LAST");
        }
#endif // STAT_TEST
        for (auto & microBlock : m_microBlocks)
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Timestamp: " <<
                         microBlock.GetHeader().GetTimestamp());
        }

        cv_scheduleFinalBlockConsensus.notify_all();
        RunConsensusOnFinalBlock();
    }
#ifdef STAT_TEST
    else if ((m_microBlocks.size() == 1) && (m_mode == PRIMARY_DS))
    {
        LOG_STATE("[MICRO][" << std::setw(15) << std::left << 
                  m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" << 
                  m_mediator.m_txBlockChain.GetBlockCount() << "] FRST");
    }
#endif // STAT_TEST

    // TODO: Re-request from shard leader if microblock is not received after a certain time. 
#endif // IS_LOOKUP_NODE
    return true;
}