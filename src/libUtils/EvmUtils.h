//
// Created by steve on 06/05/22.
//

#ifndef ZILLIQA_EVMUTILS_H
#define ZILLIQA_EVMUTILS_H

#include <json/json.h>
#include <boost/multiprecision/cpp_int.hpp>

class EvmUtils {
 public:
  static bool PrepareRootPathWVersion(const uint32_t& scilla_version,
                                      std::string& root_w_version);

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
      const boost::multiprecision::uint128_t& balance, const bool& is_library);

  /// get the command for invoking disambiguate_state_json while calling
  static Json::Value GetDisambiguateJson();
};


#endif  // ZILLIQA_EVMUTILS_H
