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
#include "libNetwork/Whitelist.h"
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
    if (IsMessageSizeInappropriate(
            message.size(), offset,
            sizeof(uint64_t) + sizeof(uint32_t) + PUB_KEY_SIZE
                + sizeof(uint64_t) + BLOCK_HASH_SIZE + BLOCK_HASH_SIZE
                + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE))
    {
        LOG_GENERAL(WARNING, "PoW2 size Inappropriate");
        return false;
    }

    unsigned int curr_offset = offset;

    // 8-byte block number
    uint64_t DSBlockNum = Serializable::GetNumber<uint64_t>(
        message, curr_offset, sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

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
    PubKey key;
    if (key.Deserialize(message, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
        return false;
    }

    if (TEST_NET_MODE
        && not Whitelist::GetInstance().IsPubkeyInShardWhiteList(key))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Submitted PoW2 but node is not in shard whitelist. Hence, "
                  "not accepted!");
        return false;
    }

    if (!Whitelist::GetInstance().IsValidIP(peer.m_ipAddress))
    {
        LOG_GENERAL(
            WARNING,
            "IP address belong to private ip subnet or is a broadcast address");
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
    curr_offset += BLOCK_HASH_SIZE;

    //64 byte signature
    Signature sign(message, curr_offset);

    if (!Schnorr::GetInstance().Verify(message, 0, curr_offset, sign, key))
    {
        LOG_GENERAL(WARNING, "PoW2 submission signature wrong");
        return false;
    }

    curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    m_mediator.UpdateDSBlockRand();

    // Log all values
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Public_key             = 0x"
                  << DataConversion::SerializableToHexStr(key));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning IP                = " << peer.GetPrintableIPAddress()
                                             << ":" << portNo);
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "dsb size               = " << m_mediator.m_dsBlockChain.GetLastBlock()
                                           .GetHeader()
                                           .GetBlockNum()
                + 1)

    // Define the PoW2 parameters
    array<unsigned char, UINT256_SIZE> rand1, rand2;
    unsigned int difficulty
        = POW2_DIFFICULTY; //TODO: Get this value dynamically

    rand1 = m_mediator.m_dsBlockRand;
    rand2.fill(0);

    // Verify nonce
    uint64_t block_num
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;

    m_timespec = r_timer_start();

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
        m_allPoWConns.emplace(key, peer);
        return true;
    }

    bool result = POW::GetInstance().PoWVerify(block_num, difficulty, rand1,
                                               rand2, ipAddr, key, false, nonce,
                                               winning_hash, winning_mixhash);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[POWSTAT] pow 2 verify (microsec): " << r_timer_end(m_timespec));

    if (result == true)
    {
        // Do another check on the state before accessing m_allPoWs
        // Accept slightly late entries as the primary DS might have received some of those entries and have those in his proposed shards
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
            m_allPoW2s.emplace(key, nonce);
            m_allPoWConns.emplace(key, peer);
        }
    }
    else
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Invalid PoW2 submission"
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

bool DirectoryService::ProcessPoW2Submission(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [8-byte block num] [4-byte listening port] [33-byte public key] [8-byte nonce] [32-byte resulting hash] [32-byte mixhash]
    //[64-byte signature]
    LOG_MARKER();

    if (m_state == DSBLOCK_CONSENSUS
        || (m_state != POW2_SUBMISSION && m_mode == Mode::IDLE
            && m_mediator.m_node->m_state == Node::POW2_SUBMISSION))
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVPOW2Submission);

        if (cv_POW2Submission.wait_for(
                cv_lk, std::chrono::seconds(POW_SUBMISSION_TIMEOUT))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Time out while waiting for state transition ");
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State transition is completed. (check for timeout)");
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
