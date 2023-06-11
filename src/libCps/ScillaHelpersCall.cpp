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

#include "common/Constants.h"

#include "libData/AccountData/TransactionReceipt.h"

#include "libScilla/ScillaUtils.h"

#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsRunScilla.h"
#include "libCps/ScillaHelpersCall.h"

#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

namespace libCps {

constexpr auto MAX_SCILLA_OUTPUT_SIZE_IN_BYTES = 5120;

ScillaCallParseResult ScillaHelpersCall::ParseCallContract(
    CpsAccountStoreInterface &acc_store, ScillaArgs &scillaArgs,
    const std::string &runnerPrint, TransactionReceipt &receipt,
    uint32_t scillaVersion) {
  Json::Value jsonOutput;
  auto parseResult =
      ParseCallContractOutput(acc_store, jsonOutput, runnerPrint, receipt);
  if (!parseResult.success) {
    return parseResult;
  }
  return ParseCallContractJsonOutput(acc_store, scillaArgs, jsonOutput, receipt,
                                     scillaVersion);
}

/// convert the interpreter output into parsable json object for calling
ScillaCallParseResult ScillaHelpersCall::ParseCallContractOutput(
    CpsAccountStoreInterface &acc_store, Json::Value &jsonOutput,
    const std::string &runnerPrint, TransactionReceipt &receipt) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (LOG_SC) {
    LOG_GENERAL(
        INFO,
        "Output: " << std::endl
                   << (runnerPrint.length() > MAX_SCILLA_OUTPUT_SIZE_IN_BYTES
                           ? runnerPrint.substr(
                                 0, MAX_SCILLA_OUTPUT_SIZE_IN_BYTES) +
                                 "\n ... "
                           : runnerPrint));
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(runnerPrint, jsonOutput)) {
    receipt.AddError(JSON_OUTPUT_CORRUPTED);
    return {};
  }
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    LOG_GENERAL(INFO, "Parse scilla-runner output (microseconds) = "
                          << r_timer_end(tpStart));
  }

  return {true, false};
}

