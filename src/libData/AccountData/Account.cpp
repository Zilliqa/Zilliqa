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

#include "Account.h"
#include "common/Messages.h"
#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

Account::Account() {}

Account::Account(const vector<unsigned char>& src, unsigned int offset)
{
    if (DeserializeAddOffset(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init Account.");
    }
}

Account::Account(const uint256_t& balance, const uint256_t& nonce)
    : m_balance(balance)
    , m_nonce(nonce)
    , m_storageRoot(h256())
    , m_codeHash(h256())
{
}

void Account::InitStorage()
{
    // LOG_MARKER();
    m_storage = AccountTrieDB<dev::h256, OverlayDB>(
        &(ContractStorage::GetContractStorage().GetStateDB()));
    m_storage.init();
    if (m_storageRoot != h256())
    {
        m_storage.setRoot(m_storageRoot);
        m_prevRoot = m_storageRoot;
    }
}

void Account::InitContract(const vector<unsigned char>& data)
{
    SetInitData(data);
    InitContract();
}

void Account::InitContract()
{
    // LOG_MARKER();
    if (m_initData.empty())
    {
        LOG_GENERAL(WARNING, "Init data for the contract is empty");
        m_initValJson = Json::arrayValue;
        return;
    }
    Json::CharReaderBuilder builder;
    unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    string dataStr(m_initData.begin(), m_initData.end());
    string errors;
    if (!reader->parse(dataStr.c_str(), dataStr.c_str() + dataStr.size(), &root,
                       &errors))
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse initialization contract json: " << errors);
        return;
    }
    m_initValJson = root;

    // Append createBlockNum
    {
        Json::Value createBlockNumObj;
        createBlockNumObj["vname"] = "_creation_block";
        createBlockNumObj["type"] = "BNum";
        createBlockNumObj["value"] = to_string(GetCreateBlockNum());
        m_initValJson.append(createBlockNumObj);
    }

    for (auto& v : root)
    {
        if (!v.isMember("vname") || !v.isMember("type") || !v.isMember("value"))
        {
            LOG_GENERAL(
                WARNING,
                "This variable in initialization of contract is corrupted");
            continue;
        }
        string vname = v["vname"].asString();
        string type = v["type"].asString();

        Json::StreamWriterBuilder writeBuilder;
        std::unique_ptr<Json::StreamWriter> writer(
            writeBuilder.newStreamWriter());
        ostringstream oss;
        writer->write(v["value"], &oss);
        string value = oss.str();

        SetStorage(vname, type, value, false);
    }
}

