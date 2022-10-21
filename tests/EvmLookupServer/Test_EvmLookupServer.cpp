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

#include "libData/BlockData/Block/DSBlock.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#define BOOST_TEST_MODULE EvmLookupServer
#define BOOST_TEST_DYN_LINK

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/range.hpp>
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

  virtual bool OpenServer(uint32_t /*version*/) override { return true; }

  bool CallRunner(uint32_t /*version*/,                 //
                  const Json::Value& request,           //
                  evmproj::CallResponse& /*response*/,  //
                  uint32_t /*counter = MAXRETRYCONN*/) override {
    LOG_GENERAL(DEBUG, "CallRunner json request:" << request);
    return true;
  };
};

static PairOfKey getTestKeyPair() { return Schnorr::GenKeyPair(); }

struct LookupServerBundle {
  std::unique_ptr<AbstractServerConnectorMock> abstractServerConnector;
  std::unique_ptr<Mediator> mediator;
  std::unique_ptr<LookupServer> lookupServer;
};

LookupServerBundle getLookupServer(
    const std::function<std::shared_ptr<EvmClient>()>& _allocator = []() {
      return std::make_shared<EvmClientMock>();
    }) {
  EvmClient::GetInstance(_allocator, true);

  const PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;

  auto mediator = std::make_unique<Mediator>(pairOfKey, peer);
  // We need some blocks, even if dummy
  mediator->m_txBlockChain.AddBlock(TxBlock{});
  mediator->m_dsBlockChain.AddBlock(DSBlock{});
  auto abstractServerConnector =
      std::make_unique<AbstractServerConnectorMock>();
  auto lookupServer =
      std::make_unique<LookupServer>(*mediator, *abstractServerConnector);
  return LookupServerBundle{
      std::move(abstractServerConnector),
      std::move(mediator),
      std::move(lookupServer),
  };
}

// Convenience fn only used to test Eth TXs.
TransactionWithReceipt constructTxWithReceipt(uint64_t nonce,
                                              const PairOfKey& keyPair,
                                              uint64_t epochNum = 1337) {
  Address toAddr{Account::GetAddressFromPublicKeyEth(keyPair.second)};

  // Stored TX receipt needs at least the epoch number
  auto txReceipt = TransactionReceipt{};
  txReceipt.SetEpochNum(epochNum);
  txReceipt.update();

  return TransactionWithReceipt(
      // Ctor: (version, nonce, toAddr, keyPair, amount, gasPrice, gasLimit,
      // code, data)
      Transaction{2,          // For EVM transaction.
                  nonce + 1,  // Zil style TXs are always one nonce ahead
                  toAddr,
                  keyPair,
                  1,
                  1,
                  2,
                  {},
                  {}},
      txReceipt);
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
  zbytes microBlockSerialized;
  microBlock.Serialize(microBlockSerialized, 0);
  BlockStorage::GetBlockStorage().PutMicroBlock(
      microBlock.GetBlockHash(), blockNum, blockNum, microBlockSerialized);
  const TxBlock txBlock =
      constructTxBlockWithTransactions(blockNum, microBlock, keyPair);
  mediator.m_txBlockChain.AddBlock(txBlock);
  return txBlock;
}

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

// Ignored in GCC, which also doesn't support this warning.
#pragma clang diagnostic ignored "-Wunused-private-field"

/**
 * @brief EvmClient mock implementation te be able to inject test responses
 * from the Evm-ds server.
 */
class GetEthCallEvmClientMock : public EvmClient {
 public:
  virtual bool OpenServer(uint32_t /*force = false*/) override { return true; };

  GetEthCallEvmClientMock(
      const uint gasLimit,  //
      const uint amount,    //
      const std::string& response, const std::string& address,
      const std::chrono::seconds& defaultWaitTime = std::chrono::seconds(0))
      : m_GasLimit(gasLimit),               // gas limit
        m_Amount(amount),                   // expected amount
        m_ExpectedResponse(response),       // expected response
        m_AccountAddress(address),          // expected response
        m_DefaultWaitTime(defaultWaitTime)  // default waittime
        {
            //
        };

