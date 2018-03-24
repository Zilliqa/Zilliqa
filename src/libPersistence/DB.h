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

#ifndef DB_H
#define DB_H

#include <leveldb/db.h>
#include <libUtils/Logger.h>
#include <string>

/// Utility class for providing database-type storage.
class DB
{
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

#endif // DB_H