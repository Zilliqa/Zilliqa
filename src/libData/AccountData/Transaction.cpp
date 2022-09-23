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
#include "libCrypto/EthCrypto.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/GasConv.h"
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

  // Generate the signature
  if (IsEth()) {
    bytes signature;
    bytes digest = GetOriginalHash(m_coreInfo, ETH_CHAINID);
    bytes pk_bytes;
    const PrivKey& privKey{senderKeyPair.first};
    privKey.Serialize(pk_bytes, 0);
    if (!SignEcdsaSecp256k1(digest, pk_bytes, signature)) {
      LOG_GENERAL(WARNING, "We failed to generate EDDSA m_signature.");
    }
    m_signature = Signature(signature, 0);
  } else {
    if (!Schnorr::Sign(txnData, senderKeyPair.first, m_coreInfo.senderPubKey,
                       m_signature)) {
      LOG_GENERAL(WARNING, "We failed to generate m_signature.");
    }
  }

  if (!SetHash(txnData)) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
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

  if (!SetHash(txnData)) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }

  // Verify the signature
  if (!IsSigned(txnData)) {
    LOG_GENERAL(WARNING,
                "We failed to verify the input signature! Just a warning...");
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

bool Transaction::Deserialize(const string& src, unsigned int offset) {
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

uint32_t Transaction::GetVersion() const { return m_coreInfo.version; }

uint32_t Transaction::GetVersionIdentifier() const {
  return DataConversion::UnpackB(this->GetVersion());
}

// Check if the version is 1 or 2 - the only valid ones for now
// this will look like 65538 or 65537
bool Transaction::VersionCorrect() const {
  auto const version = GetVersionIdentifier();

  return (version == TRANSACTION_VERSION || version == TRANSACTION_VERSION_ETH);
}

const uint64_t& Transaction::GetNonce() const { return m_coreInfo.nonce; }

const Address& Transaction::GetToAddr() const { return m_coreInfo.toAddr; }

const PubKey& Transaction::GetSenderPubKey() const {
  return m_coreInfo.senderPubKey;
}

Address Transaction::GetSenderAddr() const {
  // If a V2 Tx
  if (IsEth()) {
    LOG_GENERAL(WARNING, "Getting eth style address from pub key");
    return Account::GetAddressFromPublicKeyEth(GetSenderPubKey());
  }

  return Account::GetAddressFromPublicKey(GetSenderPubKey());
}

bool Transaction::IsEth() const {
  auto const version = GetVersionIdentifier();

  return version == TRANSACTION_VERSION_ETH;
}

const uint128_t& Transaction::GetAmountRaw() const { return m_coreInfo.amount; }

const uint128_t Transaction::GetAmountQa() const {
  if (IsEth()) {
    return m_coreInfo.amount / EVM_ZIL_SCALING_FACTOR;
  } else {
    return m_coreInfo.amount;
  }
}

const uint128_t Transaction::GetAmountWei() const {
  if (IsEth()) {
    return m_coreInfo.amount;
  } else {
    // We know the amounts in transactions are capped, so it won't overlow.
    return m_coreInfo.amount * EVM_ZIL_SCALING_FACTOR;
  }
}

const uint128_t& Transaction::GetGasPriceRaw() const {
  return m_coreInfo.gasPrice;
}

const uint128_t Transaction::GetGasPriceQa() const {
  if (IsEth()) {
    return m_coreInfo.gasPrice / EVM_ZIL_SCALING_FACTOR *
           GasConv::GetScalingFactor();
  } else {
    return m_coreInfo.gasPrice;
  }
}

const uint128_t Transaction::GetGasPriceWei() const {
  if (IsEth()) {
    return m_coreInfo.gasPrice;
  } else {
    // We know the amounts in transactions are capped, so it won't overlow.
    return m_coreInfo.gasPrice * EVM_ZIL_SCALING_FACTOR /
           GasConv::GetScalingFactor();
  }
}

uint64_t Transaction::GetGasLimitZil() const {
  if (IsEth()) {
    return GasConv::GasUnitsFromEthToCore(m_coreInfo.gasLimit);
  }
  return m_coreInfo.gasLimit;
}

uint64_t Transaction::GetGasLimitEth() const {
  if (IsEth()) {
    return m_coreInfo.gasLimit;
  }
  return GasConv::GasUnitsFromCoreToEth(m_coreInfo.gasLimit);
}

uint64_t Transaction::GetGasLimitRaw() const { return m_coreInfo.gasLimit; }

const bytes& Transaction::GetCode() const { return m_coreInfo.code; }

const bytes& Transaction::GetData() const { return m_coreInfo.data; }

const Signature& Transaction::GetSignature() const { return m_signature; }

bool Transaction::IsSignedECDSA() const {
  std::string pubKeyStr = std::string(GetCoreInfo().senderPubKey);
  std::string sigString = std::string(GetSignature());

  // Hash of the TXn data (for now just eth-style prelude)
  // Remove '0x' at beginning of hex strings before calling
  if (sigString.size() >= 2 && sigString[0] == '0' && sigString[1] == 'x') {
    sigString = sigString.substr(2);
  }
  if (pubKeyStr.size() >= 2 && pubKeyStr[0] == '0' && pubKeyStr[1] == 'x') {
    pubKeyStr = pubKeyStr.substr(2);
  }
  return VerifyEcdsaSecp256k1(GetOriginalHash(GetCoreInfo(), ETH_CHAINID),
                              sigString, pubKeyStr);
}

// Set what the hash of the transaction is, depending on its type
bool Transaction::SetHash(bytes const& txnData) {
  if (IsEth()) {
    uint64_t recid{0};
    auto const asRLP = GetTransmittedRLP(GetCoreInfo(), ETH_CHAINID,
                                         std::string(m_signature), recid);
    auto const output = CreateHash(asRLP);

    if (output.size() != TRAN_HASH_SIZE) {
      LOG_GENERAL(
          WARNING,
          "We failed to generate an eth m_tranID. Wrong size! Expected: "
              << TRAN_HASH_SIZE << " got: " << output.size());
      return false;
    }

    copy(output.begin(), output.end(), m_tranID.asArray().begin());
    return true;
  }

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return false;
  }

  copy(output.begin(), output.end(), m_tranID.asArray().begin());
  return true;
}

// Function to return whether the TX is signed
bool Transaction::IsSigned(bytes const& txnData) const {
  // Use the version number to tell which signature scheme it is using
  // If a V2 TX
  if (IsEth()) {
    return IsSignedECDSA();
  }

  return Schnorr::Verify(txnData, GetSignature(), GetCoreInfo().senderPubKey);
}

void Transaction::SetSignature(const Signature& signature) {
  m_signature = signature;
}

unsigned int Transaction::GetShardIndex(const Address& fromAddr,
                                        unsigned int numShards) {
  uint32_t x = 0;

  if (numShards == 0) {
    LOG_GENERAL(WARNING, "numShards is 0 and trying to calculate shard index");
    return 0;
  }

  // Take the last four bytes of the address
  for (unsigned int i = 0; i < 4; i++) {
    x = (x << 8) | fromAddr.asArray().at(ACC_ADDR_SIZE - 4 + i);
  }

  return x % numShards;
}

unsigned int Transaction::GetShardIndex(unsigned int numShards) const {
  const auto& fromAddr = GetSenderAddr();

  return GetShardIndex(fromAddr, numShards);
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