  bool CallRunner(uint32_t /*version*/,             //
                  const Json::Value& request,       //
                  evmproj::CallResponse& response,  //
                  uint32_t /*counter = MAXRETRYCONN*/) override {
    //
    LOG_GENERAL(DEBUG, "CallRunner json request:" << request);

    Json::Reader _reader;
    Json::Value responseJson;

    BOOST_CHECK(_reader.parse(m_ExpectedResponse, responseJson));
    LOG_GENERAL(DEBUG, "CallRunner json response:" << responseJson);
    evmproj::GetReturn(responseJson, response);
    std::this_thread::sleep_for(m_DefaultWaitTime);
    return true;
  };

 private:
  const uint m_GasLimit{};
  const uint m_Amount{};
  const std::string m_ExpectedResponse{};
  const std::string m_AccountAddress{};
  const std::chrono::seconds m_DefaultWaitTime{0};
};

BOOST_AUTO_TEST_CASE(test_eth_call_failure) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
  const std::string evmResponseString =
      "{\"apply\":[],"
      "\"exit_reason\":{\"Fatal\":\"Returned\"},"
      "\"logs\":[],"
      "\"remaining_gas\":77371,"
      "\"return_value\":\"\""
      "}";

  const std::string address{"b744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,
            address);  // this max value
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const auto startBalance =
      AccountStore::GetInstance().GetBalance(accountAddress);
  AccountStore::GetInstance().DecreaseBalance(accountAddress, startBalance);
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  try {
    Json::Value response;
    lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);
    BOOST_FAIL("Expect exception, but did not catch");
  } catch (const jsonrpc::JsonRpcException& e) {
    BOOST_CHECK_EQUAL(e.GetCode(), ServerBase::RPC_MISC_ERROR);
    BOOST_CHECK_EQUAL(e.GetMessage(), "Returned");
  }

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_eth_call_failure_return_with_object) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
  const std::string evmResponseString =
      "{\"apply\":[],"
      "\"exit_reason\":{\"Fatal\":{\"Error\":\"fatal error, unkown object "
      "type\"}},"
      "\"logs\":[],"
      "\"remaining_gas\":77371,"
      "\"return_value\":\"\""
      "}";

  const std::string address{"b744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,
            address);  // this max value
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const auto startBalance =
      AccountStore::GetInstance().GetBalance(accountAddress);
  AccountStore::GetInstance().DecreaseBalance(accountAddress, startBalance);
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  try {
    Json::Value response;
    lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);
    BOOST_FAIL("Expect exception, but did not catch");
  } catch (const jsonrpc::JsonRpcException& e) {
    BOOST_CHECK_EQUAL(e.GetCode(), ServerBase::RPC_MISC_ERROR);

    Json::Value expectedExitReason;
    expectedExitReason["Error"] = "fatal error, unkown object type";

    Json::Reader reader;
    Json::Value result;

    BOOST_REQUIRE(reader.parse(e.GetMessage(), result));
    BOOST_CHECK_EQUAL(result, expectedExitReason);
  }

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_eth_call_revert) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
  const std::string evmResponseString =
      "{\"apply\":[],"
      "\"exit_reason\":{\"Revert\":\"Reverted\"},"
      "\"logs\":[],"
      "\"remaining_gas\":77371,"
      "\"return_value\":\"\""
      "}";

  const std::string address{"b744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,
            address);  // this max value
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const auto startBalance =
      AccountStore::GetInstance().GetBalance(accountAddress);
  AccountStore::GetInstance().DecreaseBalance(accountAddress, startBalance);
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  try {
    Json::Value response;
    lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);
    BOOST_FAIL("Expect exception, but did not catch");
  } catch (const jsonrpc::JsonRpcException& e) {
    BOOST_CHECK_EQUAL(e.GetCode(), ServerBase::RPC_MISC_ERROR);
    LOG_GENERAL(DEBUG, e.GetMessage());
    BOOST_CHECK_EQUAL(e.GetMessage(), "Reverted");
  }

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_eth_call_exit_reason_unknown) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
  const std::string evmResponseString =
      "{\"apply\":[],"
      "\"exit_reason\":{\"Unknown\":\"???\"},"
      "\"logs\":[],"
      "\"remaining_gas\":77371,"
      "\"return_value\":\"\""
      "}";

  const std::string address{"b744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,
            address);  // this max value
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const auto startBalance =
      AccountStore::GetInstance().GetBalance(accountAddress);
  AccountStore::GetInstance().DecreaseBalance(accountAddress, startBalance);
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  try {
    Json::Value response;
    lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);
    BOOST_FAIL("Expect exception, but did not catch");
  } catch (const jsonrpc::JsonRpcException& e) {
    BOOST_CHECK_EQUAL(e.GetCode(), ServerBase::RPC_MISC_ERROR);
    BOOST_CHECK_EQUAL(e.GetMessage(), "Unable to process");
  }

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_eth_call_timeout, *boost::unit_test::disabled()) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
  const std::string evmResponseString =
      "{\"apply\":[],"
      "\"exit_reason\":{\"Fatal\":\"Returned\"},"
      "\"logs\":[],"
      "\"remaining_gas\":77371,"
      "\"return_value\":\"\""
      "}";

  const std::string address{"b744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,    //
            address,                      //
            std::chrono::seconds(33));
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const auto startBalance =
      AccountStore::GetInstance().GetBalance(accountAddress);
  AccountStore::GetInstance().DecreaseBalance(accountAddress, startBalance);
  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  try {
    lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);
    BOOST_FAIL("Expect exception, but did not catch");
  } catch (...) {
    // success
  }

  const auto balance = AccountStore::GetInstance().GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be unchanged
  BOOST_CHECK_EQUAL(static_cast<uint64_t>(balance), initialBalance);
}

