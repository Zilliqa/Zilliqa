/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "DataConversion.h"

using namespace std;

const bytes DataConversion::HexStrToUint8Vec(const string& hex_input) {
  vector<uint8_t> out;
  boost::algorithm::unhex(hex_input.begin(), hex_input.end(),
                          back_inserter(out));
  return out;
}

const array<unsigned char, 32> DataConversion::HexStrToStdArray(
    const string& hex_input) {
  array<unsigned char, 32> d = {0};
  bytes v = HexStrToUint8Vec(hex_input);
  copy(v.begin(), v.begin() + min((int)v.size(), 32), d.begin());
  return d;
}

const array<unsigned char, 64> DataConversion::HexStrToStdArray64(
    const string& hex_input) {
  array<unsigned char, 64> d = {0};
  bytes v = HexStrToUint8Vec(hex_input);
  copy(v.begin(), v.begin() + min((int)v.size(), 64), d.begin());
  return d;
}

const string DataConversion::Uint8VecToHexStr(const bytes& hex_vec) {
  string str;
  boost::algorithm::hex(hex_vec.begin(), hex_vec.end(), back_inserter(str));
  return str;
}

const string DataConversion::Uint8VecToHexStr(const bytes& hex_vec,
                                              unsigned int offset,
                                              unsigned int len) {
  string str;
  boost::algorithm::hex(hex_vec.begin() + offset,
                        hex_vec.begin() + offset + len, back_inserter(str));
  return str;
}

string DataConversion::SerializableToHexStr(const Serializable& input) {
  bytes tmp;
  input.Serialize(tmp, 0);
  string str;
  boost::algorithm::hex(tmp.begin(), tmp.end(), back_inserter(str));
  return str;
}

uint16_t DataConversion::charArrTo16Bits(const bytes& hex_arr) {
  if (hex_arr.size() == 0) {
    return 0;
  }
  uint32_t lsb = hex_arr.size() - 1;

  return (hex_arr.at(lsb - 1) << 8) | hex_arr.at(lsb);
}