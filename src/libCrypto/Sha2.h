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

#ifndef __SHA2_H__
#define __SHA2_H__

#include "libUtils/Logger.h"
#include <openssl/sha.h>
#include <vector>

/// List of supported hash variants.
class HASH_TYPE
{
public:
    static const unsigned int HASH_VARIANT_256 = 256;
    static const unsigned int HASH_VARIANT_512 = 512;
};

/// Implements SHA2 hash algorithm.
template<unsigned int SIZE> class SHA2
{
    static const unsigned int HASH_OUTPUT_SIZE = SIZE / 8;
    SHA256_CTX m_context;
    std::vector<unsigned char> output;

public:
    /// Constructor.
    SHA2()
        : output(HASH_OUTPUT_SIZE)
    {
        if (SIZE != HASH_TYPE::HASH_VARIANT_256)
        {
            LOG_GENERAL(FATAL,
                        "assertion failed (" << __FILE__ << ":" << __LINE__
                                             << ": " << __FUNCTION__ << ")");
        }

        Reset();
    }

    /// Destructor.
    ~SHA2() {}

    /// Hash update function.
    void Update(const std::vector<unsigned char>& input)
    {
        if (input.size() <= 0)
        {
            LOG_GENERAL(FATAL,
                        "assertion failed (" << __FILE__ << ":" << __LINE__
                                             << ": " << __FUNCTION__ << ")");
        }

        SHA256_Update(&m_context, input.data(), input.size());
    }

    /// Hash update function.
    void Update(const std::vector<unsigned char>& input, unsigned int offset,
                unsigned int size)
    {
        if ((offset + size) > input.size())
        {
            LOG_GENERAL(FATAL,
                        "assertion failed (" << __FILE__ << ":" << __LINE__
                                             << ": " << __FUNCTION__ << ")");
        }

        SHA256_Update(&m_context, input.data() + offset, size);
    }

    /// Resets the algorithm.
    void Reset() { SHA256_Init(&m_context); }

    /// Hash finalize function.
    std::vector<unsigned char> Finalize()
    {
        switch (SIZE)
        {
        case 256:
            SHA256_Final(output.data(), &m_context);
            break;
        default:
            break;
        }
        return output;
    }
};

#endif // __SHA2_H__