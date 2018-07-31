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

#ifndef __DSBLOCKMESSAGE_H__
#define __DSBLOCKMESSAGE_H__

#include "libCrypto/Schnorr.h"
#include "libNetwork/Peer.h"
#include "libUtils/SanityChecks.h"
#include <deque>
#include <map>
#include <utility>
#include <vector>

class ShardingStructure
{
    // Sharding structure message format:
    // [4-byte num of committees]
    // [4-byte committee size]
    //   [33-byte public key] [16-byte ip] [4-byte port]
    //   [33-byte public key] [16-byte ip] [4-byte port]
    //   ...
    // [4-byte committee size]
    //   [33-byte public key] [16-byte ip] [4-byte port]
    //   [33-byte public key] [16-byte ip] [4-byte port]
    //   ...

public:
    static unsigned int
    Serialize(const std::vector<std::map<PubKey, Peer>>& shards,
              std::vector<unsigned char>& output, unsigned int cur_offset)
    {
        LOG_MARKER();

        uint32_t numOfComms = shards.size();

        // 4-byte num of committees
        Serializable::SetNumber<uint32_t>(output, cur_offset, numOfComms,
                                          sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        LOG_GENERAL(INFO, "Number of committees = " << numOfComms);

        for (unsigned int i = 0; i < numOfComms; i++)
        {
            const std::map<PubKey, Peer>& shard = shards.at(i);

            // 4-byte committee size
            Serializable::SetNumber<uint32_t>(output, cur_offset, shard.size(),
                                              sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_GENERAL(INFO,
                        "Committee size = " << shard.size() << "\n"
                                            << "Members:");

            for (auto& kv : shard)
            {
                // 33-byte public key
                kv.first.Serialize(output, cur_offset);
                cur_offset += PUB_KEY_SIZE;

                // 16-byte ip + 4-byte port
                kv.second.Serialize(output, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_GENERAL(
                    INFO,
                    " PubKey = "
                        << DataConversion::SerializableToHexStr(kv.first)
                        << " at " << kv.second.GetPrintableIPAddress()
                        << " Port: " << kv.second.m_listenPortHost);
            }
        }

        return cur_offset;
    }

    static unsigned int Deserialize(const std::vector<unsigned char>& input,
                                    unsigned int cur_offset,
                                    std::vector<std::map<PubKey, Peer>>& shards)
    {
        LOG_MARKER();

        // 4-byte num of committees
        uint32_t numOfComms = Serializable::GetNumber<uint32_t>(
            input, cur_offset, sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        LOG_GENERAL(INFO, "Number of committees = " << numOfComms);

        for (unsigned int i = 0; i < numOfComms; i++)
        {
            shards.emplace_back();

            // 4-byte committee size
            uint32_t shard_size = Serializable::GetNumber<uint32_t>(
                input, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_GENERAL(INFO, "Committee size = " << shard_size);
            LOG_GENERAL(INFO, "Members:");

            for (unsigned int j = 0; j < shard_size; j++)
            {
                PubKey memberPubkey(input, cur_offset);
                cur_offset += PUB_KEY_SIZE;

                Peer memberPeer(input, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                shards.back().emplace(memberPubkey, memberPeer);

                LOG_GENERAL(
                    INFO,
                    " PubKey = "
                        << DataConversion::SerializableToHexStr(memberPubkey)
                        << " at " << memberPeer.GetPrintableIPAddress()
                        << " Port: " << memberPeer.m_listenPortHost);
            }
        }

        return cur_offset;
    }
};

class TxnSharingAssignments
{
    // Transaction body sharing assignments:
    // PART 1. Select X random nodes from DS committee for receiving Tx bodies and broadcasting to other DS nodes
    // PART 2. Select X random nodes per shard for receiving Tx bodies and broadcasting to other nodes in the shard
    // PART 3. Select X random nodes per shard for sending Tx bodies to the receiving nodes in other committees (DS and shards)

    // Message format:
    // [4-byte num of DS nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committees]
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee receiving nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // [4-byte num of committee sending nodes]
    //   [16-byte IP] [4-byte port]
    //   [16-byte IP] [4-byte port]
    //   ...
    // ...

public:
    static unsigned int
    Serialize(const std::vector<Peer>& ds_receivers,
              const std::vector<std::vector<Peer>>& shard_receivers,
              const std::vector<std::vector<Peer>>& shard_senders,
              std::vector<unsigned char>& output, unsigned int cur_offset)
    {
        LOG_MARKER();

        // [4-byte num of DS nodes]
        LOG_GENERAL(INFO,
                    "Forwarders inside the DS committee ("
                        << ds_receivers.size() << "):");
        Serializable::SetNumber<uint32_t>(
            output, cur_offset, ds_receivers.size(), sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        for (unsigned int i = 0; i < ds_receivers.size(); i++)
        {
            // [16-byte IP] [4-byte port]
            ds_receivers.at(i).Serialize(output, cur_offset);
            LOG_GENERAL(INFO, ds_receivers.at(i));
            cur_offset += IP_SIZE + PORT_SIZE;
        }

        // [4-byte num of committees]
        LOG_GENERAL(INFO, "Number of shards: " << shard_receivers.size());
        Serializable::SetNumber<uint32_t>(output, cur_offset,
                                          (uint32_t)shard_receivers.size(),
                                          sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        for (unsigned int i = 0; i < shard_receivers.size(); i++)
        {
            const std::vector<Peer>& shard_x_receivers = shard_receivers.at(i);

            // [4-byte num of committee receiving nodes]
            LOG_GENERAL(INFO, "Shard " << i << " forwarders:");
            Serializable::SetNumber<uint32_t>(
                output, cur_offset, shard_x_receivers.size(), sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            for (unsigned int j = 0; j < shard_x_receivers.size(); j++)
            {
                // [16-byte IP] [4-byte port]
                shard_x_receivers.at(j).Serialize(output, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_GENERAL(INFO, shard_x_receivers.at(j));
            }

            const std::vector<Peer>& shard_x_senders = shard_senders.at(i);

            // [4-byte num of committee sending nodes]
            LOG_GENERAL(INFO, "Shard " << i << " senders:");
            Serializable::SetNumber<uint32_t>(
                output, cur_offset, shard_x_senders.size(), sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            for (unsigned int j = 0; j < shard_x_senders.size(); j++)
            {
                // [16-byte IP] [4-byte port]
                shard_x_senders.at(j).Serialize(output, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_GENERAL(INFO, shard_x_senders.at(j));
            }
        }

        return cur_offset;
    }

    static unsigned int
    Deserialize(const std::vector<unsigned char>& input,
                unsigned int cur_offset, std::vector<Peer>& ds_receivers,
                std::vector<std::vector<Peer>>& shard_receivers,
                std::vector<std::vector<Peer>>& shard_senders)
    {
        LOG_MARKER();

        // [4-byte num of DS nodes]
        uint32_t num_ds_nodes = Serializable::GetNumber<uint32_t>(
            input, cur_offset, sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        LOG_GENERAL(INFO,
                    "Forwarders inside the DS committee (" << num_ds_nodes
                                                           << "):");

        for (unsigned int i = 0; i < num_ds_nodes; i++)
        {
            // [16-byte IP] [4-byte port]
            ds_receivers.emplace_back(input, cur_offset);
            cur_offset += IP_SIZE + PORT_SIZE;

            LOG_GENERAL(INFO,
                        "  IP: " << ds_receivers.back().GetPrintableIPAddress()
                                 << " Port: "
                                 << ds_receivers.back().m_listenPortHost);
        }

        // [4-byte num of committees]
        uint32_t num_shards = Serializable::GetNumber<uint32_t>(
            input, cur_offset, sizeof(uint32_t));
        cur_offset += sizeof(uint32_t);

        for (unsigned int i = 0; i < num_shards; i++)
        {
            shard_receivers.emplace_back();

            // [4-byte num of committee receiving nodes]
            uint32_t num_shard_receivers = Serializable::GetNumber<uint32_t>(
                input, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_GENERAL(INFO, "Shard " << i << " forwarders:");

            for (unsigned int j = 0; j < num_shard_receivers; j++)
            {
                // [16-byte IP] [4-byte port]
                shard_receivers.back().emplace_back(input, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_GENERAL(
                    INFO,
                    "  IP: "
                        << shard_receivers.back().back().GetPrintableIPAddress()
                        << " Port: "
                        << shard_receivers.back().back().m_listenPortHost);
            }

            shard_senders.emplace_back();

            // [4-byte num of committee sending nodes]
            uint32_t num_shard_senders = Serializable::GetNumber<uint32_t>(
                input, cur_offset, sizeof(uint32_t));
            cur_offset += sizeof(uint32_t);

            LOG_GENERAL(INFO, "Shard " << i << " senders:");

            for (unsigned int j = 0; j < num_shard_senders; j++)
            {
                // [16-byte IP] [4-byte port]
                shard_senders.back().emplace_back(input, cur_offset);
                cur_offset += IP_SIZE + PORT_SIZE;

                LOG_GENERAL(
                    INFO,
                    "  IP: "
                        << shard_senders.back().back().GetPrintableIPAddress()
                        << " Port: "
                        << shard_senders.back().back().m_listenPortHost);
            }
        }

        return cur_offset;
    }
};

#endif // __DSBLOCKMESSAGE_H__
