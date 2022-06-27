/*
 * Copyright (C) 2022 Zilliqa
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

#include "libData/AccountData/Address.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/Logger.h"
#include "libUtils/nlohmann/json.hpp"  // NOLINT(readability-redundant-declaration)
#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(test_EvmJsonResponseGoodCreate) {
  INIT_STDOUT_LOGGER();

  evmproj::CallResponse result = {};

  std::string input =
      "{\"apply\":[],\"exit_reason\":{\"Succeed\":\"Returned\"},\"logs\":[],"
      "\"remaining_gas\":999024,\"return_value\":"
      "\"0000000000000000000000000000000000000000000000000000000000000028\"}";

  Json::Value tmp;
  Json::Reader _reader;
  if (_reader.parse(input, tmp)) {
    std::cout << "success" << std::endl;
  }

  try {
    auto response = evmproj::GetReturn(tmp, result);
  } catch (std::exception& e) {
    cout << "Exception caught in EVMResponse " << e.what() << std::endl;
  }

  BOOST_REQUIRE(result.isSuccess() == true);
  BOOST_REQUIRE(result.Apply().empty() == true);
  BOOST_REQUIRE(result.Logs().empty() == true);
  BOOST_REQUIRE(result.Gas() == 999024);
  BOOST_REQUIRE(result.Apply().size() == 0);
  BOOST_REQUIRE(result.ExitReason() == "Returned");
  BOOST_REQUIRE(result.ReturnedBytes().size() > 0);
}

BOOST_AUTO_TEST_CASE(test_EvmJsonResponseGoodCreate2) {
  INIT_STDOUT_LOGGER();

  evmproj::CallResponse result = {};

  std::string input =
      "{\"apply\":[],\"exit_reason\":{\"Succeed\":\"Returned\"},\"logs\":[],"
      "\"remaining_gas\":99823,\"return_value\":"
      "\"608060405234801561001057600080fd5b506004361061002b5760003560e01c80636f"
      "fa1caa14610030575b600080fd5b61004a600480360381019061004591906100b1565b61"
      "0060565b60405161005791906100ed565b60405180910390f35b600081600261006f9190"
      "610137565b9050919050565b600080fd5b6000819050919050565b61008e8161007b565b"
      "811461009957600080fd5b50565b6000813590506100ab81610085565b92915050565b60"
      "00602082840312156100c7576100c6610076565b5b60006100d58482850161009c565b91"
      "505092915050565b6100e78161007b565b82525050565b60006020820190506101026000"
      "8301846100de565b92915050565b7f4e487b710000000000000000000000000000000000"
      "0000000000000000000000600052601160045260246000fd5b60006101428261007b565b"
      "915061014d8361007b565b9250827f7fffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffff048211600084136000841316161561018c5761018b61010856"
      "5b5b817f8000000000000000000000000000000000000000000000000000000000000000"
      "05831260008412600084131616156101c9576101c8610108565b5b827f80000000000000"
      "000000000000000000000000000000000000000000000000000582126000841360008412"
      "16161561020657610205610108565b5b827f7fffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffff05821260008412600084121616156102435761024261"
      "0108565b5b82820290509291505056fea26469706673582212207467486f1004599032a9"
      "e75d7511254dc013c9f001d97a66e67973da0858adc964736f6c634300080f0033\"}";

  Json::Value tmp;
  Json::Reader _reader;
  if (_reader.parse(input, tmp)) {
    std::cout << "success" << std::endl;
  }

  try {
    auto response = evmproj::GetReturn(tmp, result);
  } catch (std::exception& e) {
    cout << "Exception caught in EVMResponse " << e.what() << std::endl;
  }
  BOOST_REQUIRE(result.isSuccess() == true);
  BOOST_REQUIRE(result.Apply().empty() == true);
  BOOST_REQUIRE(result.Logs().empty() == true);
  BOOST_REQUIRE(result.Gas() == 99823);
  BOOST_REQUIRE(result.Apply().size() == 0);
  BOOST_REQUIRE(result.ExitReason() == "Returned");
  BOOST_REQUIRE(result.ReturnedBytes().size() > 0);
}

BOOST_AUTO_TEST_CASE(test_EvmJsonResponseGoodCall2) {
  INIT_STDOUT_LOGGER();

  evmproj::CallResponse result = {};

  std::string input =
      "{\"apply\":[],\"exit_reason\":{\"Succeed\":\"Stopped\"},\"logs\":[{"
      "\"address\":\"0x0c23b9e61e5fe6d9810543dc5fb9dfb7f0019549\",\"data\":[0,"
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,0,0,"
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,12,72,101,108,108,"
      "111,32,87,111,114,108,100,33,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"
      "\"topics\":["
      "\"0x0738f4da267a110d810e6e89fc59e46be6de0c37b1d5cd559b267dc3688e74e0\","
      "\"0x000000000000000000000000381f4008505e940ad7681ec3468a719060caf796\"]}"
      ",{\"address\":\"0x0c23b9e61e5fe6d9810543dc5fb9dfb7f0019549\",\"data\":["
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,0,"
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,72,101,108,"
      "108,111,32,69,86,77,33,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"
      "\"topics\":["
      "\"0x0738f4da267a110d810e6e89fc59e46be6de0c37b1d5cd559b267dc3688e74e0\","
      "\"0x000000000000000000000000381f4008505e940ad7681ec3468a719060caf796\"]}"
      ",{\"address\":\"0x0c23b9e61e5fe6d9810543dc5fb9dfb7f0019549\",\"data\":[]"
      ",\"topics\":["
      "\"0xfe1a3ad11e425db4b8e6af35d11c50118826a496df73006fc724cb27f2b99946\"]}"
      "],\"remaining_gas\":94743,\"return_value\":\"\"}";

  Json::Value tmp;
  Json::Reader _reader;
  if (_reader.parse(input, tmp)) {
    std::cout << "success" << std::endl;
  }
  Json::Value just = input;
  try {
    auto response = evmproj::GetReturn(tmp, result);
  } catch (std::exception& e) {
    cout << "Exception caught in EVMResponse " << e.what() << std::endl;
  }
  BOOST_REQUIRE(result.isSuccess());
  BOOST_REQUIRE(result.Apply().empty());
  BOOST_REQUIRE(!result.Logs().empty());
  BOOST_REQUIRE(result.Gas() == 94743);
  BOOST_REQUIRE(result.Apply().size() == 0);
  BOOST_REQUIRE(result.ExitReason() == "Stopped");
  BOOST_REQUIRE(result.ReturnedBytes().empty());
  BOOST_REQUIRE(result.ReturnedBytes().size() == 0);

  //
  // Test to make sure both parsers can understand the Json
  //
  //
  BOOST_ASSERT(result.Logs().size() > 0);
  if (result.Logs().size() > 0) {
    for (const auto& it : result.Logs()) {
      try {
        Json::Value t2;
        Json::Reader _r2;
        BOOST_REQUIRE(_r2.parse(input, t2));
      } catch (std::exception& e) {
        LOG_GENERAL(INFO,
                    "EvmJsonResponse Test routine failed to create a Json "
                    "object from input JSONCPPRPC "
                        << it);
      }
      try {
        nlohmann::json jv = nlohmann::json::parse(input);
      } catch (std::exception& ee) {
        LOG_GENERAL(INFO,
                    "EvmJsonResponse Test routine failed to create a Json "
                    "object from input nlohmann json "
                        << it);
      }
    }
  }
}
BOOST_AUTO_TEST_CASE(test_EvmJsonResponseGoodCall3) {
  INIT_STDOUT_LOGGER();

  evmproj::CallResponse result = {};

  std::string input =
      "{\"apply\":[{\"modify\":{\"address\":"
      "\"0x4b68ebd5c54ae9ad1f069260b4c89f0d3be70a45\",\"balance\":\"0x0\","
      "\"code\":null,\"nonce\":\"0x0\",\"reset_storage\":false,\"storage\":[["
      "\"CgxfZXZtX3N0b3JhZ2UQARpAMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMD"
      "AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMA==\","
      "\"CiAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAA==\"]]}}],\"exit_"
      "reason\":{\"Succeed\":\"Returned\"},\"logs\":[],\"remaining_gas\":77771,"
      "\"return_value\":"
      "\"608060405234801561001057600080fd5b50600436106100415760003560e01c80632e"
      "64cec11461004657806336b62288146100645780636057361d1461006e575b600080fd5b"
      "61004e61008a565b60405161005b91906100d0565b60405180910390f35b61006c610093"
      "565b005b6100886004803603810190610083919061011c565b6100ad565b005b60008054"
      "905090565b600073ffffffffffffffffffffffffffffffffffffffff16ff5b8060008190"
      "555050565b6000819050919050565b6100ca816100b7565b82525050565b600060208201"
      "90506100e560008301846100c1565b92915050565b600080fd5b6100f9816100b7565b81"
      "1461010457600080fd5b50565b600081359050610116816100f0565b92915050565b6000"
      "60208284031215610132576101316100eb565b5b600061014084828501610107565b9150"
      "509291505056fea26469706673582212202ea2150908951ac2bb5f9e1fe7663301a0be11"
      "ecdc6d8fc9f49333262e264db564736f6c634300080f0033\"}";

  Json::Value tmp;
  Json::Reader _reader;
  if (_reader.parse(input, tmp)) {
    std::cout << "success" << std::endl;
  }
  Json::Value just = input;

  auto response = evmproj::GetReturn(tmp, result);

  BOOST_REQUIRE(result.isSuccess());
  BOOST_REQUIRE(!result.Apply().empty());
  BOOST_REQUIRE(result.Logs().empty());
  BOOST_REQUIRE(result.Gas() == 77771);

  BOOST_REQUIRE(result.Apply().size() > 0);
  BOOST_REQUIRE(result.ExitReason() == "Returned");
  BOOST_REQUIRE(!result.ReturnedBytes().empty());
  BOOST_REQUIRE(result.ReturnedBytes().size() > 0);

  if (result.Apply().size() > 0) {
    for (const auto& it : result.Apply()) {
      BOOST_REQUIRE(it->isResetStorage() == false);
      BOOST_REQUIRE(it->OperationType() == std::string("modify"));
      BOOST_REQUIRE(it->hasAddress() && it->Address().size() > 0);
      BOOST_REQUIRE(it->Balance() == "0x0");
      BOOST_REQUIRE(it->Code().empty());
      BOOST_REQUIRE(it->Address().c_str() ==
                    std::string("0x4b68ebd5c54ae9ad1f069260b4c89f0d3be70a45"));
      BOOST_REQUIRE(it->hasBalance() && it->Balance().size() > 0);
      BOOST_REQUIRE(it->hasNonce() && it->Nonce().size() > 0 &&
                    it->Nonce() == "0x0");
      BOOST_REQUIRE(!it->Storage().empty());
      BOOST_REQUIRE(it->Storage().size() > 0);
      for (const auto& storage_iter : it->Storage()) {
        // we can do much with storage as we don't know what it is.
        const std::string& key = storage_iter.Key();
        const std::string& value = storage_iter.Value();
        BOOST_REQUIRE(key.size() > 0);
        BOOST_REQUIRE(value.size() > 0);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()