/*
 * Copyright (C) 2021 Zilliqa
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
#include "libCrypto/Sha2.h"
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

dev::h256 convertToHash(const string& input) {
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(input);

  const bytes& output = sha2.Finalize();
  dev::h256 key(output);

  return key;
}

BOOST_AUTO_TEST_CASE(contract_proof_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  map<string, bytes> t_states;
  PairOfKey kpair = Schnorr::GenKeyPair();
  Address addr = Account::GetAddressFromPublicKey(kpair.second);
  auto dummy_addr = addr;

  string key1 = ContractStorage::GetContractStorage().GenerateStorageKey(
      dummy_addr, "aaa", {});
  dev::h256 hashed_key1 = convertToHash(key1);
  string key2 = ContractStorage::GetContractStorage().GenerateStorageKey(
      dummy_addr, "aaa", {"1"});
  dev::h256 hashed_key2 = convertToHash(key2);
  string key3 = ContractStorage::GetContractStorage().GenerateStorageKey(
      dummy_addr, "aaa", {"1", "1"});
  dev::h256 hashed_key3 = convertToHash(key3);
  string key4 = ContractStorage::GetContractStorage().GenerateStorageKey(
      dummy_addr, "aaa", {"2"});
  dev::h256 hashed_key4 = convertToHash(key4);
  string key5 = ContractStorage::GetContractStorage().GenerateStorageKey(
      dummy_addr, "bbb", {});
  dev::h256 hashed_key5 = convertToHash(key5);
  t_states.emplace(hashed_key1.hex(), DataConversion::StringToCharArray("111"));
  t_states.emplace(hashed_key2.hex(),
                   DataConversion::StringToCharArray("111a"));
  t_states.emplace(hashed_key3.hex(),
                   DataConversion::StringToCharArray("111aa"));
  t_states.emplace(hashed_key4.hex(),
                   DataConversion::StringToCharArray("111b"));
  t_states.emplace(hashed_key5.hex(), DataConversion::StringToCharArray("222"));

  for (unsigned int i = 0; i < 1000; i++) {
    string key = ContractStorage::GetContractStorage().GenerateStorageKey(
        dummy_addr, to_string(i), {to_string(i)});
    dev::h256 hashed_key = convertToHash(key);
    t_states.emplace(hashed_key.hex(),
                     DataConversion::StringToCharArray(to_string(i)));
  }

  h256 root1;
  ContractStorage::GetContractStorage().UpdateStateDatasAndToDeletes(
      addr, dev::h256(), t_states, {}, root1, false, false);

  uint64_t dsBlockNum = 100;
  BOOST_CHECK(ContractStorage::GetContractStorage().CommitStateDB(dsBlockNum));

  set<string> proof;
  BOOST_CHECK(ContractStorage::GetContractStorage().FetchStateProofForContract(
      proof, root1, hashed_key2));
}

BOOST_AUTO_TEST_SUITE_END()