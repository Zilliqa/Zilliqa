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

#include "Account.h"
#include "common/Messages.h"
#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

Account::Account() {}

Account::Account(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init Account.");
  }
}

Account::Account(const uint128_t& balance, const uint64_t& nonce)
    : m_balance(balance),
      m_nonce(nonce),
      m_storageRoot(h256()),
      m_codeHash(h256()) {}

bool Account::isContract() const { return m_codeHash != dev::h256(); }

void Account::InitStorage() {
  // LOG_MARKER();
  m_storage = AccountTrieDB<dev::h256, OverlayDB>(
      &(ContractStorage::GetContractStorage().GetStateDB()));
  m_storage.init();
  if (m_storageRoot != h256()) {
    m_storage.setRoot(m_storageRoot);
    m_prevRoot = m_storageRoot;
  }
}

void Account::InitContract(const bytes& data, const Address& addr) {
  SetInitData(data);
  InitContract(addr);
}

void Account::InitContract(const Address& addr) {
  // LOG_MARKER();
  if (m_initData.empty()) {
    LOG_GENERAL(WARNING, "Init data for the contract is empty");
    m_initValJson = Json::arrayValue;
    return;
  }
  Json::CharReaderBuilder builder;
  unique_ptr<Json::CharReader> reader(builder.newCharReader());
  Json::Value root;
  string dataStr(m_initData.begin(), m_initData.end());
  string errors;
  if (!reader->parse(dataStr.c_str(), dataStr.c_str() + dataStr.size(), &root,
                     &errors)) {
    LOG_GENERAL(WARNING,
                "Failed to parse initialization contract json: " << errors);
    return;
  }
  m_initValJson = root;

  // Append createBlockNum
  {
    Json::Value createBlockNumObj;
    createBlockNumObj["vname"] = "_creation_block";
    createBlockNumObj["type"] = "BNum";
    createBlockNumObj["value"] = to_string(GetCreateBlockNum());
    m_initValJson.append(createBlockNumObj);
  }

  // Append _this_address
  {
    Json::Value createBlockNumObj;
    createBlockNumObj["vname"] = "_this_address";
    createBlockNumObj["type"] = "ByStr20";
    createBlockNumObj["value"] = "0x" + addr.hex();
    m_initValJson.append(createBlockNumObj);
  }

  for (auto& v : root) {
    if (!v.isMember("vname") || !v.isMember("type") || !v.isMember("value")) {
      LOG_GENERAL(WARNING,
                  "This variable in initialization of contract is corrupted");
      continue;
    }
    string vname = v["vname"].asString();
    string type = v["type"].asString();

    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    ostringstream oss;
    writer->write(v["value"], &oss);
    string value = oss.str();

    SetStorage(vname, type, value, false);
  }
}

void Account::SetCreateBlockNum(const uint64_t& blockNum) {
  m_createBlockNum = blockNum;
}

const uint64_t& Account::GetCreateBlockNum() const { return m_createBlockNum; }

bool Account::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetAccount(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccount failed.");
    return false;
  }

  return true;
}

