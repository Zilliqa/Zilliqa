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

#ifndef __SCILLATESTUTIL_H__
#define __SCILLATESTUTIL_H__

#include "boost/multiprecision/cpp_int.hpp"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"

namespace ScillaTestUtil {

// The constituents of a Scilla test.
struct ScillaTest {
  // Scilla ASCII source.
  bytes code;
  // inititialization, message, state and expected output JSONs.
  Json::Value init, message, state, blockchain, expOutput;
};

// Parse a JSON file from filesystem.
bool ParseJsonFile(Json::Value &j, std::string filename);

// Get the size in bytes of a file from filesystem.
uint64_t GetFileSize(const std::string &filename);

// Get ScillaTest for contract "name" and test numbered "i".
// "version" is used only if ENABLE_SCILLA_MULTI_VERSION is set.
bool GetScillaTest(ScillaTest &t, const std::string &contrName, unsigned int i,
                   const std::string &version = "0", bool isLibrary = false);
// Get _balance from output state of interpreter, from OUTPUT_JSON.
// Return 0 on failure.
uint128_t GetBalanceFromOutput(void);
// Return BLOCKNUMBER in Json. Return 0 if not found.
uint64_t GetBlockNumberFromJson(Json::Value &blockchain);
// Return the _amount in message.json. Remove that and _sender.
uint64_t PrepareMessageData(Json::Value &message, bytes &data);
// Remove _creation_block field from init JSON.
bool RemoveCreationBlockFromInit(Json::Value &init);
// Remove _this_address field from init JSON.
bool RemoveThisAddressFromInit(Json::Value &init);
}  // end namespace ScillaTestUtil

#endif  // __SCILLATESTUTIL_H__
