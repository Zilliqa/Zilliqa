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

#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__

#include <array>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"

using TxnHash = dev::h256;

struct TransactionCoreInfo {
  TransactionCoreInfo() = default;
  TransactionCoreInfo(const uint32_t& versionInput, const uint64_t& nonceInput,
                      const Address& toAddrInput,
                      const PubKey& senderPubKeyInput,
                      const uint128_t& amountInput,
                      const uint128_t& gasPriceInput,
                      const uint64_t& gasLimitInput, const bytes& codeInput,
                      const bytes& dataInput)
      : version(versionInput),
        nonce(nonceInput),
        toAddr(toAddrInput),
        senderPubKey(senderPubKeyInput),
        amount(amountInput),
        gasPrice(gasPriceInput),
        gasLimit(gasLimitInput),
        code(codeInput),
        data(dataInput) {}

  uint32_t version;
  uint64_t nonce;  // counter: the number of tx from m_fromAddr
  Address toAddr;
  PubKey senderPubKey;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit;
  bytes code;
  bytes data;
};

/// Stores information on a single transaction.
class Transaction : public SerializableDataBlock {
  TxnHash m_tranID;
  TransactionCoreInfo m_coreInfo;
  Signature m_signature;

 public:
  /// Default constructor.
  Transaction();

  /// Copy constructor.
  Transaction(const Transaction& src);

  /// Constructor with specified transaction fields.
  Transaction(const uint32_t& version, const uint64_t& nonce,
              const Address& toAddr, const PairOfKey& senderKeyPair,
              const uint128_t& amount, const uint128_t& gasPrice,
              const uint64_t& gasLimit, const bytes& code = {},
              const bytes& data = {});

  /// Constructor with specified transaction fields.
  Transaction(const TxnHash& tranID, const uint32_t& version,
              const uint64_t& nonce, const Address& toAddr,
              const PubKey& senderPubKey, const uint128_t& amount,
              const uint128_t& gasPrice, const uint64_t& gasLimit,
              const bytes& code, const bytes& data, const Signature& signature);

  /// Constructor with specified transaction fields.
  Transaction(const uint32_t& version, const uint64_t& nonce,
              const Address& toAddr, const PubKey& senderPubKey,
              const uint128_t& amount, const uint128_t& gasPrice,
              const uint64_t& gasLimit, const bytes& code, const bytes& data,
              const Signature& signature);

  /// Constructor with core information.
  Transaction(const TxnHash& tranID, const TransactionCoreInfo coreInfo,
              const Signature& signature);

  /// Constructor for loading transaction information from a byte stream.
  Transaction(const bytes& src, unsigned int offset);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  bool SerializeCoreFields(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

  /// Returns the transaction ID.
  const TxnHash& GetTranID() const;

  /// Returns the core information of transaction
  const TransactionCoreInfo& GetCoreInfo() const;

  /// Returns the current version.
  const uint32_t& GetVersion() const;

  /// Returns the transaction nonce.
  const uint64_t& GetNonce() const;

  /// Returns the transaction destination account address.
  const Address& GetToAddr() const;

  //// Returns the sender's Public Key.
  const PubKey& GetSenderPubKey() const;

  /// Returns the sender's Address
  Address GetSenderAddr() const;

  /// Returns the transaction amount.
  const uint128_t& GetAmount() const;

  /// Returns the gas price.
  const uint128_t& GetGasPrice() const;

  /// Returns the gas limit.
  const uint64_t& GetGasLimit() const;

  /// Returns the code.
  const bytes& GetCode() const;

  /// Returns the data.
  const bytes& GetData() const;

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
    if (!tx.GetData().empty() && tx.GetToAddr() != NullAddress &&
        tx.GetCode().empty()) {
      return CONTRACT_CALL;
    }

    if (!tx.GetCode().empty() && tx.GetToAddr() == NullAddress) {
      return CONTRACT_CREATION;
    }

    if (tx.GetData().empty() && tx.GetToAddr() != NullAddress &&
        tx.GetCode().empty()) {
      return NON_CONTRACT;
    }

    return ERROR;
  }

  /// Equality comparison operator.
  bool operator==(const Transaction& tran) const;

  /// Less-than comparison operator.
  bool operator<(const Transaction& tran) const;

  /// Greater-than comparison operator.
  bool operator>(const Transaction& tran) const;

  /// Assignment operator.
  Transaction& operator=(const Transaction& src);
};

#endif  // __TRANSACTION_H__
