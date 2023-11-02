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

#include "LevelDB.h"
#include "common/Constants.h"
#include "depends/common/CommonData.h"
#include "libUtils/Logger.h"

using namespace std;

namespace {

template <typename ArgT>
std::string LookupImpl(const std::shared_ptr<leveldb::DB> db,
                       const std::string& dbName, ArgT&& arg,
                       bool* found = nullptr) {
  std::string value;
  if (!db) {
    LOG_GENERAL(WARNING, "LevelDB " << dbName << " isn't available");
    if (found) {
      *found = false;
    }
    return value;
  }

  auto s = db->Get(leveldb::ReadOptions(), std::forward<ArgT>(arg), &value);
  if (!s.ok()) {
    if (!s.IsNotFound()) {
      LOG_GENERAL(WARNING, "LevelDB " << dbName << " status is not OK - "
                                      << s.ToString());
    }
    if (found) {
      *found = false;
    }
  } else if (found) {
    *found = true;
  }

  return value;
}

template <typename KeyT, typename BodyT>
int InsertImpl(const std::shared_ptr<leveldb::DB> db, const std::string& dbName,
               KeyT&& key, BodyT&& body) {
  if (!db) {
    LOG_GENERAL(WARNING, "LevelDB " << dbName << " isn't available");
    return -1;
  }

  auto s =
      db->Put(leveldb::WriteOptions(), leveldb::Slice(std::forward<KeyT>(key)),
              leveldb::Slice(std::forward<BodyT>(body)));

  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[Insert] Status: " << s.ToString());
    return -1;
  }

  return 0;
}

template <typename ArgT>
int DeleteImpl(const std::shared_ptr<leveldb::DB> db, const std::string& dbName,
               ArgT&& arg) {
  if (!db) {
    LOG_GENERAL(WARNING, "LevelDB " << dbName << " isn't available");
    return -1;
  }

  auto s = db->Delete(leveldb::WriteOptions(), std::forward<ArgT>(arg));
  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[DeleteDB] Status: " << s.ToString());
    return -1;
  }

  return 0;
}

}  // namespace

LevelDB::LevelDB(const string& dbName, const string& path,
                 const string& subdirectory)
    : m_dbName{dbName}, m_subdirectory{subdirectory} {
  if (!(std::filesystem::exists(path))) {
    LOG_GENERAL(WARNING, "Can't open " << dbName << " since " << path
                                       << " does not exist");
    return;
  }

  m_options.max_open_files = 256;
  m_options.create_if_missing = true;

  leveldb::DB* db = nullptr;
  leveldb::Status status;

  if (m_subdirectory.empty()) {
    m_open_db_path = path + "/" + this->m_dbName;
    status = leveldb::DB::Open(m_options, m_open_db_path, &db);
    LOG_GENERAL(INFO, path + "/" + this->m_dbName);
  } else {
    if (!(std::filesystem::exists(path + "/" + this->m_subdirectory))) {
      std::filesystem::create_directories(path + "/" + this->m_subdirectory);
    }
    m_open_db_path = path + "/" + this->m_subdirectory + "/" + this->m_dbName;
    status = leveldb::DB::Open(m_options, m_open_db_path, &db);
    LOG_GENERAL(INFO, path + "/" + this->m_subdirectory + "/" + this->m_dbName);
  }

  if (!status.ok()) {
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " status is not OK - "
                                    << status.ToString());
  }

  m_db.reset(db);
}

