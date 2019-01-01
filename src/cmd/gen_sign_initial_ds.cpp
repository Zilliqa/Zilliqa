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

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "libUtils/UpgradeManager.h"

namespace po = boost::program_options;

using boost::property_tree::ptree;
using namespace std;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2
#define ERROR_UNEXPECTED -3

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
  try {
    string line;
    vector<PrivKey> privKeys;
    vector<PubKey> pubKeys;
    string pubKey_string;
    string pubk_fn;
    string privk_fn;
    bytes message;
    vector<PubKey> dsComm;

    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "privk,i", po::value<string>(&privk_fn)->required(),
        "Filename containing private keys each per line")(
        "pubk,u", po::value<string>(&pubk_fn)->required(),
        "Filename containing public keys each per line");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      /** --help option
       */
      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch (boost::program_options::required_option& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    //--------------------------
    try {
      bytes key_v;
      fstream privFile(privk_fn, ios::in);
      while (getline(privFile, line)) {
        try {
          privKeys.push_back(PrivKey::GetPrivKeyFromString(line));
        } catch (std::invalid_argument& e) {
          std::cerr << e.what() << endl;
          return ERROR_IN_COMMAND_LINE;
        }
      }
    } catch (exception& e) {
      cerr << "Problem occured when reading private keys on line: "
           << privKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    try {
      bytes key_v;
      fstream pubFile(pubk_fn, ios::in);
      while (getline(pubFile, line)) {
        try {
          pubKeys.push_back(PubKey::GetPubKeyFromString(line));
        } catch (std::invalid_argument& e) {
          std::cerr << e.what() << endl;
          return ERROR_IN_COMMAND_LINE;
        }
      }
    } catch (exception& e) {
      cerr << "Problem occured when reading public keys on line: "
           << pubKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (privKeys.size() != pubKeys.size()) {
      cerr << "Private key number must equal to public key number!";
      return -1;
    }

    if (!UpgradeManager::GetInstance().LoadInitialDS(dsComm)) {
      cout << "Unable to load DS";
      return ERROR_UNEXPECTED;
    }
    unsigned int curr_offset = 0;
    for (auto& dsKey : dsComm) {
      dsKey.Serialize(message, curr_offset);
      curr_offset += PUB_KEY_SIZE;
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

  } catch (exception& e) {
    cerr << "Unhandled Exception reached the top of main: " << e.what()
         << ", application will now exit" << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
