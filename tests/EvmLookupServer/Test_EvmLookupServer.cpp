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
  /**
   * @brief EvmClient mock implementation te be able to inject test responses
   * from the Evm-ds server.
   */
  class EvmClientMock : public EvmClient {
   public:
    bool OpenServer(bool /*force = false*/) final { return true; };

    bool CallRunner(uint32_t /*version*/,             //
                    const Json::Value& request,       //
                    evmproj::CallResponse& response,  //
                    uint32_t /*counter = MAXRETRYCONN*/) final {
      //
      LOG_GENERAL(DEBUG, "Callrunner json request:" << request);

      Json::Reader _reader;

      const std::string expectedRequestString{
          "[\"a744160c3de133495ab9f9d77ea54b325b045670\","
          "\"0000000000000000000000000000000000000000\","
          "\"\","
          "\"ffa1caa00000000000000000000000000000000000000000000000000000000000"
          "00014\","
          "\"0\","
          "1000000"
          "]"};

      Json::Value expectedRequestJson;
      LOG_GENERAL(DEBUG, "expectedRequestString:" << expectedRequestString);
      BOOST_CHECK(_reader.parse(expectedRequestString, expectedRequestJson));

      BOOST_CHECK_EQUAL(request.size(), expectedRequestJson.size());

      for (Json::ArrayIndex i = 0; i < request.size(); i++) {
        LOG_GENERAL(DEBUG, "test requests(" << i << "):" << request[i] << ","
                                            << expectedRequestJson[i]);
        if (request[i].isConvertibleTo(Json::intValue)) {
          BOOST_CHECK_EQUAL(request[i].asInt(), expectedRequestJson[i].asInt());
        } else {
          BOOST_CHECK_EQUAL(request[i], expectedRequestJson[i]);
        }
      }

      Json::Value responseJson;
      const std::string evmResponseString =
          "{\"apply\":"
          "["
          "{\"modify\":"
          "{\"address\":\"0x4b68ebd5c54ae9ad1f069260b4c89f0d3be70a45\","
          "\"balance\":\"0x0\","
          "\"code\":null,"
          "\"nonce\":\"0x0\","
          "\"reset_storage\":false,"
          "\"storage\":[ ["
          "\"CgxfZXZtX3N0b3JhZ2UQARpAMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMD"
          "AwMD"
          "AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMA==\","
          "\"CiAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAA==\" ] ]"
          "}"
          "}"
          "],"
          "\"exit_reason\":"
          "{"
          " \"Succeed\":\"Returned\""
          "},"
          "\"logs\":[],"
          "\"remaining_gas\":77371,"
          "\"return_value\":"
          "\"608060405234801561001057600080fd5b50600436106100415760003560e0"
          "1c80"
          "632e64cec11461004657806336b62288146100645780636057361d1461006e57"
          "5b60"
          "0080fd5b61004e61008a565b60405161005b91906100d0565b60405180910390"
          "f35b"
          "61006c610093565b005b6100886004803603810190610083919061011c565b61"
          "00ad"
          "565b005b60008054905090565b600073ffffffffffffffffffffffffffffffff"
          "ffff"
          "ffff16ff5b8060008190555050565b6000819050919050565b6100ca816100b7"
          "565b"
          "82525050565b60006020820190506100e560008301846100c1565b9291505056"
          "5b60"
          "0080fd5b6100f9816100b7565b811461010457600080fd5b50565b6000813590"
          "5061"
          "0116816100f0565b92915050565b600060208284031215610132576101316100"
          "eb56"
          "5b5b600061014084828501610107565b9150509291505056fea2646970667358"
          "2212"
          "202ea2150908951ac2bb5f9e1fe7663301a0be11ecdc6d8fc9f49333262e264d"
          "b564"
          "736f6c634300080f0033\""
          "}";

      BOOST_CHECK(_reader.parse(evmResponseString, responseJson));
      LOG_GENERAL(DEBUG, "Callrunner json response:" << responseJson);
      evmproj::GetReturn(responseJson, response);

      return true;
    };
  };

  EvmClient::GetInstance(  //
      []() {               //
        return std::make_shared<EvmClientMock>();
      });

  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["toAddr"] = "0xa744160c3De133495aB9F9D77EA54b325b045670";
  // values["gasLimit"] = 500;
  paramsRequest[0u] = values;

  Address accountAddress{"0xa744160c3De133495aB9F9D77EA54b325b045670"};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);
  // const uint128_t initialBalance{1000000};
  // AccountStore::GetInstance().IncreaseBalance(accountAddress,
  // initialBalance);

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  lookupServer.GetEthCallI(paramsRequest, response);

  LOG_GENERAL(DEBUG, "GetEthCall response:" << response);
  BOOST_CHECK_EQUAL(response.asString(),
                    "0x608060405234801561001057600080fd5b50600436106100415760"
                    "003560e01c80632e"
                    "64cec11461004657806336b62288146100645780636057361d146100"
                    "6e575b600080fd5b"
                    "61004e61008a565b60405161005b91906100d0565b60405180910390"
                    "f35b61006c610093"
                    "565b005b6100886004803603810190610083919061011c565b6100ad"
                    "565b005b60008054"
                    "905090565b600073ffffffffffffffffffffffffffffffffffffffff"
                    "16ff5b8060008190"
                    "555050565b6000819050919050565b6100ca816100b7565b82525050"
                    "565b600060208201"
                    "90506100e560008301846100c1565b92915050565b600080fd5b6100"
                    "f9816100b7565b81"
                    "1461010457600080fd5b50565b600081359050610116816100f0565b"
                    "92915050565b6000"
                    "60208284031215610132576101316100eb565b5b6000610140848285"
                    "01610107565b9150"
                    "509291505056fea26469706673582212202ea2150908951ac2bb5f9e"
                    "1fe7663301a0be11"
                    "ecdc6d8fc9f49333262e264db564736f6c634300080f0033");

  // const auto balance =
  // AccountStore::GetInstance().GetBalance(accountAddress);
  // BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), 77371);
}

BOOST_AUTO_TEST_SUITE_END()
