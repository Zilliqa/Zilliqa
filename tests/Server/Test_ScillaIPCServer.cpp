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

#include "libServer/ScillaIPCServer.h"
#include "libUtils/Logger.h"
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/unixdomainsocketclient.h>
#include <thread>

#define BOOST_TEST_MODULE scillaipc
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace jsonrpc;

BOOST_AUTO_TEST_SUITE(scillaipc)

BOOST_AUTO_TEST_CASE(test_contract_storage2_call) {
  INIT_STDOUT_LOGGER();
  UnixDomainSocketServer s("/tmp/scillaipcservertestsocket1");
  ScillaIPCServer server(s, dev::h160());
  LOG_GENERAL(INFO, "Test ScillaIPCServer initialization done!");
  BOOST_CHECK_MESSAGE(server.testServer(),
                      "Server should be able to call ContractStorage2");
  LOG_GENERAL(INFO, "Test ScillaIPCServer calling ContractStorage2 done!");
}

BOOST_AUTO_TEST_CASE(test_rpc) {
  INIT_STDOUT_LOGGER();
  UnixDomainSocketServer s("/tmp/scillaipcservertestsocket");
  ScillaIPCServer server(s, dev::h160());
  LOG_GENERAL(INFO, "Test ScillaIPCServer initialization done!");
  server.StartListening();
  UnixDomainSocketClient client("/tmp/scillaipcservertestsocket");
  Client c(client);
  Json::Value params;
  params["query"] = "testQuery";
  params["value"] = "testValue";
  LOG_GENERAL(INFO, "About to call server method");
  BOOST_CHECK_MESSAGE(c.CallMethod("testServerRPC", params) == 
                      "Query = testQuery & Value = testValue",
                      "Server should be able to respond to RPC calls");
  server.StopListening();
  LOG_GENERAL(INFO, "Test ScillaIPCServer RPC done!");
}

BOOST_AUTO_TEST_SUITE_END()