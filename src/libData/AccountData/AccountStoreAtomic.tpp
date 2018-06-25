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

using namespace std;

template<class MAP>
AccountStoreAtomic<MAP>::AccountStoreAtomic(AccountStoreSC<MAP>& parent)
    : m_parent(parent)
{
}

template<class MAP>
Account* AccountStoreAtomic<MAP>::GetAccount(const Address& address)
{
    Account* account
        = AccountStoreBase<unordered_map<Address, Account>>::GetAccount(
            address);
    if (account != nullptr)
    {
        LOG_GENERAL(INFO, "Got From Temp");
        return account;
    }

    account = m_parent.GetAccount(address);
    if (account)
    {
        LOG_GENERAL(INFO, "Got From Parent");
        m_addressToAccount->insert(make_pair(address, *account));
        return &(m_addressToAccount->find(address))->second;
    }

    LOG_GENERAL(INFO, "Got Nullptr");

    return nullptr;
}

template<class MAP>
const shared_ptr<unordered_map<Address, Account>>&
AccountStoreAtomic<MAP>::GetAddressToAccount()
{
    return this->m_addressToAccount;
}
