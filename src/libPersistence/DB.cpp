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

#include <cassert>
#include <string>

#include "DB.h"

using namespace std;

DB::DB(const string& name)
{
    this->m_db_name = name;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status
        = leveldb::DB::Open(options, this->m_db_name, &this->m_db);
    if (!status.ok())
    {
        LOG_GENERAL(WARNING, "Cannot init DB.");
        // throw exception();
    }
}

DB::~DB() { delete m_db; }

string DB::ReadFromDB(const string& key)
{
    string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
    if (!s.ok())
    {
        return "DB_ERROR";
    }
    else
    {
        return value;
    }
}

leveldb::DB* DB::GetDB() { return this->m_db; }

int DB::WriteToDB(const string& key, const string& value)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
    if (!s.ok())
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int DB::DeleteFromDB(const string& key)
{
    leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), key);
    if (!s.ok())
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int DB::DeleteDB()
{
    delete m_db;
    leveldb::Status s = leveldb::DestroyDB(this->m_db_name, leveldb::Options());
    if (!s.ok())
    {
        LOG_GENERAL(INFO, "Status: " << s.ToString());
        return -1;
    }
    else
    {
        return 0;
    }
}
