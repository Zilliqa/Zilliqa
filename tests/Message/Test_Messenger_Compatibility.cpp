/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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

  vector<unsigned char> tmp;

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
