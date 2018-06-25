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

#include <type_traits>

#include "libUtils/Logger.h"

template<class MAP> AccountStoreBase<MAP>::AccountStoreBase()
{
    m_addressToAccount = make_shared<MAP>();
}

template<class MAP> void AccountStoreBase<MAP>::Init()
{
    m_addressToAccount->clear();
}

template<class MAP>
unsigned int AccountStoreBase<MAP>::Serialize(vector<unsigned char>& dst,
                                              unsigned int offset) const
{
    // [Total number of accounts (uint256_t)] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]

    // LOG_MARKER();

    unsigned int size_needed = UINT256_SIZE;
    unsigned int size_remaining = dst.size() - offset;
    unsigned int totalSerializedSize = size_needed;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    // [Total number of accounts]
    LOG_GENERAL(
        INFO,
        "Debug: Total number of accounts to serialize: " << GetNumOfAccounts());
    uint256_t totalNumOfAccounts = GetNumOfAccounts();
    SetNumber<uint256_t>(dst, curOffset, totalNumOfAccounts, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    vector<unsigned char> address_vec;
    // [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    for (auto entry : *m_addressToAccount)
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

template<class MAP>
int AccountStoreBase<MAP>::Deserialize(const vector<unsigned char>& src,
                                       unsigned int offset)
{
    // [Total number of accounts] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    // LOG_MARKER();

    try
    {
        unsigned int curOffset = offset;
        uint256_t totalNumOfAccounts
            = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;

        Address address;
        Account account;
        unsigned int numberOfAccountDeserialze = 0;
        while (numberOfAccountDeserialze < totalNumOfAccounts)
        {
            numberOfAccountDeserialze++;

            // Deserialize address
            copy(src.begin() + curOffset,
                 src.begin() + curOffset + ACC_ADDR_SIZE,
                 address.asArray().begin());
            curOffset += ACC_ADDR_SIZE;

            // Deserialize account
            // account.Deserialize(src, curOffset);
            if (account.DeserializeAddOffset(src, curOffset) < 0)
            {
                LOG_GENERAL(WARNING, "We failed to init account.");
                return -1;
            }
            (*m_addressToAccount)[address] = account;
        }
        PrintAccountState();
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStoreBase::Deserialize." << ' '
                                                                << e.what());
        return -1;
    }
    return 0;
}

template<class MAP>
bool AccountStoreBase<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                           const Transaction& transaction)
{
    const PubKey& senderPubKey = transaction.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    Address toAddr = transaction.GetToAddr();
    const uint256_t& amount = transaction.GetAmount();

    Account* fromAccount = this->GetAccount(fromAddr);
    if (fromAccount == nullptr)
    {
        // FIXME: remove this, temporary way to test transactions, should return false
        LOG_GENERAL(WARNING,
                    "AddAccount... FIXME: remove this, temporary way to "
                    "test transactions, should return false in the future");
        this->AddAccount(fromAddr, {10000000000, 0});
        fromAccount = this->GetAccount(fromAddr);
        // return false;
    }

    if (transaction.GetGasLimit() < NORMAL_TRAN_GAS)
    {
        LOG_GENERAL(WARNING,
                    "The gas limit "
                        << transaction.GetGasLimit()
                        << " should be larger than the normal transaction gas ("
                        << NORMAL_TRAN_GAS << ")");
        return false;
    }

    // FIXME: Possible integer overflow here
    uint256_t gasDeposit
        = transaction.GetGasLimit() * transaction.GetGasPrice();

    if (fromAccount->GetBalance() < transaction.GetAmount() + gasDeposit)
    {
        LOG_GENERAL(
            WARNING,
            "The account (balance: "
                << fromAccount->GetBalance()
                << ") "
                   "doesn't have enough balance to pay for the gas limit ("
                << gasDeposit
                << ") "
                   "with amount ("
                << transaction.GetAmount() << ") in the transaction");
        return false;
    }

    if (!DecreaseBalance(fromAddr, gasDeposit))
    {
        return false;
    }

    if (!TransferBalance(fromAddr, toAddr, amount))
    {
        IncreaseBalance(fromAddr, gasDeposit);
        return false;
    }

    // FIXME: Possible integer overflow
    IncreaseBalance(fromAddr,
                    gasDeposit - NORMAL_TRAN_GAS * transaction.GetGasPrice());

    IncreaseNonce(fromAddr);

    return true;
}

