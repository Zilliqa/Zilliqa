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
bool DirectoryService::CheckWhetherMaxSubmissionsReceived(Peer peer, PubKey key)
{
    lock(m_mutexAllPOW1, m_mutexAllPoWConns);
    lock_guard<mutex> g(m_mutexAllPOW1, adopt_lock);
    lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

    if (m_allPoW1s.size() >= MAX_POW1_WINNERS)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Already validated maximum number of PoW1 submissions - "
                  "dropping this submission but noting down the IP of "
                  "submitter");
        m_allPoWConns.insert(make_pair(key, peer));
        return true;
    }

    return false;
}

bool DirectoryService::VerifyPoW1Submission(
    const vector<unsigned char>& message, const Peer& from, PubKey& key,
    unsigned int curr_offset, uint32_t& portNo, uint64_t& nonce,
    array<unsigned char, 32>& rand1, array<unsigned char, 32>& rand2,
    unsigned int& difficulty, uint256_t& block_num)
{
    // 8-byte nonce
    nonce = Serializable::GetNumber<uint64_t>(message, curr_offset,
                                              sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    // 32-byte resulting hash
    string winning_hash = DataConversion::Uint8VecToHexStr(message, curr_offset,
                                                           BLOCK_HASH_SIZE);
    curr_offset += BLOCK_HASH_SIZE;

    // 32-byte mixhash
    string winning_mixhash = DataConversion::Uint8VecToHexStr(
        message, curr_offset, BLOCK_HASH_SIZE);

    // Log all values
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winner Public_key             = 0x"
                  << DataConversion::SerializableToHexStr(key));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winner Peer ip addr           = " << from.GetPrintableIPAddress()
                                                 << ":" << portNo);

    // Define the PoW1 parameters
    rand1 = m_mediator.m_dsBlockRand;
    rand2 = m_mediator.m_txBlockRand;

    difficulty
        = POW1_DIFFICULTY; // TODO: Need to get the latest blocknum, diff, rand1, rand2
    // Verify nonce
    block_num = m_mediator.m_dsBlockChain.GetBlockCount();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock_num            = " << block_num);

#ifdef STAT_TEST
    m_timespec = r_timer_start();
#endif // STAT_TEST

    bool result = POW::GetInstance().PoWVerify(
        block_num, difficulty, rand1, rand2, from.m_ipAddress, key, false,
        nonce, winning_hash, winning_mixhash);

#ifdef STAT_TEST
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[POWSTAT] pow1 verify (microsec): " << r_timer_end(m_timespec));
#endif // STAT_TEST

    return result;
}

bool DirectoryService::ParseMessageAndVerifyPOW1(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    unsigned int curr_offset = offset;

    // 32-byte block number
    uint256_t DSBlockNum = Serializable::GetNumber<uint256_t>(
        message, curr_offset, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    // Check block number
    if (!CheckWhetherDSBlockIsFresh(DSBlockNum))
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

    // Todo: Reject PoW1 submissions from existing members of DS committee

    if (CheckWhetherMaxSubmissionsReceived(peer, key))
    {
        return false;
    }

    if (!CheckState(VERIFYPOW1))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Too late - current state is "
                      << m_state
                      << ". Don't verify cause I have other work to do. "
                         "Assume true as it has no impact.");
        return true;
    }

    uint64_t nonce;
    array<unsigned char, 32> rand1;
    array<unsigned char, 32> rand2;
    unsigned int difficulty;
    uint256_t block_num;
    bool result
        = VerifyPoW1Submission(message, from, key, curr_offset, portNo, nonce,
                               rand1, rand2, difficulty, block_num);

    if (result == true)
    {
        // Do another check on the state before accessing m_allPoWs
        // Accept slightly late entries as we need to multicast the DSBLOCK to everyone
        // if ((m_state != POW1_SUBMISSION) && (m_state != DSBLOCK_CONSENSUS_PREP))
        if (!CheckState(VERIFYPOW1))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Too late - current state is " << m_state);
        }
        else
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "POW1 verification passed");
            lock(m_mutexAllPOW1, m_mutexAllPoWConns);
            lock_guard<mutex> g(m_mutexAllPOW1, adopt_lock);
            lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

            m_allPoWConns.insert(make_pair(key, peer));

            if (m_allPoW1s.size() >= MAX_POW1_WINNERS)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Already validated maximum number of PoW1 "
                          "submissions - dropping this submission but "
                          "noting down the IP of submitter");
                return false;
            }

            m_allPoW1s.push_back(make_pair(key, nonce));
        }
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Invalid PoW1 submission"
                      << "\n"
                      << "blockNum: " << block_num
                      << " Difficulty: " << difficulty << " nonce: " << nonce
                      << " ip: " << peer.GetPrintableIPAddress() << ":"
                      << portNo << "\n"
                      << "rand1: " << DataConversion::charArrToHexStr(rand1)
                      << " rand2: " << DataConversion::charArrToHexStr(rand2));
    }
    return result;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessPoW1Submission(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [32-byte block number] [4-byte listening port] [33-byte public key] [8-byte nonce] [32-byte resulting hash] [32-byte mixhash]
    LOG_MARKER();

    if (m_state == FINALBLOCK_CONSENSUS)
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVPOW1Submission);

        if (cv_POW1Submission.wait_for(
                cv_lk, std::chrono::seconds(POW_SUBMISSION_TIMEOUT))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Time out while waiting for state transition ");
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State transition is completed. (check for timeout)");
    }

    if (!CheckState(PROCESS_POW1SUBMISSION))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not at POW1_SUBMISSION. Current state is " << m_state);
        return false;
    }

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   UINT256_SIZE + sizeof(uint32_t)
                                       + PUB_KEY_SIZE + sizeof(uint64_t)
                                       + BLOCK_HASH_SIZE + BLOCK_HASH_SIZE))
    {
        return false;
    }

    bool result = ParseMessageAndVerifyPOW1(message, offset, from);
    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}
