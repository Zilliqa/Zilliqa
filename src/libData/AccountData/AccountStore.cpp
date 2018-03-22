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

unsigned int AccountStore::Serialize(vector<unsigned char> & dst, unsigned int offset) const
{
    // [Total number of accounts (uint256_t)] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]

    LOG_MARKER();

    unsigned int size_needed = UINT256_SIZE;
    unsigned int size_remaining = dst.size() - offset;
    unsigned int totalSerializedSize = size_needed; 

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    // [Total number of accounts]
    LOG_MESSAGE("Debug: Total number of accounts to serialize: " << GetNumOfAccounts()); 
    uint256_t totalNumOfAccounts = GetNumOfAccounts();
    SetNumber<uint256_t>(dst, curOffset, totalNumOfAccounts, UINT256_SIZE);
    curOffset += UINT256_SIZE; 

    vector<unsigned char> address_vec; 
    // [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    for(auto entry: m_addressToAccount)
    {
        // Address
        address_vec = entry.first.asBytes();

        copy(address_vec.begin(), address_vec.end(), std::back_inserter(dst));
        curOffset += ACC_ADDR_SIZE;
        totalSerializedSize += ACC_ADDR_SIZE; 

        // Account 
        size_needed = entry.second.Serialize(dst, curOffset);
        curOffset += size_needed; 
        totalSerializedSize += size_needed; 
    }

    return totalSerializedSize; 

}

void AccountStore::Deserialize(const vector<unsigned char> & src, unsigned int offset)
{
    // [Total number of accounts] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    LOG_MARKER();

    unsigned int curOffset = offset;
    uint256_t totalNumOfAccounts = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    Address address; 
    Account account; 
    unsigned int numberOfAccountDeserialze = 0;
    while(numberOfAccountDeserialze < totalNumOfAccounts)
    {
        numberOfAccountDeserialze++; 

        // Deserialize address
        copy(src.begin() + curOffset, src.begin() + curOffset + ACC_ADDR_SIZE, address.asArray().begin());
        curOffset += ACC_ADDR_SIZE;

        // Deserialize account
        account.Deserialize(src, curOffset);
        curOffset += ACCOUNT_SIZE; 

        m_addressToAccount.insert(make_pair(address, account));
	UpdateStateTrie(address, account);
    }
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

        // LOG_MESSAGE("Account " << address << " with balance " << balance << ", nonce " << nonce << 
        //              " created");
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
    const PubKey & senderPubKey = transaction.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    const Address & toAddr = transaction.GetToAddr();
    const uint256_t & amount = transaction.GetAmount();

    TransferBalance(fromAddr, toAddr, amount);
}

Account* AccountStore::GetAccount(const Address & address)
{
    auto it = m_addressToAccount.find(address);
    // LOG_MESSAGE((it != m_addressToAccount.end()));
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


uint256_t AccountStore::GetNumOfAccounts() const
{
    LOG_MARKER(); 
    return m_addressToAccount.size();
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
        LOG_MESSAGE("Balance for " << address << " increased by " << delta << ". Succeeded!");
        return true;
    }
    else if(account == nullptr)
    {
        AddAccount(address, delta, 0);
        LOG_MESSAGE("Balance for " << address << " increased by " << delta << ". Succeeded!");
        return true;
    }

    LOG_MESSAGE("Balance for " << address << " increased by " << delta << ". Failed!");
    
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
        LOG_MESSAGE("Balance for " << address << " decreased by " << delta << ". Succeeded! " <<
                    "New balance: " << account->GetBalance());
        return true;
    }
    // TODO: remove this, temporary way to test transactions
    else if(account == nullptr)
    {
        AddAccount(address, 10000000000, 0);
        LOG_MESSAGE("Balance for " << address << " decreased by " << delta << ". Succeeded! " <<
                    "New balance: " << GetAccount(address)->GetBalance());
        return true;
    }

    LOG_MESSAGE("Balance for " << address << " decreased by " << delta << ". Failed! Balance: " <<
                (account ? account->GetBalance().convert_to<string>() : "? account = nullptr"));

    return false;
}

bool AccountStore::TransferBalance(const Address & from, 
                                   const Address & to, 
                                   const boost::multiprecision::uint256_t & delta)
{
    if(DecreaseBalance(from, delta) && IncreaseBalance(to, delta))
    {
        LOG_MESSAGE("Transfer of " << delta << " from " << from << " to " << to << " succeeded");
        return true;
    }

    LOG_MESSAGE("Transfer of " << delta << " from " << from << " to " << to << " failed");

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
    LOG_MARKER(); 

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

void AccountStore::PrintAccountState()
{
    LOG_MESSAGE("Printing Account State");
    for(auto entry: m_addressToAccount)
    {
        LOG_MESSAGE(entry.first << " " << entry.second);
    }
}
