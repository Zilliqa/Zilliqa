/*
 * Copyright (C) 2020 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_MININGDATA_MINERINFO_H_
#define ZILLIQA_SRC_LIBDATA_MININGDATA_MINERINFO_H_

#include <Schnorr.h>
#include <deque>
#include <vector>

/// Stores miner info (DS committee) for one DS epoch
struct MinerInfoDSComm {
  std::deque<PubKey> m_dsNodes;
  std::vector<PubKey> m_dsNodesEjected;
};

/// Stores miner info (shards) for one DS epoch
struct MinerInfoShards {
  struct MinerInfoShard {
    uint32_t m_shardSize{};
    std::vector<PubKey> m_shardNodes;
    MinerInfoShard() : m_shardSize(0) {}
  };
  std::vector<MinerInfoShard> m_shards;
};

#endif  // ZILLIQA_SRC_LIBDATA_MININGDATA_MINERINFO_H_
