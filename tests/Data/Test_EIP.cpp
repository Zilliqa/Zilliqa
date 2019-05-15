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

#include <array>
#include <string>
#include <thread>
#include <vector>
#include "libServer/AddressChecksum.h"

#define BOOST_TEST_MODULE testeip
#define BOOST_TEST_DYN_LINK
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include "libUtils/HashUtils.h"

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(testeip)

BOOST_AUTO_TEST_CASE(testAddress) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  // Vectors generated from js lib
  const vector<string> testVector = {
      "4BAF5FADA8E5DB92C3D3242618C5B47133AE003C",
      "448261915A80CDE9BDE7C7A791685200D3A0BF4E",
      "DED02FD979FC2E55C0243BD2F52DF022C40ADA1E",
      "13F06E60297BEA6A3C402F6F64C416A6B31E586E",
      "1A90C25307C3CC71958A83FA213A2362D859CF33",
      "625ABAEBD87DAE9AB128F3B3AE99688813D9C5DF",
      "36BA34097F861191C48C839C9B1A8B5912F583CF",
      "D2453AE76C9A86AAE544FCA699DBDC5C576AEF3A",
      "72220E84947C36118CDBC580454DFAA3B918CD97",
      "50F92304C892D94A385CA6CE6CD6950CE9A36839"};

  const vector<string> resultVector = {
      "4BAF5faDA8e5Db92C3d3242618c5B47133AE003C",
      "448261915a80cdE9BDE7C7a791685200D3A0bf4E",
      "Ded02fD979fC2e55c0243bd2F52df022c40ADa1E",
      "13F06E60297bea6A3c402F6f64c416A6b31e586e",
      "1a90C25307C3Cc71958A83fa213A2362D859CF33",
      "625ABAebd87daE9ab128f3B3AE99688813d9C5dF",
      "36Ba34097f861191C48C839c9b1a8B5912f583cF",
      "D2453Ae76C9A86AAe544fca699DbDC5c576aEf3A",
      "72220e84947c36118cDbC580454DFaa3b918cD97",
      "50f92304c892D94A385cA6cE6CD6950ce9A36839"};

  BOOST_CHECK_MESSAGE(resultVector.size() == testVector.size(),
                      "Result vector size and test vector size not same "
                          << " resultVector " << resultVector.size()
                          << " testVector " << testVector.size());

  for (uint i = 0; i < testVector.size(); i++) {
    const auto& result =
        AddressChecksum::GetCheckSumedAddress(testVector.at(i));

    BOOST_CHECK_MESSAGE(
        result == resultVector.at(i),
        "Result = " << result << " resultVector[]" << resultVector.at(i));
  }
}
BOOST_AUTO_TEST_SUITE_END()
