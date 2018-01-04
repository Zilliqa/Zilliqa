/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/

#include <cassert>
#include <string>

#include <boost/filesystem.hpp>

#include "LevelDB.h"
#include "depends/common/Common.h"
#include "depends/common/CommonData.h"
#include "depends/common/FixedHash.h"

using namespace std;

LevelDB::LevelDB(const string & dbName)
{
    this->m_dbName = dbName;

    string path = "./persistence";
    boost::filesystem::create_directories(path);

    leveldb::Options options;
    options.max_open_files = 256;
    options.create_if_missing = true;

    leveldb::DB* db;

    leveldb::Status status = leveldb::DB::Open(options, path + "/" + this->m_dbName, &db);
    if(!status.ok())
    {
        throw exception();
    }

    m_db.reset(db);
}

leveldb::Slice toSlice(boost::multiprecision::uint256_t num)
{
    dev::FixedHash<32> h;
    dev::bytesRef ref(h.data(), 32);
    dev::toBigEndian(num, ref);
    return (leveldb::Slice)h.ref();
}

string LevelDB::Lookup(const std::string & key) const
{
    string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
    if (!s.ok())
    {
        // TODO
        return "";
    }
    
    return value;
}

string LevelDB::Lookup(const boost::multiprecision::uint256_t & blockNum) const
{
    string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), blockNum.convert_to<string>(), &value);

    if (!s.ok())
    {
        // TODO
        return "";
    }
    
    return value;
}

string LevelDB::Lookup(const dev::h256 & key) const
{
    string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), leveldb::Slice(key.hex()), &value);
    if (!s.ok())
    {
        // TODO
        return "";
    }
    
    return value;
}

string LevelDB::Lookup(const dev::bytesConstRef & key) const
{
    string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), ldb::Slice((char const*)key.data(), 32), 
                                  &value);
    if (!s.ok())
    {
        // TODO
        return "";
    }
    
    return value;
}

std::shared_ptr<leveldb::DB> LevelDB::GetDB()
{
    return this->m_db;
}

int LevelDB::Insert(const dev::h256 & key, dev::bytesConstRef value)
{
    return Insert(key, value.toString());
}

int LevelDB::Insert(const boost::multiprecision::uint256_t & blockNum, 
                    const vector<unsigned char> & body)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), 
                                  leveldb::Slice(blockNum.convert_to<string>()), 
                                  leveldb::Slice(vector_ref<const unsigned char>(&body[0], 
                                                                                 body.size())));

    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

int LevelDB::Insert(const boost::multiprecision::uint256_t & blockNum, 
                    const std::string & body)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), 
                                  leveldb::Slice(blockNum.convert_to<string>()), 
                                  leveldb::Slice(body.c_str(), body.size()));

    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

int LevelDB::Insert(const string & key, const vector<unsigned char> & body)
{
    return Insert(leveldb::Slice(key), leveldb::Slice(dev::bytesConstRef(&body[0], body.size())));
}

int LevelDB::Insert(const leveldb::Slice & key, dev::bytesConstRef value)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, ldb::Slice(value));
    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

int LevelDB::Insert(const dev::h256 & key, const string & value)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), 
                                  ldb::Slice((char const*)key.data(), key.size), 
                                  ldb::Slice(value.data(), value.size()));
    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

int LevelDB::Insert(const dev::h256 & key, const vector<unsigned char> & body)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), leveldb::Slice(key.hex()), 
                                  leveldb::Slice(vector_ref<const unsigned char>(&body[0], 
                                                                                 body.size())));
    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

int LevelDB::Insert(const leveldb::Slice & key, const leveldb::Slice & value)
{
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
    if (!s.ok())
    {
        return -1;
    }
    
    return 0;    
}

int LevelDB::BatchInsert(std::unordered_map<dev::h256, std::pair<std::string, unsigned>> & m_main,
                         std::unordered_map<dev::h256, std::pair<dev::bytes, bool>> & m_aux)
{
    ldb::WriteBatch batch;

    for (const auto & i: m_main)
    {
        if (i.second.second)
        {
            batch.Put(leveldb::Slice(i.first.hex()), 
                      leveldb::Slice(i.second.first.data(), i.second.first.size()));
        }
    }
    
    for (const auto & i: m_aux)
    {
        if (i.second.second)
        {
            dev::bytes b = i.first.asBytes();
            b.push_back(255);   // for aux
            batch.Put(dev::bytesConstRef(&b), dev::bytesConstRef(&i.second.first));
        }
    }

    ldb::Status s = m_db->Write(leveldb::WriteOptions(), &batch);

    if (!s.ok())
    {
        return -1;
    }
    
    return 0;
}

bool LevelDB::Exists(const dev::h256 & key) const
{
    auto ret = Lookup(key);
    return !ret.empty();
}

bool LevelDB::Exists(const boost::multiprecision::uint256_t & blockNum) const
{
    auto ret = Lookup(blockNum);
    return !ret.empty();
}

int LevelDB::DeleteKey(const dev::h256 & key)
{
    leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), ldb::Slice(key.hex()));
    if (!s.ok())
    {
        return -1;
    }

    return 0;
}

int LevelDB::DeleteDB()
{
    m_db.reset();
    leveldb::Status s = leveldb::DestroyDB(this->m_dbName, leveldb::Options()); 
    if (!s.ok())
    {
        //LOG_MESSAGE("[DeleteDB] Status: " << s.ToString());
        return -1;
    }

    return 0;
}
