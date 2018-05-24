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

#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "BlockStorage.h"

using namespace std;

unsigned int BlockStorage::m_blockFileSize = 128 * ONE_MEGABYTE;

static unsigned int GetInt(const char* src, int& pos)
{
    unsigned int result = 0;
    while ((src[pos] != '\0') && (src[pos] != '.'))
    {
        result = (result * 10) + (src[pos++] - '0');
    }
    if (src[pos] == '.')
    {
        pos++;
    }
    return result;
}

static string ConvertUInt256ToString(boost::multiprecision::uint256_t number)
{
    if (number == 0)
    {
        return "0";
    }
    string result = "";
    while (number != 0)
    {
        result += to_string((unsigned int)number % 10);
        number /= 10;
    }
    reverse(result.begin(), result.end());
    return result;
}

void BlockStorage::SetBlockFileSize(unsigned int blockFileSize)
{
    m_blockFileSize = blockFileSize;
}

unsigned int BlockStorage::GetBlockFileSize() { return m_blockFileSize; }

/// Stores information on the last generated block file.
class BlockStorage::LastBlockFileInfo
{
public:
    unsigned int filenum;

    LastBlockFileInfo(const string& src)
    {
        int pos = 0;
        filenum = GetInt(src.c_str(), pos);
    }

    LastBlockFileInfo(unsigned int filenum) { this->filenum = filenum; }

    string ToString() const
    {
        char buf[16] = {0};
        snprintf(buf, sizeof(buf), "%09d", filenum);
        return buf;
    }

    static string GenerateKey(BlockType blockType)
    {
        if (blockType == BlockType::DS)
            return "dl";
        return "tl";
    }
};

/// Stores information on a block file.
class BlockStorage::BlockFileInfo
{
public:
    unsigned int numblocks;
    unsigned int filesize;

    BlockFileInfo(const string& src)
    {
        int pos = 0;
        numblocks = GetInt(src.c_str(), pos);
        filesize = GetInt(src.c_str(), pos);
    }

    BlockFileInfo(unsigned int numblocks, unsigned int filesize)
    {
        this->numblocks = numblocks;
        this->filesize = filesize;
    }

    string ToString() const
    {
        return to_string(numblocks) + "." + to_string(filesize);
    }

    static string GenerateKey(int filenum, BlockType blockType)
    {
        char buf[16] = {0};
        if (blockType == BlockType::DS)
        {
            snprintf(buf, sizeof(buf), "dsf%09d", filenum);
        }
        else if (blockType == BlockType::Tx)
        {
            snprintf(buf, sizeof(buf), "txf%09d", filenum);
        }
        return buf;
    }

    static string GenerateFilename(int filenum, BlockType blockType)
    {
        char buf[32] = {0};
        if (blockType == BlockType::DS)
        {
            snprintf(buf, sizeof(buf), "blocks/ds/blk%09d.bin", filenum);
        }
        else if (blockType == BlockType::Tx)
        {
            snprintf(buf, sizeof(buf), "blocks/tx/blk%09d.bin", filenum);
        }
        return buf;
    }
};

/// Stores information on the location and size of a stored block.
class BlockStorage::BlockInfo
{
public:
    unsigned int filenum;
    unsigned int fileoffset;
    unsigned int blocksize;
    unsigned int decompressed_blocksize;

    BlockInfo(const string& src)
    {
        int pos = 0;
        filenum = GetInt(src.c_str(), pos);
        fileoffset = GetInt(src.c_str(), pos);
        blocksize = GetInt(src.c_str(), pos);
        decompressed_blocksize = GetInt(src.c_str(), pos);
    }

    BlockInfo(unsigned int filenum, unsigned int fileoffset,
              unsigned int blocksize, unsigned int decompressed_blocksize)
    {
        this->filenum = filenum;
        this->fileoffset = fileoffset;
        this->blocksize = blocksize;
        this->decompressed_blocksize = decompressed_blocksize;
    }

    string ToString()
    {
        char buf[64] = {0};
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", filenum, fileoffset,
                 blocksize, decompressed_blocksize);
        return buf;
    }

    static string GenerateKey(boost::multiprecision::uint256_t blocknum,
                              BlockType blockType)
    {
        string result = "";
        if (blockType == BlockType::DS)
        {
            result += ("dsb" + ConvertUInt256ToString(blocknum));
        }
        else if (blockType == BlockType::Tx)
        {
            result += ("txb" + ConvertUInt256ToString(blocknum));
        }
        return result;
    }
};

