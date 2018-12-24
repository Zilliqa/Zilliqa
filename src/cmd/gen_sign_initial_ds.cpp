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