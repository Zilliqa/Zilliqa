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

#include "UpgradeManager.h"

#include <MultiSig.h>
#include <sys/wait.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;

#define DOWNLOAD_FOLDER "download"

namespace {

const string dsNodePubProp = "pubk";
struct PTree {
  static boost::property_tree::ptree& GetInstance() {
    static boost::property_tree::ptree pt;
    read_xml(dsNodeFile.c_str(), pt);
    return pt;
  }
  PTree() = delete;
  ~PTree() = delete;
};

const vector<string> ReadDSCommFromFile() {
  auto pt = PTree::GetInstance();
  std::vector<std::string> result;
  for (auto& pubk : pt.get_child("dsnodes")) {
    if (pubk.first == dsNodePubProp) {
      result.emplace_back(pubk.second.data());
    }
  }
  return result;
}

}  // namespace

UpgradeManager& UpgradeManager::GetInstance() {
  static UpgradeManager um;
  return um;
}

bool UpgradeManager::LoadInitialDS(vector<PubKey>& initialDSCommittee) {
  string downloadUrl = "";
  try {
    vector<std::string> tempDsComm_string{ReadDSCommFromFile()};
    initialDSCommittee.clear();
    for (const auto& ds_string : tempDsComm_string) {
      zbytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(ds_string, pubkeyBytes)) {
        return false;
      }
      initialDSCommittee.push_back(PubKey(pubkeyBytes, 0));
    }

    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return false;
  }
}
