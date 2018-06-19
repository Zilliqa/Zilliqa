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

#ifndef __DSBLOCKHEADER_H__
#define __DSBLOCKHEADER_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>

#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

/// Stores information on the header part of the DS block.
class DSBlockHeader : public BlockHeaderBase
{
    uint8_t m_difficulty; // Number of PoW1 leading zeros
    BlockHash m_prevHash; // Hash of the previous block
    boost::multiprecision::uint256_t
        m_nonce; // Nonce value of the winning miner for PoW1
    PubKey m_minerPubKey; // Public key of the winning miner for PoW1
    PubKey m_leaderPubKey; // The one who proposed this DS block
    boost::multiprecision::uint256_t
        m_blockNum; // Block index, starting from 0 in the genesis block
    boost::multiprecision::uint256_t m_timestamp;

public:
    static const unsigned int SIZE = sizeof(uint8_t) + BLOCK_HASH_SIZE
        + UINT256_SIZE + PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE
        + UINT256_SIZE;

    /// Default constructor.
    DSBlockHeader(); // creates a dummy invalid placeholder BlockHeader -- blocknum is maxsize of uint256

    /// Constructor for loading DS block header information from a byte stream.
    DSBlockHeader(const std::vector<unsigned char>& src, unsigned int offset);

    /// Constructor with specified DS block header parameters.
    DSBlockHeader(const uint8_t difficulty, const BlockHash& prevHash,
                  const boost::multiprecision::uint256_t& nonce,
                  const PubKey& minerPubKey, const PubKey& leaderPubKey,
                  const boost::multiprecision::uint256_t& blockNum,
                  const boost::multiprecision::uint256_t& timestamp);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Returns the difficulty of the PoW puzzle.
    const uint8_t& GetDifficulty() const;

    /// Returns the digest of the parent block header.
    const BlockHash& GetPrevHash() const;

    /// Returns the PoW solution nonce value.
    const boost::multiprecision::uint256_t& GetNonce() const;

    /// Returns the public key of the miner who did PoW on this header.
    const PubKey& GetMinerPubKey() const;

    /// Returns the public key of the leader of the DS committee that composed this block.
    const PubKey& GetLeaderPubKey() const;

    /// Returns the number of ancestor blocks.
    const boost::multiprecision::uint256_t& GetBlockNum() const;

    /// Returns the Unix time at the time of creation of this block.
    const boost::multiprecision::uint256_t& GetTimestamp() const;

    /// Equality operator.
    bool operator==(const DSBlockHeader& header) const;

    /// Less-than comparison operator.
    bool operator<(const DSBlockHeader& header) const;

    /// Greater-than comparison operator.
    bool operator>(const DSBlockHeader& header) const;
};

#endif // __DSBLOCKHEADER_H__
