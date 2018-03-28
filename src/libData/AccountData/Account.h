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

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <vector>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"

/// Stores information on a single account.
class Account : public Serializable
{
    boost::multiprecision::uint256_t m_balance;
    boost::multiprecision::uint256_t m_nonce;

public:
    Account();

    /// Constructor for loading account information from a byte stream.
    Account(const std::vector<unsigned char>& src, unsigned int offset);

    /// Constructor with account balance, and nonce.
    Account(const boost::multiprecision::uint256_t& balance,
            const boost::multiprecision::uint256_t& nonce);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Increases account balance by the specified delta amount.
    bool IncreaseBalance(const boost::multiprecision::uint256_t& delta);

    /// Decreases account balance by the specified delta amount.
    bool DecreaseBalance(const boost::multiprecision::uint256_t& delta);

    /// Returns the account balance.
    const boost::multiprecision::uint256_t& GetBalance() const;

    /// Increases account nonce by 1.
    bool IncreaseNonce();

    /// Returns the account nonce.
    const boost::multiprecision::uint256_t& GetNonce() const;

    /// Computes an account address from a specified PubKey.
    static Address GetAddressFromPublicKey(const PubKey& pubKey);

    friend inline std::ostream& operator<<(std::ostream& _out,
                                           Account const& account);

    bool operator==(const Account& rhs) const
    {
        return m_balance == rhs.GetBalance() && m_nonce == rhs.GetNonce();
    }
};

inline std::ostream& operator<<(std::ostream& _out, Account const& account)
{
    _out << account.m_balance << " " << account.m_nonce;
    return _out;
}

#endif // __ACCOUNT_H__