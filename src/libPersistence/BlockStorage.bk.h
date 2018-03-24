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

#ifndef BLOCKSTORAGE_H
#define BLOCKSTORAGE_H

#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "DB.h"
#include "libData/BlockData/Block.h"

#include "depends/minilzo/minilzo.h"

#define ONE_MEGABYTE 1024 * 1024

typedef std::shared_ptr<DSBlock> DSBlockSharedPtr;
typedef std::shared_ptr<TxBlock> TxBlockSharedPtr;
typedef std::shared_ptr<Transaction> TxBodySharedPtr;

enum BlockStorageType : unsigned int
{
    FileSystem = 0,
    LevelDB = 1,
    None = 2
};

/// Manages persistent storage of DS and Tx blocks.
class BlockStorage
{
    class LastBlockFileInfo;
    class BlockFileInfo;
    class BlockInfo;

    BlockStorageType m_blockStorageType = BlockStorageType::FileSystem;
    DB m_metadataDB;
    DB m_txBodyDB;
    DB m_dsBlockchainDB;
    DB m_txBlockchainDB;
    std::shared_timed_mutex m_putBlockMutex;
    static unsigned int m_blockFileSize;
    static const unsigned int m_numCachedBlocks = 20;

    BlockStorage();
    ~BlockStorage() = default;
    bool PutBlockToLevelDB(const boost::multiprecision::uint256_t& blockNum,
                           const std::vector<unsigned char>& block,
                           const BlockType& blockType);
    bool GetDSBlockFromLevelDB(const boost::multiprecision::uint256_t& blockNum,
                               DSBlockSharedPtr& block);
    bool GetTxBlockFromLevelDB(const boost::multiprecision::uint256_t& blockNum,
                               TxBlockSharedPtr& block);
    bool PutBlockToDisk(
        const boost::multiprecision::uint256_t& blockNum,
        const std::vector<unsigned char>& block,
        const BlockType& blockType); // provide the raw block std::string
    // as well, to avoid redundant work when calling to std::string() within this function
    bool GetDSBlockFromDisk(const boost::multiprecision::uint256_t& blocknum,
                            DSBlockSharedPtr& block);
    bool GetTxBlockFromDisk(const boost::multiprecision::uint256_t& blocknum,
                            TxBlockSharedPtr& block);
    std::shared_timed_mutex m_dsBlockCacheMutex;
    std::shared_timed_mutex m_txBlockCacheMutex;
    std::list<std::pair<boost::multiprecision::uint256_t, DSBlockSharedPtr>>
        m_dsblock_cache;
    std::list<std::pair<boost::multiprecision::uint256_t, TxBlockSharedPtr>>
        m_txblock_cache;
    void AddBlockToDSCache(const boost::multiprecision::uint256_t& blockNum,
                           const std::vector<unsigned char>& block);
    void AddBlockToTxCache(const boost::multiprecision::uint256_t& blockNum,
                           const std::vector<unsigned char>& block);

public:
    /// Returns the singleton BlockStorage instance.
    static BlockStorage& GetBlockStorage();

    /// Sets the file size limit.
    static void SetBlockFileSize(unsigned int blockFileSize);

    /// Returns the file size limit.
    static unsigned int GetBlockFileSize();

    /// Sets the type of storage mechanism to use.
    void SetBlockStorageType(BlockStorageType _blockStorageType);

    /// Adds a DS block to storage.
    bool PutDSBlock(const boost::multiprecision::uint256_t& blockNum,
                    const std::vector<unsigned char>&
                        block); // provide the raw block std::string as well,
    // to avoid redundant work when calling to std::string() within this function

    /// Adds a Tx block to storage.
    bool PutTxBlock(const boost::multiprecision::uint256_t& blockNum,
                    const std::vector<unsigned char>&
                        block); // provide the raw block std::string as well,
    // to avoid redundant work when calling to std::string() within this function

    /// Retrieves the requested DS block.
    bool GetDSBlock(const boost::multiprecision::uint256_t& blocknum,
                    DSBlockSharedPtr& block);

    /// Retrieves the requested Tx block.
    bool GetTxBlock(const boost::multiprecision::uint256_t& blocknum,
                    TxBlockSharedPtr& block);

    /// Adds a transaction body to storage.
    bool PutTxBody(const std::string& key,
                   const std::vector<unsigned char>& body);

    /// Retrieves the requested transaction body.
    void GetTxBody(const std::string& key, TxBodySharedPtr& body);

    /// Utility function for compressing a byte stream.
    bool Compress(const unsigned char* src, int src_len,
                  std::shared_ptr<unsigned char>& dst, lzo_uint& dst_len);

    /// Utility function for decompressing a byte stream.
    bool Decompress(const unsigned char* src, const lzo_uint src_len,
                    unsigned char* dst);
};

#endif // BLOCKSTORAGE_H