BlockStorage& BlockStorage::GetBlockStorage()
{
    static BlockStorage bs;
    return bs;
}

BlockStorage::BlockStorage()
    : m_metadataDB("blocks.db")
    , m_txBodyDB("txbodies.db")
    , m_dsBlockchainDB("dsblocks.db")
    , m_txBlockchainDB("txblocks.db")
{
    struct stat s;
    string tmp = m_metadataDB.ReadFromDB(
        LastBlockFileInfo::GenerateKey(BlockType::DS));

    // Couldn't find key "dl" (or it doesn't exist) and couldn't stat Blocks/ds/Blk00001.bin --> new system
    if ((strcmp(tmp.c_str(), "DB_ERROR") == 0)
        && (stat(BlockFileInfo::GenerateFilename(1, BlockType::DS).c_str(), &s)
            != 0))
    {
        LastBlockFileInfo lbfi(1);
        if (m_metadataDB.WriteToDB(
                LastBlockFileInfo::GenerateKey(BlockType::DS), lbfi.ToString())
            != 0)
        {
            // return false;
        }

        BlockFileInfo bfi(0, 0);
        if (m_metadataDB.WriteToDB(BlockFileInfo::GenerateKey(1, BlockType::DS),
                                   bfi.ToString())
            != 0)
        {
            // return false;
        }

        if (!((stat("./blocks", &s) == 0) && (S_ISDIR(s.st_mode))))
        {
            // Create Blocks directory
            mkdir("./blocks", 0700);
        }

        if (!((stat("./blocks/ds", &s) == 0) && (S_ISDIR(s.st_mode))))
        {
            // Create Blocks directory
            mkdir("./blocks/ds", 0700);
        }
    }

    tmp = m_metadataDB.ReadFromDB(
        LastBlockFileInfo::GenerateKey(BlockType::Tx));
    // Couldn't find key "tl" (or it doesn't exist) and couldn't stat Blocks/tx/Blk00001.bin --> new system
    if ((strcmp(tmp.c_str(), "DB_ERROR") == 0)
        && (stat(BlockFileInfo::GenerateFilename(1, BlockType::Tx).c_str(), &s)
            != 0))
    {
        LastBlockFileInfo lbfi(1);
        if (m_metadataDB.WriteToDB(
                LastBlockFileInfo::GenerateKey(BlockType::Tx), lbfi.ToString())
            != 0)
        {
            // return false;
        }

        BlockFileInfo bfi(0, 0);
        if (m_metadataDB.WriteToDB(BlockFileInfo::GenerateKey(1, BlockType::Tx),
                                   bfi.ToString())
            != 0)
        {
            // return false;
        }

        if (!((stat("./blocks", &s) == 0) && (S_ISDIR(s.st_mode))))
        {
            // Create Blocks directory
            mkdir("./blocks", 0700);
        }

        if (!((stat("./blocks/tx", &s) == 0) && (S_ISDIR(s.st_mode))))
        {
            // Create Blocks directory
            mkdir("./blocks/tx", 0700);
        }
    }

    // Allocate cache
    m_dsblock_cache.resize(BlockStorage::m_numCachedBlocks,
                           make_pair(0, nullptr));
    m_txblock_cache.resize(BlockStorage::m_numCachedBlocks,
                           make_pair(0, nullptr));

    // Todo: change it to level db after junhao feels confident about testing prev work
    m_blockStorageType = BlockStorageType::None;
}

void BlockStorage::AddBlockToDSCache(
    const boost::multiprecision::uint256_t& blockNum,
    const vector<unsigned char>& block)
{
    std::unique_lock<std::shared_timed_mutex> lock(m_dsBlockCacheMutex);
    // Select first NULL entry, else select last entry
    list<pair<boost::multiprecision::uint256_t, DSBlockSharedPtr>>::iterator
        iter,
        iter_prev;
    for (iter = m_dsblock_cache.begin(); iter != m_dsblock_cache.end(); iter++)
    {
        if (iter->second == nullptr)
        {
            break;
        }
        iter_prev = iter;
    }
    if (iter == m_dsblock_cache.end())
    {
        LOG_GENERAL(INFO, "DEBUG: Evicting LRU block to cache");
        iter = iter_prev;
    }

    // Put block into the selected entry
    iter->first = blockNum;
    iter->second = DSBlockSharedPtr(new DSBlock(block, 0));

    // Move selected entry to front of list (most recently accessed)
    if (iter != m_dsblock_cache.begin())
    {
        list<pair<boost::multiprecision::uint256_t, DSBlockSharedPtr>>::iterator
            iter_next
            = iter;
        iter_next++;
        m_dsblock_cache.splice(m_dsblock_cache.begin(), m_dsblock_cache, iter,
                               iter_next);
    }
}

