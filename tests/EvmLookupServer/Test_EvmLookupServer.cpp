/*
 * Copyright (C) 2022 Zilliqa
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

#define BOOST_TEST_MODULE EvmLookupServer
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

/**
 * @brief Default Mock implementation for the evm client
 */
class EvmClientMock : public EvmClient {
 public:
  EvmClientMock() = default;

  bool OpenServer(bool /*force = false*/) { return true; };

  bool CallRunner(uint32_t /*version*/,                 //
                  const Json::Value& request,           //
                  evmproj::CallResponse& /*response*/,  //
                  uint32_t /*counter = MAXRETRYCONN*/) {
    LOG_GENERAL(DEBUG, "CallRunner json request:" << request);
    return true;
  };
};

std::unique_ptr<LookupServer> getLookupServer() {
  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  const PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  auto lookupServer =
      std::make_unique<LookupServer>(mediator, abstractServerConnector);
  return lookupServer;
}

TransactionWithReceipt constructTxWithReceipt(uint64_t nonce,
                                              const PairOfKey& keyPair) {
  return TransactionWithReceipt(
      // Ctor: (version, nonce, toAddr, keyPair, amount, gasPrice, gasLimit,
      // code, data)
      Transaction{0,
                  nonce,
                  Account::GetAddressFromPublicKey(keyPair.second),
                  keyPair,
                  1,
                  1,
                  2,
                  {},
                  {}},
      TransactionReceipt{});
}

MicroBlock constructMicroBlockWithTransactions(
    uint64_t blockNum, const std::vector<TransactionWithReceipt>& transactions,
    PairOfKey& keyPair) {
  MicroBlockHashSet mbhs{dev::h256::random(), {}, {}};
  // CTor: (shardId, gasLimit, gasUsed, rewards, epochNum, mbHashSet, numTxs,
  // minerPubKey, dsBlockNum, version, commiteeHash, prevHash)
  MicroBlockHeader mbh(0, 2, 1, 0, blockNum, mbhs, transactions.size(),
                       keyPair.first, 0, {}, {});

  std::vector<TxnHash> transactionHashes;
  for (const auto& transaction : transactions) {
    transactionHashes.push_back(transaction.GetTransaction().GetTranID());
  }

  MicroBlock mb(mbh, transactionHashes, CoSignatures{});
  return mb;
}

TxBlock constructTxBlockWithTransactions(uint64_t blockNum,
                                         const MicroBlock& microBlock,
                                         PairOfKey& keyPair) {
  // CTor: (gasLimit, gasUsed, rewards, blockNum, blockHashSet, numTxs,
  // minerPubKey, dsBlocknum, version, commiteeHash, prevHash)
  TxBlockHeader txblockheader(2, 1, 0, blockNum, {},
                              microBlock.GetTranHashes().size(), keyPair.first,
                              TXBLOCK_VERSION);

  MicroBlockInfo mbInfo{microBlock.GetBlockHash(),
                        microBlock.GetHeader().GetTxRootHash(),
                        microBlock.GetHeader().GetShardId()};
  TxBlock txblock(txblockheader, {mbInfo}, CoSignatures{});

  return txblock;
}

