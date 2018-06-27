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

#include <leveldb/db.h>

#include "AccountStore.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/SysCommand.h"

AccountStore::AccountStore()
{
    m_accountStoreTemp = make_unique<AccountStoreTemp>(*this);
}

AccountStore::~AccountStore()
{
    // boost::filesystem::remove_all("./state");
}

void AccountStore::Init()
{
    LOG_MARKER();
    AccountStoreTrie<OverlayDB, unordered_map<Address, Account>>::Init();
    m_accountStoreTemp->Init();
    ContractStorage::GetContractStorage().GetStateDB().ResetDB();
    m_db.ResetDB();
}

AccountStore& AccountStore::GetInstance()
{
    static AccountStore accountstore;
    return accountstore;
}

int AccountStore::Deserialize(const vector<unsigned char>& src,
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
        this->Init();
        while (numberOfAccountDeserialze < totalNumOfAccounts)
        {
            numberOfAccountDeserialze++;

            // Deserialize address
            copy(src.begin() + curOffset,
                 src.begin() + curOffset + ACC_ADDR_SIZE,
                 address.asArray().begin());
            curOffset += ACC_ADDR_SIZE;

            // Deserialize account
            if (account.DeserializeAddOffset(src, curOffset) < 0)
            {
                LOG_GENERAL(WARNING,
                            "failed to deserialize account: " << address);
                continue;
            }
            (*m_addressToAccount)[address] = account;
            UpdateStateTrie(address, account);
            // MoveUpdatesToDisk();
        }
        PrintAccountState();
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStore::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

void AccountStore::SerializeDelta()
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexDelta);

    m_stateDeltaSerialized.clear();
    // [Total number of acount deltas (uint256_t)] [Addr 1] [AccountDelta 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    unsigned int curOffset = 0;

    uint256_t totalNumOfAccounts = m_accountStoreTemp->GetNumOfAccounts();
    LOG_GENERAL(INFO,
                "Debug: Total number of account deltas to serialize: "
                    << totalNumOfAccounts);
    SetNumber<uint256_t>(m_stateDeltaSerialized, curOffset, totalNumOfAccounts,
                         UINT256_SIZE);
    curOffset += UINT256_SIZE;

    vector<unsigned char> address_vec;
    // [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    for (const auto& entry : *m_accountStoreTemp->GetAddressToAccount())
    {
        LOG_GENERAL(INFO, "Addr: " << entry.first);

        // Address
        address_vec = entry.first.asBytes();

        copy(address_vec.begin(), address_vec.end(),
             std::back_inserter(m_stateDeltaSerialized));
        curOffset += ACC_ADDR_SIZE;

        // Account
        Account* account = GetAccount(entry.first);
        unsigned int size_needed = Account::SerializeDelta(
            m_stateDeltaSerialized, curOffset, account, entry.second);
        curOffset += size_needed;
    }
}

unsigned int AccountStore::GetSerializedDelta(vector<unsigned char>& dst)
{
    LOG_MARKER();

    copy(m_stateDeltaSerialized.begin(), m_stateDeltaSerialized.end(),
         back_inserter(dst));

    return m_stateDeltaSerialized.size();
}

int AccountStore::DeserializeDelta(const vector<unsigned char>& src,
                                   unsigned int offset)
{
    LOG_MARKER();
    // [Total number of acount deltas (uint256_t)] [Addr 1] [AccountDelta 1] [Addr 2] [Account 2] .... [Addr n] [Account n]

    try
    {
        lock_guard<mutex> g(m_mutexDelta);

        LOG_GENERAL(INFO, "Before DeserializeDelta");
        PrintAccountState();

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
                LOG_GENERAL(INFO, "Creating new account: " << address);
                AddAccount(address, acc);
            }

            LOG_GENERAL(INFO, "Diff account: " << address);
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

            UpdateStateTrie(address, account);
        }
        LOG_GENERAL(INFO, "After DeserializeDelta");
        PrintAccountState();
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStore::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