void BlockStorage::AddBlockToTxCache(
    const boost::multiprecision::uint256_t& blockNum,
    const vector<unsigned char>& block)
{
    std::unique_lock<std::shared_timed_mutex> lock(m_txBlockCacheMutex);
    // Select first NULL entry, else select last entry
    list<pair<boost::multiprecision::uint256_t, TxBlockSharedPtr>>::iterator
        iter,
        iter_prev;
    for (iter = m_txblock_cache.begin(); iter != m_txblock_cache.end(); iter++)
    {
        if (iter->second == NULL)
        {
            break;
        }
        iter_prev = iter;
    }
    if (iter == m_txblock_cache.end())
    {
        LOG_GENERAL(INFO, "DEBUG: Evicting LRU block to cache");
        iter = iter_prev;
    }

    // Put block into the selected entry
    iter->first = blockNum;
    iter->second = TxBlockSharedPtr(new TxBlock(block, 0));

    // Move selected entry to front of list (most recently accessed)
    if (iter != m_txblock_cache.begin())
    {
        list<pair<boost::multiprecision::uint256_t, TxBlockSharedPtr>>::iterator
            iter_next
            = iter;
        iter_next++;
        m_txblock_cache.splice(m_txblock_cache.begin(), m_txblock_cache, iter,
                               iter_next);
    }
}

