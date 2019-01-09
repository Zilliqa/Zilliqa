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
#include <string>

#include "DB.h"

using namespace std;

DB::DB(const string& name) {
  this->m_db_name = name;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status =
      leveldb::DB::Open(options, this->m_db_name, &this->m_db);
  if (!status.ok()) {
    LOG_GENERAL(WARNING, "Cannot init DB.");
    // throw exception();
  }
}

DB::~DB() { delete m_db; }

string DB::ReadFromDB(const string& key) {
  string value;
  leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
  if (!s.ok()) {
    return "DB_ERROR";
  } else {
    return value;
  }
}

leveldb::DB* DB::GetDB() { return this->m_db; }

int DB::WriteToDB(const string& key, const string& value) {
  leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
  if (!s.ok()) {
    return -1;
  } else {
    return 0;
  }
}

int DB::DeleteFromDB(const string& key) {
  leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), key);
  if (!s.ok()) {
    return -1;
  } else {
    return 0;
  }
}

int DB::DeleteDB() {
  delete m_db;
  leveldb::Status s = leveldb::DestroyDB(this->m_db_name, leveldb::Options());
  if (!s.ok()) {
    LOG_GENERAL(INFO, "Status: " << s.ToString());
    return -1;
  } else {
    return 0;
  }
}