LevelDB::LevelDB(const std::string& dbName, const std::string& subdirectory,
                 bool diagnostic)
    : m_dbName{dbName}, m_subdirectory{subdirectory} {
  m_options.max_open_files = 256;
  m_options.create_if_missing = true;

  // Diagnostic tool provides the option to pass the persistance db_path
  // that might not be the current directory (case when 'diagnostic' is true).
  // Its default value is false, and the non-diagnostic path is preserved
  // from the original code.
  string db_path = diagnostic
                       ? (m_subdirectory + PERSISTENCE_PATH)
                       : (STORAGE_PATH + PERSISTENCE_PATH +
                          (m_subdirectory.empty() ? "" : "/" + m_subdirectory));
  if (!std::filesystem::exists(db_path)) {
    std::filesystem::create_directories(db_path);
  }

  m_open_db_path = db_path + "/" + this->m_dbName;
  leveldb::DB* db = nullptr;
  auto status = leveldb::DB::Open(m_options, m_open_db_path, &db);
  if (!status.ok()) {
    // throw exception();
    LOG_GENERAL(WARNING, "LevelDB " << dbName << " status is not OK - "
                                    << status.ToString());
  }

  m_db.reset(db);
}

void LevelDB::Reopen() {
  m_db.reset();
  leveldb::DB* db = nullptr;
  leveldb::Status status;

  status = leveldb::DB::Open(m_options, m_open_db_path, &db);
  if (!status.ok()) {
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " status is not OK - "
                                    << status.ToString());
  }
  m_db.reset(db);
}

leveldb::Slice toSlice(boost::multiprecision::uint256_t num) {
  dev::FixedHash<32> h;
  dev::zbytesRef ref(h.data(), 32);
  dev::toBigEndian(num, ref);
  return (leveldb::Slice)h.ref();
}

string LevelDB::GetDBName() {
  if (LOOKUP_NODE_MODE) {
    return m_dbName;
  } else {
    return m_dbName + (m_subdirectory.size() > 0 ? "/" : "") + m_subdirectory;
  }
}

string LevelDB::Lookup(const std::string& key) const {
  return LookupImpl(m_db, m_dbName, key);
}

string LevelDB::Lookup(const vector<unsigned char>& key) const {
  return LookupImpl(m_db, m_dbName,
                    vector_ref<const unsigned char>(&key[0], key.size()));
}

string LevelDB::Lookup(const boost::multiprecision::uint256_t& blockNum) const {
  return LookupImpl(m_db, m_dbName, blockNum.convert_to<string>());
}

string LevelDB::Lookup(const boost::multiprecision::uint256_t& blockNum,
                       bool& found) const {
  return LookupImpl(m_db, m_dbName, blockNum.convert_to<string>(), &found);
}

string LevelDB::Lookup(const dev::h256& key) const {
  return LookupImpl(m_db, m_dbName, key.hex());
}

string LevelDB::Lookup(const dev::zbytesConstRef& key) const {
  return LookupImpl(m_db, m_dbName, ldb::Slice((char const*)key.data(), 32));
}

int LevelDB::Insert(const dev::h256& key, dev::zbytesConstRef value) {
  return Insert(key, value.toString());
}

int LevelDB::Insert(const vector<unsigned char>& key,
                    const vector<unsigned char>& body) {
  return InsertImpl(m_db, m_dbName,
                    vector_ref<const unsigned char>(&key[0], key.size()),
                    vector_ref<const unsigned char>(&body[0], body.size()));
}

int LevelDB::Insert(const boost::multiprecision::uint256_t& blockNum,
                    const vector<unsigned char>& body) {
  return InsertImpl(m_db, m_dbName, blockNum.convert_to<string>(),
                    vector_ref<const unsigned char>(&body[0], body.size()));
}

int LevelDB::Insert(const boost::multiprecision::uint256_t& blockNum,
                    const std::string& body) {
  return InsertImpl(m_db, m_dbName, blockNum.convert_to<string>(),
                    leveldb::Slice{body.c_str(), body.size()});
}

int LevelDB::Insert(const string& key, const vector<unsigned char>& body) {
  return InsertImpl(m_db, m_dbName, key,
                    dev::zbytesConstRef(&body[0], body.size()));
}

int LevelDB::Insert(const leveldb::Slice& key, dev::zbytesConstRef value) {
  return InsertImpl(m_db, m_dbName, key, value);
}

