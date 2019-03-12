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

#ifndef __DATASENDER_H__
#define __DATASENDER_H__

#include <deque>
#include <functional>
#include <vector>

#include "ShardStruct.h"
#include "common/Singleton.h"
#include "libData/BlockData/Block/BlockBase.h"

typedef std::function<bool(bytes& message)> ComposeMessageForSenderFunc;
typedef std::function<void(const VectorOfNode& lookups, const bytes& message)>
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

  void DetermineShardToSendDataTo(unsigned int& my_cluster_num,
                                  unsigned int& my_shards_lo,
                                  unsigned int& my_shards_hi,
                                  const DequeOfShard& shards,
                                  const DequeOfNode& tmpCommittee,
                                  const uint16_t& indexB2);

  void DetermineNodesToSendDataTo(
      const DequeOfShard& shards,
      const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
      const uint16_t& consensusMyId, const unsigned int& my_shards_lo,
      const unsigned int& my_shards_hi, bool forceMulticast,
      std::deque<std::vector<Peer>>& sharded_receivers);

  bool SendDataToOthers(
      const BlockBase& blockwcosig, const DequeOfNode& sendercommittee,
      const DequeOfShard& shards,
      const std::unordered_map<uint32_t, BlockBase>& blockswcosigRecver,
      const VectorOfNode& lookups, const BlockHash& hashForRandom,
      const uint16_t& consensusMyId, const ComposeMessageForSenderFunc&,
      bool forceMulticast = false,
      const SendDataToLookupFunc& sendDataToLookupFunc =
          SendDataToLookupFuncDefault,
      const SendDataToShardFunc& sendDataToShardFunc = nullptr);
};

#endif  // __DATASENDER_H__
