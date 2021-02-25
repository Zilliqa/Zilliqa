/*
 * Copyright (C) 2021 Zilliqa
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

#include <string>
#include "libUtils/AddressConversion.h"
#include "libUtils/DataConversion.h"

#define BOOST_TEST_MODULE address_conversion
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(address_conversion)

using namespace std;

BOOST_AUTO_TEST_CASE(testAddrBech32Decode) {
  INIT_STDOUT_LOGGER();

  using strPairs = std::pair<std::string, std::string>;

  std::vector<strPairs> testPairs;
  testPairs.push_back(
      std::make_pair("zil1fwh4ltdguhde9s7nysnp33d5wye6uqpugufkz7",
                     "4baf5fada8e5db92c3d3242618c5b47133ae003c"));

  testPairs.push_back(
      std::make_pair("zil1gjpxry26srx7n008c7nez6zjqrf6p06wur4x3m",
                     "448261915A80CDE9BDE7C7A791685200D3A0BF4E"));

  testPairs.push_back(
      std::make_pair("zil1mmgzlktelsh9tspy80f02t0sytzq4ks79zdnkk",
                     "DED02FD979FC2E55C0243BD2F52DF022C40ADA1E"));

  testPairs.push_back(
      std::make_pair("zil1z0cxucpf004x50zq9ahkf3qk56e3ukrwaty4g8",
                     "13F06E60297BEA6A3C402F6F64C416A6B31E586E"));

  testPairs.push_back(
      std::make_pair("zil1r2gvy5c8c0x8r9v2s0azzw3rvtv9nnenynd33g",
                     "1A90C25307C3CC71958A83FA213A2362D859CF33"));

  testPairs.push_back(
      std::make_pair("zil1vfdt467c0khf4vfg7we6axtg3qfan3wlf9yc6y",
                     "625ABAEBD87DAE9AB128F3B3AE99688813D9C5DF"));
  testPairs.push_back(
      std::make_pair("zil1x6argztlscger3yvswwfkx5ttyf0tq703v7fre",
                     "36BA34097F861191C48C839C9B1A8B5912F583CF"));
  testPairs.push_back(
      std::make_pair("zil16fzn4emvn2r24e2yljnfnk7ut3tk4me6qx08ed",
                     "D2453AE76C9A86AAE544FCA699DBDC5C576AEF3A"));
  testPairs.push_back(
      std::make_pair("zil1wg3qapy50smprrxmckqy2n065wu33nvh35dn0v",
                     "72220E84947C36118CDBC580454DFAA3B918CD97"));
  testPairs.push_back(
      std::make_pair("zil12rujxpxgjtv55wzu5m8xe454pn56x6pedpl554",
                     "50F92304C892D94A385CA6CE6CD6950CE9A36839"));
  testPairs.push_back(
      std::make_pair("zil1r5verznnwvrzrz6uhveyrlxuhkvccwnju4aehf",
                     "1d19918a737306218b5cbb3241fcdcbd998c3a72"));
  testPairs.push_back(
      std::make_pair("zil1ej8wy3mnux6t9zeuc4vkhww0csctfpznzt4s76",
                     "cc8ee24773e1b4b28b3cc5596bb9cfc430b48453"));
  testPairs.push_back(
      std::make_pair("zil1u9zhd9zyg056ajn0z269f9qcsj4py2fc89ru3d",
                     "e14576944443e9aeca6f12b454941884aa122938"));
  testPairs.push_back(
      std::make_pair("zil1z7fkzy2vhl2nhexng50dlq2gehjvlem5w7kx8z",
                     "179361114cbfd53be4d3451edf8148cde4cfe774"));
  testPairs.push_back(
      std::make_pair("zil1tg4kvl77kc6kt9mgr5y0dntxx6hdj3uy95ash8",
                     "5a2b667fdeb6356597681d08f6cd6636aed94784"));
  testPairs.push_back(
      std::make_pair("zil12de59e0q566q9u5pu26rqxufzgawxyghq0vdk9",
                     "537342e5e0a6b402f281e2b4301b89123ae31117"));
  testPairs.push_back(
      std::make_pair("zil1tesag25495klra89e0kh7lgjjn5hgjjj0qmu8l",
                     "5e61d42a952d2df1f4e5cbed7f7d1294e9744a52"));
  testPairs.push_back(
      std::make_pair("zil1tawmrsvvehn8u5fm0aawsg89dy25ja46ndsrhq",
                     "5f5db1c18ccde67e513b7f7ae820e569154976ba"));

  for (auto& pair : testPairs) {
    Address retAddr;
    auto retCode = ToBase16Addr(pair.first, retAddr);
    BOOST_CHECK_MESSAGE(retCode == AddressConversionCode::OK,
                        "Bech32 unable to decode");

    bytes tmpAddr;
    DataConversion::HexStrToUint8Vec(pair.second, tmpAddr);

    Address correctAddr{tmpAddr};
    BOOST_CHECK_MESSAGE(retAddr == correctAddr, "Bech32 decode incorrectly");
  }
}

BOOST_AUTO_TEST_CASE(testAddrDecodeNegativeCase1) {
  INIT_STDOUT_LOGGER();

  std::vector<string> testStrings{
      "zil", "z", "asdc", "1234567890abcdef1234567890abcdef1234567890abcdef"};

  for (const auto& input : testStrings) {
    Address retAddr;
    auto retCode = ToBase16Addr(input, retAddr);
    BOOST_CHECK_MESSAGE(retCode == AddressConversionCode::WRONG_ADDR_SIZE,
                        "Address decode did not detect invalid size");
  }
}

BOOST_AUTO_TEST_CASE(testAddrDecodeNegativeCase2) {
  INIT_STDOUT_LOGGER();

  std::vector<string> testStrings{
      "zil1", "zil1abc", "zil1T413131515AWMRSVVEHN8U5FM0AAWSG89DY25JA46NDSRHQ"};

  for (const auto& input : testStrings) {
    Address retAddr;
    auto retCode = ToBase16Addr(input, retAddr);
    BOOST_CHECK_MESSAGE(retCode == AddressConversionCode::INVALID_BECH32_ADDR,
                        "Bech32 decode did not detect invalid address");
  }
}

BOOST_AUTO_TEST_CASE(testAddrDecodeNegativeCase3) {
  INIT_STDOUT_LOGGER();

  std::vector<string> testStrings{"xxx8055ea3bc78d759d10663da40d171dec992aa"};

  for (const auto& input : testStrings) {
    Address retAddr;
    auto retCode = ToBase16Addr(input, retAddr);
    BOOST_CHECK_MESSAGE(retCode == AddressConversionCode::INVALID_ADDR,
                        "Address decode did not detect invalid address");
  }
}

BOOST_AUTO_TEST_SUITE_END()
