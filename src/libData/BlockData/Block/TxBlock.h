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

#ifndef __TXBLOCK_H__
#define __TXBLOCK_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <vector>

#include "BlockBase.h"
#include "common/Serializable.h"
#include "common/Constants.h"
#include "libCrypto/Schnorr.h"
#include "libNetwork/Peer.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"

/// Stores the Tx block header and signature.
class TxBlock : public BlockBase
{
    TxBlockHeader m_header;
    std::array<unsigned char, BLOCK_SIG_SIZE> m_headerSig; // Co-signed by: sharding committee (microblock) or DS committee (finalblock)
    std::vector<TxnHash> m_microBlockHashes;

public:

    /// Default constructor.
    TxBlock(); // creates a dummy invalid placeholder block -- blocknum is maxsize of uint256

    /// Constructor for loading Tx block information from a byte stream.
    TxBlock(const std::vector<unsigned char> & src, unsigned int offset);

    /// Constructor with specified Tx block parameters.
    TxBlock
    (
        const TxBlockHeader & header,
        const std::array<unsigned char, BLOCK_SIG_SIZE> & signature,
        const std::vector<TxnHash> & microBlockHashes
    );

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char> & dst, unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    void Deserialize(const std::vector<unsigned char> & src, unsigned int offset);

    /// Returns the size in bytes when serializing the block.
    unsigned int GetSerializedSize() const;

    /// Returns the minimum size required for storing a block.
    static unsigned int GetMinSize();

    /// Returns the reference to the TxBlockHeader part of the Tx block.
    const TxBlockHeader & GetHeader() const;

    /// Returns the signature part of the Tx block.
    const std::array<unsigned char, BLOCK_SIG_SIZE> & GetHeaderSig() const;

    /// Returns the list of MicroBlockHashes.
    const std::vector<TxnHash> & GetMicroBlockHashes() const;

    /// Equality comparison operator.
    bool operator==(const TxBlock & block) const;

    /// Less-than comparison operator.
    bool operator<(const TxBlock & block) const;

    /// Greater-than comparison operator.
    bool operator>(const TxBlock & block) const;
};

#endif // __TXBLOCK_H__
