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

#include <boost/lexical_cast.hpp>

#include "Account.h"
#include "common/Messages.h"
#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

using namespace Contract;

// =======================================
// AccountBase

AccountBase::AccountBase(const uint128_t& balance, const uint64_t& nonce,
                         const uint32_t& version)
    : m_version(version),
      m_balance(balance),
      m_nonce(nonce),
      m_storageRoot(h256()),
      m_codeHash(h256()) {}

bool AccountBase::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetAccountBase(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccount failed.");
    return false;
  }

  return true;
}

bool AccountBase::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  if (!Messenger::GetAccountBase(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccount failed.");
    return false;
  }

  return true;
}

void AccountBase::SetVersion(const uint32_t& version) { m_version = version; }

const uint32_t& AccountBase::GetVersion() const { return m_version; }

bool AccountBase::IncreaseBalance(const uint128_t& delta) {
  return SafeMath<uint128_t>::add(m_balance, delta, m_balance);
}

bool AccountBase::DecreaseBalance(const uint128_t& delta) {
  if (m_balance < delta) {
    return false;
  }

  return SafeMath<uint128_t>::sub(m_balance, delta, m_balance);
}

bool AccountBase::ChangeBalance(const int256_t& delta) {
  return (delta >= 0) ? IncreaseBalance(uint128_t(delta))
                      : DecreaseBalance(uint128_t(-delta));
}

void AccountBase::SetBalance(const uint128_t& balance) { m_balance = balance; }

const uint128_t& AccountBase::GetBalance() const { return m_balance; }

bool AccountBase::IncreaseNonce() {
  return SafeMath<uint64_t>::add(m_nonce, 1, m_nonce);
}

bool AccountBase::IncreaseNonceBy(const uint64_t& nonceDelta) {
  return SafeMath<uint64_t>::add(m_nonce, nonceDelta, m_nonce);
}

void AccountBase::SetNonce(const uint64_t& nonce) { m_nonce = nonce; }

const uint64_t& AccountBase::GetNonce() const { return m_nonce; }

void Account::SetAddress(const Address& addr) {
  if (m_address == Address()) {
    m_address = addr;
  }
}

const Address& Account::GetAddress() const { return m_address; }

void AccountBase::SetStorageRoot(const h256& root) { m_storageRoot = root; }

const dev::h256& AccountBase::GetStorageRoot() const { return m_storageRoot; }

void AccountBase::SetCodeHash(const dev::h256& codeHash) {
  m_codeHash = codeHash;
}

const dev::h256& AccountBase::GetCodeHash() const { return m_codeHash; }

bool AccountBase::isContract() const { return m_codeHash != dev::h256(); }

// =======================================
// Account
Account::Account(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init Account.");
  }
}

Account::Account(const uint128_t& balance, const uint64_t& nonce,
                 const uint32_t& version)
    : AccountBase(balance, nonce, version) {}

bool Account::InitContract(const bytes& code, const bytes& initData,
                           const Address& addr, const uint64_t& blockNum,
                           bool temp) {
  LOG_MARKER();
  if (isContract()) {
    LOG_GENERAL(WARNING, "Already Initialized");
    return false;
  }

  Json::Value initDataJson;
  if (!PrepareInitDataJson(initData, addr, blockNum, initDataJson)) {
    LOG_GENERAL(WARNING, "PrepareInitDataJson failed");
    return false;
  }

  std::vector<StateEntry> state_entries;

  for (auto& v : initDataJson) {
    if (!v.isMember("vname") || !v.isMember("type") || !v.isMember("value")) {
      LOG_GENERAL(WARNING,
                  "This variable in initialization of contract is corrupted");
      return false;
    }

    string vname = v["vname"].asString();
    string type = v["type"].asString();

    string value = JSONUtils::GetInstance().convertJsontoStr(v["value"]);

    state_entries.push_back(std::make_tuple(vname, false, type, value));
  }

  if (!ContractStorage::GetContractStorage().PutContractState(
          addr, state_entries, m_storageRoot, temp)) {
    LOG_GENERAL(WARNING, "ContractStorage::PutContractState failed");
    return false;
  }

  if (!SetCode(code)) {
    LOG_GENERAL(WARNING, "SetCode failed");
    return false;
  }

  SetAddress(addr);

  return true;
}

bool Account::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetAccount(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccount failed.");
    return false;
  }

  return true;
}

bool Account::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();
  // This function is depreciated.
  if (!Messenger::GetAccount(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccount failed.");
    return false;
  }

  return true;
}

