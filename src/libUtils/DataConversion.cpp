/**
* Copyright (c) 2017 Zilliqa 
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


#include "DataConversion.h"
#include <sstream>

const std::vector<unsigned char> DataConversion::HexStrToUint8Vec(const std::string & hex_input)
{
    std::string in(hex_input);
    std::vector<uint8_t> out;
    boost::algorithm::unhex(in.begin(), in.end(), std::back_inserter(out));
    return out; 
}

const std::array<unsigned char, 32> DataConversion::HexStrToStdArray(const std::string & hex_input)
{
    std::string in(hex_input);
    std::array<unsigned char, 32> d;
    std::vector<unsigned char> v = HexStrToUint8Vec(hex_input);
    std::copy( std::begin(v), std::end(v), std::begin(d)); // this is the recommended way
    return d; 
}

const std::string DataConversion::Uint8VecToHexStr(const std::vector<unsigned char> & hex_vec)
{
    std::string str;
    boost::algorithm::hex(hex_vec.begin(), hex_vec.end(), std::back_inserter(str));
    return str;
}

const std::string DataConversion::Uint8VecToHexStr(const std::vector<unsigned char> & hex_vec, unsigned int offset, unsigned int len)
{
    std::string str;
    boost::algorithm::hex(hex_vec.begin() + offset, hex_vec.begin() + offset + len, std::back_inserter(str));
    return str;
}

std::string DataConversion::SerializableToHexStr(const Serializable & input)
{
    std::vector<unsigned char> tmp;
    input.Serialize(tmp, 0);
    std::string str;
    boost::algorithm::hex(tmp.begin(), tmp.end(), std::back_inserter(str));
    return str;
}
