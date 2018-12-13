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

#ifndef __SCILLATESTUTIL_H__
#define __SCILLATESTUTIL_H__

#include "libUtils/Logger.h"
#include "libUtils/JsonUtils.h"
#include "boost/multiprecision/cpp_int.hpp"

namespace ScillaTestUtil {

// The constituents of a Scilla test.
struct ScillaTest {
  // Scilla ASCII source.
  std::vector<unsigned char> code;
  // inititialization, message, state and expected output JSONs.
  Json::Value init, message, state, blockchain, expOutput;
};

// Parse a JSON file from filesystem.
bool ParseJsonFile(Json::Value &j, std::string filename);
// Get ScillaTest for contract "name" and test numbered "i".
bool GetScillaTest(ScillaTest &t, std::string contrName, unsigned int i);
// Get _balance from output state of interpreter, from OUTPUT_JSON.
// Return 0 on failure.
boost::multiprecision::uint128_t GetBalanceFromOutput(void);
// Return BLOCKNUMBER in Json. Return 0 if not found.
uint64_t GetBlockNumberFromJson(Json::Value &blockchain);
// Return the _amount in message.json. Remove that and _sender.
uint64_t PrepareMessageData(Json::Value &message, std::vector<unsigned char> &data);


}  // end namespace ScillaTestUtil

#endif  // __SCILLATESTUTIL_H__
