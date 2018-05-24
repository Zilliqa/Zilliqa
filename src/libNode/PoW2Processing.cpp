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
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void Node::SharePoW2WinningResultWithDS(
    const uint256_t& block_num,
    const ethash_mining_result& winning_result) const
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning nonce   = " << winning_result.winning_nonce);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning result  = " << winning_result.result);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning mixhash = " << winning_result.mix_hash);

    // Send result
    vector<unsigned char> pow2message
        = {MessageType::DIRECTORY, DSInstructionType::POW2SUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(pow2message, cur_offset, block_num,
                                       sizeof(uint256_t));
    cur_offset += sizeof(uint256_t);

    Serializable::SetNumber<uint32_t>(pow2message, cur_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    m_mediator.m_selfKey.second.Serialize(pow2message, cur_offset);
    cur_offset += PUB_KEY_SIZE;

    Serializable::SetNumber<uint64_t>(pow2message, cur_offset,
                                      winning_result.winning_nonce,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    vector<unsigned char> result_vec
        = DataConversion::HexStrToUint8Vec(winning_result.result);
    pow2message.insert(pow2message.end(), result_vec.begin(), result_vec.end());

    vector<unsigned char> mixhash_vec
        = DataConversion::HexStrToUint8Vec(winning_result.mix_hash);
    pow2message.insert(pow2message.end(), mixhash_vec.begin(),
                       mixhash_vec.end());

    P2PComm::GetInstance().SendMessage(m_mediator.m_DSCommitteeNetworkInfo,
                                       pow2message);
}

void Node::StartPoW2MiningAndShareResultWithDS(
    const uint256_t& block_num, uint8_t difficulty,
    const array<unsigned char, 32>& rand1,
    const array<unsigned char, 32>& rand2) const
{
    LOG_MARKER();

    ethash_mining_result winning_result = POW::GetInstance().PoWMine(
        block_num, difficulty, rand1, rand2, m_mediator.m_selfPeer.m_ipAddress,
        m_mediator.m_selfKey.second, false);

    if (winning_result.success)
    {
        SharePoW2WinningResultWithDS(block_num, winning_result);
    }
}

bool Node::StartPoW2(uint256_t block_num, uint8_t difficulty,
                     array<unsigned char, 32> rand1,
                     array<unsigned char, 32> rand2)
{
    // Message = [32-byte block num] [1-byte difficulty] [32-byte rand1] [32-byte rand2] [16-byte ip] [4-byte port] ... (all the DS nodes)

    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "blockNum: " << block_num << " Difficulty: " << difficulty);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "rand1: " << DataConversion::charArrToHexStr(rand1) << " rand2: "
                        << DataConversion::charArrToHexStr(rand2));

    StartPoW2MiningAndShareResultWithDS(block_num, difficulty, rand1, rand2);

    SetState(TX_SUBMISSION);
    return true;
}
#endif // IS_LOOKUP_NODE