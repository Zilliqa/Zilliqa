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

#ifndef __DATASENDER_H__
#define __DATASENDER_H__

#include <deque>
#include <functional>
#include <vector>

#include "ShardStruct.h"
#include "common/Singleton.h"
#include "libData/BlockData/Block/BlockBase.h"

typedef std::function<bool(bytes& message)> ComposeMessageForSenderFunc;
typedef std::function<void(const VectorOfLookupNode& lookups,
                           const bytes& message)>
    SendDataToLookupFunc;
typedef std::function<void(const bytes& message, const DequeOfShard& shards,
                           const unsigned int& my_shards_lo,
                           const unsigned int& my_shards_hi)>
    SendDataToShardFunc;

extern SendDataToLookupFunc SendDataToLookupFuncDefault;

extern SendDataToShardFunc SendDataToShardFuncDefault;

class DataSender : Singleton<DataSender> {
  DataSender();
  ~DataSender();

 public:
  // Singleton should not implement these
  DataSender(DataSender const&) = delete;
  void operator=(DataSender const&) = delete;

  static DataSender& GetInstance();

  void DetermineShardToSendDataTo(
      unsigned int& my_cluster_num, unsigned int& my_shards_lo,
      unsigned int& my_shards_hi, const DequeOfShard& shards,
      const std::deque<std::pair<PubKey, Peer>>& tmpCommittee,
      const uint16_t& indexB2);

  void DetermineNodesToSendDataTo(
      const DequeOfShard& shards,
      const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
      const unsigned int& my_shards_lo, const unsigned int& my_shards_hi,
      std::deque<std::vector<Peer>>& sharded_receivers);

  bool SendDataToOthers(
      const BlockBase& blockwcosig,
      const std::deque<std::pair<PubKey, Peer>>& sendercommittee,
      const DequeOfShard& shards,
      const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
      const VectorOfLookupNode& lookups, const BlockHash& hashForRandom,
      const ComposeMessageForSenderFunc&,
      const SendDataToLookupFunc& sendDataToLookupFunc =
          SendDataToLookupFuncDefault,
      const SendDataToShardFunc& sendDataToShardFunc = nullptr);
};

#endif  // __DATASENDER_H__
