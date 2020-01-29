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

#include <array>
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/TransactionReceipt.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

BOOST_AUTO_TEST_SUITE(accountstoretest)

void setDepth(TransactionReceipt& tr, uint8_t depth) {
  for (int i = 0; i < depth; i++) {
    tr.AddEdge();
  }
}

BOOST_AUTO_TEST_CASE(transactionreceipt) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TransactionReceipt tr = TransactionReceipt();
  tr.SetResult(false);
  tr.SetResult(true);

  uint8_t depth = TestUtils::RandomIntInRng(1, 10);
  setDepth(tr, depth);

  uint8_t err_code = TestUtils::RandomIntInRng(1, 10);
  tr.AddError(err_code);

  uint64_t cumGas = TestUtils::DistUint64();
  tr.SetCumGas(cumGas);
  BOOST_CHECK_EQUAL(true,
                    tr.GetJsonValue()["cumulative_gas"].asString().compare(
                        std::to_string(cumGas)) == 0);

  uint64_t epochNum = TestUtils::DistUint64();
  tr.SetEpochNum(epochNum);

  std::string tranReceiptStr_wrong = "asd:';`123|}}{";
  tr.SetString(tranReceiptStr_wrong);
  BOOST_CHECK_EQUAL(true, tr.GetString().compare("{}") == 0);

  std::string tranReceiptStr = "{\"a\":1}";
  tr.SetString(tranReceiptStr);
  BOOST_CHECK_EQUAL(true, tr.GetString().compare(tranReceiptStr) == 0);

  LogEntry entry;
  tr.AddEntry(entry);

  tr.InstallError();

  bytes src;

  BOOST_CHECK_EQUAL(true, tr.Serialize(src, 0));

  std::cout << "str " << tr.GetString() << std::endl;
  tr.clear();

  BOOST_CHECK_EQUAL(true, tr.GetString().compare("{}") == 0);
  BOOST_CHECK_EQUAL(true, tr.GetJsonValue().size() == 0);

  TransactionReceipt tr_2 = TransactionReceipt();
  BOOST_CHECK_EQUAL(true, tr_2.Deserialize(src, 0));

  std::string tranReceiptStr_2 = tr_2.GetString();
  tranReceiptStr_2.erase(
      remove_if(tranReceiptStr_2.begin(), tranReceiptStr_2.end(), isspace),
      tranReceiptStr_2.end());
  BOOST_CHECK_EQUAL(true, tranReceiptStr_2.compare(tranReceiptStr) == 0);

  std::ostringstream oss;
  Json::StreamWriterBuilder().newStreamWriter()->write(tr_2.GetJsonValue(),
                                                       &oss);

  tranReceiptStr_2 = oss.str();
  tranReceiptStr_2.erase(
      remove_if(tranReceiptStr_2.begin(), tranReceiptStr_2.end(), isspace),
      tranReceiptStr_2.end());
  BOOST_CHECK_EQUAL(true, tranReceiptStr_2.compare(tranReceiptStr) == 0);
  BOOST_CHECK_EQUAL(true, tr_2.GetCumGas() == cumGas);
}

BOOST_AUTO_TEST_CASE(transactionwithreceipt) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  std::vector<std::string> transactionStrings = {"{\"a\":1}", "{\"b\":2}",
                                                 "{\"c\":3}"};

  Transaction tran = Transaction();
  TransactionReceipt tr;
  std::vector<TransactionWithReceipt> txrs;

  SHA2<HashType::HASH_VARIANT_256> sha2;

  for (const auto& ts : transactionStrings) {
    sha2.Update(DataConversion::StringToCharArray(ts.c_str()));
    tr = TransactionReceipt();
    tr.SetString(ts);
    txrs.emplace_back(tran, tr);
  }
  TxnHash hash = TxnHash(sha2.Finalize());

  BOOST_CHECK_EQUAL(
      true,
      hash == TransactionWithReceipt::ComputeTransactionReceiptsHash(txrs));

  std::vector<TxnHash> txnOrder;
  std::unordered_map<TxnHash, TransactionWithReceipt> twr_map;
  TxnHash th_out;
  TxnHash th;

  for (const auto& ts : transactionStrings) {
    th = TxnHash::random();
    tr.SetString(ts);
    twr_map.emplace(th, TransactionWithReceipt(tran, tr));
    txnOrder.push_back(th);
  }
  BOOST_CHECK_EQUAL(true,
                    TransactionWithReceipt::ComputeTransactionReceiptsHash(
                        txnOrder, twr_map, th_out));
  BOOST_CHECK_EQUAL(true, hash == th_out);
}
BOOST_AUTO_TEST_SUITE_END()
