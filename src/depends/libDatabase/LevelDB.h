/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/


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
    std::shared_ptr<leveldb::DB> m_db;
    
public:

    /// Constructor.
    explicit LevelDB(const std::string & dbName);

    /// Destructor.
    ~LevelDB() = default;

    /// Returns the reference to the leveldb database instance.
    std::shared_ptr<leveldb::DB> GetDB();

    /// Returns the value at the specified key.
    std::string Lookup(const std::string & key) const;

    /// Returns the value at the specified key.
    std::string Lookup(const boost::multiprecision::uint256_t & blockNum) const;

    /// Returns the value at the specified key.
    std::string Lookup(const dev::h256 & key) const;

    /// Returns the value at the specified key.
    std::string Lookup(const dev::bytesConstRef & key) const;

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, dev::bytesConstRef value);

    /// Sets the value at the specified key.
    int Insert(const boost::multiprecision::uint256_t & blockNum, 
               const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const boost::multiprecision::uint256_t & blockNum, 
               const std::string & body);

    /// Sets the value at the specified key.
    int Insert(const std::string & key, const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const leveldb::Slice & key, dev::bytesConstRef value);

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, const std::string & value);

    /// Sets the value at the specified key.
    int Insert(const dev::h256 & key, const std::vector<unsigned char> & body);

    /// Sets the value at the specified key.
    int Insert(const leveldb::Slice & key, const leveldb::Slice & value);

    /// Sets the value at the specified key for multiple such pairs.
    int BatchInsert(std::unordered_map<dev::h256, std::pair<std::string, unsigned>> & m_main,
                    std::unordered_map<dev::h256, std::pair<dev::bytes, bool>> & m_aux);

    /// Returns true if value corresponding to specified key exists.
    bool Exists(const dev::h256 & key) const;
    bool Exists(const boost::multiprecision::uint256_t & blockNum) const;

    /// Deletes the value at the specified key.
    int DeleteKey(const dev::h256 & key);

    /// Deletes the entire database.
    int DeleteDB();

    /// Reset the entire database.
    bool ResetDB();
};

#endif // __LEVELDB_H__