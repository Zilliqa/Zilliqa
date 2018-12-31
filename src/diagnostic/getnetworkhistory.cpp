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
#include <map>

#include "libPersistence/BlockStorage.h"

std::string getCsvHeader(uint64_t start, uint64_t stop) {
  std::string ret = "Node";

  for (uint64_t i = start; i <= stop; ++i) {
    ret += ",DS epoch" + i;
  }

  return ret;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "[USAGE] " << argv[0]
              << " <output csv filename> <epoch width required>"
              << std::endl;
    return -1;
  }

  BlockStorage& bs = BlockStorage::GetBlockStorage();

  std::map<uint64_t, DiagnosticData>& diagnosticDataMap;
  bs.GetDiagnosticData(diagnosticDataMap);
  if (diagnosticDataMap.empty()) {
    std::cout << "Nothing to read in the Diagnostic DB" << std::endl;
    return 0;
  }

  for (auto const& it: diagnosticDataMap) {
    std::cout << it.first << std::endl;

    DequeOfShard& shards = it.second.shards;
    DequeOfDSNode& dsCommittee = it.second.dsCommittee;
  }

  return 0;
}
