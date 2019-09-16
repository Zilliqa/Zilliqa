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

#define BOOST_TEST_MODULE test_state
#include <boost/test/included/unit_test.hpp>

#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/safeserver/safehttpserver.h"
#include "libCrypto/Schnorr.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libServer/LookupServer.h"

using namespace std;

vector<string> readFileintoVec(const string& path, const string& fileName) {
  const string& totalPath = path + "/" + fileName;
  ifstream in(totalPath.c_str());

  if (!in) {
    cerr << "Cannot open file " << fileName << endl;
    return vector<string>();
  }

  string str;
  vector<string> return_vec;
  while (!in.eof()) {
    in >> str;
    if (str.size() > 0) {
      return_vec.push_back(str);
    }
  }
  in.close();
  return return_vec;
}

Json::Value readJsonFromFile(const string& path, const string& fileName) {
  const string& totalPath = path + "/" + fileName;
  ifstream in(totalPath.c_str());

  if (!in) {
    cerr << "Cannot open file \n" << fileName << endl;
    return Json::Value();
  }

  Json::Value root;
  in >> root;
  return root;
}

const string ud_contract_one = "a11de7664F55F5bDf8544a9aC711691D01378b4c";
const string ud_contract_two = "9611c53BE6d1b32058b2747bdeCECed7e1216793";
const string configfolder = ".";

BOOST_AUTO_TEST_SUITE(test_state)

BOOST_AUTO_TEST_CASE(test_state) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  PairOfKey key;
  Peer peer;

  Mediator mediator(key, peer);
  Node node(mediator, 0, false);
  auto vd = make_shared<Validator>(mediator);

  mediator.RegisterColleagues(nullptr, &node, nullptr, vd.get());

  AccountStore::GetInstance().Init();

  auto lookupServerConnector =
      make_unique<jsonrpc::SafeHttpServer>(LOOKUP_RPC_PORT);
  auto lookupServer =
      make_shared<LookupServer>(mediator, *lookupServerConnector);

  for (const auto& address_str : readFileintoVec(configfolder, "address.txt")) {
    const Address addr(address_str);
    AccountStore::GetInstance().AddAccount(addr, {uint64_t() - 1, 0});
  }

  uint32_t blocknum = 1;

  for (const auto& tranHash_str :
       readFileintoVec(configfolder, "tranhashes.txt")) {
    TxnHash tranHash(tranHash_str);
    TxBodySharedPtr tx;
    if (!BlockStorage::GetBlockStorage().GetTxBody(tranHash, tx)) {
      LOG_GENERAL(WARNING, "Missing Tx: " << tranHash);
      continue;
    }

    LOG_GENERAL(INFO, "Process txn " << tranHash);

    TransactionReceipt txreceipt;
    AccountStore::GetInstance().UpdateAccountsTemp(
        blocknum, 3  // Arbitrary as isDS is set true
        ,
        true, tx->GetTransaction(), txreceipt);

    blocknum++;
  }

  AccountStore::GetInstance().SerializeDelta();
  AccountStore::GetInstance().CommitTemp();

  auto resp1 = lookupServer->GetSmartContractState(ud_contract_one);
  BOOST_CHECK_EQUAL(readJsonFromFile(configfolder, "golden_state_1.json"),
                    resp1);
  auto resp2 = lookupServer->GetSmartContractState(ud_contract_two);
  BOOST_CHECK_EQUAL(readJsonFromFile(configfolder, "golden_state_2.json"),
                    resp2);
}

BOOST_AUTO_TEST_SUITE_END()