bool BlockStorage::PutBlockToDisk(
    const boost::multiprecision::uint256_t& blockNum,
    const vector<unsigned char>& block, const BlockType& blockType)
{
    // pid_t tid = syscall(SYS_gettid);

    // THREADLOG_MARKER(tid);

    std::unique_lock<std::shared_timed_mutex> lock(m_putBlockMutex);

    string tmp = "";

    // Get db entry on last block file
    if (blockType == BlockType::DS)
    {
        tmp = m_metadataDB.ReadFromDB(
            LastBlockFileInfo::GenerateKey(BlockType::DS));
    }
    else if (blockType == BlockType::Tx)
    {
        tmp = m_metadataDB.ReadFromDB(
            LastBlockFileInfo::GenerateKey(BlockType::Tx));
    }

    if (strcmp(tmp.c_str(), "DB_ERROR") == 0)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    LastBlockFileInfo lbfi(tmp);

    // Get db entry on last block file info
    if (blockType == BlockType::DS)
    {
        tmp = m_metadataDB.ReadFromDB(
            BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::DS));
    }
    else if (blockType == BlockType::Tx)
    {
        tmp = m_metadataDB.ReadFromDB(
            BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::Tx));
    }
    if (strcmp(tmp.c_str(), "DB_ERROR") == 0)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    BlockFileInfo bfi(tmp);

    // Compress the block
    shared_ptr<unsigned char> compressed_block;
    lzo_uint compressed_block_len = 0;
    bool res = Compress(&block[0], block.size(), compressed_block,
                        compressed_block_len);
    if (res == false)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    // Check available size of last block file
    if ((bfi.filesize > 0)
        && ((compressed_block_len + bfi.filesize)
            > BlockStorage::GetBlockFileSize()))
    {
        string blockFileInfoFileName = "";

        if (blockType == BlockType::DS)
            blockFileInfoFileName
                = BlockFileInfo::GenerateFilename(lbfi.filenum, BlockType::DS);
        else if (blockType == BlockType::Tx)
            blockFileInfoFileName
                = BlockFileInfo::GenerateFilename(lbfi.filenum, BlockType::Tx);

        // Put block into new file
        lbfi.filenum++;
        try
        {
            ofstream ofs(blockFileInfoFileName.c_str(), ios::out | ios::binary);
            LOG_GENERAL(INFO, "DEBUG: Writing compressed block to disk");
            ofs.write((const char*)compressed_block.get(),
                      compressed_block_len);
            ofs.close();
        }
        catch (...)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Add the db entry for the block info
        BlockInfo bi(lbfi.filenum, 0, compressed_block_len, block.size() + 1);

        string blockInfoKey = "";

        if (blockType == BlockType::DS)
        {
            blockInfoKey = BlockInfo::GenerateKey(blockNum, BlockType::DS);
        }
        else if (blockType == BlockType::Tx)
        {
            blockInfoKey = BlockInfo::GenerateKey(blockNum, BlockType::Tx);
        }

        if (m_metadataDB.WriteToDB(blockInfoKey, bi.ToString()) != 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Add the db entry for the block file info
        bfi.numblocks = 1;
        bfi.filesize = compressed_block_len;

        string blockFileInfoKey = "";
        string lastBlockFileInfoKey = "";

        if (blockType == BlockType::DS)
        {
            blockFileInfoKey
                = BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::DS);
            lastBlockFileInfoKey
                = LastBlockFileInfo::GenerateKey(BlockType::DS);
        }
        else if (blockType == BlockType::Tx)
        {
            blockFileInfoKey
                = BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::Tx);
            lastBlockFileInfoKey
                = LastBlockFileInfo::GenerateKey(BlockType::Tx);
        }

        if (m_metadataDB.WriteToDB(blockFileInfoKey, bfi.ToString()) != 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Update info on last block file
        if (m_metadataDB.WriteToDB(lastBlockFileInfoKey, lbfi.ToString()) != 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }
    }
    else
    {
        string blockFileInfoFilename = "";

        if (blockType == BlockType::DS)
        {
            blockFileInfoFilename
                = BlockFileInfo::GenerateFilename(lbfi.filenum, BlockType::DS);
        }
        else if (blockType == BlockType::Tx)
        {
            blockFileInfoFilename
                = BlockFileInfo::GenerateFilename(lbfi.filenum, BlockType::Tx);
        }

        // Append block to current file
        try
        {
            ofstream ofs(blockFileInfoFilename.c_str(),
                         ios::out | ios::binary | ios::app);
            ofs.seekp(bfi.filesize, ios_base::beg);
            ofs.write((const char*)compressed_block.get(),
                      compressed_block_len);
            ofs.close();
        }
        catch (...)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Add the db entry for the block info
        BlockInfo bi(lbfi.filenum, bfi.filesize, compressed_block_len,
                     block.size() + 1);

        string blockInfoKey = "";

        if (blockType == BlockType::DS)
        {
            blockInfoKey = BlockInfo::GenerateKey(blockNum, BlockType::DS);
        }
        else if (blockType == BlockType::Tx)
        {
            blockInfoKey = BlockInfo::GenerateKey(blockNum, BlockType::Tx);
        }

        if (m_metadataDB.WriteToDB(blockInfoKey, bi.ToString()) != 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Update the db entry for the block file info
        bfi.numblocks++;
        bfi.filesize += compressed_block_len;

        string blockFileInfoKey = "";

        if (blockType == BlockType::DS)
        {
            blockFileInfoKey
                = BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::DS);
        }
        else if (blockType == BlockType::Tx)
        {
            blockFileInfoKey
                = BlockFileInfo::GenerateKey(lbfi.filenum, BlockType::Tx);
        }

        if (m_metadataDB.WriteToDB(blockFileInfoKey, bfi.ToString()) != 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }
    }

    lock.unlock();

    // Put same block into cache

    if (blockType == BlockType::DS)
    {
        AddBlockToDSCache(blockNum, block);
    }
    else if (blockType == BlockType::Tx)
    {
        AddBlockToTxCache(blockNum, block);
    }

    return true;
}