bool Account::Deserialize(const bytes& src, unsigned int offset) {
  LOG_MARKER();

  if (!Messenger::GetAccount(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccount failed.");
    return false;
  }

  return true;
}

// bool Account::SerializeDelta(bytes& dst, unsigned int offset,
//                              Account* oldAccount, const Account& newAccount)
//                              {
//   if (!Messenger::SetAccountDelta(dst, offset, oldAccount, newAccount)) {
//     LOG_GENERAL(WARNING, "Messenger::SetAccountDelta failed.");
//     return false;
//   }

//   return true;
// }

// bool Account::DeserializeDelta(const bytes& src, unsigned int offset,
//                                Account& account, bool fullCopy) {
//   if (!Messenger::GetAccountDelta(src, offset, account, fullCopy)) {
//     LOG_GENERAL(WARNING, "Messenger::GetAccountDelta failed.");
//     return false;
//   }

//   return true;
// }

bool Account::IncreaseBalance(const uint128_t& delta) {
  return SafeMath<uint128_t>::add(m_balance, delta, m_balance);
}

bool Account::DecreaseBalance(const uint128_t& delta) {
  if (m_balance < delta) {
    return false;
  }

  return SafeMath<uint128_t>::sub(m_balance, delta, m_balance);
}

bool Account::ChangeBalance(const int256_t& delta) {
  return (delta >= 0) ? IncreaseBalance(uint128_t(delta))
                      : DecreaseBalance(uint128_t(-delta));
}

void Account::SetBalance(const uint128_t& balance) { m_balance = balance; }

const uint128_t& Account::GetBalance() const { return m_balance; }

bool Account::IncreaseNonce() {
  ++m_nonce;
  return true;
}

bool Account::IncreaseNonceBy(const uint64_t& nonceDelta) {
  m_nonce += nonceDelta;
  return true;
}

void Account::SetNonce(const uint64_t& nonce) { m_nonce = nonce; }

const uint64_t& Account::GetNonce() const { return m_nonce; }

void Account::SetStorageRoot(const h256& root) {
  if (!isContract()) {
    return;
  }
  m_storageRoot = root;

  if (m_storageRoot == h256()) {
    return;
  }

  m_storage.setRoot(m_storageRoot);
  m_prevRoot = m_storageRoot;
}

const dev::h256& Account::GetStorageRoot() const { return m_storageRoot; }

void Account::SetStorage(string k, string type, string v, bool is_mutable) {
  if (!isContract()) {
    return;
  }
  RLPStream rlpStream(4);
  rlpStream << k << (is_mutable ? "True" : "False") << type << v;

  m_storage.insert(GetKeyHash(k), rlpStream.out());

  m_storageRoot = m_storage.root();
}

void Account::SetStorage(const h256& k_hash, const string& rlpStr) {
  if (!isContract()) {
    LOG_GENERAL(WARNING, "Not contract account, why call Account::SetStorage!");
    return;
  }
  m_storage.insert(k_hash, rlpStr);
  m_storageRoot = m_storage.root();
}

vector<string> Account::GetStorage(const string& _k) const {
  if (!isContract()) {
    LOG_GENERAL(WARNING, "Not contract account, why call Account::GetStorage!");
    return {};
  }

  dev::RLP rlp(m_storage.at(GetKeyHash(_k)));
  // mutable, type, value
  return {rlp[1].toString(), rlp[2].toString(), rlp[3].toString()};
}

string Account::GetRawStorage(const h256& k_hash) const {
  if (!isContract()) {
    // LOG_GENERAL(WARNING,
    //             "Not contract account, why call Account::GetRawStorage!");
    return "";
  }
  return m_storage.at(k_hash);
}

Json::Value Account::GetInitJson() const { return m_initValJson; }

const bytes& Account::GetInitData() const { return m_initData; }

void Account::SetInitData(const bytes& initData) { m_initData = initData; }

vector<h256> Account::GetStorageKeyHashes() const {
  vector<h256> keyHashes;
  for (auto const& i : m_storage) {
    keyHashes.emplace_back(i.first);
  }
  return keyHashes;
}

Json::Value Account::GetStorageJson() const {
  if (!isContract()) {
    LOG_GENERAL(WARNING,
                "Not contract account, why call Account::GetStorageJson!");
    return Json::arrayValue;
  }

  Json::Value root;
  for (auto const& i : m_storage) {
    dev::RLP rlp(i.second);
    string tVname = rlp[0].toString();
    string tMutable = rlp[1].toString();
    string tType = rlp[2].toString();
    string tValue = rlp[3].toString();
    // LOG_GENERAL(INFO,
    //             "\nvname: " << tVname << " \nmutable: " << tMutable
    //                         << " \ntype: " << tType
    //                         << " \nvalue: " << tValue);
    if (tMutable == "False") {
      continue;
    }

    Json::Value item;
    item["vname"] = tVname;
    item["type"] = tType;
    if (tValue[0] == '[' || tValue[0] == '{') {
      Json::CharReaderBuilder builder;
      unique_ptr<Json::CharReader> reader(builder.newCharReader());
      Json::Value obj;
      string errors;
      if (!reader->parse(tValue.c_str(), tValue.c_str() + tValue.size(), &obj,
                         &errors)) {
        LOG_GENERAL(WARNING,
                    "The json object cannot be extracted from Storage: "
                        << tValue << endl
                        << "Error: " << errors);
        continue;
      }
      item["value"] = obj;
    } else {
      item["value"] = tValue;
    }
    root.append(item);
  }
  Json::Value balance;
  balance["vname"] = "_balance";
  balance["type"] = "Uint128";
  balance["value"] = GetBalance().convert_to<string>();
  root.append(balance);

  // LOG_GENERAL(INFO, "States: " << root);

  return root;
}

void Account::Commit() { m_prevRoot = m_storageRoot; }

void Account::RollBack() {
  if (!isContract()) {
    LOG_GENERAL(WARNING, "Not a contract, why call Account::RollBack");
    return;
  }
  m_storageRoot = m_prevRoot;
  if (m_storageRoot != h256()) {
    m_storage.setRoot(m_storageRoot);
  } else {
    m_storage.init();
  }
}

Address Account::GetAddressFromPublicKey(const PubKey& pubKey) {
  Address address;

  bytes vec;
  pubKey.Serialize(vec, 0);
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(vec);

  const bytes& output = sha2.Finalize();

  if (output.size() != 32) {
    LOG_GENERAL(WARNING, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
  }

  copy(output.end() - ACC_ADDR_SIZE, output.end(), address.asArray().begin());

  return address;
}

Address Account::GetAddressForContract(const Address& sender,
                                       const uint64_t& nonce) {
  Address address;

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  bytes conBytes;
  copy(sender.asArray().begin(), sender.asArray().end(),
       back_inserter(conBytes));
  SetNumber<uint64_t>(conBytes, conBytes.size(), nonce, sizeof(uint64_t));
  sha2.Update(conBytes);

  const bytes& output = sha2.Finalize();

  if (output.size() != 32) {
    LOG_GENERAL(WARNING, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
  }

  copy(output.end() - ACC_ADDR_SIZE, output.end(), address.asArray().begin());

  return address;
}

void Account::SetCode(const bytes& code) {
  // LOG_MARKER();

  if (code.size() == 0) {
    LOG_GENERAL(WARNING, "Code for this contract is empty");
    return;
  }

  m_codeCache = code;
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(code);
  m_codeHash = dev::h256(sha2.Finalize());
  // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);

  InitStorage();
}

const bytes& Account::GetCode() const { return m_codeCache; }

const dev::h256& Account::GetCodeHash() const { return m_codeHash; }

const h256 Account::GetKeyHash(const string& key) const {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(DataConversion::StringToCharArray(key));
  return h256(sha2.Finalize());
}
