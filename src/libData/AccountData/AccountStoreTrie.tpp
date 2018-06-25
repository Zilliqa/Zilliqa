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

#include "libPersistence/ContractStorage.h"

template<class DB, class MAP>
AccountStoreTrie<DB, MAP>::AccountStoreTrie()
    : m_db(is_same<DB, OverlayDB>::value ? "state" : "")
{
    m_state = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address>(&m_db);
}

template<class DB, class MAP> void AccountStoreTrie<DB, MAP>::Init()
{
    AccountStoreSC<MAP>::Init();
    m_state.init();
    prevRoot = m_state.root();
}

template<class DB, class MAP>
Account* AccountStoreTrie<DB, MAP>::GetAccount(const Address& address)
{
    Account* account = AccountStoreBase<MAP>::GetAccount(address);
    if (account != nullptr)
    {
        return account;
    }

    string accountDataString = m_state.at(address);
    if (accountDataString.empty())
    {
        return nullptr;
    }

    dev::RLP accountDataRLP(accountDataString);
    if (accountDataRLP.itemCount() != 4)
    {
        LOG_GENERAL(WARNING, "Account data corrupted");
        return nullptr;
    }

    auto it2 = this->m_addressToAccount->emplace(
        std::piecewise_construct, std::forward_as_tuple(address),
        std::forward_as_tuple(accountDataRLP[0].toInt<uint256_t>(),
                              accountDataRLP[1].toInt<uint256_t>()));

    // Code Hash
    if (accountDataRLP[3].toHash<h256>() != h256())
    {
        // Extract Code Content
        it2.first->second.SetCode(
            ContractStorage::GetContractStorage().GetContractCode(address));
        if (accountDataRLP[3].toHash<h256>() != it2.first->second.GetCodeHash())
        {
            LOG_GENERAL(WARNING, "Account Code Content doesn't match Code Hash")
            this->m_addressToAccount->erase(it2.first);
            return nullptr;
        }
        // Storage Root
        it2.first->second.SetStorageRoot(accountDataRLP[2].toHash<h256>());
    }

    return &it2.first->second;
}

template<class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrie(const Address& address,
                                                const Account& account)
{
    //LOG_MARKER();

    dev::RLPStream rlpStream(4);
    rlpStream << account.GetBalance() << account.GetNonce()
              << account.GetStorageRoot() << account.GetCodeHash();
    m_state.insert(address, &rlpStream.out());

    return true;
}

template<class DB, class MAP>
h256 AccountStoreTrie<DB, MAP>::GetStateRootHash() const
{
    LOG_MARKER();

    return m_state.root();
}

template<class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrieAll()
{
    for (auto const& entry : *(this->m_addressToAccount))
    {
        if (!UpdateStateTrie(entry.first, entry.second))
        {
            return false;
        }
    }

    return true;
}

template<class DB, class MAP>
void AccountStoreTrie<DB, MAP>::RepopulateStateTrie()
{
    LOG_MARKER();
    m_state.init();
    prevRoot = m_state.root();
    UpdateStateTrieAll();
}

template<class DB, class MAP>
void AccountStoreTrie<DB, MAP>::PrintAccountState()
{
    AccountStoreBase<MAP>::PrintAccountState();
    LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}
