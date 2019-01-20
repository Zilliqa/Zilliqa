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

#include "IncrementalDB.h"

using namespace std;

bool IncrementalDB::PutTxBody(const dev::h256& txID, const bytes& body,
                              const std::string& dsEpoch) {
  m_txBodyDB = make_shared<LevelDB>(m_txBodyDBName, m_path, dsEpoch);
  int ret = -1;

  ret = m_txBodyDB->Insert(txID, body);

  m_txBodyDB.reset();

  return (ret == 0);
}

bool IncrementalDB::PutMicroBlock(const BlockHash& blockHash, const bytes& body,
                                  const string& dsEpoch) {
  m_microBlockDB = make_shared<LevelDB>(m_microBlockDBName, m_path, dsEpoch);

  int ret = -1;

  ret = m_microBlockDB->Insert(blockHash, body);

  m_microBlockDB.reset();

  return (ret == 0);
}