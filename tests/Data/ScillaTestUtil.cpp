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

#include <boost/filesystem.hpp>
#include "libUtils/Logger.h"
#include "ScillaTestUtil.h"
#include "common/Constants.h"

bool ScillaTestUtil::ParseJsonFile(Json::Value &j, std::string filename) {
  if (!boost::filesystem::is_regular_file(filename)) return false;

  std::ifstream in(filename, std::ios::binary);
  std::string fstr;

  fstr = {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errors;

  return reader->parse(fstr.c_str(), fstr.c_str() + fstr.size(), &j, &errors);
}

// Get ScillaTest for contract "name" and test numbered "i".
bool ScillaTestUtil::GetScillaTest(ScillaTest &t, std::string contrName,
                                   unsigned int i) {
  if (SCILLA_ROOT.empty()) {
    return false;
  }

  // TODO: Does this require a separate entry in constants.xml?
  std::string testDir = SCILLA_ROOT + "/tests/contracts/" + contrName;
  if (!boost::filesystem::is_directory(testDir)) return false;

  if (!boost::filesystem::is_regular_file(testDir + "/contract.scilla"))
    return false;

  std::ifstream in(testDir + "/contract.scilla", std::ios::binary);
  t.code = {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};

  std::string is(std::to_string(i));

  // Get all JSONs
  if (!ParseJsonFile(t.init, testDir + "/init.json") ||
      !ParseJsonFile(t.state, testDir + "/state_" + is + ".json") ||
      !ParseJsonFile(t.blockchain, testDir + "/blockchain_" + is + ".json") ||
      !ParseJsonFile(t.expOutput, testDir + "/output_" + is + ".json") ||
      !ParseJsonFile(t.message, testDir + "/message_" + is + ".json")) {
    return false;
  }

  return true;
}
