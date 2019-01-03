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

#include "Message/ZilliqaTest.pb.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ZilliqaTest;

BOOST_AUTO_TEST_SUITE(messenger_protobuf_test)

BOOST_AUTO_TEST_CASE(test_optionalfield) {
  INIT_STDOUT_LOGGER();

  bytes tmp;

  // Serialize a OneField message
  OneField oneField;
  oneField.set_field1(12345);
  BOOST_CHECK(oneField.IsInitialized());
  LOG_GENERAL(INFO, "oneField.field1 = " << oneField.field1());
  tmp.resize(oneField.ByteSize());
  oneField.SerializeToArray(tmp.data(), oneField.ByteSize());

  // Try to deserialize it as a OneField
  OneField oneFieldDeserialized;
  oneFieldDeserialized.ParseFromArray(tmp.data(), tmp.size());
  BOOST_CHECK(oneFieldDeserialized.IsInitialized());
  BOOST_CHECK(oneFieldDeserialized.field1() == oneField.field1());
  LOG_GENERAL(
      INFO, "oneFieldDeserialized.field1 = " << oneFieldDeserialized.field1());

  // Try to deserialize it as a TwoFields
  TwoFields twoFields;
  twoFields.ParseFromArray(tmp.data(), tmp.size());
  BOOST_CHECK(twoFields.IsInitialized());
  BOOST_CHECK(twoFields.has_field1());
  BOOST_CHECK(!twoFields.has_field2());
  BOOST_CHECK(twoFields.field1() == oneField.field1());
  LOG_GENERAL(INFO, "twoFields.field1 = " << twoFields.field1());
}

BOOST_AUTO_TEST_SUITE_END()
