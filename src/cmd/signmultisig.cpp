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
#include "boost/program_options.hpp"
#include "libCrypto/Schnorr.h"
#include "libUtils/SWInfo.h"

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

using namespace std;
namespace po = boost::program_options;

int main(int argc, const char* argv[]) {
  string message_;
  string privk_fn;
  string pubk_fn;
  vector<PubKey> pubKeys;
  vector<PrivKey> privKeys;

  try {
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "message,m", po::value<string>(&message_)->required(),
        "Message string in hexadecimal format")(
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
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    vector<unsigned char> message(message_.begin(), message_.end());

    vector<uint8_t> v;
    v.push_back(53);

    PrivKey pk(v, 0);

    string line;
    try {
      vector<unsigned char> key_v;
      fstream privFile(privk_fn, ios::in);
      while (getline(privFile, line)) {
        if (line.size() != 64) {
          cerr << "Error: incorrect length of private key" << endl;
          return ERROR_IN_COMMAND_LINE;
        }
        key_v = DataConversion::HexStrToUint8Vec(line);
        privKeys.emplace_back(key_v, 0);
      }
    } catch (std::exception& e) {
      std::cerr << "Problem occured when processing private keys on line: "
                << privKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (privKeys.size() < 1) {
      std::cerr << "No private keys loaded" << endl;
      std::cerr << "Empty or corrupted or missing file: " << privk_fn << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    try {
      vector<unsigned char> key_v;
      fstream pubFile(pubk_fn, ios::in);
      while (getline(pubFile, line)) {
        //        if (line.size() != 66) {
        //          cerr << "Error: incorrect length of private key" << endl;
        //          return ERROR_IN_COMMAND_LINE;
        //        }
        //        key_v = DataConversion::HexStrToUint8Vec(line);
        //        pubKeys.emplace_back(key_v, 0);
        try {
          pubKeys.push_back(PubKey::GetPubKeyFromString(line));
        } catch (std::exception& e) {
          std::cerr << e.what() << endl;
          return ERROR_IN_COMMAND_LINE;
        }
      }
    } catch (std::exception& e) {
      std::cerr << "Problem occured when processing public keys on line: "
                << pubKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (pubKeys.size() < 1) {
      std::cerr << "No public keys loaded" << endl;
      std::cerr << "Empty or corrupted or missing file: " << pubk_fn << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (privKeys.size() != pubKeys.size()) {
      cout << "Private key number must equal to public key number!";
      return -1;
    }

    for (unsigned int i = 0; i < privKeys.size(); ++i) {
      Signature sig;
      if (!Schnorr::GetInstance().Sign(message, privKeys.at(i), pubKeys.at(i),
                                       sig)) {
        std::cerr << "Failed to sign message" << endl;
        std::cerr << "Either private key or public key on line " << i + 1
                  << " are corrupted." << endl;
        return ERROR_IN_COMMAND_LINE;
      }
      vector<unsigned char> result;
      sig.Serialize(result, 0);
      cout << DataConversion::Uint8VecToHexStr(result);
    }

  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
