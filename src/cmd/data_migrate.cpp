/*
 * Copyright (C) 2021 Zilliqa
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

#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/Retriever.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

/// Should be run from a folder named "persistence" consisting of the
/// persistence

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2
#define ERROR_UNEXPECTED -3

using namespace std;
namespace po = boost::program_options;

int main(int argc, const char* argv[]) {
  PairOfKey key;  // Dummy to initate mediator
  Peer peer;
  string ignore_checker_str;
  string disambiguation_str;
  string contract_address_output_filename;
  string normal_address_output_filename;

  try {
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "ignore_checker,i", po::value<string>(&ignore_checker_str),
        "whether ignore scilla checker result (true to ignore, default false)")(
        "disambiguation,d", po::value<string>(&disambiguation_str),
        "whether to call the migration tool for disambiguation (default "
        "false)")(
        "contract_addresses,c",
        po::value<string>(&contract_address_output_filename),
        "indicate the filename to output the contract addresses, no output if "
        "empty")("normal_addresses,n",
                 po::value<string>(&normal_address_output_filename),
                 "indicate the filename to output non-contract addresses, no "
                 "output if empty");

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

    const bool ignore_checker = (ignore_checker_str == "true");
    const bool disambiguation = (disambiguation_str == "true");

    LOG_GENERAL(INFO, "Begin");
    Mediator mediator(key, peer);
    Retriever retriever(mediator);

    LOG_GENERAL(INFO, "Start Retrieving States");

    if (!retriever.RetrieveStatesOld()) {
      LOG_GENERAL(FATAL, "RetrieveStates failed");
      return 0;
    }

    LOG_GENERAL(INFO, "Finished RetrieveStates");

    if (!retriever.MigrateContractStates(ignore_checker, disambiguation,
                                         contract_address_output_filename,
                                         normal_address_output_filename)) {
      LOG_GENERAL(WARNING, "MigrateContractStates failed");
    } else {
      LOG_GENERAL(INFO, "MigrateContractStates finished");
    }
  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }

  return SUCCESS;
}