bool BlockStorage::GetDSBlockFromDisk(
    const boost::multiprecision::uint256_t& blocknum, DSBlockSharedPtr& block)
{
    // pid_t tid = syscall(SYS_gettid);

    // THREADLOG_MARKER(tid);

    std::unique_lock<std::shared_timed_mutex> lock(m_dsBlockCacheMutex);
    // Check if block is available in cache
    list<pair<boost::multiprecision::uint256_t, DSBlockSharedPtr>>::iterator
        iter;
    for (iter = m_dsblock_cache.begin(); iter != m_dsblock_cache.end(); iter++)
    {
        if (iter->first == blocknum)
        {
            break;
        }
    }

    if (iter != m_dsblock_cache.end())
    {
        LOG_GENERAL(INFO, "DEBUG: Reading block from cache");

        // Block is in cache
        block = DSBlockSharedPtr(new DSBlock(*(iter->second)));

        // Move selected entry to front of list (most recently accessed)
        if (iter != m_dsblock_cache.begin())
        {
            list<pair<boost::multiprecision::uint256_t,
                      DSBlockSharedPtr>>::iterator iter_next
                = iter;
            iter_next++;
            m_dsblock_cache.splice(m_dsblock_cache.begin(), m_dsblock_cache,
                                   iter, iter_next);
        }
    }
    else
    {
        // Block is not in cache -- get block from file

        // Get db entry for the block info
        string tmp = m_metadataDB.ReadFromDB(
            BlockInfo::GenerateKey(blocknum, BlockType::DS));
        if (strcmp(tmp.c_str(), "DB_ERROR") == 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        BlockInfo bi(tmp);

        // Read the block from the file
        LOG_GENERAL(INFO, "DEBUG: Reading compressed block from disk");
        unique_ptr<unsigned char> buf{new unsigned char[bi.blocksize]};

        vector<unsigned char> blockVector;
        try
        {
            //cout << "Opening " << BlockFileInfo::GenerateFilename(bi.filenum, BlockType::DS) << endl;
            ifstream ifs(
                BlockFileInfo::GenerateFilename(bi.filenum, BlockType::DS)
                    .c_str(),
                ios::in | ios::binary);
            ifs.seekg(bi.fileoffset, ios_base::beg);
            ifs.read((char*)buf.get(), bi.blocksize);
            ifs.close();
        }
        catch (...)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Decompress the block
        lzo_uint compressed_block_len = bi.blocksize;
        blockVector.resize(bi.decompressed_blocksize);
        bool res = Decompress(buf.get(), compressed_block_len, &blockVector[0]);

        if (res == false)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }
        block = DSBlockSharedPtr(new DSBlock(blockVector, 0));

        // Put the same block into cache

        // Select first NULL entry, else select last entry
        list<pair<boost::multiprecision::uint256_t, DSBlockSharedPtr>>::iterator
            iter_prev;
        for (iter = m_dsblock_cache.begin(); iter != m_dsblock_cache.end();
             iter++)
        {
            if (iter->second == NULL)
            {
                break;
            }
            iter_prev = iter;
        }
        if (iter == m_dsblock_cache.end())
        {
            iter = iter_prev;
        }

        // Put block into the selected entry
        iter->first = block->GetHeader().GetBlockNum();

        iter->second = DSBlockSharedPtr(new DSBlock(blockVector, 0));

        // Move selected entry to front of list (most recently accessed)
        if (iter != m_dsblock_cache.begin())
        {
            list<pair<boost::multiprecision::uint256_t,
                      DSBlockSharedPtr>>::iterator iter_next
                = iter;
            iter_next++;
            m_dsblock_cache.splice(m_dsblock_cache.begin(), m_dsblock_cache,
                                   iter, iter_next);
        }
    }

    return true;
}

