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

#include <boost/filesystem.hpp>
#include <leveldb/db.h>

#include "AccountStore.h"
#include "Address.h"
#include "depends/common/RLP.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

AccountStore::AccountStore() : m_db("state")
{
    m_state = SecureTrieDB<Address, dev::OverlayDB>(&m_db);
    m_state.init();
    prevRoot = m_state.root();
}

AccountStore::~AccountStore()
{
    // boost::filesystem::remove_all("./state");
}

AccountStore & AccountStore::GetInstance()
{
    static AccountStore accountstore;
    return accountstore;
}

bool AccountStore::DoesAccountExist(const Address & address)
{
    if(GetAccount(address) != nullptr)
    {
        return true;
    }

    return false;
}

void AccountStore::AddAccount(const Address & address, const Account & account)
{
    if (!DoesAccountExist(address))
    {
        m_addressToAccount.insert(make_pair(address, account));
        UpdateStateTrie(address, account);
    }
}

void AccountStore::AddAccount(const PubKey & pubKey, const Account & account)
{
    Address address = Account::GetAddressFromPublicKey(pubKey);

    if (!DoesAccountExist(address))
    {
        m_addressToAccount.insert(make_pair(address, account));
        UpdateStateTrie(address, account);
    }
}

void AccountStore::AddAccount(const Address & address, 
                              const uint256_t & balance, 
                              const uint256_t & nonce)
{
    if (!DoesAccountExist(address))
    {
        Account account(balance, nonce);
        m_addressToAccount.insert(make_pair(address, account));
        UpdateStateTrie(address, account);
    }
}

void AccountStore::AddAccount(const PubKey & pubKey, 
                              const uint256_t & balance, 
                              const uint256_t & nonce)
{
    Address address = Account::GetAddressFromPublicKey(pubKey);

    if (!DoesAccountExist(address))
    {
        Account account(balance, nonce);
        m_addressToAccount.insert(make_pair(address, account));
        UpdateStateTrie(address, account);
    }
}

void AccountStore::UpdateAccounts(const Transaction & transaction)
{
    const Address & fromAddr = transaction.GetFromAddr();
    const Address & toAddr = transaction.GetToAddr();
    const uint256_t & amount = transaction.GetAmount();

    TransferBalance(fromAddr, toAddr, amount);
}

Account* AccountStore::GetAccount(const Address & address)
{
    auto it = m_addressToAccount.find(address);
    LOG_MESSAGE((it != m_addressToAccount.end()));
    if(it != m_addressToAccount.end())
    {
        return &it->second;
    }

    string accountDataString = m_state.at(address);
    if (accountDataString.empty())
    {
        return nullptr;
    }

    dev::RLP accountDataRLP(accountDataString);
    auto it2 = m_addressToAccount.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(address),
        std::forward_as_tuple(accountDataRLP[0].toInt<boost::multiprecision::uint256_t>(), 
                              accountDataRLP[1].toInt<boost::multiprecision::uint256_t>())
    );

    return &it2.first->second;
}

bool AccountStore::UpdateStateTrie(const Address & address, const Account & account) 
{
    dev::RLPStream rlpStream(2);
    rlpStream << account.GetBalance() << account.GetNonce();
    m_state.insert(address, &rlpStream.out());

    return true;
}

bool AccountStore::IncreaseBalance(const Address & address, 
                                   const boost::multiprecision::uint256_t & delta)
{
    if(delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    if(account != nullptr && account->IncreaseBalance(delta))
    {
        UpdateStateTrie(address, *account);
        return true;
    }
    
    return false;
}

bool AccountStore::DecreaseBalance(const Address & address, 
                                   const boost::multiprecision::uint256_t & delta)
{
    if(delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    if(account != nullptr && account->DecreaseBalance(delta))
    {
        UpdateStateTrie(address, *account);
        return true;
    }
    
    return false;
}

bool AccountStore::TransferBalance(const Address & from, 
                                   const Address & to, 
                                   const boost::multiprecision::uint256_t & delta)
{
    if(DecreaseBalance(from, delta) && IncreaseBalance(to, delta))
    {
        return true;
    }
    
    return false;
}

boost::multiprecision::uint256_t AccountStore::GetBalance(const Address & address)
{
    const Account* account = GetAccount(address);

    if(account != nullptr)
    {
        return account->GetBalance();
    }
    
    return 0;
}

bool AccountStore::IncreaseNonce(const Address & address)
{
    Account* account = GetAccount(address);

    if(account != nullptr && account->IncreaseNonce())
    {
        UpdateStateTrie(address, *account);
        return true;
    }

    return false;
}

boost::multiprecision::uint256_t AccountStore::GetNonce(const Address & address)
{
    Account* account = GetAccount(address);

    if(account != nullptr)
    {
        return account->GetNonce();
    }
    
    return 0;    
}

dev::h256 AccountStore::GetStateRootHash() const
{
    return m_state.root();
}

void AccountStore::MoveUpdatesToDisk()
{
    m_state.db()->commit();
    prevRoot = m_state.root();
    // m_state.init();
}

void AccountStore::DiscardUnsavedUpdates()
{
    m_state.db()->rollback();
    m_state.setRoot(prevRoot);
    m_addressToAccount.clear();
    // m_state.init();
}