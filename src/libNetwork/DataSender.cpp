/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "DataSender.h"

#include "libCrypto/Sha2.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

using namespace std;

void SendDataToLookupNodesDefault(const VectorOfNode& lookups,
                                  const bytes& message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToLookupNodesDefault not "
                "expected to be called from LookUp node.");
  }
  LOG_MARKER();

  vector<Peer> allLookupNodes;

  for (const auto& node : lookups) {
    string url = node.second.GetHostname();
    auto resolved_ip = node.second.GetIpAddress();  // existing one
    if (!url.empty()) {
      uint128_t tmpIp;
      if (IPConverter::ResolveDNS(url, node.second.GetListenPortHost(),
                                  tmpIp)) {
        resolved_ip = tmpIp;  // resolved one
      } else {
        LOG_GENERAL(WARNING, "Unable to resolve DNS for " << url);
      }
    }

    Blacklist::GetInstance().Exclude(
        resolved_ip);  // exclude this lookup ip from blacklisting
    Peer tmp(resolved_ip, node.second.GetListenPortHost());
    LOG_GENERAL(INFO, "Sending to lookup " << tmp);

    allLookupNodes.emplace_back(tmp);
  }

  P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void SendDataToShardNodesDefault(
    const bytes& message,
    const std::deque<std::vector<Peer>>& sharded_receivers,
    bool forceMulticast) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToShardNodesDefault not expected to "
                "be called from LookUp node.");
    return;
  }
  // Too few target shards - avoid asking all DS clusters to send
  LOG_MARKER();

  for (const auto& receivers : sharded_receivers) {
    if (BROADCAST_GOSSIP_MODE && !forceMulticast) {
      P2PComm::GetInstance().SendRumorToForeignPeers(receivers, message);
    } else {
      P2PComm::GetInstance().SendBroadcastMessage(receivers, message);
    }
  }
}

SendDataToLookupFunc SendDataToLookupFuncDefault =
    [](const VectorOfNode& lookups, const bytes& message) mutable -> void {
  SendDataToLookupNodesDefault(lookups, message);
};

DataSender::DataSender() {}

DataSender::~DataSender() {}

DataSender& DataSender::GetInstance() {
  static DataSender datasender;
  return datasender;
}

void DataSender::DetermineShardToSendDataTo(unsigned int& my_cluster_num,
                                            unsigned int& my_shards_lo,
                                            unsigned int& my_shards_hi,
                                            const DequeOfShard& shards,
                                            const DequeOfNode& tmpCommittee,
                                            const uint16_t& indexB2) {
  // Multicast block to my assigned shard's nodes - send BLOCK message
  // Message = [block]

  // Multicast assignments:
  // 1. Divide committee into clusters of size MULTICAST_CLUSTER_SIZE
  // 2. Each cluster talks to all shard members in each shard
  //    cluster 0 => Shard 0
  //    cluster 1 => Shard 1
  //    ...
  //    cluster 0 => Shard (num of clusters)
  //    cluster 1 => Shard (num of clusters + 1)
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::DetermineShardToSendDataTo not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  unsigned int num_clusters = tmpCommittee.size() / MULTICAST_CLUSTER_SIZE;
  if ((tmpCommittee.size() % MULTICAST_CLUSTER_SIZE) > 0) {
    num_clusters++;
  }
  LOG_GENERAL(INFO, "Clusters     = " << num_clusters)
  unsigned int shard_groups_count = 0;
  if (num_clusters != 0) {
    shard_groups_count = shards.size() / num_clusters;
    if ((shards.size() % num_clusters) > 0) {
      shard_groups_count++;
    }
  }
  LOG_GENERAL(INFO, "Shard groups = " << shard_groups_count)

  my_cluster_num = indexB2 / MULTICAST_CLUSTER_SIZE;
  my_shards_lo = my_cluster_num * shard_groups_count;
  my_shards_hi = my_shards_lo + shard_groups_count;

  if (my_shards_hi >= shards.size()) {
    my_shards_hi = shards.size();
  }
}

