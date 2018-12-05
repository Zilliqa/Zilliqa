/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "DataSender.h"

#include "libCrypto/Sha2.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;

void SendDataToLookupNodesDefault(const VectorOfLookupNode& lookups,
                                  const vector<unsigned char>& message) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToLookupNodesDefault not "
                "expected to be called from LookUp node.");
  }
  LOG_MARKER();

  // TODO: provide interface in P2PComm instead of repopulating the lookup into
  // vector of Peer
  vector<Peer> allLookupNodes;

  for (const auto& node : lookups) {
    LOG_GENERAL(INFO, "Sending msg to lookup node " << node.second);

    allLookupNodes.emplace_back(node.second);
  }

  P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void SendDataToShardNodesDefault(const vector<unsigned char>& message,
                                 const DequeOfShard& shards,
                                 const unsigned int& my_shards_lo,
                                 const unsigned int& my_shards_hi) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToShardNodesDefault not expected to "
                "be called from LookUp node.");
    return;
  }
  // Too few target shards - avoid asking all DS clusters to send
  LOG_MARKER();

  auto p = shards.begin();
  advance(p, my_shards_lo);

  for (unsigned int i = my_shards_lo; i < my_shards_hi; i++) {
    if (BROADCAST_GOSSIP_MODE) {
      // Choose N other Shard nodes to be recipient of final block
      std::vector<Peer> shardReceivers;
      unsigned int numOfReceivers =
          std::min(NUM_GOSSIP_RECEIVERS, (uint32_t)p->size());

      for (unsigned int i = 0; i < numOfReceivers; i++) {
        const auto& kv = p->at(i);
        shardReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
      }

      P2PComm::GetInstance().SendRumorToForeignPeers(shardReceivers, message);
    } else {
      vector<Peer> shard_peers;

      for (const auto& kv : *p) {
        shard_peers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
        LOG_GENERAL(INFO, " PubKey: " << DataConversion::SerializableToHexStr(
                                             std::get<SHARD_NODE_PUBKEY>(kv))
                                      << " IP:Port "
                                      << std::get<SHARD_NODE_PEER>(kv));
      }

      P2PComm::GetInstance().SendBroadcastMessage(shard_peers, message);
    }

    p++;
  }
}

SendDataToLookupFunc SendDataToLookupFuncDefault =
    [](const VectorOfLookupNode& lookups,
       const std::vector<unsigned char>& message) mutable -> void {
  SendDataToLookupNodesDefault(lookups, message);
};

SendDataToShardFunc SendDataToShardFuncDefault =
    [](const std::vector<unsigned char>& message, const DequeOfShard& shards,
       const unsigned int& my_shards_lo,
       const unsigned int& my_shards_hi) mutable -> void {
  SendDataToShardNodesDefault(message, shards, my_shards_lo, my_shards_hi);
};

DataSender::DataSender() {}

DataSender::~DataSender() {}

DataSender& DataSender::GetInstance() {
  static DataSender datasender;
  return datasender;
}

void DataSender::DetermineShardToSendDataTo(
    unsigned int& my_cluster_num, unsigned int& my_shards_lo,
    unsigned int& my_shards_hi, const DequeOfShard& shards,
    const deque<pair<PubKey, Peer>>& tmpCommittee, const uint16_t& indexB2) {
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
  LOG_GENERAL(INFO, "DEBUG num of clusters " << num_clusters)
  unsigned int shard_groups_count = shards.size() / num_clusters;
  if ((shards.size() % num_clusters) > 0) {
    shard_groups_count++;
  }
  LOG_GENERAL(INFO, "DEBUG num of shard group count " << shard_groups_count)

  my_cluster_num = indexB2 / MULTICAST_CLUSTER_SIZE;
  my_shards_lo = my_cluster_num * shard_groups_count;
  my_shards_hi = my_shards_lo + shard_groups_count;

  if (my_shards_hi >= shards.size()) {
    my_shards_hi = shards.size();
  }
}

bool DataSender::SendDataToOthers(
    const BlockBase& blockwcosig,
    const deque<pair<PubKey, Peer>>& sendercommittee,
    const DequeOfShard& shards, const VectorOfLookupNode& lookups,
    const BlockHash& hashForRandom,
    const ComposeMessageForSenderFunc& composeMessageForSenderFunc,
    const SendDataToLookupFunc& sendDataToLookupFunc,
    const SendDataToShardFunc& sendDataToShardFunc) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DataSender::SendDataToOthers not expected "
                "to be called from LookUp node.");
    return false;
  }

  LOG_MARKER();

  if (blockwcosig.GetB2().size() != sendercommittee.size()) {
    LOG_GENERAL(WARNING, "B2 size and committee size is not identical!");
    return false;
  }

  deque<pair<PubKey, Peer>> tmpCommittee;
  for (unsigned int i = 0; i < blockwcosig.GetB2().size(); i++) {
    if (blockwcosig.GetB2().at(i)) {
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
    vector<unsigned char> message;
    if (!(composeMessageForSenderFunc &&
          composeMessageForSenderFunc(message))) {
      LOG_GENERAL(
          WARNING,
          "composeMessageForSenderFunc undefined or cannot compose message");
      return false;
    }

    uint16_t randomDigits =
        DataConversion::charArrTo16Bits(hashForRandom.asBytes());
    bool committeeTooSmall = tmpCommittee.size() < TX_SHARING_CLUSTER_SIZE;
    uint16_t nodeToSendToLookUpLo =
        committeeTooSmall
            ? 0
            : (randomDigits % (tmpCommittee.size() - TX_SHARING_CLUSTER_SIZE));
    uint16_t nodeToSendToLookUpHi =
        committeeTooSmall ? tmpCommittee.size()
                          : nodeToSendToLookUpLo + TX_SHARING_CLUSTER_SIZE;

    LOG_GENERAL(INFO, "lo: " << nodeToSendToLookUpLo << " hi: "
                             << nodeToSendToLookUpHi << " my: " << indexB2);

    if (indexB2 >= nodeToSendToLookUpLo && indexB2 < nodeToSendToLookUpHi) {
      LOG_GENERAL(INFO,
                  "Part of the committeement (assigned) that will send the "
                  "Data to the lookup nodes");
      if (sendDataToLookupFunc) {
        sendDataToLookupFunc(lookups, message);
      }
    }

    unsigned int my_cluster_num = UINT_MAX;
    unsigned int my_shards_lo = 0;
    unsigned int my_shards_hi = 0;

    if (!shards.empty()) {
      DetermineShardToSendDataTo(my_cluster_num, my_shards_lo, my_shards_hi,
                                 shards, tmpCommittee, indexB2);

      LOG_GENERAL(
          INFO, "my_cluster_num + 1: " << my_cluster_num + 1
                                       << " shards.size(): " << shards.size());

      if ((my_cluster_num + 1) <= shards.size()) {
        if (sendDataToShardFunc) {
          sendDataToShardFunc(message, shards, my_shards_lo, my_shards_hi);
        }
      }
    }
  }

  return true;
}