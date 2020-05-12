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

#include "Transaction.h"
#include <algorithm>
#include "Account.h"
#include "libPersistence/ContractStorage2.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned char HIGH_BITS_MASK = 0xF0;
unsigned char LOW_BITS_MASK = 0x0F;
unsigned char ACC_COND = 0x1;
unsigned char TX_COND = 0x2;

bool Transaction::SerializeCoreFields(bytes& dst, unsigned int offset) const {
  return Messenger::SetTransactionCoreInfo(dst, offset, m_coreInfo);
}

Transaction::Transaction() {}

Transaction::Transaction(const bytes& src, unsigned int offset) {
  Deserialize(src, offset);
}

Transaction::Transaction(const uint32_t& version, const uint64_t& nonce,
                         const Address& toAddr, const PairOfKey& senderKeyPair,
                         const uint128_t& amount, const uint128_t& gasPrice,
                         const uint64_t& gasLimit, const bytes& code,
                         const bytes& data)
    : m_coreInfo(version, nonce, toAddr, senderKeyPair.second, amount, gasPrice,
                 gasLimit, code, data) {
  bytes txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Generate the signature
  if (!Schnorr::Sign(txnData, senderKeyPair.first, m_coreInfo.senderPubKey,
                     m_signature)) {
    LOG_GENERAL(WARNING, "We failed to generate m_signature.");
  }
}

Transaction::Transaction(const TxnHash& tranID, const uint32_t& version,
                         const uint64_t& nonce, const Address& toAddr,
                         const PubKey& senderPubKey, const uint128_t& amount,
                         const uint128_t& gasPrice, const uint64_t& gasLimit,
                         const bytes& code, const bytes& data,
                         const Signature& signature)
    : m_tranID(tranID),
      m_coreInfo(version, nonce, toAddr, senderPubKey, amount, gasPrice,
                 gasLimit, code, data),
      m_signature(signature) {}

Transaction::Transaction(const uint32_t& version, const uint64_t& nonce,
                         const Address& toAddr, const PubKey& senderPubKey,
                         const uint128_t& amount, const uint128_t& gasPrice,
                         const uint64_t& gasLimit, const bytes& code,
                         const bytes& data, const Signature& signature)
    : m_coreInfo(version, nonce, toAddr, senderPubKey, amount, gasPrice,
                 gasLimit, code, data),
      m_signature(signature) {
  bytes txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Verify the signature
  if (!Schnorr::Verify(txnData, m_signature, m_coreInfo.senderPubKey)) {
    LOG_GENERAL(WARNING, "We failed to verify the input signature.");
  }
}

Transaction::Transaction(const TxnHash& tranID,
                         const TransactionCoreInfo& coreInfo,
                         const Signature& signature)
    : m_tranID(tranID), m_coreInfo(coreInfo), m_signature(signature) {}

bool Transaction::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTransaction(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransaction failed.");
    return false;
  }

  return true;
}

bool Transaction::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetTransaction(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransaction failed.");
    return false;
  }

  return true;
}

const TxnHash& Transaction::GetTranID() const { return m_tranID; }

const TransactionCoreInfo& Transaction::GetCoreInfo() const {
  return m_coreInfo;
}

const uint32_t& Transaction::GetVersion() const { return m_coreInfo.version; }

const uint64_t& Transaction::GetNonce() const { return m_coreInfo.nonce; }

const Address& Transaction::GetToAddr() const { return m_coreInfo.toAddr; }

const PubKey& Transaction::GetSenderPubKey() const {
  return m_coreInfo.senderPubKey;
}

Address Transaction::GetSenderAddr() const {
  return Account::GetAddressFromPublicKey(GetSenderPubKey());
}

const uint128_t& Transaction::GetAmount() const { return m_coreInfo.amount; }

const uint128_t& Transaction::GetGasPrice() const {
  return m_coreInfo.gasPrice;
}

const uint64_t& Transaction::GetGasLimit() const { return m_coreInfo.gasLimit; }

const bytes& Transaction::GetCode() const { return m_coreInfo.code; }

const bytes& Transaction::GetData() const { return m_coreInfo.data; }

const Signature& Transaction::GetSignature() const { return m_signature; }

