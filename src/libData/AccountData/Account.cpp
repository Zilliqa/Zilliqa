/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/

#include "Account.h"
#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

Account::Account() {}

Account::Account(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
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
    LOG_MARKER();
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
    if (data.empty())
    {
        m_initValJson = Json::arrayValue;
        return;
    }
    Json::CharReaderBuilder builder;
    unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    string dataStr(data.begin(), data.end());
    string errors;
    if (reader->parse(dataStr.c_str(), dataStr.c_str() + dataStr.size(), &root,
                      &errors))
    {
        m_initValJson = root;
        for (auto v : root)
        {
            if (!v.isMember("vname") || !v.isMember("type")
                || !v.isMember("value"))
            {
                LOG_GENERAL(
                    WARNING,
                    "This variable in initialization of contract is corrupted");
                continue;
            }
            string vname = v["vname"].asString();
            string type = v["type"].asString();

            Json::StreamWriterBuilder writeBuilder;
            unique_ptr<Json::StreamWriter> writer(
                writeBuilder.newStreamWriter());
            ostringstream oss;
            writer->write(v["value"], &oss);
            string value = oss.str();

            SetStorage(vname, type, value, false);
        }
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse initialization contract json: " << errors);
    }
}

unsigned int Account::Serialize(vector<unsigned char>& dst,
                                unsigned int offset) const
{
    // LOG_MARKER();

    unsigned int size_needed = ACCOUNT_SIZE + m_codeCache.size();
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    // Balance
    SetNumber<uint256_t>(dst, curOffset, m_balance, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    // Nonce
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    // Storage Root
    copy(m_storageRoot.asArray().begin(), m_storageRoot.asArray().end(),
         dst.begin() + curOffset);
    curOffset += COMMON_HASH_SIZE;
    // Code Hash
    copy(m_codeHash.asArray().begin(), m_codeHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += COMMON_HASH_SIZE;
    // Size of Code Content
    SetNumber<uint256_t>(dst, curOffset, uint256_t(m_codeCache.size()),
                         UINT256_SIZE);
    curOffset += UINT256_SIZE;
    // Code
    if (m_codeCache.size() != 0)
    {
        copy(m_codeCache.begin(), m_codeCache.end(), dst.begin() + curOffset);
        curOffset += m_codeCache.size();

        // States
        // Num of Key Hashes
        SetNumber<uint256_t>(dst, curOffset,
                             uint256_t(GetStorageKeyHashes().size()),
                             UINT256_SIZE);
        curOffset += UINT256_SIZE;

        for (unsigned int i = 0; i < GetStorageKeyHashes().size(); i++)
        {
            // Key Hash
            unsigned int length_available = dst.size() - curOffset;
            if (length_available < COMMON_HASH_SIZE)
            {
                dst.resize(dst.size() + COMMON_HASH_SIZE - length_available);
            }

            h256 keyHash = GetStorageKeyHashes()[i];
            copy(keyHash.asArray().begin(), keyHash.asArray().end(),
                 dst.begin() + curOffset);
            curOffset += COMMON_HASH_SIZE;

            // RLP
            string rlpStr = m_storage.at(keyHash);
            // RLP size
            SetNumber<uint256_t>(dst, curOffset, uint256_t(rlpStr.size()),
                                 UINT256_SIZE);
            curOffset += UINT256_SIZE;
            length_available = dst.size() - curOffset;
            if (length_available < rlpStr.size())
            {
                dst.resize(dst.size() + rlpStr.size() - length_available);
            }
            // RLP string
            copy(rlpStr.begin(), rlpStr.end(), dst.begin() + curOffset);
            curOffset += rlpStr.size();
        }
    }

    return size_needed;
}

int Account::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    LOG_MARKER();

    int ret_size;

    try
    {
        unsigned int curOffset = offset;

        // Balance
        m_balance = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        // Nonce
        m_nonce = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        // Storage Root
        h256 t_storageRoot;
        copy(src.begin() + curOffset,
             src.begin() + curOffset + COMMON_HASH_SIZE,
             t_storageRoot.asArray().begin());
        curOffset += COMMON_HASH_SIZE;
        // Code Hash
        copy(src.begin() + curOffset,
             src.begin() + curOffset + COMMON_HASH_SIZE,
             m_codeHash.asArray().begin());
        curOffset += COMMON_HASH_SIZE;
        // Size of Code
        unsigned int codeSize
            = (unsigned int)GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        // Code
        if (codeSize > 0)
        {
            vector<unsigned char> code;
            code.resize(codeSize);
            copy(src.begin() + curOffset, src.begin() + curOffset + codeSize,
                 code.begin());
            curOffset += codeSize;
            SetCode(code);

            // States
            // Num of Key Hashes
            unsigned int numKeyHashes = (unsigned int)GetNumber<uint256_t>(
                src, curOffset, UINT256_SIZE);
            curOffset += UINT256_SIZE;
            for (unsigned int i = 0; i < numKeyHashes; i++)
            {
                // Key Hash
                h256 keyHash;
                copy(src.begin() + curOffset,
                     src.begin() + curOffset + COMMON_HASH_SIZE,
                     keyHash.asArray().begin());
                curOffset += COMMON_HASH_SIZE;

                // RLP
                // RLP size
                unsigned int rlpSize = (unsigned int)GetNumber<uint256_t>(
                    src, curOffset, UINT256_SIZE);
                curOffset += UINT256_SIZE;
                // RLP string
                string rlpStr;
                copy(src.begin() + curOffset, src.begin() + curOffset + rlpSize,
                     rlpStr.begin());
                curOffset += rlpSize;
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
        ret_size = int(curOffset - offset);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Account::Deserialize." << ' ' << e.what());
        return -1;
    }
    return ret_size;
}

unsigned int Account::SerializeDelta(vector<unsigned char>& dst,
                                     unsigned int offset, Account* oldAccount,
                                     const Account& newAccount)
{
    LOG_MARKER();

    if (!oldAccount)
    {
        return newAccount.Serialize(dst, offset);
    }
    else
    {
        unsigned int size_needed = ACCOUNT_SIZE;
        unsigned int size_remaining = dst.size() - offset;

        if (size_remaining < size_needed)
        {
            dst.resize(size_needed + offset);
        }

        unsigned int curOffset = offset;

        // Balance Delta
        SetNumber<int256_t>(dst, curOffset,
                            int256_t(newAccount.GetBalance())
                                - int256_t(oldAccount->GetBalance()),
                            INT256_SIZE);
        curOffset += INT256_SIZE;
        // Nonce Delta
        SetNumber<uint256_t>(dst, curOffset,
                             uint256_t(newAccount.GetNonce())
                                 - uint256_t(oldAccount->GetNonce()),
                             UINT256_SIZE);
        curOffset += UINT256_SIZE;
        // Storage Root
        copy(newAccount.GetStorageRoot().asArray().begin(),
             newAccount.GetStorageRoot().asArray().end(),
             dst.begin() + curOffset);
        curOffset += COMMON_HASH_SIZE;
        if (newAccount.GetStorageRoot() != oldAccount->GetStorageRoot())
        {
            // States storage
            // Num of Key Hashes
            SetNumber<uint256_t>(
                dst, curOffset,
                uint256_t(newAccount.GetStorageKeyHashes().size()),
                UINT256_SIZE);
            curOffset += UINT256_SIZE;
            for (unsigned int i = 0;
                 i < newAccount.GetStorageKeyHashes().size(); i++)
            {
                // Key Hash
                h256 keyHash = newAccount.GetStorageKeyHashes()[i];
                copy(keyHash.asArray().begin(), keyHash.asArray().end(),
                     dst.begin() + curOffset);
                curOffset += COMMON_HASH_SIZE;

                // RLP
                string rlpStr = newAccount.GetRawStorage(keyHash);
                // RLP size
                SetNumber<uint256_t>(dst, curOffset, uint256_t(rlpStr.size()),
                                     UINT256_SIZE);
                curOffset += UINT256_SIZE;
                // RLP string
                copy(rlpStr.begin(), rlpStr.end(), dst.begin() + curOffset);
                curOffset += rlpStr.size();
            }
        }

        return size_needed;
    }
}

int Account::DeserializeDelta(const vector<unsigned char>& src,
                              unsigned int offset, Account& account, bool isNew)
{
    LOG_MARKER();

    if (isNew)
    {
        return account.Deserialize(src, offset);
    }
    else
    {
        int ret_size;
        try
        {
            unsigned int curOffset = offset;

            // Balance Delta
            int256_t balanceDelta
                = GetNumber<int256_t>(src, curOffset, INT256_SIZE);
            account.ChangeBalance(balanceDelta);
            // Nonce Delta
            uint256_t nonceDelta
                = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
            account.IncreaseNonceBy(nonceDelta);
            curOffset += UINT256_SIZE;
            // Storage Root
            h256 t_storageRoot;
            copy(src.begin() + curOffset,
                 src.begin() + curOffset + COMMON_HASH_SIZE,
                 t_storageRoot.asArray().begin());
            curOffset += COMMON_HASH_SIZE;

            if (t_storageRoot != account.GetStorageRoot())
            {
                // States storage
                // Num of Key Hashes
                unsigned int numKeyHashes = (unsigned int)GetNumber<uint256_t>(
                    src, curOffset, UINT256_SIZE);
                curOffset += UINT256_SIZE;
                for (unsigned int i = 0; i < numKeyHashes; i++)
                {
                    // Key Hash
                    h256 keyHash;
                    copy(src.begin() + curOffset,
                         src.begin() + curOffset + COMMON_HASH_SIZE,
                         keyHash.asArray().begin());
                    curOffset += COMMON_HASH_SIZE;

                    // RLP
                    // RLP size
                    unsigned int rlpSize = (unsigned int)GetNumber<uint256_t>(
                        src, curOffset, UINT256_SIZE);
                    curOffset += UINT256_SIZE;
                    // RLP string
                    string rlpStr;
                    copy(src.begin() + curOffset,
                         src.begin() + curOffset + rlpSize, rlpStr.begin());
                    curOffset += rlpSize;
                    account.SetStorage(keyHash, rlpStr);
                }

                if (t_storageRoot != account.GetStorageRoot())
                {
                    LOG_GENERAL(
                        WARNING,
                        "ERROR: StorageRoots doesn't match! Investigate why!");
                    return -1;
                }
            }
            ret_size = int(curOffset - offset);
        }
        catch (const std::exception& e)
        {
            LOG_GENERAL(WARNING,
                        "Error with Account::DeserializeDelta." << ' '
                                                                << e.what());
            return -1;
        }
        return ret_size;
    }
}

bool Account::IncreaseBalance(const uint256_t& delta)
{
    m_balance += delta;
    return true;
}

bool Account::DecreaseBalance(const uint256_t& delta)
{
    if (m_balance < delta)
    {
        return false;
    }

    m_balance -= delta;
    return true;
}

bool Account::ChangeBalance(const int256_t& delta)
{
    if (delta >= 0)
    {
        return IncreaseBalance(uint256_t(delta));
    }
    else
    {
        return DecreaseBalance(uint256_t(delta));
    }
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
        return;
    m_storageRoot = root;
    if (m_storageRoot != h256())
    {
        m_storage.setRoot(m_storageRoot);
        m_prevRoot = m_storageRoot;
    }
}

void Account::SetStorage(const string& _k, const string& _type,
                         const string& _v, bool _mutable)
{
    if (!isContract())
        return;
    RLPStream rlpStream(4);
    rlpStream << _k << (_mutable ? "True" : "False") << _type << _v;

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(DataConversion::StringToCharArray(_k));
    const vector<unsigned char>& k_hash = sha2.Finalize();

    m_storage.insert(dev::h256(k_hash), rlpStream.out());

    m_storageRoot = m_storage.root();
}

void Account::SetStorage(const h256& k_hash, const string& rlpStr)
{
    if (!isContract())
        return;
    m_storage.insert(k_hash, rlpStr);
    m_storageRoot = m_storage.root();
}

vector<string> Account::GetStorage(const string& _k) const
{
    if (!isContract())
        return {};

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(DataConversion::StringToCharArray(_k));
    const vector<unsigned char>& k_hash = sha2.Finalize();

    dev::RLP rlp(m_storage.at(dev::h256(k_hash)));
    // mutable, type, value
    return {rlp[1].toString(), rlp[2].toString(), rlp[3].toString()};
}

string Account::GetRawStorage(const h256& k_hash) const
{
    if (!isContract())
        return "";
    return m_storage.at(k_hash);
}

vector<h256> Account::GetStorageKeyHashes() const
{
    vector<h256> keyHashes;
    for (auto i : m_storage)
    {
        keyHashes.push_back(i.first);
    }
    return keyHashes;
}

Json::Value Account::GetStorageJson() const
{
    if (!isContract())
        return Json::arrayValue;
    Json::Value root;
    for (auto i : m_storage)
    {
        dev::RLP rlp(i.second);
        string tVname = rlp[0].toString();
        string tMutable = rlp[1].toString();
        string tType = rlp[2].toString();
        string tValue = rlp[3].toString();
        LOG_GENERAL(INFO,
                    "ITERATE \nvname: " << tVname << " \nmutable: " << tMutable
                                        << " \ntype: " << tType
                                        << " \nvalue: " << tValue);
        if (tMutable == "False")
        {
            continue;
        }
        Json::Value item;
        item["vname"] = tVname;
        item["type"] = tType;
        if (tType == "Map" || tType == "ADT")
        {
            Json::CharReaderBuilder builder;
            unique_ptr<Json::CharReader> reader(builder.newCharReader());
            Json::Value obj;
            string errors;
            if (reader->parse(tValue.c_str(), tValue.c_str() + tValue.size(),
                              &obj, &errors))
            {
                item["value"] = obj;
            }
            else
            {
                LOG_GENERAL(
                    WARNING,
                    "The map json object cannot be extracted from Storage: "
                        << errors);
            }
        }
        else
        {
            item["value"] = tValue;
        }
        root.append(item);
    }
    Json::Value _balance;
    _balance["vname"] = "_balance";
    _balance["type"] = "Int";
    int balance = static_cast<int>(m_balance);
    _balance["value"] = to_string(balance);
    root.append(_balance);

    return root;
}

void Account::RollBack()
{
    if (!isContract())
        return;
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
        LOG_GENERAL(FATAL,
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
    assert(output.size() == 32);

    copy(output.end() - ACC_ADDR_SIZE, output.end(), address.asArray().begin());

    return address;
}

void Account::SetCode(const vector<unsigned char>& code)
{
    LOG_MARKER();
    if (code.size() == 0)
        return;
    m_codeCache = code;
    LOG_GENERAL(INFO,
                "Contract Data: \n"
                    << DataConversion::CharArrayToString(code));
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(code);
    m_codeHash = dev::h256(sha2.Finalize());

    InitStorage();
}
