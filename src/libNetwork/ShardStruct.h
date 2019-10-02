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

#ifndef ZILLIQA_SRC_LIBNETWORK_SHARDSTRUCT_H_
#define ZILLIQA_SRC_LIBNETWORK_SHARDSTRUCT_H_

#include <tuple>

#include <Schnorr.h>
#include "Peer.h"

using VectorOfPeer = std::vector<Peer>;

enum ShardData {
  SHARD_NODE_PUBKEY,
  SHARD_NODE_PEER,
  SHARD_NODE_REP,
};

using Shard = std::vector<std::tuple<PubKey, Peer, uint16_t>>;
using DequeOfShard = std::deque<Shard>;

using PairOfNode = std::pair<PubKey, Peer>;

using VectorOfNode = std::vector<PairOfNode>;
using DequeOfNode = std::deque<PairOfNode>;

enum NodeMessage { NODE_PUBKEY, NODE_PEER, NODE_MSG };

using NodeMsg = std::tuple<PubKey, Peer, bytes>;
using VectorOfNodeMsg = std::vector<NodeMsg>;

#endif  // ZILLIQA_SRC_LIBNETWORK_SHARDSTRUCT_H_