bool BlockStorage::GetTxBlockFromDisk(
    const boost::multiprecision::uint256_t& blocknum, TxBlockSharedPtr& block)
{
    // pid_t tid = syscall(SYS_gettid);

    // THREADLOG_MARKER(tid);

    std::unique_lock<std::shared_timed_mutex> lock(m_txBlockCacheMutex);
    // Check if block is available in cache
    list<pair<boost::multiprecision::uint256_t, TxBlockSharedPtr>>::iterator
        iter;
    for (iter = m_txblock_cache.begin(); iter != m_txblock_cache.end(); iter++)
    {
        if (iter->first == blocknum)
        {
            break;
        }
    }

    if (iter != m_txblock_cache.end())
    {
        LOG_GENERAL(INFO, "DEBUG: Reading block from cache");

        // Block is in cache
        block = TxBlockSharedPtr(new TxBlock(*(iter->second)));

        // Move selected entry to front of list (most recently accessed)
        if (iter != m_txblock_cache.begin())
        {
            list<pair<boost::multiprecision::uint256_t,
                      TxBlockSharedPtr>>::iterator iter_next
                = iter;
            iter_next++;
            m_txblock_cache.splice(m_txblock_cache.begin(), m_txblock_cache,
                                   iter, iter_next);
        }
    }
    else
    {
        // Block is not in cache -- get block from file

        // Get db entry for the block info
        string tmp = m_metadataDB.ReadFromDB(
            BlockInfo::GenerateKey(blocknum, BlockType::Tx));
        if (strcmp(tmp.c_str(), "DB_ERROR") == 0)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        BlockInfo bi(tmp);

        // Read the block from the file
        LOG_GENERAL(INFO, "DEBUG: Reading compressed block from disk");

        std::unique_ptr<unsigned char[]> buf{new unsigned char[bi.blocksize]};

        try
        {
            ifstream ifs(
                BlockFileInfo::GenerateFilename(bi.filenum, BlockType::Tx)
                    .c_str(),
                ios::in | ios::binary);
            ifs.seekg(bi.fileoffset, ios_base::beg);
            ifs.read((char*)buf.get(), bi.blocksize);
            ifs.close();
        }
        catch (...)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }

        // Decompress the block
        lzo_uint compressed_block_len = bi.blocksize;

        vector<unsigned char> blockVector;
        blockVector.resize(bi.decompressed_blocksize);
        bool res = Decompress(buf.get(), compressed_block_len, &blockVector[0]);

        if (res == false)
        {
            // THREAD// LOG_ERROR(tid);
            return false;
        }
        block = TxBlockSharedPtr(new TxBlock(blockVector, 0));

        // Put the same block into cache

        // Select first NULL entry, else select last entry
        list<pair<boost::multiprecision::uint256_t, TxBlockSharedPtr>>::iterator
            iter_prev;
        for (iter = m_txblock_cache.begin(); iter != m_txblock_cache.end();
             iter++)
        {
            if (iter->second == NULL)
            {
                break;
            }
            iter_prev = iter;
        }
        if (iter == m_txblock_cache.end())
        {
            iter = iter_prev;
        }

        // Put block into the selected entry
        iter->first = block->GetHeader().GetBlockNum();

        iter->second = TxBlockSharedPtr(new TxBlock(blockVector, 0));

        // Move selected entry to front of list (most recently accessed)
        if (iter != m_txblock_cache.begin())
        {
            list<pair<boost::multiprecision::uint256_t,
                      TxBlockSharedPtr>>::iterator iter_next
                = iter;
            iter_next++;
            m_txblock_cache.splice(m_txblock_cache.begin(), m_txblock_cache,
                                   iter, iter_next);
        }
    }

    return true;
}

bool BlockStorage::PutBlockToLevelDB(
    const boost::multiprecision::uint256_t& blockNum,
    const vector<unsigned char>& body, const BlockType& blockType)
{
    string key = ConvertUInt256ToString(blockNum);
    int ret;
    if (blockType == BlockType::DS)
    {
        ret = m_dsBlockchainDB.WriteToDB(
            key,
            std::string(reinterpret_cast<const char*>(&body[0]), body.size()));
    }
    else if (blockType == BlockType::Tx)
    {
        ret = m_txBlockchainDB.WriteToDB(
            key,
            std::string(reinterpret_cast<const char*>(&body[0]), body.size()));
    }
    return (ret == 0);
}

bool BlockStorage::GetDSBlockFromLevelDB(
    const boost::multiprecision::uint256_t& blockNum, DSBlockSharedPtr& block)
{
    string key = ConvertUInt256ToString(blockNum);
    string blockString = m_dsBlockchainDB.ReadFromDB(key);
    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(blockString.c_str());
    block = DSBlockSharedPtr(new DSBlock(
        std::vector<unsigned char>(raw_memory, raw_memory + blockString.size()),
        0));
    return true;
}

bool BlockStorage::GetTxBlockFromLevelDB(
    const boost::multiprecision::uint256_t& blockNum, TxBlockSharedPtr& block)
{
    string key = ConvertUInt256ToString(blockNum);
    string blockString = m_txBlockchainDB.ReadFromDB(key);
    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(blockString.c_str());
    block = TxBlockSharedPtr(new TxBlock(
        std::vector<unsigned char>(raw_memory, raw_memory + blockString.size()),
        0));
    return true;
}

