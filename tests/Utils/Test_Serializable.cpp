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

#include <cstdint>
#include <vector>

#include "common/Serializable.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

template <class number_type>
void test(const char* number_label, unsigned int size) {
  LOG_MARKER();

  bytes v;
  number_type n = 65539;

  LOG_GENERAL(INFO, number_label << " value = " << n);

  Serializable::SetNumber<number_type>(v, 0, n, size);
  LOG_PAYLOAD(INFO, "serialized", v, Logger::MAX_BYTES_TO_DISPLAY);

  n = Serializable::GetNumber<number_type>(v, 0, size);
  LOG_GENERAL(INFO, "deserialized = " << n);
}

BOOST_AUTO_TEST_CASE(testSerializable) {
  INIT_STDOUT_LOGGER();

  test<unsigned int>("unsigned int",
                     sizeof(unsigned int));  // native, machine-dependent size
  test<uint32_t>("uint32_t", sizeof(uint32_t));  // cstdint, fixed size
  test<uint256_t>("uint256_t",
                  32);  // boost, fixed size
}

BOOST_AUTO_TEST_SUITE_END()
