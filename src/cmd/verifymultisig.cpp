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

#include <boost/program_options.hpp>

#include <MultiSig.h>
#include "libUtils/SWInfo.h"

#include "libUtils/DataConversion.h"

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

using namespace std;
namespace po = boost::program_options;

int main(int argc, const char* argv[]) {
  string message_;
  string signature_;
  string pubk_fn;
  vector<PubKey> pubKeys;

  try {
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "message,m", po::value<string>(&message_)->required(),
        "Message string in hexadecimal format")(
        "signature,s", po::value<string>(&signature_)->required(),
        "Signature string in hexadecimal format")(
        "pubk,u", po::value<string>(&pubk_fn)->required(),
        "Filename containing public keys each per line");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

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

    bytes message(message_.begin(), message_.end());

    try {
      string line;
      fstream pubFile(pubk_fn, ios::in);
      while (getline(pubFile, line)) {
        try {
          pubKeys.push_back(PubKey::GetPubKeyFromString(line));
        } catch (std::invalid_argument& e) {
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

    /// Aggregate public keys
    shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubKeys);

    bytes signature;
    DataConversion::HexStrToUint8Vec(signature_, signature);
    shared_ptr<Signature> sig(new Signature(signature, 0));

    /// Multi-sig verification
    if (MultiSig::MultiSigVerify(message, *sig, *aggregatedPubkey)) {
      cout << "PASS" << endl;
    } else {
      cout << "FAIL" << endl;
    }
  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
