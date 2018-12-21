/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__

#include <array>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"

using TxnHash = dev::h256;
using KeyPair = std::pair<PrivKey, PubKey>;

struct TransactionCoreInfo {
  TransactionCoreInfo() = default;
  TransactionCoreInfo(const uint32_t& versionInput, const uint64_t& nonceInput,
                      const Address& toAddrInput,
                      const PubKey& senderPubKeyInput,
                      const boost::multiprecision::uint128_t& amountInput,
                      const boost::multiprecision::uint128_t& gasPriceInput,
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
  boost::multiprecision::uint128_t amount;
  boost::multiprecision::uint128_t gasPrice;
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
              const Address& toAddr, const KeyPair& senderKeyPair,
              const boost::multiprecision::uint128_t& amount,
              const boost::multiprecision::uint128_t& gasPrice,
              const uint64_t& gasLimit, const bytes& code = {},
              const bytes& data = {});

  /// Constructor with specified transaction fields.
  Transaction(const TxnHash& tranID, const uint32_t& version,
              const uint64_t& nonce, const Address& toAddr,
              const PubKey& senderPubKey,
              const boost::multiprecision::uint128_t& amount,
              const boost::multiprecision::uint128_t& gasPrice,
              const uint64_t& gasLimit, const bytes& code, const bytes& data,
              const Signature& signature);

  /// Constructor with specified transaction fields.
  Transaction(const uint32_t& version, const uint64_t& nonce,
              const Address& toAddr, const PubKey& senderPubKey,
              const boost::multiprecision::uint128_t& amount,
              const boost::multiprecision::uint128_t& gasPrice,
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
  const boost::multiprecision::uint128_t& GetAmount() const;

  /// Returns the gas price.
  const boost::multiprecision::uint128_t& GetGasPrice() const;

  /// Returns the gas limit.
  const uint64_t& GetGasLimit() const;

  /// Returns the code.
  const bytes& GetCode() const;

  /// Returns the data.
  const bytes& GetData() const;

  /// Returns the EC-Schnorr signature over the transaction data.
  const Signature& GetSignature() const;

  /// Set the signature
  void SetSignature(const Signature& signature);

  /// Identifies the shard number that should process the transaction.
  static unsigned int GetShardIndex(const Address& fromAddr,
                                    unsigned int numShards);

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