unsigned int Account::Serialize(vector<unsigned char>& dst,
                                unsigned int offset) const
{
    // LOG_MARKER();

    unsigned int curOffset = offset;

    // Balance
    SetNumber<uint256_t>(dst, curOffset, m_balance, UINT256_SIZE);
    // LOG_GENERAL(INFO, "balance: " << m_balance);
    curOffset += UINT256_SIZE;
    // Nonce
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    // LOG_GENERAL(INFO, "nonce: " << m_nonce);
    curOffset += UINT256_SIZE;
    // Storage Root
    copy(m_storageRoot.asArray().begin(), m_storageRoot.asArray().end(),
         back_inserter(dst));
    // LOG_GENERAL(INFO, "storageRoot: " << m_storageRoot);
    curOffset += COMMON_HASH_SIZE;
    // Code Hash
    copy(m_codeHash.asArray().begin(), m_codeHash.asArray().end(),
         back_inserter(dst));
    // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);
    curOffset += COMMON_HASH_SIZE;
    // Size of Code Content
    SetNumber<uint256_t>(dst, curOffset, uint256_t(m_codeCache.size()),
                         UINT256_SIZE);
    // LOG_GENERAL(INFO, "codeSize: " << m_codeCache.size());
    curOffset += UINT256_SIZE;
    if (m_codeCache.empty())
    {
        // non-contract account
        return curOffset - offset;
    }
    // Code
    copy(m_codeCache.begin(), m_codeCache.end(), back_inserter(dst));
    curOffset += m_codeCache.size();

    // Init Data Size
    SetNumber<uint256_t>(dst, curOffset, uint256_t(m_initData.size()),
                         UINT256_SIZE);
    curOffset += UINT256_SIZE;
    // LOG_GENERAL(INFO, "initData size: " << m_initData.size());
    // Init Data
    copy(m_initData.begin(), m_initData.end(), back_inserter(dst));
    // LOG_GENERAL(INFO, "initData: " << m_initData);
    curOffset += m_initData.size();

    // Create Block Num
    SetNumber<uint64_t>(dst, curOffset, m_createBlockNum, sizeof(uint64_t));
    // LOG_GENERAL(INFO, "createBlockNum: " << m_createBlockNum);
    curOffset += sizeof(uint64_t);

    // States
    // Num of Key Hashes
    SetNumber<uint256_t>(dst, curOffset,
                         uint256_t(GetStorageKeyHashes().size()), UINT256_SIZE);
    // LOG_GENERAL(INFO,
    //             "numKeyHashes: " << uint256_t(GetStorageKeyHashes().size()));
    curOffset += UINT256_SIZE;

    for (unsigned int i = 0; i < GetStorageKeyHashes().size(); i++)
    {
        // Key Hash
        h256 keyHash = GetStorageKeyHashes()[i];
        copy(keyHash.asArray().begin(), keyHash.asArray().end(),
             std::back_inserter(dst));
        // LOG_GENERAL(INFO, "KeyHash: " << keyHash);
        curOffset += COMMON_HASH_SIZE;

        // RLP
        string rlpStr = m_storage.at(keyHash);
        // RLP size
        SetNumber<uint256_t>(dst, curOffset, uint256_t(rlpStr.size()),
                             UINT256_SIZE);
        // LOG_GENERAL(INFO, "rlpSize: " << rlpStr.size());
        curOffset += UINT256_SIZE;
        // RLP string
        copy(rlpStr.begin(), rlpStr.end(), std::back_inserter(dst));
        // LOG_GENERAL(INFO, "rlpStr: " << rlpStr);
        curOffset += rlpStr.size();
    }

    return curOffset - offset;
}

