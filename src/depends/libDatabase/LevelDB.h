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


#ifndef __LEVELDB_H__
#define __LEVELDB_H__

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <leveldb/db.h>

#include "depends/common/Common.h"
#include "depends/common/FixedHash.h"
//#include "libUtils/Logger.h"

leveldb::Slice toSlice(boost::multiprecision::uint256_t num);

/// Utility class for providing database-type storage.
class LevelDB
{
    std::string m_dbName;

    std::string m_subdirectory;

    std::shared_ptr<leveldb::DB> m_db;

    leveldb::Options m_options;

    std::string m_open_db_path;

    void log_error(leveldb::Status status) const;

public:

    /// Constructor.
    explicit LevelDB(const std::string & dbName, const std::string& subdirectory = "", bool diagnostic = false);
    explicit LevelDB(const std::string& dbName, const std::string& path, const std::string& subdirectory = "");
    /// Destructor.
    ~LevelDB() = default;

    /// manually compact, might be helpful in future
    void compact () {
        m_db->CompactRange(NULL, NULL);
    }

    /// Reopen the leveldb object to trigger compact and cleaning of LOG/MANIFEST files
    void Reopen();

    /// Returns the reference to the leveldb database instance.
    std::shared_ptr<leveldb::DB> GetDB();

    /// Returns the DB Name
    std::string GetDBName();

    /// Returns the value at the specified key.
    std::string Lookup(const std::string & key) const;

    /// Returns the value at the specified key.
    std::string Lookup(const std::vector<unsigned char>& key) const;

    /// Returns the value at the specified key.
    std::string Lookup(const boost::multiprecision::uint256_t & blockNum) const;

    /// Returns the value at the specified key and also mark if key was found or not
    std::string Lookup(const boost::multiprecision::uint256_t & blockNum, bool &found) const;

    /// Returns the value at the specified key.
    std::string Lookup(const dev::h256 & key) const;

    /// Returns the value at the specified key.
    std::string Lookup(const dev::zbytesConstRef & key) const;

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, dev::zbytesConstRef value);

    /// Sets the value at the specified key.
    int Insert(const std::vector<unsigned char>& key,
               const std::vector<unsigned char>& body);

    /// Sets the value at the specified key.
    int Insert(const boost::multiprecision::uint256_t & blockNum,
               const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const boost::multiprecision::uint256_t & blockNum,
               const std::string & body);

    /// Sets the value at the specified key.
    int Insert(const std::string & key, const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const leveldb::Slice & key, dev::zbytesConstRef value);

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, const std::string & value);

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const leveldb::Slice & key, const leveldb::Slice & value);

    /// Sets the value at the specified key for multiple such pairs.
    bool BatchInsert(const std::unordered_map<dev::h256, std::pair<std::string, unsigned>> & m_main,
                     const std::unordered_map<dev::h256, std::pair<dev::zbytes, bool>> & m_aux, std::unordered_set<dev::h256>& inserted);
    bool BatchInsert(const std::unordered_map<std::string, std::string>& kv_map);

    /// Remove the kv pair for multiple specified key.
    bool BatchDelete(const std::vector<dev::h256>& toDelete);

    /// Returns true if value corresponding to specified key exists.
    bool Exists(const dev::h256 & key) const;
    bool Exists(const boost::multiprecision::uint256_t & blockNum) const;
    bool Exists(const std::string & key) const;
    bool Exists(const std::vector<unsigned char> & key) const;

    /// Deletes the value at the specified key.
    int DeleteKey(const dev::h256 & key);

    /// Deletes the value at the specified key.
    int DeleteKey(const boost::multiprecision::uint256_t & blockNum);

    /// Deletes the value at the specified key.
    int DeleteKey(const std::string & key);

    /// Deletes the value at the specified key.
    int DeleteKey(const std::vector<unsigned char> & key);

    /// Deletes the entire database.
    int DeleteDB();
    int DeleteDBForNormalNode();
    int DeleteDBForLookupNode();

    /// Reset the entire database.
    bool ResetDB();

    /// Refresh the entire database.
    bool RefreshDB();

private:
    bool ResetDBForNormalNode();
    bool ResetDBForLookupNode();
};

#endif // __LEVELDB_H__
