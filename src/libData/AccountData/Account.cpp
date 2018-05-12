/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/

#include "Account.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

Account::Account() { InitStorage(); }

Account::Account(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init Account.");
    }
    InitStorage();
}

Account::Account(const uint256_t& balance, const uint256_t& nonce,
                 const dev::h256& storageRoot, const dev::h256& codeHash)
    : m_balance(balance)
    , m_nonce(nonce)
    , m_storageRoot(storageRoot)
    , m_codeHash(codeHash)
{
    InitStorage();
}

void Account::InitStorage()
{
    m_storage = SecureTrieDB<bytesConstRef, OverlayDB>(
        &(ContractStorage::GetContractStorage().GetDB()));
    m_storage.init();
    if (m_storageRoot != h256())
    {
        m_storage.setRoot(m_storageRoot);
        m_prevRoot = m_storageRoot;
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

    SetNumber<uint256_t>(dst, curOffset, m_balance, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_storageRoot.asArray().begin(), m_storageRoot.asArray().end(),
         dst.begin() + curOffset);
    curOffset += COMMON_HASH_SIZE;
    copy(m_codeHash.asArray().begin(), m_codeHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += COMMON_HASH_SIZE;

    return size_needed;
}

int Account::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        unsigned int curOffset = offset;

        m_balance = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        m_nonce = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset,
             src.begin() + curOffset + COMMON_HASH_SIZE,
             m_storageRoot.asArray().begin());
        curOffset += COMMON_HASH_SIZE;
        copy(src.begin() + curOffset,
             src.begin() + curOffset + COMMON_HASH_SIZE,
             m_codeHash.asArray().begin());
        curOffset += COMMON_HASH_SIZE;
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

void Account::SetStorage(string _k, string _mutable, string _type, string _v)
{
    RLPStream rlpStream(3);
    rlpStream << _mutable << _type << _v;
    m_storage.insert(
        bytesConstRef(DataConversion::HexStrToUint8Vec(_k).data(), _k.size()),
        rlpStream.out());
    m_storageRoot = m_storage.root();
}

vector<string> Account::GetKeys()
{
    vector<string> ret;
    for (auto i : m_storage)
    {
        ret.push_back(i.first.toString());
    }
    return ret;
}

vector<string> Account::GetStorage(string _k)
{
    dev::RLP rlp(m_storage[_k]);
    return {rlp[0].toString(), rlp[1].toString(), rlp[2].toString()};
}

string Account::GetStorageValue(string _k) { return GetStorage(_k)[2]; }

void Account::RollBack()
{
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

void Account::SetCode(std::vector<unsigned char>&& code)
{
    m_codeCache = std::move(code);

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(m_codeCache);
    m_codeHash = dev::h256(sha2.Finalize());
}