void Transaction::SetSignature(const Signature& signature) {
  m_signature = signature;
}

unsigned int Transaction::GetShardIndex(unsigned int numShards) const {
  const auto& fromAddr = GetSenderAddr();
  const auto ds_shard = numShards;

  const auto tt = GetTransactionType(*this);

  if (tt == CONTRACT_CALL) {
    std::chrono::system_clock::time_point tpStart;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
            tpStart = r_timer_start();
      }

      const auto& toAddr = GetToAddr();
      Account* toAccount =
        AccountStore::GetInstance().GetAccount(toAddr);
      Json::Value sh_info;
      auto& cs = Contract::ContractStorage2::GetContractStorage();

      std::vector<Address> extlibs;
      bool is_library = false;
      uint32_t scilla_version = 0;

      if (toAccount != nullptr && toAccount->isContract()
          && cs.FetchContractShardingInfo(toAddr, sh_info)
          && toAccount->GetContractAuxiliaries(is_library, scilla_version, extlibs)
          && !is_library) {

        // Prepare request to the sharding decider
        auto td = GetData();
        std::string dataStr(td.begin(), td.end());
        // SECURITY TODO: do we need to sanity check tx_data? should be handled above us
        Json::Value tx_data;
        if (!JSONUtils::GetInstance().convertStrtoJson(dataStr, tx_data)) {
          return ds_shard;
        }
        std::string prepend = "0x";
        tx_data["_sender"] =
            prepend + fromAddr.hex();
        tx_data["_amount"] = GetAmount().convert_to<std::string>();

        Json::Value req_t = Json::objectValue;
        req_t["req_type"] = "get_shard";
        req_t["sender_shard"] = AddressShardIndex(fromAddr, numShards);
        req_t["contract_shard"] = AddressShardIndex(toAddr, numShards);
        req_t["ds_shard"] = ds_shard;
        req_t["num_shards"] = numShards;
        req_t["sharding_info"] = sh_info;
        req_t["param_contracts"] = Json::arrayValue;
        req_t["tx_data"] = tx_data;

        // Provide info about which addresses in tx_data are contracts
        // _sender is never a contract
        for (const auto& param: tx_data["params"]) {
          if (param.isMember("type") && param.isMember("value")
              && param.isMember("vname")
              && param["type"].asString() == "ByStr20" ) {
            string addr_str = param["value"].asString().erase(0, prepend.length());
            Address paddr (addr_str);
            Account* pacc = AccountStore::GetInstance().GetAccount(paddr);
            if (pacc != nullptr && pacc->isContract()) {
              req_t["param_contracts"].append(param["vname"].asString());
            }
          }
        }
        // We can't send the JSON dictionary directly; serialize it
        string req_str = JSONUtils::GetInstance().convertJsontoStr(req_t);
        Json::Value req = Json::objectValue;
        req["req"] = req_str;

        string result = "";
        bool call_succeeded =
          ScillaClient::GetInstance().CallSharding(scilla_version, req, result);
        Json::Value resp;
        auto shard = ds_shard;
        if (call_succeeded
            && JSONUtils::GetInstance().convertStrtoJson(result, resp)
            && resp.isMember("shard") && resp["shard"].isIntegral()) {

          if (LOG_SC) {
            LOG_GENERAL(INFO, "GetShardIndex\nRequest: " << req_str
                        << "\nResponse: " << result);
          }
          shard = resp["shard"].asUInt();
        }

        if (ENABLE_CHECK_PERFORMANCE_LOG) {
          LOG_GENERAL(INFO, "Routed CONTRACT_CALL to shard in "
                              << r_timer_end(tpStart) << " microseconds");
        }
        return shard;
      // The transaction is junk
      } else {
        LOG_GENERAL(INFO, "GetShardIndex saw JUNK transaction!");
        return ds_shard;
      }
  }

  return AddressShardIndex(fromAddr, numShards);
}

bool Transaction::operator==(const Transaction& tran) const {
  return ((m_tranID == tran.m_tranID) && (m_signature == tran.m_signature));
}

bool Transaction::operator<(const Transaction& tran) const {
  return tran.m_tranID > m_tranID;
}

bool Transaction::operator>(const Transaction& tran) const {
  return tran < *this;
}
