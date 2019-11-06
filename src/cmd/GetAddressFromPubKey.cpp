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
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

#include <boost/program_options.hpp>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libData/AccountData/Address.h"
#include "libUtils/CryptoUtils.h"
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
  std::cout << "\tAccepts public key and prints computed address on stdout."
            << endl;
}

int main(int argc, const char* argv[]) {
  try {
    string pubk;
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "pubk,u", po::value<string>(&pubk)->required(), "33-byte public key");

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

    PubKey key;

    try {
      key = PubKey::GetPubKeyFromString(pubk);
    } catch (std::invalid_argument& e) {
      std::cerr << e.what() << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    Address toAddr = CryptoUtils::GetAddressFromPubKey(key);
    cout << toAddr << endl;
  } catch (exception& e) {
    cerr << "Unhandled Exception reached the top of main: " << e.what()
         << ", application will now exit" << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
