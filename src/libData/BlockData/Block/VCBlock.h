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
    std::array<unsigned char, BLOCK_SIG_SIZE> m_signature1;
    std::array<unsigned char, BLOCK_SIG_SIZE> m_signature2;
    std::vector<bool> m_headerSigBitmap1;
    std::vector<bool> m_headerSigBitmap2;

public:
    /// Default constructor.
    VCBlock(); // creates a dummy invalid placeholder block

    /// Constructor for loading VC block information from a byte stream.
    VCBlock(const std::vector<unsigned char>& src, unsigned int offset);

    /// Constructor with specified VC block parameters.
    // TODO: Future work: To add in cosi sig 1 and 2, bitmap 1 and 2;
    VCBlock(const VCBlockHeader& header,
            const std::array<unsigned char, BLOCK_SIG_SIZE>& signature);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Returns the size in bytes when serializing the VC block.
    static unsigned int GetSerializedSize();

    /// Sets the signature part of the DS block. i.e cosi_sig_1
    void SetSignature1(const std::vector<unsigned char>& signature);

    /// Sets the signature part of the DS block. i.e cosi_sig_2
    void SetSignature2(const std::vector<unsigned char>& signature);

    /// Sets the bitmap 1 of the DS block. i.e cosi_bitmap_1
    void SetHeaderSigBitmap1(const std::vector<bool>& signatureBitmap);

    /// Sets the bitmap 2 of the DS block. i.e cosi_bitmap_2
    void SetHeaderSigBitmap2(const std::vector<bool>& signatureBitmap);

    /// Returns the reference to the VCBlockHeader part of the VC block.
    const VCBlockHeader& GetHeader() const;

    /// Returns the signature 1 of the DS block.
    const std::array<unsigned char, BLOCK_SIG_SIZE>& GetSignature1() const;

    /// Returns the signature 2 of the DS block.
    const std::array<unsigned char, BLOCK_SIG_SIZE>& GetSignature2() const;

    /// Return bitmap 1
    const std::vector<bool> GetSigBitmap1();

    /// Return bitmap 2
    const std::vector<bool> GetSigBitmap2();

    /// Equality comparison operator.
    bool operator==(const VCBlock& block) const;

    /// Less-than comparison operator.
    bool operator<(const VCBlock& block) const;

    /// Greater-than comparison operator.
    bool operator>(const VCBlock& block) const;
};

#endif // __VCBLOCK_H__
