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

#include "Transaction.h"
#include "libCrypto/Sha2.h"
#include "libUtils/Logger.h"
#include <algorithm>

using namespace std;
using namespace boost::multiprecision;

unsigned char HIGH_BITS_MASK = 0xF0;
unsigned char LOW_BITS_MASK = 0x0F;
unsigned char ACC_COND = 0x1;
unsigned char TX_COND = 0x2;

Transaction::Transaction() {}

Transaction::Transaction(const vector<unsigned char>& src, unsigned int offset)
{
    Deserialize(src, offset);
}

Transaction::Transaction(uint32_t version, const uint256_t& nonce,
                         const Address& toAddr, const PubKey& senderPubKey,
                         const uint256_t& amount,
                         const array<unsigned char, TRAN_SIG_SIZE>& signature)
    : m_version(version)
    , m_nonce(nonce)
    , m_toAddr(toAddr)
    , m_senderPubKey(senderPubKey)
    , m_amount(amount)
    , m_signature(signature) //, m_pred(pred)
{
    // [TODO] m_signature should be generated from the rest

    vector<unsigned char> vec;
    vec.resize(sizeof(uint32_t) + UINT256_SIZE + ACC_ADDR_SIZE + PUB_KEY_SIZE
               + UINT256_SIZE);

    unsigned int curOffset = 0;

    SetNumber<uint32_t>(vec, curOffset, m_version, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint256_t>(vec, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_toAddr.asArray().begin(), m_toAddr.asArray().end(),
         vec.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    m_senderPubKey.Serialize(vec, curOffset);
    curOffset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(vec, curOffset, m_amount, UINT256_SIZE);

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    const vector<unsigned char>& output = sha2.Finalize();

    assert(output.size() == 32);

    copy(output.begin(), output.end(), m_tranID.asArray().begin());
}

unsigned int Transaction::Serialize(vector<unsigned char>& dst,
                                    unsigned int offset) const
{
    // LOG_MARKER();

    unsigned int size_needed = TRAN_HASH_SIZE + sizeof(uint32_t) + UINT256_SIZE
        + PUB_KEY_SIZE + ACC_ADDR_SIZE + UINT256_SIZE
        + TRAN_SIG_SIZE; // + predicate_size_needed;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    copy(m_tranID.asArray().begin(), m_tranID.asArray().end(),
         dst.begin() + curOffset);
    curOffset += TRAN_HASH_SIZE;
    SetNumber<uint32_t>(dst, curOffset, m_version, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_toAddr.asArray().begin(), m_toAddr.asArray().end(),
         dst.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    m_senderPubKey.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_amount, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_signature.begin(), m_signature.end(), dst.begin() + curOffset);

    return size_needed;
}

//TODO: eliminiate the duplicated code by reusing this inside Transaction::Serialize
unsigned int Transaction::SerializeWithoutSignature(vector<unsigned char>& dst,
                                                    unsigned int offset) const
{
    unsigned int size_needed = TRAN_HASH_SIZE + sizeof(uint32_t) + UINT256_SIZE
        + PUB_KEY_SIZE + ACC_ADDR_SIZE + UINT256_SIZE;
    assert(dst.size() > offset);
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    copy(m_tranID.asArray().begin(), m_tranID.asArray().end(),
         dst.begin() + curOffset);
    curOffset += TRAN_HASH_SIZE;
    SetNumber<uint32_t>(dst, curOffset, m_version, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_toAddr.asArray().begin(), m_toAddr.asArray().end(),
         dst.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    m_senderPubKey.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_amount, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    return size_needed;
}

int Transaction::Deserialize(const vector<unsigned char>& src,
                             unsigned int offset)
{
    // LOG_MARKER();

    unsigned int curOffset = offset;

    try
    {
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_HASH_SIZE,
             m_tranID.asArray().begin());
        curOffset += TRAN_HASH_SIZE;
        m_version = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_nonce = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + ACC_ADDR_SIZE,
             m_toAddr.asArray().begin());
        curOffset += ACC_ADDR_SIZE;
        // m_senderPubKey.Deserialize(src, curOffset);
        if (m_senderPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init m_senderPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        m_amount = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_SIG_SIZE,
             m_signature.begin());
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Transaction::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

const TxnHash& Transaction::GetTranID() const { return m_tranID; }

const uint32_t& Transaction::GetVersion() const { return m_version; }

const uint256_t& Transaction::GetNonce() const { return m_nonce; }

const Address& Transaction::GetToAddr() const { return m_toAddr; }

const PubKey& Transaction::GetSenderPubKey() const { return m_senderPubKey; }

const uint256_t& Transaction::GetAmount() const { return m_amount; }

const array<unsigned char, TRAN_SIG_SIZE>& Transaction::GetSignature() const
{
    return m_signature;
}

void Transaction::SetSignature(std::array<unsigned char, TRAN_SIG_SIZE> sig)
{
    m_signature = sig;
}

void Transaction::SetSignature(std::vector<unsigned char> sig)
{
    copy_n(sig.begin(), min(sig.size(), m_signature.size()),
           m_signature.begin());
}

unsigned int Transaction::GetShardIndex(const Address& fromAddr,
                                        unsigned int numShards)
{
    unsigned int target_shard = 0;
    unsigned int numbits = log2(numShards);
    unsigned int numbytes = numbits / 8;
    unsigned int extrabits = numbits % 8;

    if (extrabits > 0)
    {
        unsigned char msb_mask = 0;
        for (unsigned int i = 0; i < extrabits; i++)
        {
            msb_mask |= 1 << i;
        }
        target_shard
            = fromAddr.asArray().at(ACC_ADDR_SIZE - numbytes - 1) & msb_mask;
    }

    for (unsigned int i = ACC_ADDR_SIZE - numbytes; i < ACC_ADDR_SIZE; i++)
    {
        target_shard = (target_shard << 8) + fromAddr.asArray().at(i);
    }

    return target_shard;
}

unsigned int Transaction::GetSerializedSize()
{
    unsigned int size_needed_wo_predicate = TRAN_HASH_SIZE + sizeof(uint32_t)
        + UINT256_SIZE + ACC_ADDR_SIZE + PUB_KEY_SIZE + UINT256_SIZE
        + TRAN_SIG_SIZE;

    return size_needed_wo_predicate;
}

bool Transaction::Verify(const Transaction& tran)
{

    vector<unsigned char> data;
    data.resize(sizeof(uint32_t) + UINT256_SIZE + ACC_ADDR_SIZE + PUB_KEY_SIZE
                + UINT256_SIZE);
    unsigned int curOffset = 0;

    SetNumber<uint32_t>(data, curOffset, tran.m_version, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);

    SetNumber<uint256_t>(data, curOffset, tran.m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    copy(tran.m_toAddr.begin(), tran.m_toAddr.end(), data.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;

    tran.m_senderPubKey.Serialize(data, curOffset);
    curOffset += PUB_KEY_SIZE;

    SetNumber<uint256_t>(data, curOffset, tran.m_amount, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    vector<unsigned char> sign_ser;
    sign_ser.resize(TRAN_SIG_SIZE);
    copy(tran.m_signature.begin(), tran.m_signature.end(), sign_ser.begin());

    // Signature sign(sign_ser,0);
    Signature sign;
    if (sign.Deserialize(sign_ser, 0) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize sign.");
        return false;
    }

    return Schnorr::GetInstance().Verify(data, sign, tran.m_senderPubKey);
}

bool Transaction::operator==(const Transaction& tran) const
{
    return ((m_tranID == tran.m_tranID) && (m_version == tran.m_version)
            && (m_nonce == tran.m_nonce) && (m_toAddr == tran.m_toAddr)
            && (m_senderPubKey == tran.m_senderPubKey)
            && (m_amount == tran.m_amount)
            && (m_signature == tran.m_signature));
}

bool Transaction::operator<(const Transaction& tran) const
{
    if (m_tranID < tran.m_tranID)
    {
        return true;
    }
    else if (m_tranID > tran.m_tranID)
    {
        return false;
    }
    else if (m_version < tran.m_version)
    {
        return true;
    }
    else if (m_version > tran.m_version)
    {
        return false;
    }
    else if (m_nonce < tran.m_nonce)
    {
        return true;
    }
    else if (m_nonce > tran.m_nonce)
    {
        return false;
    }
    else if (m_toAddr < tran.m_toAddr)
    {
        return true;
    }
    else if (m_toAddr > tran.m_toAddr)
    {
        return false;
    }
    else if (m_senderPubKey < tran.m_senderPubKey)
    {
        return true;
    }
    else if (m_senderPubKey > tran.m_senderPubKey)
    {
        return false;
    }
    else if (m_amount < tran.m_amount)
    {
        return true;
    }
    else if (m_amount > tran.m_amount)
    {
        return false;
    }
    else if (m_signature < tran.m_signature)
    {
        return true;
    }
    else if (m_signature > tran.m_signature)
    {
        return false;
    }
#if 0
    else if (m_pred < tran.m_pred)
    {
        return true;
    }
#endif
    else
    {
        return false;
    }
}

bool Transaction::operator>(const Transaction& tran) const
{
    return !((*this == tran) || (*this < tran));
}

Transaction& Transaction::operator=(const Transaction& src)
{
    copy(src.m_tranID.asArray().begin(), src.m_tranID.asArray().end(),
         m_tranID.asArray().begin());
    m_version = src.m_version;
    m_nonce = src.m_nonce;
    copy(src.m_toAddr.begin(), src.m_toAddr.end(), m_toAddr.asArray().begin());
    m_senderPubKey = src.m_senderPubKey;
    m_amount = src.m_amount;
    copy(src.m_signature.begin(), src.m_signature.end(), m_signature.begin());

    return *this;
}

#if 0

unsigned int Predicate::Serialize(vector<unsigned char> & dst, unsigned int offset) const
{
    LOG_MARKER();

    unsigned int size_needed = sizeof(uint8_t) + ACC_ADDR_SIZE + sizeof(uint8_t) + UINT256_SIZE + ACC_ADDR_SIZE + ACC_ADDR_SIZE + UINT256_SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    dst.at(curOffset) = m_type;
    curOffset += sizeof(uint8_t);
    copy(m_accConAddr.begin(), m_accConAddr.end(), dst.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    dst.at(curOffset) = m_ops;
    curOffset += sizeof(uint8_t);
    SetNumber<uint256_t>(dst, curOffset, m_accConBalance, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_txConToAddr.begin(), m_txConToAddr.end(), dst.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    copy(m_txConFromAddr.begin(), m_txConFromAddr.end(), dst.begin() + curOffset);
    curOffset += ACC_ADDR_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_txConAmount, UINT256_SIZE);

    return size_needed;
}

void Predicate::Deserialize(const vector<unsigned char> & src, unsigned int offset)
{
    LOG_MARKER();

    unsigned int curOffset = offset;

    m_type = src.at(curOffset);
    curOffset += sizeof(uint8_t);
    copy(src.begin() + offset + 1, src.begin() + offset + 1 + ACC_ADDR_SIZE, m_accConAddr.begin());
    curOffset += ACC_ADDR_SIZE;
    m_ops = src.at(curOffset);
    curOffset += sizeof(uint8_t);
    m_accConBalance = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(src.begin() + curOffset, src.begin() + curOffset + ACC_ADDR_SIZE, m_txConToAddr.begin());
    curOffset += ACC_ADDR_SIZE;
    copy(src.begin() + curOffset, src.begin() + curOffset+ ACC_ADDR_SIZE, m_txConFromAddr.begin());
    curOffset += ACC_ADDR_SIZE;
    m_txConAmount = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
}

Predicate::Predicate()
{
    m_type = 0;
}

Predicate::Predicate(const vector<unsigned char> & src, unsigned int offset)
{
    Deserialize(src, offset);
}

Predicate::Predicate(uint8_t type, const array<unsigned char, ACC_ADDR_SIZE> & accConAddr, unsigned char ops, uint256_t accConBalance, const array<unsigned char, ACC_ADDR_SIZE> & txConToAddr, const array<unsigned char, ACC_ADDR_SIZE> & txConFromAddr, uint256_t txConAmount)
    : m_type(type), m_accConAddr(accConAddr), m_ops(ops), m_accConBalance(accConBalance), m_txConToAddr(txConToAddr), m_txConFromAddr(txConFromAddr), m_txConAmount(txConAmount)
{

}

Predicate::Predicate(uint8_t type, const array<unsigned char, ACC_ADDR_SIZE> & accConAddr, unsigned char accConOp, uint256_t accConBalance, const array<unsigned char, ACC_ADDR_SIZE> & txConToAddr, const array<unsigned char, ACC_ADDR_SIZE> & txConFromAddr, uint256_t txConAmount, unsigned char txConOp)
    : m_type(type), m_accConAddr(accConAddr), m_accConBalance(accConBalance), m_txConToAddr(txConToAddr), m_txConFromAddr(txConFromAddr), m_txConAmount(txConAmount)
{
    m_ops = txConOp;
    m_ops |= (accConOp << 4);
}

uint8_t Predicate::GetType() const
{
    return m_type;
}

const array<unsigned char, ACC_ADDR_SIZE> & Predicate::GetAccConAddr() const
{
    assert((m_type & ACC_COND) == ACC_COND);

    return m_accConAddr;
}

uint8_t Predicate::GetAccConOp() const
{
    assert((m_type & ACC_COND) == ACC_COND);

    return (m_ops & HIGH_BITS_MASK) >> 4;
}

uint256_t Predicate::GetAccConBalance() const
{
    assert((m_type & ACC_COND) == ACC_COND);

    return m_accConBalance;
}

const array<unsigned char, ACC_ADDR_SIZE> & Predicate::GetTxConToAddr() const
{
    assert((m_type & TX_COND) == TX_COND);

    return m_txConToAddr;
}

const array<unsigned char, ACC_ADDR_SIZE> & Predicate::GetTxConFromAddr() const
{
    assert((m_type & TX_COND) == TX_COND);

    return m_txConFromAddr;
}

uint256_t Predicate::GetTxConAmount() const
{
    assert((m_type & TX_COND) == TX_COND);

    return m_txConAmount;
}

unsigned char Predicate::GetTxConOp() const
{
    assert((m_type & TX_COND) == TX_COND);

    return m_ops & LOW_BITS_MASK;
}

bool Predicate::operator==(const Predicate & pred) const
{
    return ((m_type == pred.m_type) && (m_accConAddr == pred.m_accConAddr) && (m_ops == pred.m_ops) && (m_accConBalance == pred.m_accConBalance) && (m_txConToAddr == pred.m_txConToAddr) && (m_txConFromAddr == pred.m_txConFromAddr) && (m_txConAmount == pred.m_txConAmount));
}

bool Predicate::operator<(const Predicate & pred) const
{
    if (m_type < pred.m_type)
    {
        return true;
    }
    else if (m_type > pred.m_type)
    {
        return false;
    }
    else if (m_accConAddr < pred.m_accConAddr)
    {
        return true;
    }
    else if (m_accConAddr > pred.m_accConAddr)
    {
        return false;
    }
    else if (m_ops < pred.m_ops)
    {
        return true;
    }
    else if (m_ops > pred.m_ops)
    {
        return false;
    }
    else if (m_accConBalance < pred.m_accConBalance)
    {
        return true;
    }
    else if (m_accConBalance > pred.m_accConBalance)
    {
        return false;
    }
    else if (m_txConToAddr < pred.m_txConToAddr)
    {
        return true;
    }
    else if (m_txConToAddr > pred.m_txConToAddr)
    {
        return false;
    }
    else if (m_txConFromAddr < pred.m_txConFromAddr)
    {
        return true;
    }
    else if (m_txConFromAddr > pred.m_txConFromAddr)
    {
        return false;
    }
    else if (m_txConAmount < pred.m_txConAmount)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Predicate::operator>(const Predicate & pred) const
{
    return !((*this == pred) || ((*this < pred)));
}

#endif
