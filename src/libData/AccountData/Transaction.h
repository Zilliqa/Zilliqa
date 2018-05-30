/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__

#include <array>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"

using TxnHash = dev::h256;
using KeyPair = std::pair<PrivKey, PubKey>;

/// Stores information on a single transaction.
class Transaction : public Serializable
{
    TxnHash m_tranID;
    boost::multiprecision::uint256_t m_version;
    boost::multiprecision::uint256_t
        m_nonce; // counter: the number of tx from m_fromAddr
    Address m_toAddr;
    PubKey m_senderPubKey;
    boost::multiprecision::uint256_t m_amount;
    boost::multiprecision::uint256_t m_gasPrice;
    boost::multiprecision::uint256_t m_gasLimit;
    std::vector<unsigned char> m_code;
    std::vector<unsigned char> m_data;
    Signature m_signature;

public:
    /// Default constructor.
    Transaction();

    /// Copy constructor.
    Transaction(const Transaction& src);

    /// Constructor with specified transaction fields.
    Transaction(boost::multiprecision::uint256_t version,
                const boost::multiprecision::uint256_t& nonce,
                const Address& toAddr, const KeyPair& senderKeyPair,
                const boost::multiprecision::uint256_t& amount,
                const boost::multiprecision::uint256_t& gasPrice,
                const boost::multiprecision::uint256_t& gasLimit,
                const std::vector<unsigned char>& code,
                const std::vector<unsigned char>& data);

    /// Constructor with specified transaction fields.
    Transaction(boost::multiprecision::uint256_t version,
                const boost::multiprecision::uint256_t& nonce,
                const Address& toAddr, const PubKey& senderPubKey,
                const boost::multiprecision::uint256_t& amount,
                const boost::multiprecision::uint256_t& gasPrice,
                const boost::multiprecision::uint256_t& gasLimit,
                const std::vector<unsigned char>& code,
                const std::vector<unsigned char>& data,
                const Signature& signature);

    /// Constructor for loading transaction information from a byte stream.
    Transaction(const std::vector<unsigned char>& src, unsigned int offset);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    unsigned int SerializeCoreFields(std::vector<unsigned char>& dst,
                                     unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Returns the size in bytes when serializing the transaction.
    unsigned int GetSerializedSize();

    /// Return the size of static typed variables for a minimum size check
    static unsigned int GetMinSerializedSize();

    /// Returns the transaction ID.
    const TxnHash& GetTranID() const;

    /// Returns the current version.
    const boost::multiprecision::uint256_t& GetVersion() const;

    /// Returns the transaction nonce.
    const boost::multiprecision::uint256_t& GetNonce() const;

    /// Returns the transaction destination account address.
    const Address& GetToAddr() const;

    //// Returns the sender's Public Key.
    const PubKey& GetSenderPubKey() const;

    /// Returns the transaction amount.
    const boost::multiprecision::uint256_t& GetAmount() const;

    /// Returns the gas price.
    const boost::multiprecision::uint256_t& GetGasPrice() const;

    /// Returns the gas limit.
    const boost::multiprecision::uint256_t& GetGasLimit() const;

    /// Returns the code.
    const std::vector<unsigned char>& GetCode() const;

    /// Returns the data.
    const std::vector<unsigned char>& GetData() const;

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

#endif // __TRANSACTION_H__

#if 0

// ========================================================
// The Predicate class facilitates conditional transactions
// ========================================================

// Each Predicate can specify a condition on the balance of 
//   an account, or a condition on the existence of a prior
//   transaction (A->B, amount), or both

class Predicate: public Serializable
{
    uint8_t m_type; // 6 high bits reserved; bit0 (lowest): account condition active; bit1: transaction condition active
    std::array<unsigned char, ACC_ADDR_SIZE> m_accConAddr; // for account condition: account address
    uint8_t m_ops; // 4 high bits for account condition: account balance comparison operator: 
                         //   000: ==, 001: !=, 010: >, 011: >=, 100: <, 101: <=
                         // 4 low bits for tx cnodition:
                         //   comparison operator. 0: on blockchain, 1: not on blockchain
    boost::multiprecision::uint256_t m_accConBalance; // for account condition: balance specified
    std::array<unsigned char, ACC_ADDR_SIZE> m_txConToAddr; // for transaction condition: to account address
    std::array<unsigned char, ACC_ADDR_SIZE> m_txConFromAddr; // for transaction condition: from account address
    boost::multiprecision::uint256_t m_txConAmount; // for transaction condition: the amount

public:
    unsigned int Serialize(std::vector<unsigned char> & dst, unsigned int offset) const;
    void Deserialize(const std::vector<unsigned char> & src, unsigned int offset);

    Predicate();

    Predicate(const std::vector<unsigned char> & src, unsigned int offset);

    Predicate(uint8_t type, const std::array<unsigned char, ACC_ADDR_SIZE> & accConAddr, unsigned char ops, boost::multiprecision::uint256_t accConBalance, const std::array<unsigned char, ACC_ADDR_SIZE> & txConToAddr, const std::array<unsigned char, ACC_ADDR_SIZE> & txConFromAddr, boost::multiprecision::uint256_t txConAmount);

    Predicate(uint8_t type, const std::array<unsigned char, ACC_ADDR_SIZE> & accConAddr, unsigned char accConOp, boost::multiprecision::uint256_t accConBalance, const std::array<unsigned char, ACC_ADDR_SIZE> & txConToAddr, const std::array<unsigned char, ACC_ADDR_SIZE> & txConFromAddr, boost::multiprecision::uint256_t txConAmount, unsigned char txConOp);

    uint8_t GetType() const;

    const std::array<unsigned char, ACC_ADDR_SIZE> & GetAccConAddr() const;

    uint8_t GetAccConOp() const;

    boost::multiprecision::uint256_t GetAccConBalance() const;

    const std::array<unsigned char, ACC_ADDR_SIZE> & GetTxConToAddr() const;

    const std::array<unsigned char, ACC_ADDR_SIZE> & GetTxConFromAddr() const;

    boost::multiprecision::uint256_t GetTxConAmount() const;

    unsigned char GetTxConOp() const;

    bool operator==(const Predicate & pred) const;

    bool operator<(const Predicate & pred) const;

    bool operator>(const Predicate & pred) const;
};

#endif