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
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
bool DirectoryService::VerifyPOW2(const vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(uint256_t) + sizeof(uint32_t)
                                       + PUB_KEY_SIZE + sizeof(uint64_t)
                                       + BLOCK_HASH_SIZE + BLOCK_HASH_SIZE))
    {
        return false;
    }

    unsigned int curr_offset = offset;

    // 32-byte block number
    uint256_t DSBlockNum = Serializable::GetNumber<uint256_t>(
        message, curr_offset, sizeof(uint256_t));
    curr_offset += sizeof(uint256_t);

    // Check block number
    if (!CheckWhetherDSBlockIsFresh(DSBlockNum + 1))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, curr_offset,
                                                        sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    // 33-byte public key
    // PubKey key(message, curr_offset);
    PubKey key;
    if (key.Deserialize(message, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
        return false;
    }
    curr_offset += PUB_KEY_SIZE;

    // To-do: Reject PoW2 submissions from existing members of DS committee

    // 8-byte nonce
    uint64_t nonce = Serializable::GetNumber<uint64_t>(message, curr_offset,
                                                       sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    // 32-byte resulting hash
    string winning_hash = DataConversion::Uint8VecToHexStr(message, curr_offset,
                                                           BLOCK_HASH_SIZE);
    curr_offset += BLOCK_HASH_SIZE;

    // 32-byte mixhash
    string winning_mixhash = DataConversion::Uint8VecToHexStr(
        message, curr_offset, BLOCK_HASH_SIZE);

    m_mediator.UpdateDSBlockRand();

    // Log all values
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Public_key             = 0x"
                  << DataConversion::SerializableToHexStr(key));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning IP                = " << peer.GetPrintableIPAddress()
                                             << ":" << portNo);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsb size               = "
                  << m_mediator.m_dsBlockChain.GetBlockCount())

    // Define the PoW2 parameters
    array<unsigned char, UINT256_SIZE> rand1, rand2;
    unsigned int difficulty
        = POW2_DIFFICULTY; //TODO: Get this value dynamically

    rand1 = m_mediator.m_dsBlockRand;
    rand2.fill(0);

    // Verify nonce
    uint256_t block_num = m_mediator.m_txBlockChain.GetBlockCount();

#ifdef STAT_TEST
    m_timespec = r_timer_start();
#endif // STAT_TEST

    lock(m_mutexAllPOW2, m_mutexAllPoWConns);
    lock_guard<mutex> g(m_mutexAllPOW2, adopt_lock);
    lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

    // if ((m_state != POW2_SUBMISSION) && (m_state != SHARDING_CONSENSUS_PREP))
    if (!CheckState(VERIFYPOW2))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Too late - current state is "
                      << m_state
                      << ". Don't verify cause I got other work to do. "
                         "Assume true as it has no impact.");

        // TODO: This need to be changed.
        m_allPoWConns.insert(make_pair(key, peer));
        return true;
    }

    bool result = POW::GetInstance().PoWVerify(block_num, difficulty, rand1,
                                               rand2, ipAddr, key, false, nonce,
                                               winning_hash, winning_mixhash);

#ifdef STAT_TEST
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[POWSTAT] pow 2 verify (microsec): " << r_timer_end(m_timespec));
#endif // STAT_TEST

    if (result == true)
    {
        // Do another check on the state before accessing m_allPoWs
        // Accept slightly late entries as the primary DS might have received some of those entries and have those in his proposed shards
        // if ((m_state != POW2_SUBMISSION) && (m_state != SHARDING_CONSENSUS_PREP))
        if (!CheckState(VERIFYPOW2))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Too late - current state is " << m_state);
        }
        else
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "POW2 verification passed");
            //lock(m_mutexAllPOW2, m_mutexAllPoWConns);
            //lock_guard<mutex> g(m_mutexAllPOW2, adopt_lock);
            //lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);
            m_allPoW2s.insert(make_pair(key, nonce));
            m_allPoWConns.insert(make_pair(key, peer));
        }
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Invalid PoW2 submission");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "blockNum: " << block_num << " Difficulty: " << difficulty
                               << " nonce: " << nonce
                               << " ip: " << peer.GetPrintableIPAddress() << ":"
                               << portNo);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "rand1: " << DataConversion::charArrToHexStr(rand1)
                            << " rand2: "
                            << DataConversion::charArrToHexStr(rand2));
    }
    return result;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessPoW2Submission(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [32-byte block num] [4-byte listening port] [33-byte public key] [8-byte nonce] [32-byte resulting hash] [32-byte mixhash]
    LOG_MARKER();
    // shared_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);
    unsigned int sleep_time_while_waiting = 100;
    if (m_state == DSBLOCK_CONSENSUS
        || (m_state != POW2_SUBMISSION && m_mode == Mode::IDLE
            && m_mediator.m_node->m_state == Node::POW2_SUBMISSION))
    {
        for (unsigned int i = 0; i < POW_SUB_BUFFER_TIME; i++)
        {
            if (m_state == POW2_SUBMISSION)
            {
                break;
            }
            if (i % 100 == 0)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for POW2_SUBMISSION state before "
                          "processing. Current state is "
                              << m_state);
            }

            // Magic number for now.
            // Basically, I want to wait for awhile for the dsblock to arrive before
            // I request for one.
            if (!m_requesting_last_ds_block and i == 300)
            {
                m_requesting_last_ds_block = true;
                LastDSBlockRequest();
            }
            this_thread::sleep_for(
                chrono::milliseconds(sleep_time_while_waiting));
        }
    }

    if (!CheckState(PROCESS_POW2SUBMISSION))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not at POW2_SUBMISSION. Current state is " << m_state);
        return false;
    }

    bool result = VerifyPOW2(message, offset, from);
    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}