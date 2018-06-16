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

#include "wire.h"
#include "common/Messages.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

namespace wire
{

    void ShardInfo::pack(vector<unsigned char>* dst) const
    {
        assert(dst);
        assert(dst->empty());

        const size_t expectedSize = 2 + (3 * sizeof(uint32_t)) + UINT256_SIZE
            + ((PUB_KEY_SIZE + IP_SIZE + PORT_SIZE) * peers.size());
        dst->reserve(expectedSize);

        // Message = [32-byte DS blocknum] [4-byte shard ID] [4-byte committee size] [33-byte public key]
        // [16-byte ip] [4-byte port] ... (all nodes; first entry is leader)
        dst->push_back(MessageType::NODE);
        dst->push_back(NodeInstructionType::SHARDING);

        unsigned int curr_offset = MessageOffset::BODY;

        Serializable::SetNumber<uint256_t>(*dst, curr_offset, blockNumber,
                                           UINT256_SIZE);
        curr_offset += UINT256_SIZE;

        // 4-byte shard ID - get from the leader's info in m_publicKeyToShardIdMap
        Serializable::SetNumber<uint32_t>(*dst, curr_offset, shardID,
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        // 4-byte number of shards
        Serializable::SetNumber<uint32_t>(*dst, curr_offset, numShards,
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        // 4-byte committee size
        Serializable::SetNumber<uint32_t>(*dst, curr_offset, peers.size(),
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        for (const auto& peer : peers)
        {
            // 33-byte public key
            peer.first.Serialize(*dst, curr_offset);
            curr_offset += PUB_KEY_SIZE;

            // 16-byte ip + 4-byte port
            peer.second.Serialize(*dst, curr_offset);
            curr_offset += IP_SIZE + PORT_SIZE;
        }

        assert(dst->size() == expectedSize);
    }

    bool ShardInfo::load(ShardInfo* dst, const vector<unsigned char>& message,
                         unsigned int cur_offset)
    {
        assert(dst);
        assert(dst->peers.empty());

        if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                       (4 * sizeof(uint32_t)) + UINT256_SIZE))
        {
            return false;
        }

        // 32-byte block number
        dst->blockNumber = Serializable::GetNumber<uint256_t>(
            message, cur_offset, UINT256_SIZE);
        cur_offset += UINT256_SIZE;

        // 4-byte shard ID
        dst->shardID = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                         sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        // 4-byte number of shards
        dst->numShards = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                           sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        // 4-byte committee size
        const uint32_t comm_size = Serializable::GetNumber<uint32_t>(
            message, cur_offset, sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                       (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE)
                                           * comm_size))
        {
            return false;
        }

        // All nodes; first entry is leader
        for (uint32_t i = 0; i < comm_size; i++)
        {
            PubKey key(message, cur_offset);
            cur_offset += PUB_KEY_SIZE;

            Peer peer(message, cur_offset);
            cur_offset += IP_SIZE + PORT_SIZE;

            dst->peers.emplace_back(
                std::make_pair(std::move(key), std::move(peer)));
        }

        return true;
    }
}