BOOST_AUTO_TEST_CASE(test_eth_call_success) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  const auto gasLimit{2 * DS_MICROBLOCK_GAS_LIMIT};
  const auto amount{4200U};
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

  const std::string address{"a744160c3de133495ab9f9d77ea54b325b045670"};
  const auto lookupServer =
      getLookupServer([amount, evmResponseString, address]() {  //
        return std::make_shared<GetEthCallEvmClientMock>(
            2 * DS_MICROBLOCK_GAS_LIMIT,  // gas limit will not exceed
            amount, evmResponseString,
            address);  // this max value
      });

  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  Json::Value values;
  values["data"] =
      "ffa1caa0000000000000000000000000000000000000000000000000000000000000"
      "014";
  values["to"] = address;
  values["gas"] = gasLimit;
  values["value"] = amount;
  paramsRequest[0u] = values;
  paramsRequest[1u] = Json::Value("latest");

  Address accountAddress{address};
  Account account;

  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  lookupServer.lookupServer->GetEthCallEthI(paramsRequest, response);

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
  lookupServer.lookupServer->GetWeb3ClientVersionI(paramsRequest, response);

  LOG_GENERAL(DEBUG, "GetWeb3ClientVersion response:" << response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "Zilliqa/v8.2");
}

BOOST_AUTO_TEST_CASE(test_web3_sha3) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  paramsRequest[0u] = "0x68656c6c6f20776f726c64";
  lookupServer.lookupServer->GetWeb3Sha3I(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(
      response.asString(),
      "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad");

  // test with empty string
  paramsRequest[0u] = "";
  lookupServer.lookupServer->GetWeb3Sha3I(paramsRequest, response);

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
  lookupServer.lookupServer->GetEthMiningI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "false");
}

BOOST_AUTO_TEST_CASE(test_eth_coinbase) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();

  Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};

  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer.lookupServer->GetEthCoinbaseI(paramsRequest, response);

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
  lookupServer.lookupServer->GetNetVersionI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "0x8001");
}

