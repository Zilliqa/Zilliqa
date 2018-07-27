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

#ifndef __DATACONVERSION_H__
#define __DATACONVERSION_H__

#include <array>
#include <boost/algorithm/hex.hpp>
#include <string>
#include <vector>

#include "common/Serializable.h"

/// Utility class for data conversion operations.
class DataConversion
{
public:
    /// Converts alphanumeric hex string to byte vector.
    static const std::vector<unsigned char>
    HexStrToUint8Vec(const std::string& hex_input);

    /// Converts alphanumeric hex string to 32-byte array.
    static const std::array<unsigned char, 32>
    HexStrToStdArray(const std::string& hex_input);

    /// Converts alphanumeric hex string to 64-byte array.
    static const std::array<unsigned char, 64>
    HexStrToStdArray64(const std::string& hex_input);

    /// Converts byte vector to alphanumeric hex string.
    static const std::string
    Uint8VecToHexStr(const std::vector<unsigned char>& hex_vec);

    /// Converts byte vector to alphanumeric hex string.
    static const std::string
    Uint8VecToHexStr(const std::vector<unsigned char>& hex_vec,
                     unsigned int offset, unsigned int len);

    /// Converts fixed-sized byte array to alphanumeric hex string.
    template<size_t SIZE>
    static std::string
    charArrToHexStr(const std::array<unsigned char, SIZE>& hex_arr)
    {
        std::string str;
        boost::algorithm::hex(hex_arr.begin(), hex_arr.end(),
                              std::back_inserter(str));
        return str;
    }

    /// Converts a serializable object to alphanumeric hex string.
    static std::string SerializableToHexStr(const Serializable& input);

    static const std::string
    CharArrayToString(const std::vector<unsigned char>& v);

    static const std::vector<unsigned char>
    StringToCharArray(const std::string& input);
};

#endif // __DATACONVERSION_H__
