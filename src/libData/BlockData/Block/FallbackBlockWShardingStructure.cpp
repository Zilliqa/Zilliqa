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

#include "FallbackBlockWShardingStructure.h"
#include "libMessage/Messenger.h"

FallbackBlockWShardingStructure::FallbackBlockWShardingStructure() {}

FallbackBlockWShardingStructure::FallbackBlockWShardingStructure(
    const FallbackBlock& fallbackblock, const DequeOfShard& shards)
    : m_fallbackblock(fallbackblock), m_shards(shards) {}

FallbackBlockWShardingStructure::FallbackBlockWShardingStructure(
    const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "Failed to initialize");
  }
}

bool FallbackBlockWShardingStructure::Serialize(bytes& dst,
                                                unsigned int offset) const {
  if (!Messenger::SetFallbackBlockWShardingStructure(
          dst, offset, m_fallbackblock, SHARDINGSTRUCTURE_VERSION, m_shards)) {
    LOG_GENERAL(WARNING, "Unable to serialize");
    return false;
  }
  return true;
}

bool FallbackBlockWShardingStructure::Deserialize(const bytes& src,
                                                  unsigned int offset) {
  uint32_t shardingStructureVersion = 0;

  if (!Messenger::GetFallbackBlockWShardingStructure(
          src, offset, m_fallbackblock, shardingStructureVersion, m_shards)) {
    LOG_GENERAL(WARNING, "Unable to Deserialize");
    return false;
  }

  if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
    LOG_CHECK_FAIL("Sharding structure version", shardingStructureVersion,
                   SHARDINGSTRUCTURE_VERSION);
    return false;
  }

  return true;
}
