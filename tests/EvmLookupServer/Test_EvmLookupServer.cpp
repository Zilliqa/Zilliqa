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

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/format.hpp>
#include <boost/range/numeric.hpp>
#include <boost/test/unit_test.hpp>
#include "libData/AccountData/EvmClient.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libMediator/Mediator.h"
#include "libServer/LookupServer.h"
#include "libUtils/EvmJsonResponse.h"

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

static PairOfKey getTestKeyPair() { return Schnorr::GenKeyPair(); }

std::unique_ptr<LookupServer> getLookupServer() {
  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  const PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  auto lookupServer =
      std::make_unique<LookupServer>(mediator, abstractServerConnector);
  return lookupServer;
}

TransactionWithReceipt constructTxWithReceipt(uint64_t nonce,
                                              const PairOfKey& keyPair) {
  Address toAddr{Account::GetAddressFromPublicKeyEth(keyPair.second)};
  return TransactionWithReceipt(
      // Ctor: (version, nonce, toAddr, keyPair, amount, gasPrice, gasLimit,
      // code, data)
      Transaction{2,  // For EVM transaction.
                  nonce,
                  toAddr,
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
  values["to"] = "a744160c3De133495aB9F9D77EA54b325b045670";
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};
  Account account;
  if (!AccountStore::GetInstance().IsAccountExist(accountAddress)) {
    AccountStore::GetInstance().AddAccount(accountAddress, account);
  }
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  lookupServer->GetEthCallEthI(paramsRequest, response);

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
  paramsRequest[0u] = "0x68656c6c6f20776f726c64";
  lookupServer->GetWeb3Sha3I(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(
      response.asString(),
      "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad");

  // test with empty string
  paramsRequest[0u] = "";
  lookupServer->GetWeb3Sha3I(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(
      response.asString(),
      "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
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

  BOOST_CHECK_EQUAL(response.asString(),
                    "0x0000000000000000000000000000000000000000");
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

  BOOST_CHECK_EQUAL(response.asString(), "0x8000");
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

  BOOST_CHECK_EQUAL(response.asString(), "0x41");
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

  const Json::Value expectedResponse = "0x0";
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

  const Json::Value expectedResponse = "0x0";
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_net_version) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetNetVersionI(paramsRequest, response);
  BOOST_CHECK_EQUAL(response, Json::Value("0x8000"));
}

BOOST_AUTO_TEST_CASE(test_eth_get_balance) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  Json::Value response;

  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  const std::string address{"0x6cCAa29b6cD36C8238E8Fa137311de6153b0b4e7"};
  paramsRequest[0u] = address;
  paramsRequest[1u] = "latest";

  const Address accountAddress{address};
  if (!AccountStore::GetInstance().IsAccountExist(accountAddress)) {
    Account account;
    AccountStore::GetInstance().AddAccount(accountAddress, account);
  }

  const uint128_t initialBalance{1'000'000U};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  LOG_GENERAL(INFO, "Account balance: " << AccountStore::GetInstance()
                                               .GetAccount(accountAddress)
                                               ->GetBalance());

  const auto lookupServer = getLookupServer();
  lookupServer->GetEthBalanceI(paramsRequest, response);
  LOG_GENERAL(INFO, "Got balance: " << response);
  // expected return value should be 1.000.000 times greater
  BOOST_CHECK_EQUAL(boost::algorithm::to_lower_copy(response.asString()),
                    "0xe8d4a51000");
}

BOOST_AUTO_TEST_CASE(test_eth_get_block_by_number) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  std::vector<TransactionWithReceipt> transactions;

  constexpr uint32_t TRANSACTIONS_COUNT = 2;
  for (uint32_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    TransactionWithReceipt twr = constructTxWithReceipt(i, pairOfKey);
    transactions.emplace_back(twr);

    bytes body;
    transactions.back().Serialize(body, 0);
    BlockStorage::GetBlockStorage().PutTxBody(
        1, transactions.back().GetTransaction().GetTranID(), body);
  }

  constexpr auto FIRST_VALID_BLOCK_NUM = 1;
  const auto firstValidTxBlock = buildCommonEthBlockCase(
      mediator, FIRST_VALID_BLOCK_NUM, transactions, pairOfKey);

  // Case with retrieving block by number
  {
    // Construct all relevant structures (sample transactions, microblock and
    // txBlock)

    // call the method on the lookup server with params
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = std::to_string(FIRST_VALID_BLOCK_NUM);
    paramsRequest[1u] = false;

    Json::Value response;
    lookupServer.GetEthBlockByNumberI(paramsRequest, response);

    BOOST_CHECK_EQUAL(
        response["hash"].asString(),
        (boost::format("0x%x") % firstValidTxBlock.GetBlockHash().hex()).str());

    std::vector<std::string> expectedHashes;
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      expectedHashes.emplace_back(
          "0x" + transactions[i].GetTransaction().GetTranID().hex());
    }
    std::sort(expectedHashes.begin(), expectedHashes.end());

    std::vector<std::string> receivedHashes;
    const Json::Value arrayOfHashes = response["transactions"];
    for (auto jsonIter = arrayOfHashes.begin(); jsonIter != arrayOfHashes.end();
         ++jsonIter) {
      receivedHashes.emplace_back(jsonIter->asString());
    }
    std::sort(receivedHashes.begin(), receivedHashes.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(
        expectedHashes.cbegin(), expectedHashes.cend(), receivedHashes.cbegin(),
        receivedHashes.cend());
  }

  // Case with retrieving block by number (with includeTransaction set to True)
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = std::to_string(FIRST_VALID_BLOCK_NUM);
    paramsRequest[1u] = true;

    Json::Value response;
    lookupServer.GetEthBlockByNumberI(paramsRequest, response);

    BOOST_CHECK_EQUAL(
        response["hash"].asString(),
        (boost::format("0x%x") % firstValidTxBlock.GetBlockHash().hex()).str());

    std::vector<std::string> expectedHashes;
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      expectedHashes.emplace_back(
          "0x" + transactions[i].GetTransaction().GetTranID().hex());
    }
    std::sort(expectedHashes.begin(), expectedHashes.end());

    std::vector<std::string> receivedHashes;
    const Json::Value arrayOfTransactions = response["transactions"];
    for (auto jsonIter = arrayOfTransactions.begin();
         jsonIter != arrayOfTransactions.end(); ++jsonIter) {
      BOOST_TEST_CHECK(jsonIter->isObject() == true);
      const auto tranJsonObject = *jsonIter;
      receivedHashes.emplace_back(tranJsonObject["hash"].asString());
    }
    std::sort(receivedHashes.begin(), receivedHashes.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(
        expectedHashes.cbegin(), expectedHashes.cend(), receivedHashes.cbegin(),
        receivedHashes.cend());
  }

  // Case with retrieving block by TAGs (previous block already exists)
  {
    std::vector<TransactionWithReceipt> new_transactions;

    constexpr uint32_t NEW_TRANSACTIONS_COUNT = 123;
    for (uint32_t i = 0; i < NEW_TRANSACTIONS_COUNT; ++i) {
      new_transactions.emplace_back(constructTxWithReceipt(i, pairOfKey));
    }

    constexpr auto SECOND_VALID_BLOCK_NUM = 2;
    const auto secondValidTxBlock = buildCommonEthBlockCase(
        mediator, SECOND_VALID_BLOCK_NUM, new_transactions, pairOfKey);

    Json::Value paramsRequest = Json::Value(Json::arrayValue);

    // Latest
    paramsRequest[0u] = "latest";
    Json::Value response;

    lookupServer.GetEthBlockByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(
        response["hash"].asString(),
        (boost::format("0x%x") % secondValidTxBlock.GetBlockHash().hex())
            .str());

    // Pending
    paramsRequest[0u] = "pending";

    lookupServer.GetEthBlockByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(response, Json::nullValue);

    // Earliest
    paramsRequest[0u] = "earliest";

    lookupServer.GetEthBlockByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(response, Json::nullValue);
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_block_by_hash) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = getTestKeyPair();
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

  BOOST_CHECK_EQUAL(
      response["hash"].asString(),
      (boost::format("0x%x") % txBlock.GetBlockHash().hex()).str());
  BOOST_CHECK_EQUAL(
      response["number"].asString(),
      (boost::format("0x%x") % txBlock.GetHeader().GetBlockNum()).str());

  std::vector<std::string> expectedHashes;
  for (uint32_t i = 0; i < transactions.size(); ++i) {
    expectedHashes.emplace_back(
        "0x" + transactions[i].GetTransaction().GetTranID().hex());
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  PairOfKey pairOfKey = getTestKeyPair();
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

  constexpr uint64_t EPOCH_NUM = 1;
  for (const auto& transaction : transactions) {
    bytes body;
    transaction.Serialize(body, 0);
    BlockStorage::GetBlockStorage().PutTxBody(
        EPOCH_NUM, transaction.GetTransaction().GetTranID(), body);
  }

  for (uint32_t i = 0; i < transactions.size(); ++i) {
    // call the method on the lookup server with params
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = transactions[i].GetTransaction().GetTranID().hex();

    Json::Value response;

    lookupServer.GetEthTransactionByHashI(paramsRequest, response);

    BOOST_TEST_CHECK(response["hash"] ==
                     "0x" + transactions[i].GetTransaction().GetTranID().hex());
    BOOST_TEST_CHECK(
        response["nonce"] ==
        (boost::format("0x%x") % transactions[i].GetTransaction().GetNonce())
            .str());
    BOOST_TEST_CHECK(response["value"] ==
                     (boost::format("0x%x") %
                      transactions[i].GetTransaction().GetAmountWei())
                         .str());
  }

  // Get non-existing transaction
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = "abcdeffedcba";

  Json::Value response;

  lookupServer.GetEthTransactionByHashI(paramsRequest, response);
  BOOST_TEST_CHECK(response == Json::nullValue);
}

BOOST_AUTO_TEST_CASE(test_eth_get_transaction_count_by_hash_or_num) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  // Construct all relevant structures (sample transactions, microblock and
  // txBlock)
  std::vector<TransactionWithReceipt> transactions;

  constexpr uint32_t TRANSACTIONS_COUNT = 31;
  for (uint32_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    transactions.emplace_back(constructTxWithReceipt(i, pairOfKey));
  }

  constexpr auto BLOCK_NUM = 1;
  const auto txBlock =
      buildCommonEthBlockCase(mediator, BLOCK_NUM, transactions, pairOfKey);

  // Existing block by Hash
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = txBlock.GetBlockHash().hex();

    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByHashI(paramsRequest, response);
    BOOST_TEST_CHECK(response.asString() ==
                     (boost::format("0x%x") % TRANSACTIONS_COUNT).str());
  }

  // Existing block by Hash (with extra '0x' prefix)
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = "0x" + txBlock.GetBlockHash().hex();

    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByHashI(paramsRequest, response);
    BOOST_TEST_CHECK(response.asString() ==
                     (boost::format("0x%x") % TRANSACTIONS_COUNT).str());
  }

  // Non existing block by Hash
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = "abcdeffedcba01234567890";

    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByHashI(paramsRequest, response);
    BOOST_TEST_CHECK(response.asString() == "0x0");
  }

  // Existing block by number
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = std::to_string(txBlock.GetHeader().GetBlockNum());

    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByNumberI(paramsRequest, response);
    BOOST_TEST_CHECK(response.asString() ==
                     (boost::format("0x%x") % TRANSACTIONS_COUNT).str());
  }

  // Non Existing block by number
  {
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = "1234";

    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByNumberI(paramsRequest, response);
    BOOST_TEST_CHECK(response.asString() == "0x0");
  }
  // Block by TAGs
  {
    std::vector<TransactionWithReceipt> new_transactions;

    constexpr uint32_t NEW_TRANSACTIONS_COUNT = 2;
    for (uint32_t i = 0; i < NEW_TRANSACTIONS_COUNT; ++i) {
      new_transactions.emplace_back(constructTxWithReceipt(i, pairOfKey));
    }

    constexpr auto SECOND_VALID_BLOCK_NUM = 2;
    const auto secondValidTxBlock = buildCommonEthBlockCase(
        mediator, SECOND_VALID_BLOCK_NUM, new_transactions, pairOfKey);

    Json::Value paramsRequest = Json::Value(Json::arrayValue);

    // Latest
    paramsRequest[0u] = "latest";
    Json::Value response;

    lookupServer.GetEthBlockTransactionCountByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(response.asString(),
                      (boost::format("0x%x") % NEW_TRANSACTIONS_COUNT).str());

    // Pending
    paramsRequest[0u] = "pending";

    lookupServer.GetEthBlockTransactionCountByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(response.asString(), "0x0");

    // Earliest
    paramsRequest[0u] = "earliest";

    lookupServer.GetEthBlockTransactionCountByNumberI(paramsRequest, response);
    BOOST_CHECK_EQUAL(
        response.asString(),
        (boost::format("0x%x") %
         mediator.m_txBlockChain.GetBlock(0).GetHeader().GetNumTxs())
            .str());
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_transaction_by_block_and_index) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); });

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  constexpr std::array<uint32_t, 4> TRANSACTIONS_IN_BLOCKS = {3, 15, 22, 7};

  std::vector<TransactionWithReceipt> transactions;
  std::vector<MicroBlock> microBlocks;
  uint32_t nonce = 0;

  for (uint32_t i = 0; i < TRANSACTIONS_IN_BLOCKS.size(); ++i) {
    std::vector<TransactionWithReceipt> thisBlockTransactions;
    for (uint32_t txCount = 0; txCount < TRANSACTIONS_IN_BLOCKS[i]; ++txCount) {
      const auto transaction = constructTxWithReceipt(nonce++, pairOfKey);
      thisBlockTransactions.push_back(transaction);
      transactions.push_back(transaction);
      bytes body;
      transaction.Serialize(body, 0);
      BlockStorage::GetBlockStorage().PutTxBody(
          1, transaction.GetTransaction().GetTranID(), body);
    }
    const auto blockNum = i + 1;
    const MicroBlock microBlock = constructMicroBlockWithTransactions(
        blockNum, thisBlockTransactions, pairOfKey);
    bytes microBlockSerialized;
    microBlock.Serialize(microBlockSerialized, 0);
    BlockStorage::GetBlockStorage().PutMicroBlock(
        microBlock.GetBlockHash(), blockNum, blockNum, microBlockSerialized);
    microBlocks.push_back(microBlock);
  }

  // CTor: (gasLimit, gasUsed, rewards, blockNum, blockHashSet, numTxs,
  // minerPubKey, blockVersion)
  TxBlockHeader txblockheader(2, 1, 0, 1, {}, transactions.size(),
                              pairOfKey.first, TXBLOCK_VERSION);
  std::vector<MicroBlockInfo> mbInfos;
  for (const auto& microBlock : microBlocks) {
    mbInfos.emplace_back(MicroBlockInfo{microBlock.GetBlockHash(),
                                        microBlock.GetHeader().GetTxRootHash(),
                                        microBlock.GetHeader().GetShardId()});
  }

  TxBlock txBlock(txblockheader, {mbInfos}, CoSignatures{});
  mediator.m_txBlockChain.AddBlock(txBlock);

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value response;

  // Query for all existing transactions using block hash
  {
    for (uint32_t i = 0; i < transactions.size(); ++i) {
      paramsRequest[0u] = txBlock.GetBlockHash().hex();
      paramsRequest[1u] = std::to_string(i);

      lookupServer.GetEthTransactionByBlockHashAndIndexI(paramsRequest,
                                                         response);
      BOOST_TEST_CHECK(response["hash"].asString() ==
                       "0x" +
                           transactions[i].GetTransaction().GetTranID().hex());
    }
  }

  // Query non-existing transaction using block hash
  {
    uint32_t onePassRange = boost::accumulate(TRANSACTIONS_IN_BLOCKS, 0);
    paramsRequest[0u] = txBlock.GetBlockHash().hex();
    paramsRequest[1u] = std::to_string(onePassRange);

    Json::Value response;
    lookupServer.GetEthTransactionByBlockHashAndIndexI(paramsRequest, response);
    BOOST_TEST_CHECK(response == Json::nullValue);
  }

  // Query by valid block num and Tag = 'latest'
  {
    const std::vector<std::string> BLOCKS = {"1", "latest"};
    for (const auto& block : BLOCKS) {
      for (uint32_t i = 0; i < transactions.size(); ++i) {
        paramsRequest[0u] = block;
        paramsRequest[1u] = std::to_string(i);

        lookupServer.GetEthTransactionByBlockNumberAndIndexI(paramsRequest,
                                                             response);
        BOOST_TEST_CHECK(
            response["hash"].asString() ==
            "0x" + transactions[i].GetTransaction().GetTranID().hex());
      }
    }
  }

  // Query by non-existing block number or Tags: 'earliest' and 'pending'
  {
    const std::vector<std::string> BLOCKS = {"123", "earliest", "pending"};
    for (const auto& block : BLOCKS) {
      for (uint32_t i = 0; i < transactions.size(); ++i) {
        paramsRequest[0u] = block;
        paramsRequest[1u] = std::to_string(i);

        lookupServer.GetEthTransactionByBlockNumberAndIndexI(paramsRequest,
                                                             response);
        BOOST_TEST_CHECK(response == Json::nullValue);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