bool Account::SerializeBase(bytes& dst, unsigned int offset) const {
  return AccountBase::Serialize(dst, offset);
}

bool Account::DeserializeBase(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  return AccountBase::Deserialize(src, offset);
}

bool Account::SetStorage(const vector<StateEntry>& state_entries, bool temp) {
  if (!isContract()) {
    return false;
  }

  return ContractStorage::GetContractStorage().PutContractState(
      m_address, state_entries, m_storageRoot, temp);
}

bool Account::SetStorage(const Address& addr,
                         const vector<pair<dev::h256, bytes>>& entries,
                         bool temp, bool revertible) {
  if (!isContract()) {
    return false;
  }

  if (!ContractStorage::GetContractStorage().PutContractState(
          addr, entries, m_storageRoot, temp, revertible, {}, false)) {
    LOG_GENERAL(WARNING, "PutContractState failed");
    return false;
  }

  SetAddress(addr);

  return true;
}

string Account::GetRawStorage(const h256& k_hash, bool temp) const {
  if (!isContract()) {
    // LOG_GENERAL(WARNING,
    //             "Not contract account, why call Account::GetRawStorage!");
    return "";
  }
  return ContractStorage::GetContractStorage().GetContractStateData(k_hash,
                                                                    temp);
}

bool Account::PrepareInitDataJson(const bytes& initData, const Address& addr,
                                  const uint64_t& blockNum, Json::Value& root) {
  if (initData.empty()) {
    LOG_GENERAL(WARNING, "Init data for the contract is empty");
    return false;
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(
          {initData.begin(), initData.end()}, root)) {
    return false;
  }

  // Append createBlockNum
  Json::Value createBlockNumObj;
  createBlockNumObj["vname"] = "_creation_block";
  createBlockNumObj["type"] = "BNum";
  createBlockNumObj["value"] = to_string(blockNum);
  root.append(createBlockNumObj);

  // Append _this_address
  Json::Value thisAddressObj;
  thisAddressObj["vname"] = "_this_address";
  thisAddressObj["type"] = "ByStr20";
  thisAddressObj["value"] = "0x" + addr.hex();
  root.append(thisAddressObj);

  return true;
}

Json::Value Account::GetInitJson(bool temp) const {
  if (!isContract()) {
    return Json::arrayValue;
  }

  pair<Json::Value, Json::Value> roots;
  if (!GetStorageJson(roots, temp)) {
    LOG_GENERAL(WARNING, "GetStorageJson failed");
    return Json::arrayValue;
  }
  return roots.first;
}

vector<h256> Account::GetStorageKeyHashes(bool temp) const {
  if (!isContract()) {
    return {};
  }

  return ContractStorage::GetContractStorage().GetContractStateIndexes(
      m_address, temp);
}

Json::Value Account::GetStateJson(bool temp) const {
  if (!isContract()) {
    return Json::arrayValue;
  }

  pair<Json::Value, Json::Value> roots;
  if (!GetStorageJson(roots, temp)) {
    LOG_GENERAL(WARNING, "GetStorageJson failed");
    return Json::arrayValue;
  }
  return roots.second;
}

bool Account::GetStorageJson(pair<Json::Value, Json::Value>& roots, bool temp,
                             uint32_t& scilla_version) const {
  if (!isContract()) {
    LOG_GENERAL(WARNING,
                "Not contract account, why call Account::GetStorageJson!");
    return false;
  }

  // Init, Other
  if (!ContractStorage::GetContractStorage().GetContractStateJson(
          m_address, roots, scilla_version, temp)) {
    LOG_GENERAL(WARNING, "ContractStorage::GetContractStateJson failed");
    return false;
  }

  try {
    Json::Value balance;
    balance["vname"] = "_balance";
    balance["type"] = "Uint128";
    balance["value"] = GetBalance().convert_to<string>();
    roots.second.append(balance);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }
  // LOG_GENERAL(INFO, "States: " << root);

  return true;
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

bool Account::SetCode(const bytes& code) {
  // LOG_MARKER();

  if (code.size() == 0) {
    LOG_GENERAL(WARNING, "Code for this contract is empty");
    return false;
  }

  m_codeCache = code;
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(code);
  SetCodeHash(dev::h256(sha2.Finalize()));
  // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);
  return true;
}

const bytes Account::GetCode() const {
  if (!isContract()) {
    return {};
  }

  if (m_codeCache.empty()) {
    return ContractStorage::GetContractStorage().GetContractCode(m_address);
  } else {
    return m_codeCache;
  }
}
