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
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "libPersistence/BlockStorage.h"

std::string getCsvHeader() {
  return "DS Epoch,Total Nodes,Total Cosigs,Lookups,Total Reward,Base "
         "Reward,Base Reward Each,Lookup Reward,Lookup Reward Each,Cosigs "
         "Reward,Cosigs "
         "Reward Each,Lucky Draw Reward,Lucky Draw Winner Key,Lucky Draw "
         "Winner Addr";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "[USAGE] " << argv[0] << " <output csv filename> [db path]"
              << std::endl;
    return -1;
  }

  std::string path = "./";
  if (argc == 3) {
    path = std::string(argv[2]);
    path += (path.back() == '/' ? "" : "/");
  }

  BlockStorage& bs = BlockStorage::GetBlockStorage(path, true);

  std::map<uint64_t, DiagnosticDataCoinbase> diagnosticDataMap =
      std::map<uint64_t, DiagnosticDataCoinbase>();
  bs.GetDiagnosticDataCoinbase(diagnosticDataMap);
  if (diagnosticDataMap.empty()) {
    std::cout << "Nothing to read in the Diagnostic DB" << std::endl;
    return 0;
  }

  // Write to csv file.
  std::ofstream out(argv[1]);
  out << getCsvHeader() << std::endl;
  for (auto const& it : diagnosticDataMap) {
    auto const& entry = it.second;
    out << it.first << "," << entry.nodeCount << "," << entry.sigCount << ","
        << entry.lookupCount << "," << entry.totalReward << ","
        << entry.baseReward << "," << entry.baseRewardEach << ","
        << entry.lookupReward << "," << entry.rewardEachLookup << ","
        << entry.nodeReward << "," << entry.rewardEach << ","
        << entry.balanceLeft << "," << entry.luckyDrawWinnerKey << ","
        << entry.luckyDrawWinnerAddr << std::endl;
  }

  out.close();

  return 0;
}
