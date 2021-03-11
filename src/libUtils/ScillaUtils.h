/*
 * Copyright (C) 2020 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBUTILS_SCILLAUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_SCILLAUTILS_H_

#include <json/json.h>

#include <boost/multiprecision/cpp_int.hpp>

class ScillaUtils {
 public:
  static bool PrepareRootPathWVersion(const uint32_t& scilla_version,
                                      std::string& root_w_version);

  /// get the json format file for the current blocknum
  static Json::Value GetBlockStateJson(const uint64_t& BlockNum);

  /// get the command for invoking the scilla_checker while deploying
  static Json::Value GetContractCheckerJson(const std::string& root_w_version,
                                            bool is_library,
                                            const uint64_t& available_gas);

  /// get the command for invoking the scilla_runner while deploying
  static Json::Value GetCreateContractJson(
      const std::string& root_w_version, bool is_library,
      const uint64_t& available_gas,
      const boost::multiprecision::uint128_t& balance);

  /// get the command for invoking the scilla_runner while calling
  static Json::Value GetCallContractJson(
      const std::string& root_w_version, const uint64_t& available_gas,
      const boost::multiprecision::uint128_t& balance);

  /// get the command for invoking disambiguate_state_json while calling
  static Json::Value GetDisambiguateJson();
};

#endif  // ZILLIQA_SRC_LIBUTILS_SCILLAUTILS_H_