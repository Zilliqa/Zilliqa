/*
 * Copyright (C) 202 Zilliqa
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

#ifndef ZILLIQA_EVMUTILS_H
#define ZILLIQA_EVMUTILS_H

#include <json/json.h>
#include <boost/multiprecision/cpp_int.hpp>
#include "libUtils/RunnerDetails.h"

class EvmUtils {
 public:
  static bool         PrepareRootPathWVersion(const uint32_t& scilla_version,
                                      std::string& root_w_version);

  /// get the command for invoking the evm_runner while deploying
  static Json::Value GetCreateContractJson(const RunnerDetails& details);

  /// get the command for invoking the evm_runner while calling
  static Json::Value GetCallContractJson(const RunnerDetails& details);

  static std::string GetDataFromItemData(const std::string& itemData);
};

#endif  // ZILLIQA_EVMUTILS_H
