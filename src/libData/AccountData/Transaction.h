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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTION_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTION_H_

#include <Schnorr.h>
#include "Address.h"
#include "common/Constants.h"
#include "common/Hashes.h"
#include "common/Serializable.h"
#include "libUtils/Logger.h"

struct TransactionCoreInfo {
  TransactionCoreInfo() = default;
  TransactionCoreInfo(const uint32_t& versionInput, const uint64_t& nonceInput,
                      const Address& toAddrInput,
                      const PubKey& senderPubKeyInput,
                      const uint128_t& amountInput,
                      const uint128_t& gasPriceInput,
                      const uint64_t& gasLimitInput, const zbytes& codeInput,
                      const zbytes& dataInput)
      : version(versionInput),
        nonce(nonceInput),
        toAddr(toAddrInput),
        senderPubKey(senderPubKeyInput),
        amount(amountInput),
        gasPrice(gasPriceInput),
        gasLimit(gasLimitInput),
        code(codeInput),
        data(dataInput) {}

  uint32_t version{};
  uint64_t nonce{};  // counter: the number of tx from m_fromAddr
  Address toAddr;
  PubKey senderPubKey;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit{};
  zbytes code;
  zbytes data;
};

/// Stores information on a single transaction.
class Transaction : public SerializableDataBlock {
  TxnHash m_tranID;
  TransactionCoreInfo m_coreInfo;
  Signature m_signature;

  bool IsSignedECDSA() const;
  bool SetHash(const zbytes& txnData);

 public:
  /// Default constructor.
  Transaction();

  /// Constructor with specified transaction fields.
  Transaction(const uint32_t& version, const uint64_t& nonce,
              const Address& toAddr, const PairOfKey& senderKeyPair,
              const uint128_t& amount, const uint128_t& gasPrice,
              const uint64_t& gasLimit, const zbytes& code = {},
              const zbytes& data = {});

  /// Constructor with specified transaction fields.
  Transaction(const TxnHash& tranID, const uint32_t& version,
              const uint64_t& nonce, const Address& toAddr,
              const PubKey& senderPubKey, const uint128_t& amount,
              const uint128_t& gasPrice, const uint64_t& gasLimit,
              const zbytes& code, const zbytes& data,
              const Signature& signature);

  /// Constructor with specified transaction fields.
  Transaction(const uint32_t& version, const uint64_t& nonce,
              const Address& toAddr, const PubKey& senderPubKey,
              const uint128_t& amount, const uint128_t& gasPrice,
              const uint64_t& gasLimit, const zbytes& code, const zbytes& data,
              const Signature& signature);

  /// Constructor with core information.
  Transaction(const TxnHash& tranID, const TransactionCoreInfo& coreInfo,
              const Signature& signature);

  /// Constructor for loading transaction information from a byte stream.
  Transaction(const zbytes& src, unsigned int offset);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const override;

  bool SerializeCoreFields(zbytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

  /// Returns the transaction ID.
  const TxnHash& GetTranID() const;

  /// Returns the core information of transaction
  const TransactionCoreInfo& GetCoreInfo() const;

  /// Returns the current version.
  uint32_t GetVersion() const;

  /// Returns the current version identifier (zil or eth).
  uint32_t GetVersionIdentifier() const;

  bool IsEth() const;

  /// Returns whether the current version is correct
  bool VersionCorrect() const;

  bool IsSigned(zbytes const& txnData) const;

  /// Returns the transaction nonce.
  /// There is an edge case for Eth nonces,
  /// See PR #2995
  const uint64_t& GetNonce() const;

  /// Returns the transaction destination account address.
  const Address& GetToAddr() const;

  //// Returns the sender's Public Key.
  const PubKey& GetSenderPubKey() const;

  /// Returns the sender's Address
  Address GetSenderAddr() const;

  /// Returns the transaction amount in Qa.
  const uint128_t GetAmountQa() const;

  /// Returns the transaction amount in Wei.
  const uint128_t GetAmountWei() const;

  /// Returns the transaction amount in raw units regardless of Qa or Wei.
  const uint128_t& GetAmountRaw() const;

  /// Returns the gas price in Qa.
  const uint128_t GetGasPriceQa() const;

  /// Returns the gas price in Wei.
  const uint128_t GetGasPriceWei() const;

  /// Returns the gas price in raw uints regadless of Qa or Wei.
  const uint128_t& GetGasPriceRaw() const;

  /// Returns the normalized to ZIL gas limit
  uint64_t GetGasLimitZil() const;

  /// Returns gas limit received from API.
  uint64_t GetGasLimitRaw() const;

  /// Returns gas limit used by ETH.
  uint64_t GetGasLimitEth() const;

  /// Returns the code.
  const zbytes& GetCode() const;

  /// Returns the data.
  const zbytes& GetData() const;

  /// Returns the EC-Schnorr signature over the transaction data.
  const Signature& GetSignature() const;

  unsigned int GetShardIndex(unsigned int numShards) const;

  /// Set the signature
  void SetSignature(const Signature& signature);

  /// Identifies the shard number that should process the transaction.
  static unsigned int GetShardIndex(const Address& fromAddr,
                                    unsigned int numShards);

  enum ContractType {
    NON_CONTRACT = 0,
    CONTRACT_CREATION,
    CONTRACT_CALL,
    ERROR
  };

  static ContractType GetTransactionType(const Transaction& tx) {
    auto const nullAddr = IsNullAddress(tx.GetToAddr());

    LOG_GENERAL(INFO, "RRW2: Data Empty? " << tx.GetData().empty() <<
                "Code empty?" << tx.GetCode().empty() << " nulladdr?  " << nullAddr);
    if ((!tx.GetData().empty() && !nullAddr) && tx.GetCode().empty()) {
      return CONTRACT_CALL;
    }

    if (!tx.GetCode().empty() && nullAddr) {
      return CONTRACT_CREATION;
    }

    if ((tx.GetData().empty() && !nullAddr) && tx.GetCode().empty()) {
      return NON_CONTRACT;
    }

    return ERROR;
  }

  static bool Verify(const Transaction& tx);

  /// Equality comparison operator.
  bool operator==(const Transaction& tran) const;

  /// Less-than comparison operator.
  bool operator<(const Transaction& tran) const;

  /// Greater-than comparison operator.
  bool operator>(const Transaction& tran) const;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTION_H_