BOOST_AUTO_TEST_CASE(test_net_listening) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);
  lookupServer.lookupServer->GetNetListeningI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "true");
}

BOOST_AUTO_TEST_CASE(test_net_peer_count) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.lookupServer->GetNetPeerCountI(paramsRequest, response);

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

  lookupServer.lookupServer->GetProtocolVersionI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthChainIdI(paramsRequest, response);

  LOG_GENERAL(DEBUG, response.asString());

  BOOST_CHECK_EQUAL(response.asString(), "0x8001");
}

BOOST_AUTO_TEST_CASE(test_eth_syncing) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto lookupServer = getLookupServer();
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.lookupServer->GetEthSyncingI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthAccountsI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthUncleBlockI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthUncleBlockI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthUncleCountI(paramsRequest, response);

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

  lookupServer.lookupServer->GetEthUncleCountI(paramsRequest, response);

  const Json::Value expectedResponse = "0x0";
  BOOST_CHECK_EQUAL(response, expectedResponse);
}

BOOST_AUTO_TEST_CASE(test_eth_net_version) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);
  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  lookupServer.GetNetVersionI(paramsRequest, response);
  BOOST_CHECK_EQUAL(response, Json::Value("0x8001"));
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
  lookupServer.lookupServer->GetEthBalanceI(paramsRequest, response);
  LOG_GENERAL(INFO, "Got balance: " << response);
  // expected return value should be 1.000.000 times greater
  BOOST_CHECK_EQUAL(boost::algorithm::to_lower_copy(response.asString()),
                    "0xe8d4a51000");
}

BOOST_AUTO_TEST_CASE(test_eth_get_block_by_number) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

    zbytes body;
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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  Address accountAddress{"b744160c3de133495ab9f9d77ea54b325b045670"};
  Account account;
  AccountStore::GetInstance().AddAccount(accountAddress, account);

  const uint128_t initialBalance{1'000'000};
  AccountStore::GetInstance().IncreaseBalance(accountAddress, initialBalance);

  Json::Value response;
  // call the method on the lookup server with params
  Json::Value paramsRequest = Json::Value(Json::arrayValue);

  Json::Value values;
  values["from"] = accountAddress.hex();
  paramsRequest[0u] = values;

  lookupServer.GetEthEstimateGasI(paramsRequest, response);

  if (response.asString()[0] != '0') {
    BOOST_FAIL("Failed to get gas price");
  } else {
    std::cerr << "Received gas: " << response.asString() << std::endl;
  }
}

