#define BOOST_TEST_MODULE lookup_server
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include "libData/AccountData/EvmClient.h"
#include "libMediator/Mediator.h"
#include "libServer/LookupServer.h"

class AbstractServerConnectorMock : public jsonrpc::AbstractServerConnector {
 public:
  bool StartListening() final { return true; }
  bool StopListening() final { return true; }
};

BOOST_AUTO_TEST_SUITE(lookup_server)

BOOST_AUTO_TEST_CASE(test_get_eth_call) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "6ffa1caa00000000000000000000000000000000000000000000000000000000000000"
      "14";
  values["toAddr"] = "0xa744160c3De133495aB9F9D77EA54b325b045670";
  paramsRequest[0u] = values;

  Address accountAddress{"0xa744160c3De133495aB9F9D77EA54b325b045670"};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  Json::Value response;
  LookupServer lookupServer(mediator, abstractServerConnector);
  lookupServer.GetEthCallI(paramsRequest, response);
}

BOOST_AUTO_TEST_SUITE_END()