void AccountStore::MoveRootToDisk(const h256& root)
{
    //convert h256 to bytes
    if (!BlockStorage::GetBlockStorage().PutMetadata(STATEROOT, root.asBytes()))
        LOG_GENERAL(INFO, "FAIL: Put metadata failed");
}

void AccountStore::MoveUpdatesToDisk()
{
    LOG_MARKER();

    ContractStorage::GetContractStorage().GetStateDB().commit();
    for (auto i : *m_addressToAccount)
    {
        if (!ContractStorage::GetContractStorage().PutContractCode(
                i.first, i.second.GetCode()))
        {
            LOG_GENERAL(WARNING, "Write Contract Code to Disk Failed");
            continue;
        }
        i.second.Commit();
    }
    m_state.db()->commit();
    prevRoot = m_state.root();
    MoveRootToDisk(prevRoot);
}

void AccountStore::DiscardUnsavedUpdates()
{
    LOG_MARKER();

    ContractStorage::GetContractStorage().GetStateDB().rollback();
    for (auto i : *m_addressToAccount)
    {
        i.second.RollBack();
    }
    m_state.db()->rollback();
    m_state.setRoot(prevRoot);
    m_addressToAccount->clear();
}

bool AccountStore::RetrieveFromDisk()
{
    LOG_MARKER();
    vector<unsigned char> rootBytes;
    if (!BlockStorage::GetBlockStorage().GetMetadata(STATEROOT, rootBytes))
    {
        return false;
    }
    h256 root(rootBytes);
    m_state.setRoot(root);
    for (auto i : m_state)
    {
        Address address(i.first);
        LOG_GENERAL(INFO, "Address: " << address.hex());
        dev::RLP rlp(i.second);
        if (rlp.itemCount() != 4)
        {
            LOG_GENERAL(WARNING, "Account data corrupted");
            continue;
        }
        Account account(rlp[0].toInt<uint256_t>(), rlp[1].toInt<uint256_t>());
        // Code Hash
        if (rlp[3].toHash<h256>() != h256())
        {
            // Extract Code Content
            account.SetCode(
                ContractStorage::GetContractStorage().GetContractCode(address));
            if (rlp[3].toHash<h256>() != account.GetCodeHash())
            {
                LOG_GENERAL(WARNING,
                            "Account Code Content doesn't match Code Hash")
                continue;
            }
            // Storage Root
            account.SetStorageRoot(rlp[2].toHash<h256>());
        }
        m_addressToAccount->insert({address, account});
    }
    return true;
}

bool AccountStore::UpdateAccountsTemp(const uint64_t& blockNum,
                                      const Transaction& transaction)
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexDelta);

    return m_accountStoreTemp->UpdateAccounts(blockNum, transaction);
}

bool AccountStore::UpdateCoinbaseTemp(const Address& rewardee,
                                      const Address& genesisAddress,
                                      const uint256_t& amount)
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexDelta);
    if (m_accountStoreTemp->GetAccount(rewardee) == nullptr)
    {
        m_accountStoreTemp->AddAccount(rewardee, {0, 0});
    }
    return m_accountStoreTemp->TransferBalance(genesisAddress, rewardee,
                                               amount);
    //Should the nonce increase ??
}

StateHash AccountStore::GetStateDeltaHash()
{
    vector<unsigned char> vec;
    GetSerializedDelta(vec);

    bool isEmpty = true;

    for (unsigned int i = 0; i < vec.size(); i++)
    {
        if (vec[i] != 0)
        {
            isEmpty = false;
            break;
        }
    }

    if (isEmpty)
    {
        return StateHash();
    }

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    return StateHash(sha2.Finalize());
}

void AccountStore::CommitTemp()
{
    LOG_MARKER();

    LOG_GENERAL(INFO, "Before CommitTemp");

    LOG_PAYLOAD(INFO, "m_stateDeltaSerialized: ", m_stateDeltaSerialized, 2000);
    DeserializeDelta(m_stateDeltaSerialized, 0);

    LOG_GENERAL(INFO, "After CommitTemp");
    InitTemp();
}
