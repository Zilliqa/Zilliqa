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

#include <array>
#include <boost/program_options.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

namespace po = boost::program_options;
using namespace std;
using namespace boost::multiprecision;

void description() {
  std::cout << endl << "Description:\n";
  std::cout << "\tAccepts private key and prints computed public key on stdout."
            << endl;
}

int main(int argc, const char* argv[]) {
  try {
    string privk;
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "privk,i", po::value<string>(&privk)->required(),
        "32-byte private key");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      /** --help option
       */
      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        description();
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

    SHA2<HashType::HASH_VARIANT_256> sha2;
    sha2.Reset();
    bytes message;

    PrivKey privKey;

    try {
      privKey = PrivKey::GetPrivKeyFromString(privk);
    } catch (std::invalid_argument& e) {
      std::cerr << e.what() << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    PubKey pubKey{privKey};

    cout << pubKey << endl;

  } catch (exception& e) {
    cerr << "Unhandled Exception reached the top of main: " << e.what()
         << ", application will now exit" << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
