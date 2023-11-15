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

#include "libNetwork/Blacklist.h"
// XXX #include "libNetwork/P2PComm.h"
#include "libNetwork/P2P.h"
#include "libUtils/DataConversion.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

using namespace std;

void SendDataToLookupNodesDefault(const VectorOfNode& lookups,
                                  const zbytes& message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToLookupNodesDefault not "
                "expected to be called from LookUp node.");
  }
  LOG_MARKER();

  vector<Peer> allLookupNodes;

  for (auto& node : lookups) {
    Blacklist::GetInstance().Whitelist(
        {node.second.GetIpAddress(), node.second.GetListenPortHost(),
         node.second.GetNodeIndentifier()});  // exclude this lookup ip from
                                              // blacklisting
    Peer tmp(node.second.GetIpAddress(), node.second.GetListenPortHost(),
             node.second.GetHostname());
    LOG_GENERAL(INFO, "Sending to lookup " << tmp);

    allLookupNodes.emplace_back(tmp);
  }

  // XXX P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
  zil::p2p::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void SendDataToShardNodesDefault(
    const zbytes& message, const std::deque<VectorOfPeer>& sharded_receivers,
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
      // XXX P2PComm::GetInstance().SendRumorToForeignPeers(receivers, message);
      zil::p2p::GetInstance().SendRumorToForeignPeers(receivers, message);
    } else {
      zil::p2p::GetInstance().SendBroadcastMessage(receivers, message);
    }
  }
}

SendDataToLookupFunc SendDataToLookupFuncDefault =
    [](const VectorOfNode& lookups, const zbytes& message) mutable -> void {
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
                                            const DequeOfShardMembers& shards,
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
    const DequeOfShardMembers& shardMembers, const uint16_t& consensusMyId,
    bool forceMulticast, std::deque<VectorOfPeer>& sharded_receivers) {
  VectorOfPeer shardReceivers;

  if (BROADCAST_GOSSIP_MODE && !forceMulticast) {
    // No cosig found, use default order
    // pick node from index based on consensusMyId
    unsigned int node_to_send_from = 0;
    if (shardMembers.size() > NUM_GOSSIP_RECEIVERS) {
      node_to_send_from =
          consensusMyId % (shardMembers.size() - NUM_GOSSIP_RECEIVERS);
    }

    for (unsigned int i = node_to_send_from;
         i < min(node_to_send_from + NUM_GOSSIP_RECEIVERS,
                 (unsigned int)shardMembers.size());
         i++) {
      const auto& kv = shardMembers.at(i);
      shardReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
    }
  } else {
    for (const auto& kv : shardMembers) {
      shardReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
    }
  }
  sharded_receivers.emplace_back(shardReceivers);
}

bool DataSender::SendDataToOthers(
    const BlockBase& blockwcosigSender, const DequeOfNode& sendercommittee,
    const DequeOfShardMembers& shards,
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
    LOG_GENERAL(INFO, "I'm in B2 set, so I'll try to send data to others");
    zbytes message;
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
    } else {
      LOG_GENERAL(WARNING,
                  "I'm not going to send data to others because: IndexB2 is: "
                      << indexB2
                      << ", nodeLookupLo is : " << nodeToSendToLookUpLo
                      << ", nodeLookupHi is: " << nodeToSendToLookUpHi);
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
          std::deque<VectorOfPeer> sharded_receivers;
          DetermineNodesToSendDataTo(shards, consensusMyId, forceMulticast,
                                     sharded_receivers);
          SendDataToShardNodesDefault(message, sharded_receivers,
                                      forceMulticast);
        }
      }
    } else {
      LOG_GENERAL(WARNING, "Shards size is: " << shards.size()
                                              << ", so no data was sent there");
    }
  } else {
    LOG_GENERAL(WARNING, "I'm NOT in B2 set! "
                             << "B2 size " << blockwcosigSender.GetB2().size()
                             << " and committee size "
                             << sendercommittee.size());
  }

  return true;
}
