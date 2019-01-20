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
#include <boost/filesystem.hpp>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"

class IncrementalDB : public Singleton<IncrementalDB> {
  std::shared_ptr<LevelDB> m_txBodyDB;
  std::shared_ptr<LevelDB> m_microBlockDB;
  std::shared_ptr<LevelDB> m_blockLinkDB;
  std::shared_ptr<LevelDB> m_VCBlockDB;
  std::shared_ptr<LevelDB> m_DSBlockDB;
  std::shared_ptr<LevelDB> m_FallBackBlockDB;

  const std::string m_path;
  const std::string m_txBodyDBName;
  const std::string m_microBlockDBName;

  IncrementalDB(const std::string& path)
      : m_path(path),
        m_txBodyDBName("txBodies"),
        m_microBlockDBName("microBlockDB") {
    const std::string path_abs = "./" + path;
    if (!boost::filesystem::exists(path_abs)) {
      boost::filesystem::create_directories(path_abs);
    }
  }

 public:
  bool PutTxBody(const dev::h256& txID, const bytes& body,
                 const std::string& dsEpoch);
  bool PutMicroBlock(const BlockHash& blockHash, const bytes& body,
                     const std::string& dsEpoch);
};