bool BlockStorage::PutDSBlock(const boost::multiprecision::uint256_t& blockNum,
                              const vector<unsigned char>& body)
{
    switch (m_blockStorageType)
    {
    case BlockStorageType::FileSystem:
        return PutBlockToDisk(blockNum, body, BlockType::DS);
    case BlockStorageType::LevelDB:
        return PutBlockToLevelDB(blockNum, body, BlockType::DS);
    case BlockStorageType::None:
        return false;
    }
    return false;
}

bool BlockStorage::PutTxBlock(const boost::multiprecision::uint256_t& blockNum,
                              const vector<unsigned char>& body)
{
    switch (m_blockStorageType)
    {
    case BlockStorageType::FileSystem:
        return PutBlockToDisk(blockNum, body, BlockType::Tx);
    case BlockStorageType::LevelDB:
        return PutBlockToLevelDB(blockNum, body, BlockType::Tx);
    case BlockStorageType::None:
        return false;
    }
    return false;
}

bool BlockStorage::GetDSBlock(const boost::multiprecision::uint256_t& blockNum,
                              DSBlockSharedPtr& block)
{
    switch (m_blockStorageType)
    {
    case BlockStorageType::FileSystem:
        return GetDSBlockFromDisk(blockNum, block);
    case BlockStorageType::LevelDB:
        return GetDSBlockFromLevelDB(blockNum, block);
    case BlockStorageType::None:
        return false;
    }
    return false;
}

bool BlockStorage::GetTxBlock(const boost::multiprecision::uint256_t& blockNum,
                              TxBlockSharedPtr& block)
{
    switch (m_blockStorageType)
    {
    case BlockStorageType::FileSystem:
        return GetTxBlockFromDisk(blockNum, block);
    case BlockStorageType::LevelDB:
        return GetTxBlockFromLevelDB(blockNum, block);
    case BlockStorageType::None:
        return false;
    }
    return false;
}

bool BlockStorage::PutTxBody(const string& key,
                             const vector<unsigned char>& body)
{
    int ret = m_txBodyDB.WriteToDB(
        key, std::string(reinterpret_cast<const char*>(&body[0]), body.size()));
    return (ret == 0);
}

void BlockStorage::GetTxBody(const string& key, TxBodySharedPtr& body)
{
    string bodyString = m_txBodyDB.ReadFromDB(key);
    const unsigned char* raw_memory
        = reinterpret_cast<const unsigned char*>(bodyString.c_str());
    body = TxBodySharedPtr(new Transaction(
        std::vector<unsigned char>(raw_memory, raw_memory + bodyString.size()),
        0));
}

void BlockStorage::SetBlockStorageType(BlockStorageType _blockStorageType)
{
    m_blockStorageType = _blockStorageType;
}

bool BlockStorage::Compress(const unsigned char* src, int src_len,
                            shared_ptr<unsigned char>& dst, lzo_uint& dst_len)
{
    // pid_t tid = syscall(SYS_gettid);

    // THREADLOG_MARKER(tid);

    const lzo_uint src_len_lzo = src_len + 1;

    shared_ptr<unsigned char> compressed_object{
        new unsigned char
            __LZO_MMODEL[src_len_lzo + src_len_lzo / 16 + 64 + 3]};

    if (compressed_object.get() == NULL)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    if (lzo_init() != LZO_E_OK)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    lzo_align_t __LZO_MMODEL
        wrkmem[((LZO1X_1_MEM_COMPRESS) + (sizeof(lzo_align_t) - 1))
               / sizeof(lzo_align_t)];

    int r = lzo1x_1_compress((const unsigned char*)src, src_len_lzo,
                             compressed_object.get(), &dst_len, wrkmem);

    // THREADLOG_GENERAL(INFO, tid, "Compression: " << src_len << " -> " << dst_len);

    if (r != LZO_E_OK)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    dst = compressed_object;

    return true;
}

bool BlockStorage::Decompress(const unsigned char* src, const lzo_uint src_len,
                              unsigned char* dst)
{
    // pid_t tid = syscall(SYS_gettid);

    // THREADLOG_MARKER(tid);

    if (lzo_init() != LZO_E_OK)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    lzo_uint dst_len = src_len;

    int r = lzo1x_decompress(src, src_len, (unsigned char*)dst, &dst_len, NULL);
    if (r != LZO_E_OK)
    {
        // THREAD// LOG_ERROR(tid);
        return false;
    }

    return true;
}