int LevelDB::Insert(const dev::h256& key, const string& value) {
  return InsertImpl(m_db, m_dbName,
                    ldb::Slice((char const*)key.data(), key.size),
                    ldb::Slice(value.data(), value.size()));
}

int LevelDB::Insert(const dev::h256& key, const vector<unsigned char>& body) {
  return InsertImpl(m_db, m_dbName, key.hex(),
                    vector_ref<const unsigned char>(&body[0], body.size()));
}

int LevelDB::Insert(const leveldb::Slice& key, const leveldb::Slice& value) {
  return InsertImpl(m_db, m_dbName, key, value);
}

bool LevelDB::BatchInsert(
    const std::unordered_map<dev::h256, std::pair<std::string, unsigned>>&
        m_main,
    const std::unordered_map<dev::h256, std::pair<dev::zbytes, bool>>& m_aux,
    unordered_set<dev::h256>& inserted) {
  ldb::WriteBatch batch;

  for (const auto& i : m_main) {
    if (i.second.second || (LOOKUP_NODE_MODE && KEEP_HISTORICAL_STATE)) {
      batch.Put(leveldb::Slice(i.first.hex()),
                leveldb::Slice(i.second.first.data(), i.second.first.size()));
      if (i.second.second) {
        inserted.emplace(i.first);
      }
    }
  }

  for (const auto& i : m_aux) {
    if (i.second.second) {
      dev::zbytes b = i.first.asBytes();
      b.push_back(255);  // for aux
      batch.Put(dev::zbytesConstRef(&b), dev::zbytesConstRef(&i.second.first));
      inserted.emplace(i.first);
    }
  }

  if (!m_db) {
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " isn't available");
    return false;
  }

  ldb::Status s = m_db->Write(leveldb::WriteOptions(), &batch);

  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[BatchInsert] Status: " << s.ToString());
    return false;
  }

  return true;
}

bool LevelDB::BatchInsert(
    const std::unordered_map<std::string, std::string>& kv_map) {
  ldb::WriteBatch batch;

  for (const auto& i : kv_map) {
    if (!i.second.empty()) {
      batch.Put(leveldb::Slice(i.first), leveldb::Slice(i.second));
    }
  }

  if (!m_db) {
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " isn't available");
    return false;
  }

  ldb::Status s = m_db->Write(leveldb::WriteOptions(), &batch);

  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[BatchInsert] Status: " << s.ToString());
    return false;
  }

  return true;
}

bool LevelDB::BatchDelete(const std::vector<dev::h256>& toDelete) {
  ldb::WriteBatch batch;
  for (const auto& i : toDelete) {
    batch.Delete(leveldb::Slice(i.hex()));
  }

  if (!m_db) {
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " isn't available");
    return false;
  }

  ldb::Status s = m_db->Write(leveldb::WriteOptions(), &batch);

  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[BatchDelete] Status: " << s.ToString());
    return false;
  }

  return true;
}

bool LevelDB::Exists(const dev::h256& key) const {
  auto ret = Lookup(key);
  return !ret.empty();
}

bool LevelDB::Exists(const vector<unsigned char>& key) const {
  auto ret = Lookup(key);
  return !ret.empty();
}

bool LevelDB::Exists(const boost::multiprecision::uint256_t& blockNum) const {
  auto ret = Lookup(blockNum);
  return !ret.empty();
}

bool LevelDB::Exists(const std::string& key) const {
  auto ret = Lookup(key);
  return !ret.empty();
}

int LevelDB::DeleteKey(const dev::h256& key) {
  return DeleteImpl(m_db, m_dbName, key.hex());
}

int LevelDB::DeleteKey(const boost::multiprecision::uint256_t& blockNum) {
  return DeleteImpl(m_db, m_dbName, blockNum.convert_to<string>());
}

int LevelDB::DeleteKey(const std::string& key) {
  return DeleteImpl(m_db, m_dbName, key);
}

