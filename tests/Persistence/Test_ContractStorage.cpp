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

#define BOOST_TEST_MODULE trietest
#include <json/json.h>
#include <boost/filesystem/path.hpp>
#include <boost/test/included/unit_test.hpp>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;
using namespace Contract;

BOOST_AUTO_TEST_SUITE(contractstorage)
/*
BOOST_AUTO_TEST_CASE(contract_proof_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  map<string, bytes> t_states;
  string key1 =
      ContractStorage::GetContractStorage().GenerateStorageKey("aaa", {});
  string key2 =
      ContractStorage::GetContractStorage().GenerateStorageKey("aaa", {"1"});
  string key3 = ContractStorage::GetContractStorage().GenerateStorageKey(
      "aaa", {"1", "1"});
  string key4 =
      ContractStorage::GetContractStorage().GenerateStorageKey("aaa", {"2"});
  string key5 =
      ContractStorage::GetContractStorage().GenerateStorageKey("bbb", {});
  t_states.emplace(key1, DataConversion::StringToCharArray("111"));
  t_states.emplace(key2, DataConversion::StringToCharArray("111a"));
  t_states.emplace(key3, DataConversion::StringToCharArray("111aa"));
  t_states.emplace(key4, DataConversion::StringToCharArray("111b"));
  t_states.emplace(key5, DataConversion::StringToCharArray("222"));

  for (unsigned int i = 0; i < 1000; i++) {
    string key = ContractStorage::GetContractStorage().GenerateStorageKey(
        to_string(i), {to_string(i)});
    t_states.emplace(key, DataConversion::StringToCharArray(to_string(i)));
  }

  PairOfKey kpair = Schnorr::GenKeyPair();
  Address addr = Account::GetAddressFromPublicKey(kpair.second);
  h256 root1;
  ContractStorage::GetContractStorage().UpdateStateDatasAndToDeletes(
      addr, dev::h256(), t_states, {}, root1, false, false);

  uint64_t dsBlockNum = 100;
  BOOST_CHECK(ContractStorage::GetContractStorage().CommitStateDB(dsBlockNum));

  set<string> proof;
  BOOST_CHECK(ContractStorage::GetContractStorage().FetchStateProofForContract(
      proof, root1, "aaa", {"1"}));

  dev::MemoryDB t_db;
  size_t size = 0;
  for (const auto& p : proof) {
    LOG_GENERAL(INFO, "h256: " << dev::sha3(p).hex() << " value: " << p);
    t_db.insert(dev::sha3(p), &p);
    size += p.size();
  }
  LOG_GENERAL(INFO, "total size: " << size);

  GenericTrieDB<dev::MemoryDB> t_trie(&t_db);
  t_trie.setRoot(root1);

  for (const auto& s : t_states) {
    LOG_GENERAL(INFO, "key: " << s.first << " value: "
                              << t_trie.at(bytesConstRef(s.first)));
  }

  // LOG_GENERAL(INFO, "key1: " << t_trie.at(bytesConstRef(key1)));
  // LOG_GENERAL(INFO, "key2: " << t_trie.at(bytesConstRef(key2)));
  // LOG_GENERAL(INFO, "key3: " << t_trie.at(bytesConstRef(key3)));
  // LOG_GENERAL(INFO, "key4: " << t_trie.at(bytesConstRef(key4)));
  // LOG_GENERAL(INFO, "key5: " << t_trie.at(bytesConstRef(key5)));
  // for (unsigned int i = 0;)
  // BOOST_CHECK(t_trie.at(bytesConstRef(key1)) == "");
  // BOOST_CHECK(t_trie.at(bytesConstRef(key2)) == "111a");
  // BOOST_CHECK(t_trie.at(bytesConstRef(key3)) == "111aa");
  // BOOST_CHECK(t_trie.at(bytesConstRef(key4)) == "");
  // BOOST_CHECK(t_trie.at(bytesConstRef(key5)) == "");
}
*/

BOOST_AUTO_TEST_SUITE_END()