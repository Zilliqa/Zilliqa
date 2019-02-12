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
#ifndef __INRCREMENTAL_DB_H__
#define __INRCREMENTAL_DB_H__

#include <boost/filesystem.hpp>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "BlockStorage.h"
#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libValidator/Validator.h"

class IncrementalDB : public Singleton<IncrementalDB> {
  std::unordered_map<std::string, std::pair<uint64_t, std::shared_ptr<LevelDB>>>
      m_DBPointer;
  std::shared_ptr<LevelDB> m_blockLinkDB;
  std::shared_ptr<LevelDB> m_baseStateDB;

  const std::string m_path;
  const std::string m_txBodyDBName;
  const std::string m_microBlockDBName;
  const std::string m_TxBlockDBName;
  const std::string m_VCBlockDBName;
  const std::string m_FallbackBlockDBName;
  const std::string m_DSBlockDBName;
  const std::string m_blockLinkDBName;
  const std::string m_baseStateDBName;
  const std::string m_stateDeltaDBName;

  void ChangeDBPointer(const uint64_t& dsEpoch, const std::string& dbName);

 public:
  IncrementalDB()
      : m_path(INCR_DB_PATH),
        m_txBodyDBName("txBodiesDB"),
        m_microBlockDBName("microBlockDB"),
        m_TxBlockDBName("TxBlockDB"),
        m_VCBlockDBName("VCBlockDB"),
        m_FallbackBlockDBName("FallbackBlockDB"),
        m_DSBlockDBName("DSBlockDB"),
        m_blockLinkDBName("blockLinkDB"),
        m_baseStateDBName("baseStateDB") {}

  void Init();

  bool PutTxBody(const dev::h256& txID, const bytes& body,
                 const uint64_t& dsEpoch);
  bool PutMicroBlock(const BlockHash& blockHash, const bytes& body,
                     const uint64_t& dsEpoch);
  bool PutTxBlock(const uint64_t& blockNum, const bytes& body,
                  const uint64_t& dsEpoch);
  bool PutDSBlock(const uint64_t& blockNum, const bytes& body,
                  const uint64_t& dsEpoch);
  bool PutFallbackBlock(const BlockHash& blockHash, const bytes& body,
                        const uint64_t& dsEpoch);
  bool PutVCBlock(const BlockHash& blockHash, const bytes& body,
                  const uint64_t& dsEpoch);
  bool PutBaseState(const uint64_t& epochNum, const bytes& body);

  bool PutBlockLink(const uint64_t& index, const bytes& body);

  bool PutStateDelta(const uint64_t& dsEpochNum, const uint64_t& txEpoch,
                     bytes& body);

  bool GetAllBlockLink(std::list<BlockLink>& blocklinks);

  bool GetLatestDSEpochStorage(uint64_t& lastDSEpoch);

  bool GetAllTxBlocksEpoch(std::list<TxBlock>& blocks, const uint64_t& dsEpoch);

  bool GetDSBlock(const uint64_t& blocknum, DSBlockSharedPtr& block);

  bool GetVCBlock(const uint64_t& dsEpochNum, const BlockHash& blockhash,
                  VCBlockSharedPtr& block);

  bool GetFallbackBlock(const uint64_t& dsEpochNum, const BlockHash& blockhash,
                        FallbackBlockSharedPtr& fallbackblockwsharding);

  bool GetMicroBlock(const uint64_t& dsEpochNum, const BlockHash& blockhash,
                     MicroBlockSharedPtr& microblock);
  bool GetTxnBody(const uint64_t& dsEpochNum, const dev::h256& key,
                  TxBodySharedPtr& body);
  bool GetBaseState(uint64_t& epochNum, bytes& body);

  bool GetStateDelta(const uint64_t& dsEpochNum, const uint64_t& txEpoch,
                     bytes& stateDelta);

  bool VerifyAll(const DequeOfNode& initialDScommittee,
                 const ValidatorBase& validator);
};

#endif  //__INRCREMENTAL_DB_H__