int Account::DeserializeAddOffset(const vector<unsigned char>& src,
                                  unsigned int& offset)
{
    LOG_MARKER();

    try
    {
        // Balance
        m_balance = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        // LOG_GENERAL(INFO, "balance: " << m_balance);
        offset += UINT256_SIZE;
        // Nonce
        m_nonce = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        // LOG_GENERAL(INFO, "nonce: " << m_nonce);
        offset += UINT256_SIZE;
        // Storage Root
        h256 t_storageRoot;
        copy(src.begin() + offset, src.begin() + offset + COMMON_HASH_SIZE,
             t_storageRoot.asArray().begin());
        // LOG_GENERAL(INFO, "storageRoot: " << t_storageRoot);
        offset += COMMON_HASH_SIZE;
        // Code Hash
        copy(src.begin() + offset, src.begin() + offset + COMMON_HASH_SIZE,
             m_codeHash.asArray().begin());
        // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);
        offset += COMMON_HASH_SIZE;
        // Size of Code
        unsigned int codeSize
            = (unsigned int)GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        // LOG_GENERAL(INFO, "codeSize: " << codeSize);
        offset += UINT256_SIZE;
        // Code
        if (codeSize > 0)
        {
            vector<unsigned char> code;
            code.resize(codeSize);
            copy(src.begin() + offset, src.begin() + offset + codeSize,
                 code.begin());
            // LOG_PAYLOAD(INFO, "code: ", code, 2000);
            offset += codeSize;
            SetCode(code);

            // Init Data Size
            unsigned int initDataSize
                = (unsigned int)GetNumber<uint256_t>(src, offset, UINT256_SIZE);
            offset += UINT256_SIZE;
            // LOG_GENERAL(INFO, "InitData size: " << initDataSize);
            // Init Data
            vector<unsigned char> initData;
            copy(src.begin() + offset, src.begin() + offset + initDataSize,
                 back_inserter(initData));
            // LOG_GENERAL(INFO, "InitData: " << initData);
            offset += initDataSize;
            if (!initData.empty())
            {
                InitContract(initData);
            }

            // Create Block Num
            m_createBlockNum
                = GetNumber<uint64_t>(src, offset, sizeof(uint64_t));
            // LOG_GENERAL(INFO, "createBlockNum: " << m_createBlockNum);
            offset += sizeof(uint64_t);

            // States
            // Num of Key Hashes
            unsigned int numKeyHashes
                = (unsigned int)GetNumber<uint256_t>(src, offset, UINT256_SIZE);
            // LOG_GENERAL(INFO, "numKeyHashes: " << numKeyHashes);
            offset += UINT256_SIZE;
            for (unsigned int i = 0; i < numKeyHashes; i++)
            {
                // Key Hash
                h256 keyHash;
                copy(src.begin() + offset,
                     src.begin() + offset + COMMON_HASH_SIZE,
                     keyHash.asArray().begin());
                // LOG_GENERAL(INFO, "KeyHash: " << keyHash);
                offset += COMMON_HASH_SIZE;

                // RLP
                // RLP size
                unsigned int rlpSize = (unsigned int)GetNumber<uint256_t>(
                    src, offset, UINT256_SIZE);
                // LOG_GENERAL(INFO, "rlpSize: " << rlpSize);
                offset += UINT256_SIZE;
                // RLP string
                string rlpStr;
                copy(src.begin() + offset, src.begin() + offset + rlpSize,
                     rlpStr.begin());
                offset += rlpSize;
                // LOG_GENERAL(INFO, "rlpStr: " << rlpStr);
                m_storage.insert(keyHash, rlpStr);
                m_storageRoot = m_storage.root();
            }

            if (t_storageRoot != m_storageRoot)
            {
                LOG_GENERAL(
                    WARNING,
                    "ERROR: StorageRoots doesn't match! Investigate why!");
                return -1;
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Account::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

unsigned int Account::SerializeDelta(vector<unsigned char>& dst,
                                     unsigned int offset, Account* oldAccount,
                                     const Account& newAccount)
{
    // LOG_MARKER();

    Account acc(0, 0);

    if (oldAccount == nullptr)
    {
        oldAccount = &acc;
    }
    // unsigned int size_needed = ACCOUNT_SIZE;
    // unsigned int size_remaining = dst.size() - offset;

    // if (size_remaining < size_needed)
    // {
    //     dst.resize(size_needed + offset);
    // }

    unsigned int curOffset = offset;

    // Balance Delta
    int256_t balanceDelta = int256_t(newAccount.GetBalance())
        - int256_t(oldAccount->GetBalance());
    // LOG_GENERAL(INFO, "Balance Delta: " << balanceDelta);
    // Sign
    dst.push_back(balanceDelta > 0 ? NumberSign::POSITIVE
                                   : NumberSign::NEGATIVE);
    curOffset += 1;
    uint256_t balanceDeltaNum(abs(balanceDelta));
    // Number
    SetNumber<uint256_t>(dst, curOffset, balanceDeltaNum, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    // Nonce Delta
    uint256_t nonceDelta = newAccount.GetNonce() - oldAccount->GetNonce();
    // LOG_GENERAL(INFO,
    //             "newNonce: " << newAccount.GetNonce()
    //                          << " oldNonce: " << oldAccount->GetNonce());
    SetNumber<uint256_t>(dst, curOffset, nonceDelta, UINT256_SIZE);
    // LOG_GENERAL(INFO, "Nonce Delta: " << nonceDelta);
    curOffset += UINT256_SIZE;

    // Code Size
    SetNumber<uint256_t>(dst, curOffset, uint256_t(newAccount.GetCode().size()),
                         UINT256_SIZE);
    // LOG_GENERAL(INFO, "codeSize: " << newAccount.GetCode().size());
    curOffset += UINT256_SIZE;
    if (newAccount.GetCode().empty())
    {
        // non-contract account
        return curOffset - offset;
    }
    // Code
    copy(newAccount.GetCode().begin(), newAccount.GetCode().end(),
         back_inserter(dst));
    curOffset += newAccount.GetCode().size();

    // Init Data Size
    SetNumber<uint256_t>(dst, curOffset,
                         uint256_t(newAccount.GetInitData().size()),
                         UINT256_SIZE);
    curOffset += UINT256_SIZE;
    // LOG_GENERAL(INFO, "initData size: " << newAccount.GetInitData().size());
    // Init Data
    copy(newAccount.GetInitData().begin(), newAccount.GetInitData().end(),
         back_inserter(dst));
    // LOG_GENERAL(INFO, "InitData: " << newAccount.GetInitData());
    curOffset += newAccount.GetInitData().size();

    // Create Block Num
    SetNumber<uint64_t>(dst, curOffset, newAccount.GetCreateBlockNum(),
                        sizeof(uint64_t));
    // LOG_GENERAL(INFO, "createBlockNum: " << newAccount.GetCreateBlockNum());
    curOffset += sizeof(uint64_t);

    // Storage Root
    copy(newAccount.GetStorageRoot().asArray().begin(),
         newAccount.GetStorageRoot().asArray().begin() + COMMON_HASH_SIZE,
         back_inserter(dst));
    // LOG_GENERAL(INFO,
    //             "new StorageRoot: " << newAccount.GetStorageRoot()
    //                                 << " old StorageRoot: "
    //                                 << oldAccount->GetStorageRoot());
    curOffset += COMMON_HASH_SIZE;
    if (newAccount.GetStorageRoot() != oldAccount->GetStorageRoot())
    {
        // LOG_GENERAL(INFO, "StorageRoot Changed");

        // States storage
        // Num of Key Hashes
        SetNumber<uint256_t>(dst, curOffset,
                             uint256_t(newAccount.GetStorageKeyHashes().size()),
                             UINT256_SIZE);
        // LOG_GENERAL(
        //     INFO,
        //     "Num of KeyHash: " << newAccount.GetStorageKeyHashes().size());
        curOffset += UINT256_SIZE;
        for (unsigned int i = 0; i < newAccount.GetStorageKeyHashes().size();
             i++)
        {
            // Key Hash
            h256 keyHash = newAccount.GetStorageKeyHashes()[i];
            copy(keyHash.asArray().begin(),
                 keyHash.asArray().begin() + COMMON_HASH_SIZE,
                 back_inserter(dst));
            // LOG_GENERAL(INFO, "KeyHash: " << keyHash);
            curOffset += COMMON_HASH_SIZE;

            // RLP
            string rlpStr = newAccount.GetRawStorage(keyHash);
            // LOG_GENERAL(INFO,
            //             "RLP: "
            //                 << rlpStr.substr(
            //                        0, 50 > rlpStr.size() ? rlpStr.size() : 50)
            //                 << " ... ");
            // RLP size
            SetNumber<uint256_t>(dst, curOffset, uint256_t(rlpStr.size()),
                                 UINT256_SIZE);
            curOffset += UINT256_SIZE;
            // RLP string
            copy(rlpStr.begin(), rlpStr.end(), back_inserter(dst));
            curOffset += rlpStr.size();
        }
    }

    return curOffset - offset;
}

int Account::DeserializeDelta(const vector<unsigned char>& src,
                              unsigned int& offset, Account& account)
{
    // LOG_MARKER();

    try
    {
        // LOG_GENERAL(INFO, "Account before changing: " << account);
        // Balance Delta
        // Sign
        unsigned char numsign = src[offset];
        offset += 1;
        // Num
        uint256_t balanceDeltaNum
            = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        int balanceDelta = (numsign == NumberSign::POSITIVE)
            ? (int)balanceDeltaNum
            : 0 - (int)balanceDeltaNum;
        // LOG_GENERAL(INFO, "balanceDelta: " << balanceDelta);
        account.ChangeBalance(balanceDelta);
        // Nonce Delta
        uint256_t nonceDelta = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        // LOG_GENERAL(INFO, "nonceDelta: " << nonceDelta);
        account.IncreaseNonceBy(nonceDelta);
        offset += UINT256_SIZE;
        // Code Size
        unsigned int codeSize
            = (unsigned int)GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        // LOG_GENERAL(INFO, "codeSize: " << codeSize);
        if (codeSize > 0)
        {
            // Code
            vector<unsigned char> t_code;
            copy(src.begin() + offset, src.begin() + offset + codeSize,
                 back_inserter(t_code));
            offset += codeSize;
            if (t_code != account.GetCode())
            {
                account.SetCode(t_code);
            }

            // Init Data Size
            unsigned int initDataSize
                = (unsigned int)GetNumber<uint256_t>(src, offset, UINT256_SIZE);
            offset += UINT256_SIZE;
            // LOG_GENERAL(INFO, "InitData size: " << initDataSize);
            // Init Data
            vector<unsigned char> initData;
            copy(src.begin() + offset, src.begin() + offset + initDataSize,
                 back_inserter(initData));
            // LOG_GENERAL(INFO, "InitData: " << initData);
            offset += initDataSize;
            bool doInitContract = false;
            if (!initData.empty() && account.GetInitData().empty())
            {
                account.SetInitData(initData);
                doInitContract = true;
            }

            // Create Block Num
            uint64_t createBlockNum
                = GetNumber<uint64_t>(src, offset, sizeof(uint64_t));
            // LOG_GENERAL(INFO, "createBlockNum: " << createBlockNum);
            account.SetCreateBlockNum(createBlockNum);
            offset += sizeof(uint64_t);

            // Storage Root
            h256 t_storageRoot;
            copy(src.begin() + offset, src.begin() + offset + COMMON_HASH_SIZE,
                 t_storageRoot.asArray().begin());
            // LOG_GENERAL(INFO, "t_storageRoot: " << t_storageRoot);
            offset += COMMON_HASH_SIZE;

            // LOG_GENERAL(INFO,
            //             "t_storageRoot: " << t_storageRoot
            //                               << " old StorageRoot: "
            //                               << account.GetStorageRoot());

            if (t_storageRoot != account.GetStorageRoot())
            {
                // LOG_GENERAL(INFO, "StorageRoot Changed");
                if (doInitContract)
                {
                    account.InitContract();
                }

                // States storage
                // Num of Key Hashes
                unsigned int numKeyHashes = (unsigned int)GetNumber<uint256_t>(
                    src, offset, UINT256_SIZE);
                offset += UINT256_SIZE;
                // LOG_GENERAL(INFO, "numKeyHashes:" << numKeyHashes);

                for (unsigned int i = 0; i < numKeyHashes; i++)
                {
                    // Key Hash
                    h256 keyHash;
                    copy(src.begin() + offset,
                         src.begin() + offset + COMMON_HASH_SIZE,
                         keyHash.asArray().begin());
                    offset += COMMON_HASH_SIZE;
                    // LOG_GENERAL(INFO, "keyHash: " << keyHash);

                    // RLP
                    // RLP size
                    unsigned int rlpSize = (unsigned int)GetNumber<uint256_t>(
                        src, offset, UINT256_SIZE);
                    offset += UINT256_SIZE;
                    // LOG_GENERAL(INFO, "rlpSize: " << rlpSize);

                    // RLP string
                    string rlpStr;
                    copy(src.begin() + offset, src.begin() + offset + rlpSize,
                         back_inserter(rlpStr));
                    offset += rlpSize;
                    // LOG_GENERAL(INFO,
                    //             "RLP: " << rlpStr.substr(0,
                    //                                      50 > rlpStr.size()
                    //                                          ? rlpStr.size()
                    //                                          : 50)
                    //                     << " ... ");
                    account.SetStorage(keyHash, rlpStr);
                }

                if (t_storageRoot != account.GetStorageRoot())
                {
                    // LOG_GENERAL(
                    //     WARNING,
                    //     "ERROR: StorageRoots doesn't match! Investigate why!");
                    // LOG_GENERAL(INFO, "t_storageRoot: " << t_storageRoot);
                    // LOG_GENERAL(
                    //     INFO,
                    //     "account.GetStorageRoot: " << account.GetStorageRoot());
                    return -1;
                }
            }
        }
        // LOG_GENERAL(INFO, "Account after changing: " << account);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Account::DeserializeDelta." << ' ' << e.what());
        return -1;
    }
    return 0;
}

bool Account::IncreaseBalance(const uint256_t& delta)
{
    return SafeMath::add(m_balance, delta, m_balance);
}

bool Account::DecreaseBalance(const uint256_t& delta)
{
    if (m_balance < delta)
    {
        return false;
    }

    return SafeMath::sub(m_balance, delta, m_balance);
}

bool Account::ChangeBalance(const int256_t& delta)
{
    return (delta >= 0) ? IncreaseBalance(uint256_t(delta))
                        : DecreaseBalance(uint256_t(-delta));
}

bool Account::IncreaseNonce()
{
    ++m_nonce;
    return true;
}

bool Account::IncreaseNonceBy(const uint256_t& nonceDelta)
{
    m_nonce += nonceDelta;
    return true;
}

void Account::SetStorageRoot(const h256& root)
{
    if (!isContract())
    {
        return;
    }
    m_storageRoot = root;

    if (m_storageRoot == h256())
    {
        return;
    }

    m_storage.setRoot(m_storageRoot);
    m_prevRoot = m_storageRoot;
}

void Account::SetStorage(string k, string type, string v, bool is_mutable)
{
    if (!isContract())
    {
        return;
    }
    RLPStream rlpStream(4);
    rlpStream << k << (is_mutable ? "True" : "False") << type << v;

    m_storage.insert(GetKeyHash(k), rlpStream.out());

    m_storageRoot = m_storage.root();
}

void Account::SetStorage(const h256& k_hash, const string& rlpStr)
{
    if (!isContract())
    {
        LOG_GENERAL(WARNING,
                    "Not contract account, why call Account::SetStorage!");
        return;
    }
    m_storage.insert(k_hash, rlpStr);
    m_storageRoot = m_storage.root();
}

vector<string> Account::GetStorage(const string& _k) const
{
    if (!isContract())
    {
        LOG_GENERAL(WARNING,
                    "Not contract account, why call Account::GetStorage!");
        return {};
    }

    dev::RLP rlp(m_storage.at(GetKeyHash(_k)));
    // mutable, type, value
    return {rlp[1].toString(), rlp[2].toString(), rlp[3].toString()};
}

string Account::GetRawStorage(const h256& k_hash) const
{
    if (!isContract())
    {
        LOG_GENERAL(WARNING,
                    "Not contract account, why call Account::GetRawStorage!");
        return "";
    }
    return m_storage.at(k_hash);
}

vector<h256> Account::GetStorageKeyHashes() const
{
    vector<h256> keyHashes;
    for (auto const& i : m_storage)
    {
        keyHashes.emplace_back(i.first);
    }
    return keyHashes;
}

Json::Value Account::GetStorageJson() const
{
    if (!isContract())
    {
        LOG_GENERAL(WARNING,
                    "Not contract account, why call Account::GetStorageJson!");
        return Json::arrayValue;
    }

    Json::Value root;
    for (auto const& i : m_storage)
    {
        dev::RLP rlp(i.second);
        string tVname = rlp[0].toString();
        string tMutable = rlp[1].toString();
        string tType = rlp[2].toString();
        string tValue = rlp[3].toString();
        // LOG_GENERAL(INFO,
        //             "\nvname: " << tVname << " \nmutable: " << tMutable
        //                         << " \ntype: " << tType
        //                         << " \nvalue: " << tValue);
        if (tMutable == "False")
        {
            continue;
        }

        Json::Value item;
        item["vname"] = tVname;
        item["type"] = tType;
        if (tValue[0] == '[' || tValue[0] == '{')
        {
            Json::CharReaderBuilder builder;
            unique_ptr<Json::CharReader> reader(builder.newCharReader());
            Json::Value obj;
            string errors;
            if (!reader->parse(tValue.c_str(), tValue.c_str() + tValue.size(),
                               &obj, &errors))
            {
                LOG_GENERAL(WARNING,
                            "The json object cannot be extracted from Storage: "
                                << tValue << endl
                                << "Error: " << errors);
                continue;
            }
            item["value"] = obj;
        }
        else
        {
            item["value"] = tValue;
        }
        root.append(item);
    }
    Json::Value balance;
    balance["vname"] = "_balance";
    balance["type"] = "Uint128";
    balance["value"] = GetBalance().convert_to<string>();
    root.append(balance);

    // LOG_GENERAL(INFO, "States: " << root);

    return root;
}

void Account::RollBack()
{
    if (!isContract())
    {
        LOG_GENERAL(WARNING, "Not a contract, why call Account::RollBack");
        return;
    }
    m_storageRoot = m_prevRoot;
    if (m_storageRoot != h256())
    {
        m_storage.setRoot(m_storageRoot);
    }
    else
    {
        m_storage.init();
    }
}

Address Account::GetAddressFromPublicKey(const PubKey& pubKey)
{
    Address address;

    vector<unsigned char> vec;
    pubKey.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);

    const vector<unsigned char>& output = sha2.Finalize();

    if (output.size() != 32)
    {
        LOG_GENERAL(WARNING,
                    "assertion failed (" << __FILE__ << ":" << __LINE__ << ": "
                                         << __FUNCTION__ << ")");
    }

    copy(output.end() - ACC_ADDR_SIZE, output.end(), address.asArray().begin());

    return address;
}

Address Account::GetAddressForContract(const Address& sender,
                                       const uint256_t& nonce)
{
    Address address;

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(sender.asBytes());
    vector<unsigned char> nonceBytes;
    SetNumber<uint256_t>(nonceBytes, 0, nonce, UINT256_SIZE);
    sha2.Update(nonceBytes);

    const vector<unsigned char>& output = sha2.Finalize();

    if (output.size() != 32)
    {
        LOG_GENERAL(WARNING,
                    "assertion failed (" << __FILE__ << ":" << __LINE__ << ": "
                                         << __FUNCTION__ << ")");
    }

    copy(output.end() - ACC_ADDR_SIZE, output.end(), address.asArray().begin());

    return address;
}

void Account::SetCode(const vector<unsigned char>& code)
{
    // LOG_MARKER();

    if (code.size() == 0)
    {
        LOG_GENERAL(WARNING, "Code for this contract is empty");
        return;
    }

    m_codeCache = code;
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(code);
    m_codeHash = dev::h256(sha2.Finalize());
    // LOG_GENERAL(INFO, "m_codeHash: " << m_codeHash);

    InitStorage();
}

const h256 Account::GetKeyHash(const string& key) const
{
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(DataConversion::StringToCharArray(key));
    return h256(sha2.Finalize());
}
