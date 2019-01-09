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

#include "FallbackBlock.h"
#include "libNetwork/ShardStruct.h"

#ifndef __FALLBACKBLOCKWSHARDINGSTRUCTURE_H__
#define __FALLBACKBLOCKWSHARDINGSTRUCTURE_H__

struct FallbackBlockWShardingStructure : public SerializableDataBlock {
  FallbackBlock m_fallbackblock;
  DequeOfShard m_shards;

  FallbackBlockWShardingStructure();
  FallbackBlockWShardingStructure(const bytes& src, unsigned int offset);
  FallbackBlockWShardingStructure(const FallbackBlock& fallbackblock,
                                  const DequeOfShard& shards);

  bool Serialize(bytes& dst, unsigned int offset) const;

  bool Deserialize(const bytes& src, unsigned int offset);
};

#endif  // __FALLBACKBLOCKWSHARDINGSTRUCTURE_H__
