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
    m_storage = InSecureTrieDB<bytesConstRef, OverlayDB>(
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
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
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
            std::unique_ptr<Json::StreamWriter> writer(
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

    unsigned int size_needed = ACCOUNT_SIZE;
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
    }

    return size_needed;
}

int Account::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    LOG_MARKER();

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
        copy(src.begin() + curOffset,
             src.begin() + curOffset + COMMON_HASH_SIZE,
             m_storageRoot.asArray().begin());
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
            SetCode(code);
            curOffset += codeSize;
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

bool Account::IncreaseNonce()
{
    ++m_nonce;
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

void Account::SetStorage(string _k, string _type, string _v, bool _mutable)
{
    if (!isContract())
        return;
    LOG_GENERAL(INFO, "SetStorage key: " << _k);
    RLPStream rlpStream(3);
    rlpStream << (_mutable ? "True" : "False") << _type << _v;
    // vector<unsigned char> k_bytes(_k.begin(), _k.end());
    // LOG_GENERAL(INFO,
    // "Key to Insert: " << string(k_bytes.begin(), k_bytes.end()));
    // m_storage.insert(bytesConstRef(k_bytes.data(), k_bytes.size()),
    //                  rlpStream.out());
    m_storage.insert(_k, rlpStream.out());

    // for (auto i : m_storage)
    // {
    //     dev::RLP rlp(i.second);
    //     LOG_GENERAL(INFO,
    //                 "ITERATE k:" << i.first.toString()
    //                              << " v[0]:" << rlp[0].toString()
    //                              << " v[1]:" << rlp[1].toString()
    //                              << " v[2]:" << rlp[2].toString());
    //     //     ret.push_back(i.first.toString());
    // }

    m_keys.push_back(_k);
    m_storageRoot = m_storage.root();
}

vector<string> Account::GetKeys() const
{
    // if (!isContract())
    //     return {};
    // vector<string> ret;
    for (auto i : m_storage)
    {
        dev::RLP rlp(i.second);
        LOG_GENERAL(INFO,
                    "ITERATE k:" << i.first.toString()
                                 << " v[0]:" << rlp[0].toString()
                                 << " v[1]:" << rlp[1].toString()
                                 << " v[2]:" << rlp[2].toString());
        //     ret.push_back(i.first.toString());
    }

    for (auto i : m_keys)
    {
        LOG_GENERAL(INFO, "my_key: " << i);
    }
    // return ret;
    return m_keys;
}

vector<string> Account::GetStorage(string _k) const
{
    if (!isContract())
        return {};
    // vector<unsigned char> k_bytes(_k.begin(), _k.end());
    // dev::RLP rlp(m_storage[bytesConstRef(k_bytes.data(), k_bytes.size())]);
    dev::RLP rlp(m_storage[_k]);
    // mutable, type, value
    return {rlp[0].toString(), rlp[1].toString(), rlp[2].toString()};
}

Json::Value Account::GetStorageJson() const
{
    Json::Value root;
    // vector<string> keys = GetKeys();
    // if (keys.empty())
    // {
    //     root = Json::arrayValue;
    // }
    // for (auto k : keys)
    // {
    // vector<string> v = GetStorage(k);
    // if (v[0] == "False")
    // {
    //     continue;
    // }
    // Json::Value item;
    // item["vname"] = k;
    // LOG_GENERAL(INFO, "GetStorage vname: " << k);
    // item["type"] = v[1];
    // LOG_GENERAL(INFO, "GetStorage type: " << v[1]);
    // if (v[1] == "Map" || v[1] == "ADT")
    // {
    //     Json::CharReaderBuilder builder;
    //     std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    //     Json::Value obj;
    //     string errors;
    //     if (reader->parse(v[2].c_str(), v[2].c_str() + v[2].size(), &obj,
    //                       &errors))
    //     {
    //         item["value"] = obj;
    //     }
    //     else
    //     {
    //         LOG_GENERAL(
    //             WARNING,
    //             "The map json object cannot be extracted from Storage: "
    //                 << errors);
    //     }
    // }
    // else
    // {
    //     item["value"] = v[2];
    // }
    // LOG_GENERAL(INFO, "GetStorage value: " << v[2]);
    // root.append(item);
    // }
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
    assert(output.size() == 32);

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

void Account::SetCode(const std::vector<unsigned char>& code)
{
    if (code.size() == 0)
        return;
    m_codeCache = code;
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(code);
    m_codeHash = dev::h256(sha2.Finalize());

    InitStorage();
}