/// parse the output from interpreter for calling and update states
ScillaCallParseResult ScillaHelpersCall::ParseCallContractJsonOutput(
    CpsAccountStoreInterface &acc_store, ScillaArgs &scillaArgs,
    const Json::Value &_json, TransactionReceipt &receipt,
    uint32_t preScillaVersion) {
  std::chrono::system_clock::time_point tpStart;
  if (ENABLE_CHECK_PERFORMANCE_LOG) {
    tpStart = r_timer_start();
  }

  if (!_json.isMember("gas_remaining")) {
    LOG_GENERAL(
        WARNING,
        "The json output of this contract didn't contain gas_remaining");
    if (scillaArgs.gasLimit > CONTRACT_INVOKE_GAS) {
      scillaArgs.gasLimit -= CONTRACT_INVOKE_GAS;
    } else {
      scillaArgs.gasLimit = 0;
    }
    receipt.AddError(NO_GAS_REMAINING_FOUND);
    return {};
  }
  // uint64_t startGas = gasRemained;
  try {
    scillaArgs.gasLimit = std::min(
        scillaArgs.gasLimit,
        boost::lexical_cast<uint64_t>(_json["gas_remaining"].asString()));
  } catch (...) {
    LOG_GENERAL(WARNING, "_amount " << _json["gas_remaining"].asString()
                                    << " is not numeric");
    return {};
  }
  LOG_GENERAL(INFO, "gasRemained: " << scillaArgs.gasLimit);

  if (!_json.isMember("messages") || !_json.isMember("events")) {
    if (_json.isMember("errors")) {
      LOG_GENERAL(WARNING, "Call contract failed");
      receipt.AddError(CALL_CONTRACT_FAILED);
      receipt.AddException(_json["errors"]);
    } else {
      LOG_GENERAL(WARNING, "JSON output of this contract is corrupted");
      receipt.AddError(OUTPUT_ILLEGAL);
    }
    return {};
  }

  if (!_json.isMember("_accepted")) {
    LOG_GENERAL(WARNING,
                "The json output of this contract doesn't contain _accepted");
    receipt.AddError(NO_ACCEPTED_FOUND);
    return {};
  }

  ScillaCallParseResult results;

  results.success = true;
  results.accepted = (_json["_accepted"].asString() == "true");

  if (scillaArgs.depth == 0) {
    // first call in a txn
    receipt.AddAccepted(results.accepted);
  } else {
    if (!receipt.AddAcceptedForLastTransition(results.accepted)) {
      LOG_GENERAL(WARNING, "AddAcceptedForLastTransition failed");
      return {};
    }
  }

  try {
    for (const auto &e : _json["events"]) {
      LogEntry entry;
      if (!entry.Install(e, scillaArgs.dest)) {
        receipt.AddError(LOG_ENTRY_INSTALL_FAILED);
        return {};
      }
      receipt.AddLogEntry(entry);
    }
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return {};
  }

  if (_json["messages"].type() != Json::arrayValue) {
    LOG_GENERAL(INFO, "messages is not in array value");
    return {};
  }

  // If output message is null
  if (_json["messages"].empty()) {
    LOG_GENERAL(INFO,
                "empty message in scilla output when invoking a "
                "contract, transaction finished");
  }

  for (const auto &msg : _json["messages"]) {
    // Non-null messages must have few mandatory fields.
    if (!msg.isMember("_tag") || !msg.isMember("_amount") ||
        !msg.isMember("params") || !msg.isMember("_recipient")) {
      LOG_GENERAL(
          WARNING,
          "The message in the json output of this contract is corrupted");
      receipt.AddError(MESSAGE_CORRUPTED);
      return {};
    }

    Amount amount;
    try {
      amount = Amount::fromQa(
          boost::lexical_cast<uint128_t>(msg["_amount"].asString()));
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "_amount " << msg["_amount"].asString() << " is not numeric");
      return {};
    }

    // At this point we don't support any named calls from Scilla to EVM
    for (const auto &param : msg["params"]) {
      if (param.isMember("vname") && param["vname"] == "_EvmCall") {
        receipt.AddError(CALL_CONTRACT_FAILED);
        return ScillaCallParseResult{
            .success = false,
            .failureType = ScillaCallParseResult::NON_RECOVERABLE};
      }
    }
    const auto recipient = Address(msg["_recipient"].asString());

    // Recipient is contract
    // _tag field is empty
    const bool isNextContract = !msg["_tag"].asString().empty();

    // Stop going further as transaction has been finished
    if (!isNextContract) {
      results.entries.emplace_back(ScillaCallParseResult::SingleResult{
          {}, recipient, amount, isNextContract});
      continue;
    }

    // Transitions are always recorded in the receipt, even if their destination
    // is an account and therefore doesn't accept them - tested on 8.2.x
    // - rrw 2023-03-31.
    receipt.AddTransition(scillaArgs.dest, msg, scillaArgs.depth);

    if (ENABLE_CHECK_PERFORMANCE_LOG) {
      LOG_GENERAL(INFO, "LDB Write (microseconds) = " << r_timer_end(tpStart));
    }

    // ZIL-5165: Don't fail if the recipient is a user account.
    {
      const CpsAccountStoreInterface::AccountType accountType =
          acc_store.GetAccountType(recipient);
      LOG_GENERAL(INFO, "Target is accountType " << accountType);
      if (accountType == CpsAccountStoreInterface::DoesNotExist ||
          accountType == CpsAccountStoreInterface::EOA) {
        LOG_GENERAL(INFO, "Target is EOA: processing.");
        // Message sent to a non-contract account. Add something to
        // results.entries so that if this message attempts to transfer funds,
        // it succeeds.
        results.entries.emplace_back(
            ScillaCallParseResult::SingleResult{{}, recipient, amount, false});
        continue;
      }
    }

    if (acc_store.isAccountEvmContract(recipient)) {
      // Workaround before we have full interop: treat EVM contracts as EOA
      // accounts only if there's receiver_address set to 0x0, otherwise revert
      if (!scillaArgs.extras ||
          scillaArgs.extras->scillaReceiverAddress != Address{}) {
        return ScillaCallParseResult{
            .success = false,
            .failureType = ScillaCallParseResult::NON_RECOVERABLE};
      } else {
        results.entries.emplace_back(
            ScillaCallParseResult::SingleResult{{}, recipient, amount, false});
        continue;
      }
    }

    if (scillaArgs.edge > MAX_CONTRACT_EDGES) {
      LOG_GENERAL(
          WARNING,
          "maximum contract edges reached, cannot call another contract");
      receipt.AddError(MAX_EDGES_REACHED);
      return {};
    }

    bool isLibrary;
    std::vector<Address> extlibs;
    uint32_t scillaVersion;

    if (!acc_store.GetContractAuxiliaries(recipient, isLibrary, scillaVersion,
                                          extlibs)) {
      LOG_GENERAL(WARNING, "GetContractAuxiliaries failed");
      receipt.AddError(INTERNAL_ERROR);
      return {};
    }
    if (scillaVersion != preScillaVersion) {
      LOG_GENERAL(WARNING, "Scilla version inconsistent");
      receipt.AddError(VERSION_INCONSISTENT);
      return {};
    }

    Json::Value inputMessage;
    inputMessage["_sender"] = "0x" + scillaArgs.dest.hex();
    inputMessage["_origin"] = "0x" + scillaArgs.origin.hex();
    inputMessage["_amount"] = msg["_amount"];
    inputMessage["_tag"] = msg["_tag"];
    inputMessage["params"] = msg["params"];

    results.entries.emplace_back(ScillaCallParseResult::SingleResult{
        std::move(inputMessage), recipient, amount, isNextContract});
  }

  LOG_GENERAL(INFO, "Returning success " << results.success << " entries "
                                         << results.entries.size());
  return results;
}

}  // namespace libCps
