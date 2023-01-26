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

#ifndef ZILLIQA_SRC_LIBCPS_SCILLAHELPERS_H_
#define ZILLIQA_SRC_LIBCPS_SCILLAHELPERS_H_

#include "common/BaseType.h"

#include <json/json.h>

class Transaction;
class TransactionReceipt;

namespace libCps {

struct CpsAccountStoreInterface;

class ScillaHelpers final {
 public:
  using Address = dev::h160;
  static bool ParseCreateContract(uint64_t &gasRemained,
                                  const std::string &runnerPrint,
                                  TransactionReceipt &receipt, bool is_library);

  /// convert the interpreter output into parsable json object for deployment
  static bool ParseCreateContractOutput(Json::Value &jsonOutput,
                                        const std::string &runnerPrint,
                                        TransactionReceipt &receipt);

  /// parse the output from interpreter for deployment
  static bool ParseCreateContractJsonOutput(const Json::Value &_json,
                                            uint64_t &gasRemained,
                                            TransactionReceipt &receipt,
                                            bool is_library);

  /// Contract Calling
  /// verify the return from scilla_runner for calling is valid
  static bool ParseCallContract(CpsAccountStoreInterface &acc_store,
                                uint64_t &gasRemained,
                                const std::string &runnerPrint,
                                TransactionReceipt &receipt,
                                uint32_t tree_depth, uint32_t scilla_version);

  /// convert the interpreter output into parsable json object for calling
  static bool ParseCallContractOutput(CpsAccountStoreInterface &acc_store,
                                      Json::Value &jsonOutput,
                                      const std::string &runnerPrint,
                                      TransactionReceipt &receipt);

  /// parse the output from interpreter for calling and update states
  static bool ParseCallContractJsonOutput(CpsAccountStoreInterface &acc_store,
                                          const Json::Value &_json,
                                          uint64_t &gasRemained,
                                          TransactionReceipt &receipt,
                                          uint32_t tree_depth,
                                          uint32_t pre_scilla_version);

  /// export files that ExportCreateContractFiles and ExportContractFiles
  /// both needs
  static void ExportCommonFiles(
      CpsAccountStoreInterface &acc_store, std::ofstream &os,
      const Address &contract,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  static bool ExportCreateContractFiles(
      CpsAccountStoreInterface &acc_store, const Address &contract,
      bool is_library, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_export);

  /// generate the files for initdata, contract state, blocknum for interpreter
  /// to call contract
  static bool ExportContractFiles(
      CpsAccountStoreInterface &acc_store, const Address &contract,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  /// generate the files for message from txn for interpreter to call contract
  static bool ExportCallContractFiles(
      CpsAccountStoreInterface &acc_store, const Address &sender,
      const Address &contract, const Transaction &transaction,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  /// generate the files for message from previous contract output for
  /// interpreter to call another contract
  static bool ExportCallContractFiles(
      CpsAccountStoreInterface &acc_store, const Address &contract,
      const Json::Value &contractData, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  static void CreateScillaCodeFiles(
      CpsAccountStoreInterface &acc_store, const Address &contract,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports,
      const std::string &scillaCodeExtension);

  static bool ParseContractCheckerOutput(
      CpsAccountStoreInterface &acc_store, const Address &addr,
      const std::string &checkerPrint, TransactionReceipt &receipt,
      std::map<std::string, zbytes> &metadata, uint64_t &gasRemained,
      bool is_library = false);

  static bool PopulateExtlibsExports(
      CpsAccountStoreInterface &acc_store, uint32_t scilla_version,
      const std::vector<Address> &extlibs,
      std::map<Address, std::pair<std::string, std::string>> &extlibs_exports);
};
}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_SCILLAHELPERS_H_