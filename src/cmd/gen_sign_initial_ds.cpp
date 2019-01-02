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

#include <iostream>
#include <string>
#include "libUtils/UpgradeManager.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using boost::property_tree::ptree;
using namespace std;

namespace {

const string publicKeyProp = "publicKey";
const string signatureProp = "signature";

struct PTree {
  static boost::property_tree::ptree& GetInstance() {
    static boost::property_tree::ptree pt;
    read_xml(dsNodeFile.c_str(), pt);

    return pt;
  }
  PTree() = delete;
  ~PTree() = delete;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    cout << "Input format "
         << " "
         << "[privKeyFile]"
         << " "
         << "[pubKeyFile]";
    return -1;
  }

  bytes message;
  vector<PubKey> dsComm;

  if (!UpgradeManager::GetInstance().LoadInitialDS(dsComm)) {
    cout << "unable to load ";
    return -1;
  }
  unsigned int curr_offset = 0;
  for (auto& dsKey : dsComm) {
    dsKey.Serialize(message, curr_offset);
    curr_offset += PUB_KEY_SIZE;
  }

  string line;
  vector<PrivKey> privKeys;
  {
    fstream privFile(argv[1], ios::in);

    while (getline(privFile, line)) {
      privKeys.emplace_back(DataConversion::HexStrToUint8Vec(line), 0);
    }
  }

  vector<PubKey> pubKeys;
  string pubKey_string;
  {
    fstream pubFile(argv[2], ios::in);

    while (getline(pubFile, line)) {
      pubKey_string = line;
      pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(line), 0);
    }
  }

  if (privKeys.size() != pubKeys.size()) {
    if (pubKeys.size() != 1) {
      cout << "Only one key allowed";
      return -1;
    }
    cout << "Private key number must equal to public key number!";
    return -1;
  }

  string sig_str;

  for (unsigned int i = 0; i < privKeys.size(); ++i) {
    Signature sig;
    Schnorr::GetInstance().Sign(message, privKeys.at(i), pubKeys.at(i), sig);
    bytes result;
    sig.Serialize(result, 0);
    sig_str = DataConversion::Uint8VecToHexStr(result);
  }

  auto pt = PTree::GetInstance();
  if (!sig_str.empty()) {
    pt.push_back(ptree::value_type(signatureProp, ptree(sig_str.c_str())));
  }
  if (!pubKey_string.empty()) {
    pt.push_back(
        ptree::value_type(publicKeyProp, ptree(pubKey_string.c_str())));
  }

  write_xml(dsNodeFile.c_str(), pt);

  return 0;
}
