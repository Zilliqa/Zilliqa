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

#include "AccountStore.h"

using namespace std;

AccountStoreTemp::AccountStoreTemp(AccountStore& parent)
    : m_parent(parent)
{
}

Account* AccountStoreTemp::GetAccount(const Address& address)
{
    Account* account
        = AccountStoreBase<map<Address, Account>>::GetAccount(address);
    if (account != nullptr)
    {
        // LOG_GENERAL(INFO, "Got From Temp");
        return account;
    }

    account = m_parent.GetAccount(address);
    if (account)
    {
        // LOG_GENERAL(INFO, "Got From Parent");
        Account newaccount(*account);
        m_addressToAccount->insert(make_pair(address, newaccount));
        return &(m_addressToAccount->find(address))->second;
    }

    // LOG_GENERAL(INFO, "Got Nullptr");

    return nullptr;
}

const shared_ptr<map<Address, Account>>& AccountStoreTemp::GetAddressToAccount()
{
    return this->m_addressToAccount;
}

int AccountStoreTemp::DeserializeDelta(const vector<unsigned char>& src,
                                       unsigned int offset)
{
    LOG_MARKER();
    // [Total number of acount deltas (uint256_t)] [Addr 1] [AccountDelta 1] [Addr 2] [Account 2] .... [Addr n] [Account n]

    try
    {
        unsigned int curOffset = offset;
        uint256_t totalNumOfAccounts
            = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        LOG_GENERAL(INFO,
                    "Total Number of Accounts Delta: " << totalNumOfAccounts);
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

            // Deserialize accountDelta
            Account* oriAccount = GetAccount(address);
            if (oriAccount == nullptr)
            {
                Account acc(0, 0);
                // LOG_GENERAL(INFO, "Creating new account: " << address);
                AddAccount(address, acc);
            }

            // LOG_GENERAL(INFO, "Diff account: " << address);
            oriAccount = GetAccount(address);
            account = *oriAccount;
            if (Account::DeserializeDelta(src, curOffset, account) < 0)
            {
                LOG_GENERAL(
                    WARNING,
                    "We failed to parse accountDelta for account: " << address);

                continue;
            }
            (*m_addressToAccount)[address] = account;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStoreTemp::Deserialize." << ' '
                                                                << e.what());
        return -1;
    }
    return 0;
}