int LevelDB::DeleteKey(const vector<unsigned char>& key) {
  return DeleteImpl(m_db, m_dbName,
                    vector_ref<const unsigned char>(&key[0], key.size()));
}

int LevelDB::DeleteDB() {
  if (LOOKUP_NODE_MODE) {
    return DeleteDBForLookupNode();
  } else {
    return DeleteDBForNormalNode();
  }
}

bool LevelDB::ResetDB() {
  if (LOOKUP_NODE_MODE) {
    return ResetDBForLookupNode();
  } else {
    return ResetDBForNormalNode();
  }
}

bool LevelDB::RefreshDB() {
  m_db.reset();

  leveldb::Options options;
  options.max_open_files = 256;
  options.create_if_missing = true;

  leveldb::DB* db = nullptr;

  leveldb::Status status = leveldb::DB::Open(
      options, STORAGE_PATH + PERSISTENCE_PATH + "/" + this->m_dbName, &db);
  if (!status.ok()) {
    // throw exception();
    LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " status is not OK - "
                                    << status.ToString());
    return false;
  }

  m_db.reset(db);
  return true;
}

int LevelDB::DeleteDBForNormalNode() {
  m_db.reset();
  leveldb::Status s = leveldb::DestroyDB(
      STORAGE_PATH + PERSISTENCE_PATH +
          (this->m_subdirectory.size() ? "/" + this->m_subdirectory : "") +
          "/" + this->m_dbName,
      leveldb::Options());
  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[DeleteDB] Status: " << s.ToString());
    return -1;
  }

  if (this->m_subdirectory.size()) {
    std::filesystem::remove_all(STORAGE_PATH + PERSISTENCE_PATH + "/" +
                                this->m_subdirectory + "/" + this->m_dbName);
  }

  return 0;
}

bool LevelDB::ResetDBForNormalNode() {
  if (DeleteDBForNormalNode() == 0 && this->m_subdirectory.empty()) {
    std::filesystem::remove_all(STORAGE_PATH + PERSISTENCE_PATH + "/" +
                                this->m_dbName);

    leveldb::Options options;
    options.max_open_files = 256;
    options.create_if_missing = true;

    leveldb::DB* db = nullptr;

    leveldb::Status status = leveldb::DB::Open(
        options, STORAGE_PATH + PERSISTENCE_PATH + "/" + this->m_dbName, &db);
    if (!status.ok()) {
      // throw exception();
      LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " status is not OK - "
                                      << status.ToString());
    }

    m_db.reset(db);
    return true;
  } else if (this->m_subdirectory.size()) {
    LOG_GENERAL(INFO, "DB in subdirectory cannot be reset");
  }
  LOG_GENERAL(WARNING, "Didn't reset DB, investigate why!");
  return false;
}

int LevelDB::DeleteDBForLookupNode() {
  m_db.reset();
  leveldb::Status s = leveldb::DestroyDB(this->m_dbName, leveldb::Options());
  if (!s.ok()) {
    LOG_GENERAL(WARNING, "[DeleteDB] Status: " << s.ToString());
    return -1;
  }

  return 0;
}

bool LevelDB::ResetDBForLookupNode() {
  if (DeleteDBForLookupNode() == 0) {
    std::filesystem::remove_all(STORAGE_PATH + PERSISTENCE_PATH + "/" +
                                this->m_dbName);

    leveldb::Options options;
    options.max_open_files = 256;
    options.create_if_missing = true;

    leveldb::DB* db = nullptr;

    leveldb::Status status = leveldb::DB::Open(
        options, STORAGE_PATH + PERSISTENCE_PATH + "/" + this->m_dbName, &db);
    if (!status.ok()) {
      // throw exception();
      LOG_GENERAL(WARNING, "LevelDB " << m_dbName << " status is not OK - "
                                      << status.ToString());
      return false;
    }

    m_db.reset(db);
    return true;
  }
  return false;
}