void DataSender::DetermineNodesToSendDataTo(
    const DequeOfShard& shards,
    const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
    const uint16_t& consensusMyId, const unsigned int& my_shards_lo,
    const unsigned int& my_shards_hi, bool forceMulticast,
    std::deque<std::vector<Peer>>& sharded_receivers) {
  auto p = shards.begin();
  advance(p, my_shards_lo);

  for (unsigned int i = my_shards_lo; i < my_shards_hi; i++) {
    std::vector<Peer> shardReceivers;
    if (BROADCAST_GOSSIP_MODE && !forceMulticast) {
      auto blockRecver = blockswcosigRecver.find(i);
      if (blockRecver != blockswcosigRecver.end()) {
        // cosigs found, select nodes with cosig
        std::vector<Peer> nodes_cosigned;
        std::vector<Peer> nodes_not_cosigned;
        for (unsigned int i = 0; i < p->size(); i++) {
          const auto& kv = p->at(i);
          if (blockRecver->second.GetB2().at(i)) {
            nodes_cosigned.emplace_back(std::get<SHARD_NODE_PEER>(kv));
          } else {
            nodes_not_cosigned.emplace_back(std::get<SHARD_NODE_PEER>(kv));
          }
        }

        unsigned int node_to_send_from_cosigned = 0;
        unsigned int node_to_send_from_not_cosigned = 0;

        if (nodes_cosigned.size() > NUM_GOSSIP_RECEIVERS) {
          // pick from index based on consensusMyId
          node_to_send_from_cosigned =
              consensusMyId % (nodes_cosigned.size() - NUM_GOSSIP_RECEIVERS);
        } else {
          // if nodes_cosigned is not enough to meet NUM_GOSSIP_RECEIVERS, try
          // to get node from not_cosigned
          if (nodes_not_cosigned.size() >
              NUM_GOSSIP_RECEIVERS - nodes_cosigned.size()) {
            node_to_send_from_cosigned =
                consensusMyId % (nodes_not_cosigned.size() -
                                 NUM_GOSSIP_RECEIVERS + nodes_cosigned.size());
          }

          for (unsigned int i = node_to_send_from_not_cosigned;
               i < min(nodes_not_cosigned.size(),
                       node_to_send_from_not_cosigned + NUM_GOSSIP_RECEIVERS -
                           nodes_cosigned.size());
               i++) {
            shardReceivers.emplace_back(nodes_not_cosigned.at(i));
          }
        }

        for (unsigned int i = node_to_send_from_cosigned;
             i < min(node_to_send_from_cosigned + NUM_GOSSIP_RECEIVERS,
                     (unsigned int)nodes_cosigned.size());
             i++) {
          shardReceivers.emplace_back(nodes_cosigned.at(i));
        }
      } else {
        // No cosig found, use default order
        // pick node from index based on consensusMyId
        unsigned int node_to_send_from = 0;
        if (p->size() > NUM_GOSSIP_RECEIVERS) {
          node_to_send_from =
              consensusMyId % (p->size() - NUM_GOSSIP_RECEIVERS);
        }

        for (unsigned int i = node_to_send_from;
             i < min(node_to_send_from + NUM_GOSSIP_RECEIVERS,
                     (unsigned int)p->size());
             i++) {
          const auto& kv = p->at(i);
          shardReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
        }
      }
    } else {
      for (const auto& kv : *p) {
        shardReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
      }
    }
    sharded_receivers.emplace_back(shardReceivers);
    p++;
  }
}

bool DataSender::SendDataToOthers(
    const BlockBase& blockwcosigSender, const DequeOfNode& sendercommittee,
    const DequeOfShard& shards,
    const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
    const VectorOfNode& lookups, const BlockHash& hashForRandom,
    const uint16_t& consensusMyId,
    const ComposeMessageForSenderFunc& composeMessageForSenderFunc,
    bool forceMulticast, const SendDataToLookupFunc& sendDataToLookupFunc,
    const SendDataToShardFunc& sendDataToShardFunc) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToOthers not expected "
                "to be called from LookUp node.");
    return false;
  }

  LOG_MARKER();

  if (blockwcosigSender.GetB2().size() != sendercommittee.size()) {
    LOG_GENERAL(WARNING, "B2 size " << blockwcosigSender.GetB2().size()
                                    << " and committee size "
                                    << sendercommittee.size()
                                    << " is not identical!");
    return false;
  }

  DequeOfNode tmpCommittee;
  for (unsigned int i = 0; i < blockwcosigSender.GetB2().size(); i++) {
    if (blockwcosigSender.GetB2().at(i)) {
      tmpCommittee.push_back(sendercommittee.at(i));
    }
  }

  bool inB2 = false;
  uint16_t indexB2 = 0;
  for (const auto& entry : tmpCommittee) {
    if (entry.second == Peer()) {
      inB2 = true;
      break;
    }
    indexB2++;
  }

  if (inB2) {
    bytes message;
    if (!(composeMessageForSenderFunc &&
          composeMessageForSenderFunc(message))) {
      LOG_GENERAL(
          WARNING,
          "composeMessageForSenderFunc undefined or cannot compose message");
      return false;
    }

    uint16_t randomDigits =
        DataConversion::charArrTo16Bits(hashForRandom.asBytes());
    bool committeeTooSmall = tmpCommittee.size() <= TX_SHARING_CLUSTER_SIZE;
    uint16_t nodeToSendToLookUpLo =
        committeeTooSmall
            ? 0
            : (randomDigits % (tmpCommittee.size() - TX_SHARING_CLUSTER_SIZE));
    uint16_t nodeToSendToLookUpHi =
        committeeTooSmall ? tmpCommittee.size()
                          : nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    if (indexB2 >= nodeToSendToLookUpLo && indexB2 < nodeToSendToLookUpHi) {
      LOG_GENERAL(INFO, "I will send data to the lookups");
      if (sendDataToLookupFunc) {
        sendDataToLookupFunc(lookups, message);
      }
    }

    if (!shards.empty()) {
      unsigned int my_cluster_num = UINT_MAX;
      unsigned int my_shards_lo = 0;
      unsigned int my_shards_hi = 0;

      DetermineShardToSendDataTo(my_cluster_num, my_shards_lo, my_shards_hi,
                                 shards, tmpCommittee, indexB2);

      if ((my_cluster_num + 1) <= shards.size()) {
        LOG_GENERAL(INFO, "I will send data to the shards");
        if (sendDataToShardFunc) {
          sendDataToShardFunc(message, shards, my_shards_lo, my_shards_hi);
        } else {
          std::deque<std::vector<Peer>> sharded_receivers;
          DetermineNodesToSendDataTo(shards, blockswcosigRecver, consensusMyId,
                                     my_shards_lo, my_shards_hi, forceMulticast,
                                     sharded_receivers);
          SendDataToShardNodesDefault(message, sharded_receivers,
                                      forceMulticast);
        }
      }
    }
  }

  return true;
}
