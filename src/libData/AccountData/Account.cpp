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
#include "libPersistence/ContractStorage2.h"
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
  if (!m_address) {
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
                           const Address& addr, const uint64_t& blockNum) {
  LOG_MARKER();
  if (isContract()) {
    LOG_GENERAL(WARNING, "Already Initialized");
    return false;
  }

  if (!PrepareInitDataJson(initData, addr, blockNum, m_initDataJson,
                           m_scilla_version, m_is_library, m_extlibs)) {
    LOG_GENERAL(WARNING, "PrepareInitDataJson failed");
    return false;
  }

  if (!SetImmutable(code, DataConversion::StringToCharArray(
                              JSONUtils::GetInstance().convertJsontoStr(
                                  m_initDataJson)))) {
    LOG_GENERAL(WARNING, "SetImmutable failed");
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

bool Account::ParseInitData(const Json::Value& root, uint32_t& scilla_version,
                            bool& is_library, vector<Address>& extlibs) {
  is_library = false;
  extlibs.clear();

  bool found_scilla_version = false;
  bool found_library = false;
  bool found_extlibs = false;
  for (const auto& entry : root) {
    if (entry.isMember("vname") && entry.isMember("type") &&
        entry.isMember("value")) {
      if (entry["vname"].asString() == "_scilla_version" &&
          entry["type"].asString() == "Uint32") {
        if (found_scilla_version) {
          LOG_GENERAL(WARNING, "Got multiple field of \"_scilla_version\"");
          return false;
        }
        try {
          scilla_version =
              boost::lexical_cast<uint32_t>(entry["value"].asString());
          found_scilla_version = true;
        } catch (...) {
          LOG_GENERAL(WARNING,
                      "invalid value for _scilla_version " << entry["value"]);
          return false;
        }
        // break;
        if (found_library && found_extlibs) {
          break;
        }
      }

      if (entry["vname"].asString() == "_library" &&
          entry["type"].asString() == "Bool") {
        if (found_library) {
          LOG_GENERAL(WARNING, "Got multiple field of \"_library\"");
          return false;
        }
        if (entry["value"].isMember("constructor") &&
            entry["value"]["constructor"] == "True") {
          is_library = true;
        }
        found_library = true;
        if (found_scilla_version && found_extlibs) {
          break;
        }
      }

      if (entry["vname"].asString() == "_extlibs") {
        if (found_extlibs) {
          LOG_GENERAL(WARNING, "Got multiple field of \"_extlibs\"");
          return false;
        }

        if (entry["value"].type() != Json::arrayValue) {
          LOG_GENERAL(WARNING, "entry value is not array type");
          return false;
        }

        for (const auto& lib_entry : entry["value"]) {
          if (lib_entry.isMember("arguments") &&
              lib_entry["arguments"].type() == Json::arrayValue &&
              lib_entry["arguments"].size() == 2) {
            bool foundAddr = false;
            for (const auto& arg : lib_entry["arguments"]) {
              if (arg.asString().size() == ((ACC_ADDR_SIZE * 2) + 2) &&
                  arg.asString().find("0x") != std::string::npos) {
                try {
                  Address addr(arg.asString());
                  extlibs.emplace_back(addr);
                  foundAddr = true;
                  break;
                } catch (...) {
                  LOG_GENERAL(WARNING, "invalid to convert string to address: "
                                           << arg.asString());
                  continue;
                }
              }
            }

            if (!foundAddr) {
              LOG_GENERAL(WARNING, "Didn't find address for extlib");
              return false;
            }

            break;
          }
        }

        found_extlibs = true;

        if (found_scilla_version && found_library) {
          break;
        }
      }
    } else {
      LOG_GENERAL(WARNING, "Wrong data format spotted");
      return false;
    }
  }

  if (!found_scilla_version) {
    LOG_GENERAL(WARNING, "scilla_version not found in init data");
    return false;
  }

  return true;
}

bool Account::PrepareInitDataJson(const bytes& initData, const Address& addr,
                                  const uint64_t& blockNum, Json::Value& root,
                                  uint32_t& scilla_version, bool& is_library,
                                  vector<Address>& extlibs) {
  if (initData.empty()) {
    LOG_GENERAL(WARNING, "Init data for the contract is empty");
    return false;
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(
          DataConversion::CharArrayToString(initData), root)) {
    return false;
  }

  if (!ParseInitData(root, scilla_version, is_library, extlibs)) {
    LOG_GENERAL(WARNING, "ParseInitData failed");
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

void Account::GetUpdatedStates(std::map<std::string, bytes>& t_states,
                               std::vector<std::string>& toDeleteIndices,
                               bool temp) const {
  ContractStorage2::GetContractStorage().FetchUpdatedStateValuesForAddress(
      GetAddress(), t_states, toDeleteIndices, temp);
}

void Account::UpdateStates(const Address& addr,
                           const std::map<std::string, bytes>& t_states,
                           const std::vector<std::string>& toDeleteIndices,
                           bool temp, bool revertible) {
  ContractStorage2::GetContractStorage().UpdateStateDatasAndToDeletes(
      addr, t_states, toDeleteIndices, m_storageRoot, temp, revertible);
  if (!m_address) {
    SetAddress(addr);
  }
}

bool Account::FetchStateJson(Json::Value& root, const string& vname,
                             const vector<string>& indices, bool temp) const {
  if (!isContract()) {
    LOG_GENERAL(WARNING,
                "Not contract account, why call Account::FetchStateJson!");
    return false;
  }

  if (vname != "_balance") {
    if (!ContractStorage2::GetContractStorage().FetchStateJsonForContract(
            root, GetAddress(), vname, indices, temp)) {
      LOG_GENERAL(WARNING,
                  "ContractStorage2::FetchStateJsonForContract failed");
      return false;
    }
  }

  if ((vname.empty() && indices.empty()) || vname == "_balance") {
    root["_balance"] = GetBalance().convert_to<string>();
  }

  if (LOG_SC) {
    LOG_GENERAL(INFO,
                "States: " << JSONUtils::GetInstance().convertJsontoStr(root));
  }

  return true;
}

Address Account::GetAddressFromPublicKey(const PubKey& pubKey) {
  Address address;

  bytes vec;
  pubKey.Serialize(vec, 0);
  SHA2<HashType::HASH_VARIANT_256> sha2;
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

  SHA2<HashType::HASH_VARIANT_256> sha2;
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
  return true;
}

const bytes Account::GetCode() const {
  if (!isContract()) {
    return {};
  }

  if (m_codeCache.empty()) {
    return ContractStorage2::GetContractStorage().GetContractCode(m_address);
  }
  return m_codeCache;
}

bool Account::GetContractAuxiliaries(bool& is_library, uint32_t& scilla_version,
                                     std::vector<Address>& extlibs) {
  if (!isContract()) {
    return false;
  }

  if (m_initDataJson == Json::nullValue) {
    if (!RetrieveContractAuxiliaries()) {
      LOG_GENERAL(WARNING, "RetrieveContractAuxiliaries failed");
      return false;
    }
  }
  is_library = m_is_library;
  scilla_version = m_scilla_version;
  extlibs = m_extlibs;
  return true;
}

bool Account::RetrieveContractAuxiliaries() {
  if (!isContract()) {
    LOG_GENERAL(WARNING, "Not a contract");
    return false;
  }

  bytes initData = GetInitData();
  if (!JSONUtils::GetInstance().convertStrtoJson(
          DataConversion::CharArrayToString(initData), m_initDataJson)) {
    LOG_GENERAL(WARNING, "Convert InitData to Json failed"
                             << endl
                             << DataConversion::CharArrayToString(initData));
    return false;
  }

  return ParseInitData(m_initDataJson, m_scilla_version, m_is_library,
                       m_extlibs);
}

bool Account::SetInitData(const bytes& initData) {
  // LOG_MARKER();

  if (initData.size() == 0) {
    LOG_GENERAL(WARNING, "InitData for this contract is empty");
    return false;
  }

  m_initDataCache = initData;
  return true;
}

const bytes Account::GetInitData() const {
  if (!isContract()) {
    return {};
  }

  if (m_initDataCache.empty()) {
    return ContractStorage2::GetContractStorage().GetInitData(m_address);
  }
  return m_initDataCache;
}

bool Account::SetImmutable(const bytes& code, const bytes& initData) {
  if (!SetCode(code) || !SetInitData(initData)) {
    return false;
  }

  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(code);
  sha2.Update(initData);
  SetCodeHash(dev::h256(sha2.Finalize()));
  // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);
  return true;
}
