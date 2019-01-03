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

#include <string>

#define BOOST_TEST_MODULE logentrytest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include "libData/AccountData/LogEntry.h"

BOOST_AUTO_TEST_SUITE(logentrytest)

BOOST_AUTO_TEST_CASE(commitAndRollback) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  LogEntry le = LogEntry();
  Address addr;
  Json::Value jv;
  std::string RET;
  std::string errs;
  Json::CharReaderBuilder readBuilder;
  Json::CharReader* cr = readBuilder.newCharReader();
  Json::StreamWriterBuilder writeBuilder;
  Json::StreamWriter* writer = writeBuilder.newStreamWriter();
  std::ostringstream oss;

  BOOST_CHECK_MESSAGE(le.Install(jv, addr) == false,
                      "There should be nothing to install.");

  Json::Value jv_ret = le.GetJsonObject();
  writer->write(jv_ret, &oss);
  RET = oss.str();
  BOOST_CHECK_MESSAGE(RET.compare("null") == 0,
                      "LogEntry.GetJsonObject() should return empty "
                      "Json::Value when nothing installed.");

  std::string jv_s =
      "{ \"_eventname\": \"invalid params\", \"params\": [{\"vname\": 1, "
      "\"type\":2, \"value\":3}, {\"type\":2, \"value\":3}]}";
  cr->parse(jv_s.c_str(), jv_s.c_str() + jv_s.size(), &jv, &errs);
  BOOST_CHECK_MESSAGE(le.Install(jv, addr) == false,
                      "Incomplete eventObj shouldn't be installed.");

  jv_s =
      "{ \"_eventname\": \"valid params\", \"params\": [{\"vname\": 1, "
      "\"type\":2, \"value\":3}, {\"vname\": 1, \"type\":2, \"value\":3}]}";
  cr->parse(jv_s.c_str(), jv_s.c_str() + jv_s.size(), &jv, &errs);
  BOOST_CHECK_MESSAGE(le.Install(jv, addr) == true,
                      "Unexpected eventObj, structure had to be changed, test "
                      "is probably obsolete.");
}

BOOST_AUTO_TEST_SUITE_END()
