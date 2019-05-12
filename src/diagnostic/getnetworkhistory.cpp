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

std::string getCsvHeader(uint64_t start, uint64_t stop) {
  std::string ret = "Node";

  for (uint64_t i = start; i <= stop; ++i) {
    ret += ",DS epoch " + std::to_string(i);
  }

  return ret;
}

void processShards(
    const DequeOfShard& shards,
    std::map<std::string, std::map<uint64_t, std::string>>& results,
    uint64_t dsEpochNo) {
  uint64_t shardIndex = 0;

  for (auto shardItr = shards.begin(); shardItr != shards.end();
       ++shardItr, ++shardIndex) {
    for (size_t peerIndex = 0; peerIndex < shardItr->size(); ++peerIndex) {
      // get the peer and convert to ip.
      const Peer& peer = std::get<1>((*shardItr)[peerIndex]);
      std::string ip = peer.GetPrintableIPAddress();

      if (results.find(ip) == results.end()) {
        results[ip] = std::map<uint64_t, std::string>();
      }

      results[ip][dsEpochNo] = "Shard " + std::to_string(shardIndex) +
                               " Index " + std::to_string(peerIndex);
    }
  }
}

void processDSCommittee(
    const DequeOfNode& dsCommittee,
    std::map<std::string, std::map<uint64_t, std::string>>& results,
    uint64_t dsEpochNo) {
  uint64_t dsCommitteeIndex = 0;

  for (auto peerItr = dsCommittee.begin(); peerItr != dsCommittee.end();
       ++peerItr, ++dsCommitteeIndex) {
    // get the peer and convert to ip.
    const Peer& peer = std::get<1>(*peerItr);
    std::string ip = peer.GetPrintableIPAddress();

    if (results.find(ip) == results.end()) {
      results[ip] = std::map<uint64_t, std::string>();
    }

    results[ip][dsEpochNo] = "DS Index " + std::to_string(dsCommitteeIndex);
  }
}

void processResults(
    std::map<std::string, std::map<uint64_t, std::string>>& results,
    std::string& output, uint64_t blockStart, uint64_t blockStop) {
  output = getCsvHeader(blockStart, blockStop) + "\n";

  for (auto const& it : results) {
    std::string ip = it.first;

    std::string row = ip + ",";
    for (uint64_t block = blockStart; block < blockStop; ++block) {
      if (results[ip].find(block) != results[ip].end()) {
        row += results[ip][block] + ",";
      } else {
        row += "Not sharded,";
      }
    }
    // Last block.
    if (results[ip].find(blockStop) != results[ip].end()) {
      row += results[ip][blockStop];
    } else {
      row += "Not sharded";
    }

    output += row + "\n";
  }
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

  std::map<uint64_t, DiagnosticDataNodes> diagnosticDataMap =
      std::map<uint64_t, DiagnosticDataNodes>();
  bs.GetDiagnosticDataNodes(diagnosticDataMap);
  if (diagnosticDataMap.empty()) {
    std::cout << "Nothing to read in the Diagnostic DB" << std::endl;
    return 0;
  }

  size_t blockCount = diagnosticDataMap.size();
  uint64_t blockStart = diagnosticDataMap.begin()->first;

  std::map<std::string, std::map<uint64_t, std::string>> results =
      std::map<std::string, std::map<uint64_t, std::string>>();
  for (auto const& it : diagnosticDataMap) {
    uint64_t dsEpochNo = it.first;
    processShards(it.second.shards, results, dsEpochNo);
    processDSCommittee(it.second.dsCommittee, results, dsEpochNo);
  }

  std::string output;
  processResults(results, output, blockStart, blockStart + blockCount);

  // Write to csv file.
  std::ofstream out(argv[1]);
  out << output;
  out.close();

  return 0;
}