template<class MAP>
bool AccountStoreBase<MAP>::IsAccountExist(const Address& address)
{
    LOG_MARKER();
    return (nullptr != GetAccount(address));
}

template<class MAP>
void AccountStoreBase<MAP>::AddAccount(const Address& address,
                                       const Account& account)
{
    LOG_MARKER();

    if (!IsAccountExist(address))
    {
        m_addressToAccount->insert(make_pair(address, account));
        // UpdateStateTrie(address, account);
    }
}

template<class MAP>
void AccountStoreBase<MAP>::AddAccount(const PubKey& pubKey,
                                       const Account& account)
{
    AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

template<class MAP>
Account* AccountStoreBase<MAP>::GetAccount(const Address& address)
{
    auto it = m_addressToAccount->find(address);
    if (it != m_addressToAccount->end())
    {
        return &it->second;
    }
    return nullptr;
}

template<class MAP> uint256_t AccountStoreBase<MAP>::GetNumOfAccounts() const
{
    LOG_MARKER();
    return m_addressToAccount->size();
}

template<class MAP>
bool AccountStoreBase<MAP>::IncreaseBalance(const Address& address,
                                            const uint256_t& delta)
{
    // LOG_MARKER();

    if (delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    LOG_GENERAL(INFO, "address: " << address);

    if (account != nullptr && account->IncreaseBalance(delta))
    {
        // UpdateStateTrie(address, *account);
        LOG_GENERAL(INFO, "account: " << *account);
        return true;
    }
    // FIXME: remove this, temporary way to test transactions, should return false
    else if (account == nullptr)
    {
        LOG_GENERAL(WARNING,
                    "AddAccount... FIXME: remove this, temporary way to test "
                    "transactions");
        AddAccount(address, {delta, 0});
        return true;
    }

    return false;
}

template<class MAP>
bool AccountStoreBase<MAP>::DecreaseBalance(const Address& address,
                                            const uint256_t& delta)
{
    // LOG_MARKER();

    if (delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    LOG_GENERAL(INFO, "address: " << address);
    LOG_GENERAL(INFO, "account: " << *account);

    // FIXME: remove this, temporary way to test transactions, should return false
    if (nullptr == account)
    {
        LOG_GENERAL(WARNING,
                    "AddAccount... FIXME: remove this, temporary way to test "
                    "transactions");
        AddAccount(address, {10000000000, 0});
        return true;
    }

    return account->DecreaseBalance(delta);
}

template<class MAP>
bool AccountStoreBase<MAP>::TransferBalance(const Address& from,
                                            const Address& to,
                                            const uint256_t& delta)
{
    // LOG_MARKER();

    if (DecreaseBalance(from, delta) && IncreaseBalance(to, delta))
    {
        return true;
    }

    return false;
}

template<class MAP>
uint256_t AccountStoreBase<MAP>::GetBalance(const Address& address)
{
    LOG_MARKER();

    const Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetBalance();
    }

    return 0;
}

template<class MAP>
bool AccountStoreBase<MAP>::IncreaseNonce(const Address& address)
{
    LOG_MARKER();

    Account* account = GetAccount(address);

    LOG_GENERAL(INFO, "address: " << address << " account: " << *account);

    if (nullptr == account)
    {
        LOG_GENERAL(INFO, "Increase nonce failed");

        return false;
    }

    if (account->IncreaseNonce())
    {
        LOG_GENERAL(INFO, "Increase nonce done");
        // UpdateStateTrie(address, *account);
        return true;
    }
    else
    {
        LOG_GENERAL(INFO, "Increase nonce failed");
        return false;
    }
}

template<class MAP>
uint256_t AccountStoreBase<MAP>::GetNonce(const Address& address)
{
    //LOG_MARKER();

    Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetNonce();
    }

    return 0;
}

template<class MAP> void AccountStoreBase<MAP>::PrintAccountState()
{
    LOG_GENERAL(INFO, "Printing Account State");
    for (auto entry : *m_addressToAccount)
    {
        LOG_GENERAL(INFO, entry.first << " " << entry.second);
    }
}
