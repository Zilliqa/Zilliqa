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
#include <vector>
#include <map>
#include <unordered_map>

#include "libPersistence/BlockStorage.h"

std::string getCsvHeader(uint64_t start, uint64_t stop) {
  std::string ret = "Node";

  for (uint64_t i = start; i <= stop; ++i) {
    ret += ",DS epoch " + i;
  }

  return ret;
}

void processShards(const DequeOfShard& shards, std::map<std::string, std::map<uint64_t, std::string>>& results, uint64_t dsEpochNo) {
  uint64_t shardIndex = 0;

  for (auto shardItr = shards.begin(); shardItr != shards.end(); ++shardItr, ++shardIndex) {
    for (size_t peerIndex = 0; peerIndex < shardItr->size(); ++peerIndex) {
      // get the peer and convert to ip.
      Peer peer = std::get<1>((*shardItr)[peerIndex]);
      std::string ip = peer.GetPrintableIPAddress();

      if (results.find(ip) == results.end()) {
        results[ip] = std::map<uint64_t, std::string>();
      }

      results[ip][dsEpochNo] = "Shard " + std::to_string(shardIndex) + " Index " + std::to_string(peerIndex);
    }
  }
}

void processDSCommittee(const DequeOfDSNode& dsCommittee, std::map<std::string, std::map<uint64_t, std::string>>& results, uint64_t dsEpochNo) {
  uint64_t dsCommitteeIndex = 0;

  for (auto peerItr = dsCommittee.begin(); peerItr != dsCommittee.end(); ++peerItr, ++dsCommitteeIndex) {
    // get the peer and convert to ip.
    Peer peer = std::get<1>(*peerItr);
    std::string ip = peer.GetPrintableIPAddress();

    if (results.find(ip) == results.end()) {
      results[ip] = std::map<uint64_t, std::string>();
    }

    results[ip][dsEpochNo] = "DS Index " + std::to_string(dsCommitteeIndex);
  }
}

void processResults(std::map<std::string, std::map<uint64_t, std::string>>& results, std::string& output, uint64_t blockStart, uint64_t blockStop) {
  output = getCsvHeader(blockStart, blockStop) + "\n";

  for (auto const& it: results) {
    std::string ip = it.first;

    std::string row = "";
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
    std::cout << "[USAGE] " << argv[0]
              << " <output csv filename> <epoch width required>"
              << std::endl;
    return -1;
  }

  BlockStorage& bs = BlockStorage::GetBlockStorage();

  std::map<uint64_t, DiagnosticData> diagnosticDataMap = std::map<uint64_t, DiagnosticData>();
  bs.GetDiagnosticData(diagnosticDataMap);
  if (diagnosticDataMap.empty()) {
    std::cout << "Nothing to read in the Diagnostic DB" << std::endl;
    return 0;
  }

  size_t blockCount = diagnosticDataMap.size();
  uint64_t blockStart = diagnosticDataMap.begin()->first;

  std::map<std::string, std::map<uint64_t, std::string>> results = std::map<std::string, std::map<uint64_t, std::string>>();
  for (auto const& it: diagnosticDataMap) {
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
