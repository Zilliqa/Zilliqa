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
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
bool Node::StartPoW1(const uint256_t& block_num, uint8_t difficulty,
                     const array<unsigned char, UINT256_SIZE>& rand1,
                     const array<unsigned char, UINT256_SIZE>& rand2)
{
    LOG_MARKER();
    // if (m_state == POW1_SUBMISSION)
    if (!CheckState(STARTPOW1))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in POW1_SUBMISSION state");
        return false;
    }

    // SetState(POW1_SUBMISSION);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Current dsblock is " << block_num);
    //POW POWClient;
    ethash_mining_result winning_result = POW::GetInstance().PoWMine(
        block_num, difficulty, rand1, rand2, m_mediator.m_selfPeer.m_ipAddress,
        m_mediator.m_selfKey.second, true);

    if (winning_result.success)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Winning nonce   = 0x" << hex
                                         << winning_result.winning_nonce);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Winning result  = 0x" << hex << winning_result.result);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Winning mixhash = 0x" << hex << winning_result.mix_hash);
        vector<unsigned char> result_vec
            = DataConversion::HexStrToUint8Vec(winning_result.result);
        vector<unsigned char> mixhash_vec
            = DataConversion::HexStrToUint8Vec(winning_result.mix_hash);

        // Send PoW1 result
        // Message = [32-byte block number] [4-byte listening port] [33-byte public key]
        // [8-byte nonce] [32-byte resulting hash] [32-byte mixhash] [64-byte Signature]
        vector<unsigned char> pow1message
            = {MessageType::DIRECTORY, DSInstructionType::POW1SUBMISSION};
        unsigned int cur_offset = MessageOffset::BODY;

        Serializable::SetNumber<uint256_t>(pow1message, cur_offset, block_num,
                                           UINT256_SIZE);
        cur_offset += UINT256_SIZE;

        Serializable::SetNumber<uint32_t>(
            pow1message, cur_offset, m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        m_mediator.m_selfKey.second.Serialize(pow1message, cur_offset);
        cur_offset += PUB_KEY_SIZE;

        Serializable::SetNumber<uint64_t>(pow1message, cur_offset,
                                          winning_result.winning_nonce,
                                          sizeof(uint64_t));
        cur_offset += sizeof(uint64_t);

        pow1message.insert(pow1message.end(), result_vec.begin(),
                           result_vec.end());
        cur_offset += BLOCK_HASH_SIZE;
        pow1message.insert(pow1message.end(), mixhash_vec.begin(),
                           mixhash_vec.end());
        cur_offset += BLOCK_HASH_SIZE;

        Signature sign;
        if (!Schnorr::GetInstance().Sign(pow1message,
                                         m_mediator.m_selfKey.first,
                                         m_mediator.m_selfKey.second, sign))
        {
            LOG_GENERAL(WARNING, "Failed to sign PoW1");
        }
        sign.Serialize(pow1message, cur_offset);

        P2PComm::GetInstance().SendMessage(m_mediator.m_DSCommitteeNetworkInfo,
                                           pow1message);
    }

    SetState(POW2_SUBMISSION);
    return true;
}

bool Node::ReadVariablesFromStartPoW1Message(
    const vector<unsigned char>& message, unsigned int cur_offset,
    uint256_t& block_num, uint8_t& difficulty, array<unsigned char, 32>& rand1,
    array<unsigned char, 32>& rand2)
{
    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   UINT256_SIZE + sizeof(uint8_t) + UINT256_SIZE
                                       + UINT256_SIZE,
                                   PUB_KEY_SIZE + IP_SIZE + PORT_SIZE))
    {
        return false;
    }

    // 32-byte block num
    block_num
        = Serializable::GetNumber<uint256_t>(message, cur_offset, UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    // 1-byte difficulty
    difficulty = Serializable::GetNumber<uint8_t>(message, cur_offset,
                                                  sizeof(uint8_t));
    cur_offset += sizeof(uint8_t);

    // 32-byte rand1
    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + UINT256_SIZE, rand1.begin());
    cur_offset += UINT256_SIZE;

    // 32-byte rand2
    copy(message.begin() + cur_offset,
         message.begin() + cur_offset + UINT256_SIZE, rand2.begin());
    cur_offset += UINT256_SIZE;
    LOG_STATE("[START][EPOCH][" << std::setw(15) << std::left
                                << m_mediator.m_selfPeer.GetPrintableIPAddress()
                                << "][" << block_num << "]");

    // Log all values
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "My IP address     = " << m_mediator.m_selfPeer.GetPrintableIPAddress());
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "My Listening Port = " << m_mediator.m_selfPeer.m_listenPortHost);
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Difficulty        = " << to_string(difficulty));
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Rand1             = " << DataConversion::charArrToHexStr(rand1));
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Rand2             = " << DataConversion::charArrToHexStr(rand2));
    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Pubkey            = " << DataConversion::SerializableToHexStr(m_mediator.m_selfKey.second));

    // DS nodes ip addr and port
    const unsigned int numDS
        = (message.size() - cur_offset) / (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE);

    // Create and keep a view of the DS committee
    // We'll need this if we win PoW1
    m_mediator.m_DSCommitteeNetworkInfo.clear();
    m_mediator.m_DSCommitteePubKeys.clear();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DS nodes count    = " << numDS);
    for (unsigned int i = 0; i < numDS; i++)
    {
        m_mediator.m_DSCommitteePubKeys.push_back(PubKey(message, cur_offset));
        cur_offset += PUB_KEY_SIZE;

        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            Peer(message, cur_offset));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DS Node IP: " << m_mediator.m_DSCommitteeNetworkInfo.back()
                                        .GetPrintableIPAddress()
                                 << " Port: "
                                 << m_mediator.m_DSCommitteeNetworkInfo.back()
                                        .m_listenPortHost);
        cur_offset += IP_SIZE + PORT_SIZE;
    }

    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessStartPoW1(const vector<unsigned char>& message,
                            unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Note: This function should only be invoked on a new node that was not part of the sharding committees in previous epoch
    // Message = [32-byte block num] [1-byte difficulty] [32-byte rand1] [32-byte rand2] [33-byte pubkey] [16-byte ip] [4-byte port] ... (all the DS nodes)

    LOG_MARKER();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "START OF EPOCH " << m_mediator.m_dsBlockChain.GetBlockCount());

    uint256_t block_num;
    uint8_t difficulty;
    array<unsigned char, 32> rand1;
    array<unsigned char, 32> rand2;

    if (!ReadVariablesFromStartPoW1Message(message, offset, block_num,
                                           difficulty, rand1, rand2))
    {
        return false;
    }

    if (m_mediator.m_isRetrievedHistory)
    {
        block_num = m_mediator.m_dsBlockChain.GetBlockCount();
        difficulty = POW1_DIFFICULTY;
        rand1 = m_mediator.m_dsBlockRand;
        rand2 = m_mediator.m_txBlockRand;
    }

    // Start mining
    StartPoW1(block_num, difficulty, rand1, rand2);
#endif // IS_LOOKUP_NODE

    return true;
}