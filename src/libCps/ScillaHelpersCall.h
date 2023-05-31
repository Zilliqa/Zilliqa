/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBCPS_SCILLAHELPERSCALL_H_
#define ZILLIQA_SRC_LIBCPS_SCILLAHELPERSCALL_H_

#include <json/json.h>

class TransactionReceipt;

namespace libCps {

struct ScillaArgs;

struct ScillaCallParseResult {
  using Address = dev::h160;
  bool success = false;
  // if contract accepted sent amount from a call (should be succeeded by a
  // transfer)
  bool accepted = false;
  enum FailureType { RECOVERABLE = 0, NON_RECOVERABLE };
  FailureType failureType = FailureType::RECOVERABLE;

  struct SingleResult {
    Json::Value nextInputMessage;
    Address nextAddress;
    Amount amount;
    bool isNextContract = false;
  };
  std::vector<SingleResult> entries = {};
};

class ScillaHelpersCall final {
 public:
  using Address = dev::h160;

  /// Contract Calling
  /// verify the return from scilla_runner for calling is valid
  static ScillaCallParseResult ParseCallContract(
      CpsAccountStoreInterface &acc_store, ScillaArgs &args,
      const std::string &runnerPrint, TransactionReceipt &receipt,
      uint32_t scilla_version);

  /// convert the interpreter output into parsable json object for calling
  static ScillaCallParseResult ParseCallContractOutput(
      CpsAccountStoreInterface &acc_store, Json::Value &jsonOutput,
      const std::string &runnerPrint, TransactionReceipt &receipt);

  /// parse the output from interpreter for calling and update states
  static ScillaCallParseResult ParseCallContractJsonOutput(
      CpsAccountStoreInterface &acc_store, ScillaArgs &args,
      const Json::Value &_json, TransactionReceipt &receipt,
      uint32_t pre_scilla_version);
};
}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_SCILLAHELPERSCALL_H_