TxBlock buildCommonEthBlockCase(
    Mediator& mediator, uint64_t blockNum,
    const std::vector<TransactionWithReceipt>& transactions,
    PairOfKey& keyPair) {
  const MicroBlock microBlock =
      constructMicroBlockWithTransactions(blockNum, transactions, keyPair);
  bytes microBlockSerialized;
  microBlock.Serialize(microBlockSerialized, 0);
  BlockStorage::GetBlockStorage().PutMicroBlock(
      microBlock.GetBlockHash(), blockNum, blockNum, microBlockSerialized);
  const TxBlock txBlock =
      constructTxBlockWithTransactions(blockNum, microBlock, keyPair);
  mediator.m_txBlockChain.AddBlock(txBlock);
  return txBlock;
}

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_eth_call) {
  /**
   * @brief EvmClient mock implementation te be able to inject test responses
   * from the Evm-ds server.
   */
  class GetEthCallEvmClientMock : public EvmClientMock {
   public:
    GetEthCallEvmClientMock(const uint gasLimit, const uint amount)
        : m_GasLimit(gasLimit),  //
          m_Amount(amount){};

    bool CallRunner(uint32_t /*version*/,             //
                    const Json::Value& request,       //
                    evmproj::CallResponse& response,  //
                    uint32_t /*counter = MAXRETRYCONN*/) final {
      //
      LOG_GENERAL(DEBUG, "CallRunner json request:" << request);

      Json::Reader _reader;

      std::stringstream expectedRequestString;
      expectedRequestString
          << "["
          << "\"a744160c3de133495ab9f9d77ea54b325b045670\","
          << "\"0000000000000000000000000000000000000000\","
          << "\"\","
          << "\"ffa1caa000000000000000000000000000000000000000000000000000000"
             "0000000014\","
          << "\"" << m_Amount << "\"";
      expectedRequestString << "," << std::to_string(m_GasLimit);  // gas value
      expectedRequestString << "]";

      Json::Value expectedRequestJson;
      LOG_GENERAL(DEBUG,
                  "expectedRequestString:" << expectedRequestString.str());
      BOOST_CHECK(
          _reader.parse(expectedRequestString.str(), expectedRequestJson));

      BOOST_CHECK_EQUAL(request.size(), expectedRequestJson.size());
      auto i{0U};
      for (const auto& r : request) {
        LOG_GENERAL(DEBUG, "test requests(" << i << "):" << r << ","
                                            << expectedRequestJson[i]);
        if (r.isConvertibleTo(Json::intValue)) {
          BOOST_CHECK_EQUAL(r.asInt(), expectedRequestJson[i].asInt());
        } else {
          BOOST_CHECK_EQUAL(r, expectedRequestJson[i]);
        }
        i++;
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
      LOG_GENERAL(DEBUG, "CallRunner json response:" << responseJson);
      evmproj::GetReturn(responseJson, response);

      return true;
    };

   private:
    const uint m_GasLimit{};
    const uint m_Amount{};
  };

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT + 500U};
  const auto amount{4200U};
  EvmClient::GetInstance(  //
      [amount]() {         //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT, amount);  // gas limit will not exceed
                                                   // this max value
      });

  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["toAddr"] = "a744160c3De133495aB9F9D77EA54b325b045670";
  values["gasLimit"] = gasLimit;
  values["amount"] = amount;
  paramsRequest[0u] = values;

  Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};
  Account account;
  if (!AccountStore::GetInstance().IsAccountExist(accountAddress)) {
    AccountStore::GetInstance().AddAccount(accountAddress, account);
  }
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  lookupServer->GetEthCallI(paramsRequest, response);

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

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_web3_clientVersion) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  Json::Value response;
  const Json::Value paramsRequest = Json::Value(Json::arrayValue);
  // call the method on the lookup server with params
  const auto lookupServer = getLookupServer();
  lookupServer->GetWeb3ClientVersionI(paramsRequest, response);

  LOG_GENERAL(DEBUG, "GetWeb3ClientVersion response:" << response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "to do implement web3 version string");
}

BOOST_AUTO_TEST_CASE(test_web3_sha3) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = "68656c6c6f20776f726c64";
  lookupServer->GetWeb3Sha3I(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(
      response.asString(),
      "b1e9ddd229f9a21ef978f6fcd178e74e37a4fa3d87f453bc34e772ec91328181");

  // test with empty string
  paramsRequest[0u] = "";
  lookupServer->GetWeb3Sha3I(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(
      response.asString(),
      "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
}

BOOST_AUTO_TEST_CASE(test_eth_mining) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer->GetEthMiningI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "false");
}

BOOST_AUTO_TEST_CASE(test_eth_coinbase) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();

  Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};
  if (!AccountStore::GetInstance().IsAccountExist(accountAddress)) {
    Account account;
    AccountStore::GetInstance().AddAccount(accountAddress, account);
  }
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer->GetEthCoinbaseI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "");
}

