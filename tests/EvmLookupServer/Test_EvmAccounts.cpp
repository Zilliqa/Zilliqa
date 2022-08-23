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

#define BOOST_TEST_MODULE EvmAccounts
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include "EvmClientMock.h"
#include "libData/AccountData/AccountStore.h"

class AccountStoreMock : public AccountStore {
 public:
  uint64_t InvokeEvmInterpreter(Account* contractAccount,
                                INVOKE_TYPE invoke_type,
                                EvmCallParameters& params,
                                const uint32_t& version,  //
                                bool& ret,                //
                                TransactionReceipt& receipt,
                                evmproj::CallResponse& evmReturnValues) {
    return AccountStore::InvokeEvmInterpreter(contractAccount, invoke_type,
                                              params, version, ret, receipt,
                                              evmReturnValues);
  }
};

class EvmAccountEvmClientMock : public EvmClientMock {
 public:
  explicit EvmAccountEvmClientMock(const std::string& balance,
                                   const std::string& nonce,
                                   const std::string& address)
      : m_Balance(balance),  //
        m_Nonce(nonce),
        m_Address(address)  //
  {}

  virtual bool CallRunner(uint32_t /*version*/,             //
                          const Json::Value& request,       //
                          evmproj::CallResponse& response,  //
                          uint32_t /*counter = MAXRETRYCONN*/) {
    LOG_GENERAL(DEBUG, "CallRunner json request:" << request);
    Json::Value responseJson;
    const std::string evmResponseString =
        "{\"apply\":"
        "["
        "{\"modify\":{"
        "\"address\":\"0x" +
        m_Address +
        "\","
        "\"balance\":\"" +
        m_Balance +
        "\","
        "\"code\":\"42\","
        "\"nonce\":\"" +
        m_Nonce +
        "\","
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
    Json::Reader _reader;

    BOOST_CHECK(_reader.parse(evmResponseString, responseJson));
    LOG_GENERAL(DEBUG, "CallRunner json response:" << responseJson);
    const auto reply = evmproj::GetReturn(responseJson, response);

    return reply.GetSuccess();
  };

 private:
  const std::string m_Balance{};
  const std::string m_Nonce{};
  const std::string m_Address{};
};

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_evm_account_balance_nonce_check) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  const auto expectedBalance{12'345U};
  const auto expectedNonce{4'389'567U};
  const std::string address = "a744160c3De133495aB9F9D77EA54b325b045670";
  EvmClient::GetInstance(
      [&expectedBalance, &expectedNonce, &address]() {
        return std::make_shared<EvmAccountEvmClientMock>(
            std::to_string(expectedBalance),  //
            std::to_string(expectedNonce), address);
      },
      true);

  const auto accountStoreMock{std::make_shared<AccountStoreMock>()};

  AccountStore::GetInstance([&accountStoreMock]() { return accountStoreMock; },
                            true);
  auto& accountStore = AccountStore::GetInstance();
  accountStore.Init();

  Address accountAddress{address};
  Account account;
  if (!accountStore.IsAccountExist(accountAddress)) {
    accountStore.AddAccount(accountAddress, account);
  }

  const uint128_t initialBalance{1'000'000U};
  accountStore.IncreaseBalance(accountAddress, initialBalance);
  BOOST_CHECK_EQUAL(accountStore.GetBalance(accountAddress), initialBalance);
  BOOST_CHECK_EQUAL(accountStore.GetNonce(accountAddress), 0);

  EvmCallParameters evmParameters;
  auto returnValue{false};

  TransactionReceipt transactionReceipt;
  evmproj::CallResponse evmCallResponseValues;
  accountStoreMock->InvokeEvmInterpreter(&account, RUNNER_CALL, evmParameters,
                                         2, returnValue, transactionReceipt,
                                         evmCallResponseValues);

  const auto balance = accountStore.GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be changed to what is set in the response message
  BOOST_CHECK_EQUAL(balance, expectedBalance);

  const auto nonce = accountStore.GetNonce(accountAddress);
  LOG_GENERAL(DEBUG, "Nonce:" << nonce);
  // the balance should be changed to what is set in the response message
  BOOST_CHECK_EQUAL(nonce, expectedNonce);
}

BOOST_AUTO_TEST_CASE(test_evm_account_balance_nonce_overflow) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  uint256_t expectedBalance{std::numeric_limits<uint128_t>::max()};
  expectedBalance++;
  uint128_t expectedNonce{std::numeric_limits<uint64_t>::max()};
  expectedNonce++;

  LOG_GENERAL(DEBUG, "Expected balance:0x" << std::hex << expectedBalance);
  LOG_GENERAL(DEBUG, "Expected Nonce:0x" << std::hex << expectedNonce);
  const std::string address = "b744160c3De133495aB9F9D77EA54b325b045670";
  EvmClient::GetInstance(
      [&expectedBalance, &expectedNonce, &address]() {
        return std::make_shared<EvmAccountEvmClientMock>(
            expectedBalance.str(),
            expectedNonce.str(),  //
            address);
      },
      true);

  const auto accountStoreMock{std::make_shared<AccountStoreMock>()};

  AccountStore::GetInstance([&accountStoreMock]() { return accountStoreMock; },
                            true);
  auto& accountStore = AccountStore::GetInstance();
  accountStore.Init();

  Address accountAddress{address};
  Account account;
  if (!accountStore.IsAccountExist(accountAddress)) {
    accountStore.AddAccount(accountAddress, account);
  }

  const uint128_t initialBalance{1'000'000};
  accountStore.IncreaseBalance(accountAddress, initialBalance);
  BOOST_CHECK_EQUAL(accountStore.GetBalance(accountAddress), initialBalance);

  EvmCallParameters evmParameters;
  auto returnValue{false};
  TransactionReceipt transactionReceipt;
  evmproj::CallResponse evmCallResponseValues;
  accountStoreMock->InvokeEvmInterpreter(&account, RUNNER_CALL, evmParameters,
                                         2, returnValue, transactionReceipt,
                                         evmCallResponseValues);

  const auto balance = accountStore.GetBalance(accountAddress);
  LOG_GENERAL(DEBUG, "Balance:" << balance);
  // the balance should be not be changed from the initial balance
  BOOST_CHECK_EQUAL(balance, initialBalance);

  const auto nonce = accountStore.GetNonce(accountAddress);
  LOG_GENERAL(DEBUG, "Nonce:" << nonce);
  // the balance should be changed from the original nonce
  BOOST_CHECK_EQUAL(nonce, 0);
}

BOOST_AUTO_TEST_SUITE_END()
