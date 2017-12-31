/**
* Copyright (c) 2017 Zilliqa 
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
#include <cassert>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "Address.h"
#include "common/Serializable.h"
#include "common/Constants.h"
#include "depends/common/FixedHash.h"

using TxnHash = dev::h256;

/// Stores information on a single transaction.
class Transaction : public Serializable
{
    TxnHash m_tranID;
    uint32_t m_version;
    boost::multiprecision::uint256_t m_nonce; // counter: the number of tx from m_fromAddr
    Address m_toAddr;
    Address m_fromAddr;
    boost::multiprecision::uint256_t m_amount;
    std::array<unsigned char, TRAN_SIG_SIZE> m_signature;

public:

    /// Default constructor.
    Transaction();

    /// Constructor with specified transaction fields.
    Transaction
    (
        uint32_t version,
        const boost::multiprecision::uint256_t & nonce,
        const Address & toAddr,
        const Address & fromAddr,
        const boost::multiprecision::uint256_t & amount,
        const std::array<unsigned char, TRAN_SIG_SIZE> & signature
    );

    /// Constructor for loading transaction information from a byte stream.
    Transaction(const std::vector<unsigned char> & src, unsigned int offset);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char> & dst, unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    void Deserialize(const std::vector<unsigned char> & src, unsigned int offset);

    /// Returns the size in bytes when serializing the transaction.
    static unsigned int GetSerializedSize();

    /// Returns the transaction ID.
    const TxnHash & GetTranID() const;

    /// Returns the current version.
    const uint32_t & GetVersion() const;

    /// Returns the transaction nonce.
    const boost::multiprecision::uint256_t & GetNonce() const;

    /// Returns the transaction destination account address.
    const Address & GetToAddr() const;

    /// Returns the transaction source account address.
    const Address & GetFromAddr() const;

    /// Returns the transaction amount.
    const boost::multiprecision::uint256_t & GetAmount() const;

    /// Returns the EC-Schnorr signature over the transaction data.
    const std::array<unsigned char, TRAN_SIG_SIZE> & GetSignature() const;

    /// Identifies the shard number that should process the transaction.
    static unsigned int GetShardIndex(const Address & fromAddr, unsigned int numShards);

    /// Equality comparison operator.
    bool operator==(const Transaction & tran) const;

    /// Less-than comparison operator.
    bool operator<(const Transaction & tran) const;

    /// Greater-than comparison operator.
    bool operator>(const Transaction & tran) const;

    /// Assignment operator.
    Transaction & operator=(const Transaction & src);
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