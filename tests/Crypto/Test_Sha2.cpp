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
 *
 * Test cases obtained from https://www.di-mgt.com.au/sha_testvectors.html
 */

#include <iomanip>
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"

#define BOOST_TEST_MODULE sha2test
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

/// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings
/// in windows consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))

BOOST_AUTO_TEST_SUITE(sha2test)

/**
 * \brief SHA256_check_896bitsx3
 *
 * \details Test the SHA256 hash function
 */
BOOST_AUTO_TEST_CASE(SHA256_001_check_896bitsx3) {
  const unsigned char input[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  unsigned int inputSize = strlen((const char*)input);
  bytes vec;
  copy(input, input + inputSize, back_inserter(vec));

  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(vec);
  sha2.Update(vec);
  sha2.Update(vec);
  bytes output = sha2.Finalize();

  bytes expected;
  DataConversion::HexStrToUint8Vec(
      "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C",
      expected);
  bool is_equal = std::equal(expected.begin(), expected.end(), output.begin(),
                             output.end());
  BOOST_CHECK_EQUAL(is_equal, true);

  sha2.Reset();
  sha2.Update(vec);
  output = sha2.Finalize();
  DataConversion::HexStrToUint8Vec(
      "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1",
      expected);
  is_equal = std::equal(expected.begin(), expected.end(), output.begin(),
                        output.end());
  BOOST_CHECK_EQUAL(is_equal, true);
}

/**
 * \brief SHA256_check_896bitsx3_updatewithoffset
 *
 * \details Test the SHA256 hash function
 */
BOOST_AUTO_TEST_CASE(SHA256_002_check_896bitsx3_updatewithoffset) {
  const unsigned char input[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  unsigned int inputSize = strlen((const char*)input);
  bytes vec;
  copy(input, input + inputSize, back_inserter(vec));

  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(vec, 0, inputSize);
  sha2.Update(vec, 0, inputSize);
  sha2.Update(vec, 0, inputSize);
  bytes output = sha2.Finalize();

  bytes expected;
  DataConversion::HexStrToUint8Vec(
      "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C",
      expected);
  bool is_equal = std::equal(expected.begin(), expected.end(), output.begin(),
                             output.end());
  BOOST_CHECK_EQUAL(is_equal, true);

  sha2.Reset();
  sha2.Update(vec, 0, inputSize);
  output = sha2.Finalize();
  DataConversion::HexStrToUint8Vec(
      "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1",
      expected);
  is_equal = std::equal(expected.begin(), expected.end(), output.begin(),
                        output.end());
  BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_SUITE_END()
