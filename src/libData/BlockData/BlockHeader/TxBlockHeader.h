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

#ifndef __TXBLOCKHEADER_H__
#define __TXBLOCKHEADER_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"

/// Stores information on the header part of the Tx block.
class TxBlockHeader : public BlockHeaderBase
{
    uint8_t m_type; // 0: microblock proposed by a committee, 1: final tx block
    uint32_t m_version;
    boost::multiprecision::uint256_t m_gasLimit;
    boost::multiprecision::uint256_t m_gasUsed;
    BlockHash m_prevHash; // Hash of the previous block
    uint64_t m_blockNum; // Block index, starting from 0 in the genesis block
    boost::multiprecision::uint256_t m_timestamp;
    TxBlockHashSet m_hash;
    uint32_t m_numTxs; // Total number of txs included in the block
    uint32_t
        m_numMicroBlockHashes; // Total number of microblock hashes included in the block
    PubKey m_minerPubKey; // Leader of the committee who proposed this block
    uint64_t
        m_dsBlockNum; // DS Block index at the time this Tx Block was proposed
    BlockHash m_dsBlockHeader; // DS Block hash

public:
    static const unsigned int SIZE = sizeof(uint8_t) + sizeof(uint32_t)
        + UINT256_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE + sizeof(uint64_t)
        + UINT256_SIZE + TxBlockHashSet::size() + sizeof(uint32_t)
        + sizeof(uint32_t) + PUB_KEY_SIZE + sizeof(uint64_t) + BLOCK_HASH_SIZE;

    /// Default constructor.
    TxBlockHeader();

    /// Constructor for loading Tx block header information from a byte stream.
    TxBlockHeader(const std::vector<unsigned char>& src, unsigned int offset);

    /// Constructor with specified Tx block header parameters.
    TxBlockHeader(const uint8_t type, const uint32_t version,
                  const boost::multiprecision::uint256_t& gasLimit,
                  const boost::multiprecision::uint256_t& gasUsed,
                  const BlockHash& prevHash, const uint64_t& blockNum,
                  const boost::multiprecision::uint256_t& timestamp,
                  const TxnHash& txRootHash, const StateHash& stateRootHash,
                  const StateHash& deltaRootHash, const uint32_t numTxs,
                  const uint32_t numMicroBlockHashes, const PubKey& minerPubKey,
                  const uint64_t& dsBlockNum, const BlockHash& dsBlockHeader);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Returns the type of the block.
    const uint8_t& GetType() const;

    /// Returns the current version.
    const uint32_t& GetVersion() const;

    /// Returns the current limit for gas expenditure per block.
    const boost::multiprecision::uint256_t& GetGasLimit() const;

    /// Returns the total gas used by transactions in this block.
    const boost::multiprecision::uint256_t& GetGasUsed() const;

    /// Returns the digest of the parent block header.
    const BlockHash& GetPrevHash() const;

    /// Returns the number of ancestor blocks.
    const uint64_t& GetBlockNum() const;

    /// Returns the Unix time at the time of creation of this block.
    const boost::multiprecision::uint256_t& GetTimestamp() const;

    /// Returns the digest that represents the root of the Merkle tree that stores all microblocks in this block.
    const TxnHash& GetTxRootHash() const;

    /// Returns the digest that represents the root of the Merkle tree that stores all state uptil this block.
    const StateHash& GetStateRootHash() const;

    /// Returns the digest that represents the root of the Merkle tree that stores all state delta uptil this block.
    const StateHash& GetDeltaRootHash() const;

    /// Returns the number of transactions in this block.
    const uint32_t& GetNumTxs() const;

    /// Returns the number of MicroBlockHashes in this block.
    const uint32_t& GetNumMicroBlockHashes() const;

    /// Returns the public key of the leader of the committee that composed this block.
    const PubKey& GetMinerPubKey() const;

    /// Returns the parent DS block number.
    const uint64_t& GetDSBlockNum() const;

    /// Returns the digest of the parent DS block header.
    const BlockHash& GetDSBlockHeader() const;

    /// Equality comparison operator.
    bool operator==(const TxBlockHeader& header) const;

    /// Less-than comparison operator.
    bool operator<(const TxBlockHeader& header) const;

    /// Greater-than comparison operator.
    bool operator>(const TxBlockHeader& header) const;

    friend std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t)
{
    os << "m_type : " << t.m_type << std::endl
       << "m_version : " << t.m_version << std::endl
       << "m_gasLimit : " << t.m_gasLimit.convert_to<std::string>() << std::endl
       << "m_gasUsed : " << t.m_gasUsed.convert_to<std::string>() << std::endl
       << "m_prevHash : " << t.m_prevHash.hex() << std::endl
       << "m_blockNum : " << to_string(t.m_blockNum) << std::endl
       << "m_timestamp : " << t.m_timestamp.convert_to<std::string>()
       << std::endl
       << t.m_hash << std::endl
       << "m_numTxs : " << t.m_numTxs << std::endl
       << "m_numMicroBlockHashes : " << t.m_numMicroBlockHashes << std::endl
       << "m_minerPubKey : " << t.m_minerPubKey << std::endl
       << "m_dsBlockNum : " << to_string(t.m_dsBlockNum) << std::endl
       << "m_dsBlockHeader : " << t.m_dsBlockHeader.hex();
    return os;
}

#endif // __TXBLOCKHEADER_H__