BOOST_AUTO_TEST_CASE(test_net_version) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer->GetNetVersionI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "");
}

BOOST_AUTO_TEST_CASE(test_net_listening) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer->GetNetListeningI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "false");
}

BOOST_AUTO_TEST_CASE(test_net_peer_count) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer->GetNetPeerCountI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "0x0");
}

BOOST_AUTO_TEST_CASE(test_net_protocol_version) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer->GetProtocolVersionI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "");
}

BOOST_AUTO_TEST_CASE(test_eth_chain_id) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer->GetEthChainIdI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "0x814d");
}

BOOST_AUTO_TEST_CASE(test_eth_syncing) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer->GetEthSyncingI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());
  const Json::Value expectedResponse{false};
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_accounts) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer->GetEthAccountsI(paramsRequest, response);

  const Json::Value expectedResponse = Json::arrayValue;
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_get_uncle_by_hash_and_idx) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  paramsRequest[0u] = "0x68656c6c6f20776f726c64";
  paramsRequest[1u] = "0x1";

  lookupServer->GetEthUncleBlockI(paramsRequest, response);

  const Json::Value expectedResponse = Json::nullValue;
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_get_uncle_by_num_and_idx) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  paramsRequest[0u] = "0x666";
  paramsRequest[1u] = "0x1";

  lookupServer->GetEthUncleBlockI(paramsRequest, response);

  const Json::Value expectedResponse = Json::nullValue;
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_get_uncle_count_by_hash) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  paramsRequest[0u] = "0x68656c6c6f20776f726c64";

  lookupServer->GetEthUncleCountI(paramsRequest, response);

  const Json::Value expectedResponse = Json::Value{0};
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_get_uncle_count_by_number) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  paramsRequest[0u] = "0x10";

  lookupServer->GetEthUncleCountI(paramsRequest, response);

  const Json::Value expectedResponse = Json::Value{0};
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_net_version) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetNetVersionI(paramsRequest, response);

  if (response.asString().size() > 0) {
    BOOST_FAIL("Failed to get net version");
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_balance) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;

  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = "0x6cCAa29b6cD36C8238E8Fa137311de6153b0b4e7";

  lookupServer.GetEthBalanceI(paramsRequest, response);

  if (!(response.asString() == "0x0")) {
    BOOST_FAIL("Failed to get empty balance!");
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_block_by_nummber) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  // Construct all relevant structures (sample transactions, microblock and
  // txBlock)
  std::vector<TransactionWithReceipt> transactions;

  constexpr uint32_t TRANSACTIONS_COUNT = 2;
  for (uint32_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    transactions.emplace_back(constructTxWithReceipt(i, pairOfKey));
  }

  constexpr auto BLOCK_NUM = 1;
  const auto txBlock =
      buildCommonEthBlockCase(mediator, BLOCK_NUM, transactions, pairOfKey);

  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = std::to_string(BLOCK_NUM);
  paramsRequest[1u] = false;

  Json::Value response;
  lookupServer.GetEthBlockByNumberI(paramsRequest, response);

  BOOST_CHECK_EQUAL(response["hash"].asString(), txBlock.GetBlockHash().hex());

  std::vector<std::string> expectedHashes;
  for (uint32_t i = 0; i < transactions.size(); ++i) {
    expectedHashes.emplace_back(
        transactions[i].GetTransaction().GetTranID().hex());
  }
  std::sort(expectedHashes.begin(), expectedHashes.end());

  std::vector<std::string> receivedHashes;
  const Json::Value arrayOfHashes = response["transactions"];
  for (auto jsonIter = arrayOfHashes.begin(); jsonIter != arrayOfHashes.end();
       ++jsonIter) {
    receivedHashes.emplace_back(jsonIter->asString());
  }
  std::sort(receivedHashes.begin(), receivedHashes.end());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedHashes.cbegin(), expectedHashes.cend(),
                                receivedHashes.cbegin(), receivedHashes.cend());
}

