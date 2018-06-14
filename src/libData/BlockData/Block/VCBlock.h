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

#ifndef __VCBLOCK_H__
#define __VCBLOCK_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/VCBlockHeader.h"

/// Stores the VC header and signatures.
class VCBlock : public BlockBase
{
    VCBlockHeader m_header;

public:
    /// Default constructor.
    VCBlock(); // creates a dummy invalid placeholder block

    /// Constructor for loading VC block information from a byte stream.
    VCBlock(const std::vector<unsigned char>& src, unsigned int offset);

    /// Constructor with specified VC block parameters.
    VCBlock(VCBlockHeader&& header, CoSignatures&& cosigs);

    /// Implements the Serialize function inherited from Serializable.
    /// Return size of serialized structure
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    /// Return 0 if successed, -1 if failed
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Returns the size in bytes when serializing the VC block.
    unsigned int GetSerializedSize() const;

    /// Returns the minimum required size in bytes for obtaining a VC block from a byte stream.
    static unsigned int GetMinSize();

    /// Returns the reference to the VCBlockHeader part of the VC block.
    const VCBlockHeader& GetHeader() const;

    /// Equality comparison operator.
    bool operator==(const VCBlock& block) const;

    /// Less-than comparison operator.
    bool operator<(const VCBlock& block) const;

    /// Greater-than comparison operator.
    bool operator>(const VCBlock& block) const;
};

#endif // __VCBLOCK_H__
