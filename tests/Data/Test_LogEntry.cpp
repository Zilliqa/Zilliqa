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

#include <string>

#define BOOST_TEST_MODULE logentrytest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include "libData/AccountData/LogEntry.h"

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(logentrytest)

BOOST_AUTO_TEST_CASE(commitAndRollback) {
  INIT_STDOUT_LOGGER();

  LogEntry le = LogEntry();
  Address addr;
  Json::Value jv;
  Json::Reader reader;
  Json::FastWriter fastWriter;
  std::string RET;

  BOOST_CHECK_EQUAL(false, le.Install(jv, addr));
  Json::Value jv_ret = le.GetJsonObject();
  RET = fastWriter.write(jv_ret);

  BOOST_CHECK_EQUAL(true, RET.compare("null"));

  std::string jv_s =
      "{ \"_eventname\": \"invalid params\", \"params\": [{\"vname\": 1, "
      "\"type\":2, \"value\":3}, {\"type\":2, \"value\":3}]}";
  reader.parse(jv_s, jv);
  BOOST_CHECK_EQUAL(false, le.Install(jv, addr));

  jv_s =
      "{ \"_eventname\": \"valid params\", \"params\": [{\"vname\": 1, "
      "\"type\":2, \"value\":3}, {\"vname\": 1, \"type\":2, \"value\":3}]}";
  reader.parse(jv_s, jv);
  BOOST_CHECK_EQUAL(true, le.Install(jv, addr));

  LOG_MARKER();
}

BOOST_AUTO_TEST_SUITE_END()