BOOST_AUTO_TEST_CASE(test_eth_get_block_by_hash) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  // Construct all relevant structures (sample transactions, microblock and
  // txBlock)
  std::vector<TransactionWithReceipt> transactions;

  constexpr uint32_t TRANSACTIONS_COUNT = 2;
  for (uint32_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    transactions.emplace_back(constructTxWithReceipt(i, pairOfKey));
  }

  constexpr auto BLOCK_NUM = 1;
  const auto txBlock =
      buildCommonEthBlockCase(mediator, BLOCK_NUM, transactions, pairOfKey);

  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = txBlock.GetBlockHash().hex();
  paramsRequest[1u] = false;

  Json::Value response;
  lookupServer.GetEthBlockByHashI(paramsRequest, response);

  BOOST_CHECK_EQUAL(response["hash"].asString(), txBlock.GetBlockHash().hex());
  BOOST_CHECK_EQUAL(response["number"].asString(),
                    std::to_string(txBlock.GetHeader().GetBlockNum()));

  std::vector<std::string> expectedHashes;
  for (uint32_t i = 0; i < transactions.size(); ++i) {
    expectedHashes.emplace_back(
        transactions[i].GetTransaction().GetTranID().hex());
  }
  std::sort(expectedHashes.begin(), expectedHashes.end());

  std::vector<std::string> receivedHashes;
  const Json::Value arrayOfHashes = response["transactions"];
  for (auto jsonIter = arrayOfHashes.begin(); jsonIter != arrayOfHashes.end();
       ++jsonIter) {
    receivedHashes.emplace_back(jsonIter->asString());
  }
  std::sort(receivedHashes.begin(), receivedHashes.end());
  BOOST_CHECK_EQUAL_COLLECTIONS(expectedHashes.cbegin(), expectedHashes.cend(),
                                receivedHashes.cbegin(), receivedHashes.cend());
}

BOOST_AUTO_TEST_CASE(test_eth_get_gas_price) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetEthGasPriceI(paramsRequest, response);

  if (response.asString()[0] != '0') {
    BOOST_FAIL("Failed to get gas price");
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_transaction_count) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};
  Account account;
  if (!AccountStore::GetInstance().IsAccountExist(accountAddress)) {
    AccountStore::GetInstance().AddAccount(accountAddress, account);
  }

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = "0xa744160c3De133495aB9F9D77EA54b325b045670";

  lookupServer.GetEthTransactionCountI(paramsRequest, response);

  // 0x response
  if (response.asString()[0] != '0') {
    BOOST_FAIL("Failed to get TX count");
  }
}

/*
BOOST_AUTO_TEST_CASE(test_eth_send_raw_transaction) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] =
"f86e80850d9e63a68c82520894673e5ef1ae0a2ef7d0714a96a734ffcd1d8a381f881bc16d674ec8000080820cefa04728e87b280814295371adf0b7ccc3ec802a45bd31d13668b5ab51754c110f8ea02d0450641390c9ed56fcbbc64dcb5b07f7aece78739ef647f10cc93d4ecaa496";

  lookupServer.GetEthSendRawTransactionI(paramsRequest, response);

  const Json::Value expectedResponse = Json::arrayValue;
  BOOST_CHECK_EQUAL(response, expectedResponse);
}
*/

BOOST_AUTO_TEST_CASE(test_eth_blockNumber) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetEthBlockNumberI(paramsRequest, response);

  if (!(response.asString()[0] == '0' && response.asString()[1] == 'x')) {
    BOOST_FAIL("Failed to get block number!");
  }
}

BOOST_AUTO_TEST_CASE(test_eth_estimate_gas) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetEthEstimateGasI(paramsRequest, response);

  if (response.asString()[0] != '0') {
    BOOST_FAIL("Failed to get gas price");
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_transaction_by_hash) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = Schnorr::GenKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  BOOST_TEST_CHECK(true == true);
}

BOOST_AUTO_TEST_SUITE_END()
