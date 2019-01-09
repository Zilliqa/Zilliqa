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

#include <vector>
#include "BaseDB.h"
#include "common/Serializable.h"

class ArchiveDB : public BaseDB {
 public:
  ArchiveDB(std::string dbname, std::string txn, std::string txBlock,
            std::string dsBlock, std::string accountState)
      : BaseDB(dbname, txn, txBlock, dsBlock, accountState) {}
  bool InsertTxn(const TransactionWithReceipt& txn);
  bool InsertTxBlock(const TxBlock& txblock);
  bool InsertDSBlock(const DSBlock& dsblock);
  bool InsertSerializable(const Serializable& sz, const std::string& index,
                          const std::string& collectionName);
  // Temporary function for use by data blocks
  bool InsertSerializable(const SerializableDataBlock& sz,
                          const std::string& index,
                          const std::string& collectionName);
  bool InsertAccount(const Address& addr, const Account& acc);
  bool GetSerializable(bytes& retVec, const std::string& index,
                       const std::string& collectionName);
};