BOOST_AUTO_TEST_CASE(test_eth_get_transaction_by_hash) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  // Construct all relevant structures (sample transactions, microblock and
  // txBlock)
  std::vector<TransactionWithReceipt> transactions;

  constexpr uint64_t EPOCH_NUM = 1;
  constexpr uint32_t TRANSACTIONS_COUNT = 2;

  for (uint32_t i = 0; i < TRANSACTIONS_COUNT; ++i) {
    transactions.emplace_back(constructTxWithReceipt(i, pairOfKey, EPOCH_NUM));
  }

  for (const auto& transaction : transactions) {
    zbytes body;
    transaction.Serialize(body, 0);
    BlockStorage::GetBlockStorage().PutTxBody(
        EPOCH_NUM, transaction.GetTransaction().GetTranID(), body);
  }

  // Need to create a block with our TXs in since that is referenced in the
  // transaction receipt (tx index)
  buildCommonEthBlockCase(mediator, EPOCH_NUM, transactions, pairOfKey);

  for (uint32_t i = 0; i < transactions.size(); ++i) {
    // call the method on the lookup server with params
    Json::Value paramsRequest = Json::Value(Json::arrayValue);
    paramsRequest[0u] = transactions[i].GetTransaction().GetTranID().hex();

    Json::Value response;

    lookupServer.GetEthTransactionByHashI(paramsRequest, response);

    BOOST_TEST_CHECK(response["hash"] ==
                     "0x" + transactions[i].GetTransaction().GetTranID().hex());
    // The internal nonce representation is always one ahead for Eth TXs than
    // was originally sent due to accounting differences with Zil
    BOOST_TEST_CHECK(response["nonce"] ==
                     (boost::format("0x%x") %
                      (transactions[i].GetTransaction().GetNonce() - 1))
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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

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
      zbytes body;
      transaction.Serialize(body, 0);
      BlockStorage::GetBlockStorage().PutTxBody(
          1, transaction.GetTransaction().GetTranID(), body);
    }
    const auto blockNum = i + 1;
    const MicroBlock microBlock = constructMicroBlockWithTransactions(
        blockNum, thisBlockTransactions, pairOfKey);
    zbytes microBlockSerialized;
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

BOOST_AUTO_TEST_CASE(test_ethGasPrice) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  const uint256_t GAS_PRICE_CORE = 420;
  const DSBlockHeader dsHeader{
      1,  1,  {}, 1,  1, GAS_PRICE_CORE.convert_to<uint32_t>(),
      {}, {}, {}, {}, {}};
  const DSBlock dsBlock{dsHeader, {}};
  mediator.m_dsBlockChain.AddBlock(dsBlock);

  Json::Value response;
  // call the method on the lookup server with params

  lookupServer.GetEthGasPriceI({}, response);

  const auto EXPECTED_NUM = ((GAS_PRICE_CORE * EVM_ZIL_SCALING_FACTOR) /
                             GasConv::GetScalingFactor()) +
                            1000000;

  auto EXPECTED_RESPONSE = (boost::format("0x%x") % (EXPECTED_NUM)).str();
  auto responseStr = response.asString();
  boost::to_lower(responseStr);
  boost::to_lower(EXPECTED_RESPONSE);
  BOOST_TEST_CHECK(responseStr == EXPECTED_RESPONSE);
}

BOOST_AUTO_TEST_CASE(test_ethGasPriceRounding) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockStorage::GetBlockStorage().ResetAll();

  EvmClient::GetInstance([]() { return std::make_shared<EvmClientMock>(); },
                         true);

  PairOfKey pairOfKey = getTestKeyPair();
  Peer peer;
  Mediator mediator(pairOfKey, peer);
  AbstractServerConnectorMock abstractServerConnector;

  LookupServer lookupServer(mediator, abstractServerConnector);

  const uint256_t BLOCK_GAS_PRICES[] = {2000000000, 2121121121, 2123456789,
                                        3987654321, 9999999999, 11111111111,
                                        9876543210};

  for (uint32_t i = 0; i < boost::size(BLOCK_GAS_PRICES); ++i) {
    const uint256_t GAS_PRICE_CORE = BLOCK_GAS_PRICES[i];
    const DSBlockHeader dsHeader{
        1,  1,  {}, i + 1, 1, GAS_PRICE_CORE.convert_to<uint64_t>(),
        {}, {}, {}, {},    {}};
    const DSBlock dsBlock{dsHeader, {}};
    mediator.m_dsBlockChain.AddBlock(dsBlock);

    Json::Value response;
    // call the method on the lookup server with params

    lookupServer.GetEthGasPriceI({}, response);

    const auto responseStr = response.asString();
    uint128_t apiGasPrice = uint128_t{responseStr};
    Transaction tx{2,  1, {}, pairOfKey, 1, apiGasPrice, /* gasLimit = */ 100,
                   {}, {}};

    BOOST_TEST_CHECK(tx.GetGasPriceQa() >= GAS_PRICE_CORE);
  }
}

