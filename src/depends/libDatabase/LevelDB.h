/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
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
    
#ifndef IS_LOOKUP_NODE
    std::string m_subdirectory;
#endif // IS_LOOKUP_NODE

    std::shared_ptr<leveldb::DB> m_db;
    
public:

    /// Constructor.
#ifndef IS_LOOKUP_NODE
    explicit LevelDB(const std::string & dbName, const std::string & subdirectory = "");
#else //IS_LOOKUP_NODE
    explicit LevelDB(const std::string & dbName);
#endif //IS_LOOKUP_NODE

    /// Destructor.
    ~LevelDB() = default;

    /// Returns the reference to the leveldb database instance.
    std::shared_ptr<leveldb::DB> GetDB();

#ifndef IS_LOOKUP_NODE
    std::string GetDBName() { return m_dbName + (m_subdirectory.size() > 0 ? "/" : "") + m_subdirectory; }
#else //IS_LOOKUP_NODE
    std::string GetDBName() { return m_dbName; }
#endif //IS_LOOKUP_NODE

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
    bool Exists(const std::string & key) const;

    /// Deletes the value at the specified key.
    int DeleteKey(const dev::h256 & key);

    /// Deletes the value at the specified key.
    int DeleteKey(const boost::multiprecision::uint256_t & blockNum);

    /// Deletes the value at the specified key.
    int DeleteKey(const std::string & key);

    /// Deletes the entire database.
    int DeleteDB();

    /// Reset the entire database.
    bool ResetDB();
};

#endif // __LEVELDB_H__