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
#include "libNetwork/Whitelist.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

bool DirectoryService::ProcessPoWSubmission(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] const Peer& from)
{
    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "DirectoryService::ProcessPoWSubmission not expected to be "
                    "called from LookUp node.");
        return true;
    }

    if (m_state == FINALBLOCK_CONSENSUS)
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVPOWSubmission);

        if (cv_POWSubmission.wait_for(
                cv_lk, std::chrono::seconds(POW_SUBMISSION_TIMEOUT))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Time out while waiting for state transition ");
        }

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "State transition is completed. (check for timeout)");
    }

    if (!CheckState(PROCESS_POWSUBMISSION))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not at POW_SUBMISSION. Current state is " << m_state);
        return false;
    }

    uint64_t blockNumber = 0;
    Peer submitterPeer;
    PubKey submitterPubKey;
    uint64_t nonce = 0;
    string resultingHash;
    string mixHash;
    Signature signature;

    if (!Messenger::GetDSPoWSubmission(message, offset, blockNumber,
                                       submitterPeer, submitterPubKey, nonce,
                                       resultingHash, mixHash, signature))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Messenger::GetDSPoWSubmission failed.");
        return false;
    }

    // Check block number
    if (!CheckWhetherDSBlockIsFresh(blockNumber))
    {
        return false;
    }

    if (TEST_NET_MODE
        && not Whitelist::GetInstance().IsNodeInDSWhiteList(submitterPeer,
                                                            submitterPubKey))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Submitted PoW but node is not in DS whitelist. Hence, "
                  "not accepted!");
    }

    // Todo: Reject PoW submissions from existing members of DS committee

    if (!CheckState(VERIFYPOW))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Too late - current state is "
                      << m_state
                      << ". Don't verify cause I have other work to do. "
                         "Assume true as it has no impact.");
        return true;
    }

    if (!Whitelist::GetInstance().IsValidIP(submitterPeer.m_ipAddress))
    {
        LOG_GENERAL(WARNING,
                    "IP belong to private ip subnet or is a broadcast address");
        return false;
    }

    // Log all values
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winner Public_key             = 0x"
                  << DataConversion::SerializableToHexStr(submitterPubKey));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winner Peer ip addr           = " << submitterPeer);

    // Define the PoW parameters
    array<unsigned char, 32> rand1 = m_mediator.m_dsBlockRand;
    array<unsigned char, 32> rand2 = m_mediator.m_txBlockRand;

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock_num            = " << blockNumber);

    unsigned int difficulty = POW_DIFFICULTY;
    if (blockNumber > 1)
    {
        difficulty
            = m_mediator.m_dsBlockChain.GetLastBlock()
                  .GetHeader()
                  .GetDifficulty(); // TODO: Need to get the latest blocknum, diff, rand1, rand2
    }

    m_timespec = r_timer_start();

    bool result = POW::GetInstance().PoWVerify(
        blockNumber, difficulty, rand1, rand2, submitterPeer.m_ipAddress,
        submitterPubKey, false, nonce, resultingHash, mixHash);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "[POWSTAT] pow verify (microsec): " << r_timer_end(m_timespec));

    if (result == true)
    {
        // Do another check on the state before accessing m_allPoWs
        // Accept slightly late entries as we need to multicast the DSBLOCK to everyone
        // if ((m_state != POW_SUBMISSION) && (m_state != DSBLOCK_CONSENSUS_PREP))
        if (!CheckState(VERIFYPOW))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Too late - current state is " << m_state);
        }
        else
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "POW verification passed");
            lock(m_mutexAllPOW, m_mutexAllPoWConns);
            lock_guard<mutex> g(m_mutexAllPOW, adopt_lock);
            lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

            m_allPoWConns.emplace(submitterPubKey, submitterPeer);
            m_allPoWs.emplace_back(submitterPubKey, nonce);
        }
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Invalid PoW submission"
                      << "\n"
                      << "blockNum: " << blockNumber
                      << " Difficulty: " << difficulty << " nonce: " << nonce
                      << " ip: " << submitterPeer
                      << " rand1: " << DataConversion::charArrToHexStr(rand1)
                      << " rand2: " << DataConversion::charArrToHexStr(rand2));
    }

    return result;
}