BOOST_AUTO_TEST_CASE(test_bloomFilters) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // Various test cases captured from etherscan
  {
    Json::Value input;
    Json::Value logObject;
    logObject["address"] = "0xf4dd946d1406e215a87029db56c69e1bcf3e1773";
    logObject["data"] =
        "0x00000000000000000000000000000000000000000000000000000000000000010000"
        "000000000000000000000000000000000000000000000000000000000001";
    Json::Value topics;
    topics.append(
        "0xc3d58168c5ae7397731d063d5bbf3d657854427343f4c083240f7aacaa2d0f62");
    topics.append(
        "0x0000000000000000000000009d1f9d4d70a35d18797e2495a8f73b9c8a08e399");
    topics.append(
        "0x0000000000000000000000000000000000000000000000000000000000000000");
    topics.append(
        "0x0000000000000000000000009d1f9d4d70a35d18797e2495a8f73b9c8a08e399");
    logObject["topics"] = topics;

    input.append(logObject);

    const auto EXPECTED_RESPONSE =
        "0000000000000001000000080000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000002000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000200000000"
        "0000000000080000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000001000000000000000000000000000000000000"
        "0000000010000000000000000000000000000000000000000000000000000000000000"
        "0000000800000000000000002000000000000000000000000100000000000000000000"
        "0040000000080000000000";
    const auto bloom = Eth::BuildBloomForLogs(input);
    BOOST_TEST_CHECK(bloom.hex() == EXPECTED_RESPONSE);
  }
  {
    Json::Value input;
    Json::Value logObject;
    logObject["address"] = "0x00000000006c3852cbef3e08e8df289169ede581";
    logObject["data"] =
        "0x8781ba8c3f4a66f4a5e9eb2686ae4c0fc8d1d10c5441e8d45f4f76ffa91d416f";
    Json::Value topics;
    topics.append(
        "0x6bacc01dbe442496068f7d234edd811f1a5f833243e0aec824f86ab861f3c90d");
    topics.append(
        "0x00000000000000000000000041f59b30673a14a263e195af07c804c47cfb3bb0");
    topics.append(
        "0x000000000000000000000000004c00500000ad104d7dbd00e3ae0a5c00560c00");
    logObject["topics"] = topics;

    input.append(logObject);

    const auto EXPECTED_RESPONSE =
        "0000000000000000000000000000002000000000000000000080000000000000000000"
        "0000000000800000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000080000000000000000000000000000"
        "0000000000000000000000000000000000000000000000020000000002000000000000"
        "0000000000000000000000000000000000000000800000010000000000000000000000"
        "0100000000000000020000000000000000000000000000000000000000000000000000"
        "0040000000000000080000000000000000000000000000000000000000000000000000"
        "0000000000000000000000";
    const auto bloom = Eth::BuildBloomForLogs(input);
    BOOST_TEST_CHECK(bloom.hex() == EXPECTED_RESPONSE);
  }
  {
    Json::Value input;
    Json::Value logObject;
    logObject["address"] = "0x9cf8424389e922d09d252714d61108b1378aaf0b";
    logObject["data"] =
        "0x00000000000000000000000000000000000000000000002567ac70392b880000";
    Json::Value topics;
    topics.append(
        "0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef");
    topics.append(
        "0x0000000000000000000000008412b10a972205cce9095620e4d779a5c650c74f");
    topics.append(
        "0x000000000000000000000000fa4b4636bf8fa24a1e39762864e098616b0016d7");
    logObject["topics"] = topics;

    input.append(logObject);

    const auto EXPECTED_RESPONSE =
        "0000000000000000000000000000000000000000400000000000000000000000000000"
        "0000000000000000000000000000000000000800000000000000000000000000000000"
        "0000000000080000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000001000000000000000000000000000002000"
        "0000000000000000000000000000000000000000000000000000000000000000001000"
        "0000000000000000000000000000000000000020020000000010000000000002000000"
        "0002000000000000000000000000000000000000800000000000000000000000000000"
        "0000000000000000000000";
    const auto bloom = Eth::BuildBloomForLogs(input);
    BOOST_TEST_CHECK(bloom.hex() == EXPECTED_RESPONSE);
  }
}

BOOST_AUTO_TEST_SUITE_END()
