/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "DataConversion.h"

using namespace std;

bool DataConversion::HexStrToUint8Vec(const string& hex_input, zbytes& out) {
  int offset = 0;
  if (hex_input.size() >= 2 && hex_input[0] == '0' && hex_input[1] == 'x') {
    offset = 2;
  }
  try {
    out.clear();
    boost::algorithm::unhex(hex_input.begin() + offset, hex_input.end(),
                            back_inserter(out));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed HexStrToUint8Vec conversion with exception: "
                             << e.what());
    return false;
  }
  return true;
}

zbytes DataConversion::HexStrToUint8VecRet(const string& hex_input) {
  zbytes out;
  int offset = 0;
  if (hex_input.size() >= 2 && hex_input[0] == '0' && hex_input[1] == 'x') {
    offset = 2;
  }
  try {
    out.clear();
    boost::algorithm::unhex(hex_input.begin() + offset, hex_input.end(),
                            back_inserter(out));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed HexStrToUint8Vec conversion with exception: "
                             << e.what());
    return out;
  }
  return out;
}

bool DataConversion::HexStrToStdArray(const string& hex_input,
                                      array<uint8_t, 32>& d) {
  d = {{0}};
  zbytes v;
  if (HexStrToUint8Vec(hex_input, v)) {
    copy(v.begin(), v.begin() + min((int)v.size(), 32), d.begin());
    return true;
  }
  LOG_GENERAL(WARNING, "Failed HexStrToStdArray conversion");
  return false;
}

bool DataConversion::HexStrToStdArray64(const string& hex_input,
                                        array<uint8_t, 64>& d) {
  d = {{0}};
  zbytes v;
  if (HexStrToUint8Vec(hex_input, v)) {
    copy(v.begin(), v.begin() + min((int)v.size(), 64), d.begin());
    return true;
  }
  LOG_GENERAL(WARNING, "Failed HexStrToStdArray conversion");
  return false;
}

bool DataConversion::Uint8VecToHexStr(const zbytes& hex_vec, string& str) {
  try {
    str = "";
    boost::algorithm::hex(hex_vec.begin(), hex_vec.end(), back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed Uint8VecToHexStr conversion");
    return false;
  }
  return true;
}

std::string DataConversion::Uint8VecToHexStrRet(const zbytes& hex_vec) {
  std::string str;
  try {
    str = "";
    boost::algorithm::hex(hex_vec.begin(), hex_vec.end(), back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed Uint8VecToHexStr conversion");
    return str;
  }
  return str;
}

bool DataConversion::StringToHexStr(const string& hex_str, string& str) {
  try {
    str = "";
    boost::algorithm::hex(hex_str, back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed StringToHexStr conversion");
    return false;
  }
  return true;
}

bool DataConversion::Uint8VecToHexStr(const zbytes& hex_vec,
                                      unsigned int offset, unsigned int len,
                                      string& str) {
  try {
    str = "";
    boost::algorithm::hex(hex_vec.begin() + offset,
                          hex_vec.begin() + offset + len, back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed Uint8VecToHexStr conversion");
    return false;
  }
  return true;
}

bool DataConversion::SerializableToHexStr(const Serializable& input,
                                          string& str) {
  zbytes tmp;
  input.Serialize(tmp, 0);
  try {
    str = "";
    boost::algorithm::hex(tmp.begin(), tmp.end(), back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed SerializableToHexStr conversion");
    return false;
  }
  return true;
}

bool DataConversion::SerializableToHexStr(const SerializableCrypto& input,
                                          string& str) {
  zbytes tmp;
  input.Serialize(tmp, 0);
  try {
    str = "";
    boost::algorithm::hex(tmp.begin(), tmp.end(), back_inserter(str));
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed SerializableToHexStr conversion");
    return false;
  }
  return true;
}

uint16_t DataConversion::charArrTo16Bits(const zbytes& hex_arr) {
  if (hex_arr.size() == 0) {
    return 0;
  }
  uint32_t lsb = hex_arr.size() - 1;

  return (hex_arr.at(lsb - 1) << 8) | hex_arr.at(lsb);
}

std::string DataConversion::AddOXPrefix(std::string&& s) {
  if (s.size() <= 2 || s[1] == 'x' || s[1] == 'X') {
    return s;
  }

  std::string ret{"0x"};
  ret += s;
  return ret;
}

bool DataConversion::NormalizeHexString(std::string& s) {
  if (s.size() < 2) {
    return false;
  }

  unsigned pos = 0;
  unsigned prefix_size = 0;

  for (char& c : s) {
    pos++;
    c = tolower(c);

    if (std::isdigit(c) || (('a' <= c) && (c <= 'f'))) {
      continue;
    }
    if ((c == 'x') && (pos == 2)) {
      prefix_size = 2;
      continue;
    }
    return false;
  }
  // remove prefix "0x"
  s.erase(0, prefix_size);

  return s.size() > 0;
}
