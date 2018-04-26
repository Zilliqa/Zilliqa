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

#ifndef __MICROBLOCKHEADER_H__
#define __MICROBLOCKHEADER_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>

#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

/// Stores information on the header part of the microblock.
class MicroBlockHeader : public BlockHeaderBase
{
    uint8_t m_type; // 0: microblock proposed by a committee, 1: final tx block
    uint32_t m_version;
    boost::multiprecision::uint256_t m_gasLimit;
    boost::multiprecision::uint256_t m_gasUsed;
    BlockHash m_prevHash; // Hash of the previous block
    boost::multiprecision::uint256_t
        m_blockNum; // Block index, starting from 0 in the genesis block
    boost::multiprecision::uint256_t m_timestamp;
    TxnHash m_txRootHash; // Tx merkle tree root hash
    uint32_t m_numTxs; // Total number of txs included in the block
    PubKey m_minerPubKey; // Leader of the committee who proposed this block
    boost::multiprecision::uint256_t
        m_dsBlockNum; // DS Block index at the time this Tx Block was proposed
    BlockHash m_dsBlockHeader; // DS Block hash

public:
    static const unsigned int SIZE = sizeof(uint8_t) + sizeof(uint32_t)
        + UINT256_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE + UINT256_SIZE
        + UINT256_SIZE + TRAN_HASH_SIZE + sizeof(uint32_t) + PUB_KEY_SIZE
        + UINT256_SIZE + BLOCK_HASH_SIZE;

    /// Default constructor.
    MicroBlockHeader();

    /// Constructor for loading existing microblock header from a byte stream.
    MicroBlockHeader(const std::vector<unsigned char>& src,
                     unsigned int offset);

    /// Constructor with predefined member values.
    MicroBlockHeader(const uint8_t type, const uint32_t version,
                     const boost::multiprecision::uint256_t& gasLimit,
                     const boost::multiprecision::uint256_t& gasUsed,
                     const BlockHash& prevHash,
                     const boost::multiprecision::uint256_t& blockNum,
                     const boost::multiprecision::uint256_t& timestamp,
                     const TxnHash& txRootHash, const uint32_t numTxs,
                     const PubKey& minerPubKey,
                     const boost::multiprecision::uint256_t& dsBlockNum,
                     const BlockHash& dsBlockHeader);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    // [TODO] These methods are all supposed to be moved into BlockHeaderBase, so no need to add Doxygen tags for now
    const uint8_t& GetType() const;
    const uint32_t& GetVersion() const;
    const boost::multiprecision::uint256_t& GetGasLimit() const;
    const boost::multiprecision::uint256_t& GetGasUsed() const;
    const BlockHash& GetPrevHash() const;
    const boost::multiprecision::uint256_t& GetBlockNum() const;
    const boost::multiprecision::uint256_t& GetTimestamp() const;
    const TxnHash& GetTxRootHash() const;
    const uint32_t& GetNumTxs() const;
    const PubKey& GetMinerPubKey() const;
    const boost::multiprecision::uint256_t& GetDSBlockNum() const;
    const BlockHash& GetDSBlockHeader() const;

    // Operators
    bool operator==(const MicroBlockHeader& header) const;
    bool operator<(const MicroBlockHeader& header) const;
    bool operator>(const MicroBlockHeader& header) const;
};

#endif // __MICROBLOCKHEADER_H__