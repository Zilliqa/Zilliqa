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

#ifndef DB_H
#define DB_H

#include <leveldb/db.h>
#include <libUtils/Logger.h>
#include <string>

/// Utility class for providing database-type storage.
class DB {
  std::string m_db_name;
  leveldb::DB* m_db;

 public:
  /// Constructor.
  DB(const std::string& name = "db.txt");

  /// Destructor.
  ~DB();

  /// Returns the reference to the leveldb database instance.
  leveldb::DB* GetDB();

  /// Returns the value at the specified key.
  std::string ReadFromDB(const std::string& key);

  /// Sets the value at the specified key.
  int WriteToDB(const std::string& key, const std::string& value);

  /// Deletes the value at the specified key.
  int DeleteFromDB(const std::string& key);

  /// Deletes the entire database.
  int DeleteDB();
};

#endif  // DB_H
