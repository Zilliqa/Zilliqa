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

unsigned int Transaction::SerializeCoreFields(std::vector<unsigned char>& dst,
                                              unsigned int offset) const
{
    unsigned int size_needed = UINT256_SIZE /*m_version*/
        + UINT256_SIZE /*m_nonce*/ + ACC_ADDR_SIZE /*m_toAddr*/
        + PUB_KEY_SIZE /*m_senderPubKey*/ + UINT256_SIZE /*m_amount*/
        + UINT256_SIZE /*m_gasPrice*/ + UINT256_SIZE /*m_gasLimit*/
        + sizeof(uint32_t) + m_code.size() /*m_code*/
        + sizeof(uint32_t) + m_data.size() /*m_data*/;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    SetNumber<uint256_t>(dst, offset, m_version, UINT256_SIZE);
    offset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, offset, m_nonce, UINT256_SIZE);
    offset += UINT256_SIZE;
    copy(m_toAddr.asArray().begin(), m_toAddr.asArray().end(),
         dst.begin() + offset);
    offset += ACC_ADDR_SIZE;
    m_senderPubKey.Serialize(dst, offset);
    offset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(dst, offset, m_amount, UINT256_SIZE);
    offset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, offset, m_gasPrice, UINT256_SIZE);
    offset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, offset, m_gasLimit, UINT256_SIZE);
    offset += UINT256_SIZE;
    SetNumber<uint32_t>(dst, offset, (uint32_t)m_code.size(), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    copy(m_code.begin(), m_code.end(), dst.begin() + offset);
    offset += m_code.size();
    SetNumber<uint32_t>(dst, offset, (uint32_t)m_data.size(), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    copy(m_data.begin(), m_data.end(), dst.begin() + offset);

    return size_needed;
}

Transaction::Transaction() {}

Transaction::Transaction(const Transaction& src)
    : m_tranID(src.m_tranID)
    , m_version(src.m_version)
    , m_nonce(src.m_nonce)
    , m_toAddr(src.m_toAddr)
    , m_senderPubKey(src.m_senderPubKey)
    , m_amount(src.m_amount)
    , m_gasPrice(src.m_gasPrice)
    , m_gasLimit(src.m_gasLimit)
    , m_code(src.m_code)
    , m_data(src.m_data)
    , m_signature(src.m_signature)
{
}

Transaction::Transaction(const vector<unsigned char>& src, unsigned int offset)
{
    Deserialize(src, offset);
}

Transaction::Transaction(uint256_t version, const uint256_t& nonce,
                         const Address& toAddr, const KeyPair& senderKeyPair,
                         const uint256_t& amount, const uint256_t& gasPrice,
                         const uint256_t& gasLimit,
                         const vector<unsigned char>& code,
                         const vector<unsigned char>& data)
    : m_version(version)
    , m_nonce(nonce)
    , m_toAddr(toAddr)
    , m_senderPubKey(senderKeyPair.second)
    , m_amount(amount)
    , m_gasPrice(gasPrice)
    , m_gasLimit(gasLimit)
    , m_code(code)
    , m_data(data)
{
    vector<unsigned char> txnData;
    SerializeCoreFields(txnData, 0);

    // Generate the transaction ID
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(txnData);
    const vector<unsigned char>& output = sha2.Finalize();
    if (output.size() != TRAN_HASH_SIZE)
    {
        LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
        return;
    }
    copy(output.begin(), output.end(), m_tranID.asArray().begin());

    // Generate the signature
    if (Schnorr::GetInstance().Sign(txnData, senderKeyPair.first,
                                    m_senderPubKey, m_signature)
        == false)
    {
        LOG_GENERAL(WARNING, "We failed to generate m_signature.");
    }
}

Transaction::Transaction(uint256_t version, const uint256_t& nonce,
                         const Address& toAddr, const PubKey& senderPubKey,
                         const uint256_t& amount, const uint256_t& gasPrice,
                         const uint256_t& gasLimit,
                         const std::vector<unsigned char>& code,
                         const std::vector<unsigned char>& data,
                         const Signature& signature)
    : m_version(version)
    , m_nonce(nonce)
    , m_toAddr(toAddr)
    , m_senderPubKey(senderPubKey)
    , m_amount(amount)
    , m_gasPrice(gasPrice)
    , m_gasLimit(gasLimit)
    , m_code(code)
    , m_data(data)
    , m_signature(signature)
{
    vector<unsigned char> txnData;
    SerializeCoreFields(txnData, 0);

    // Generate the transaction ID
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(txnData);
    const vector<unsigned char>& output = sha2.Finalize();
    if (output.size() != TRAN_HASH_SIZE)
    {
        LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
        return;
    }
    copy(output.begin(), output.end(), m_tranID.asArray().begin());

    // Verify the signature
    if (Schnorr::GetInstance().Verify(txnData, m_signature, m_senderPubKey)
        == false)
    {
        LOG_GENERAL(WARNING, "We failed to verify the input signature.");
    }
}

unsigned int Transaction::Serialize(vector<unsigned char>& dst,
                                    unsigned int offset) const
{
    // LOG_MARKER();

    if ((dst.size() - offset) < TRAN_HASH_SIZE)
    {
        dst.resize(TRAN_HASH_SIZE + offset);
    }

    copy(m_tranID.asArray().begin(), m_tranID.asArray().end(),
         dst.begin() + offset);
    offset += TRAN_HASH_SIZE;
    offset += m_signature.Serialize(dst, offset);
    offset += SerializeCoreFields(dst, offset);

    return offset;
}

int Transaction::Deserialize(const vector<unsigned char>& src,
                             unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        copy(src.begin() + offset, src.begin() + offset + TRAN_HASH_SIZE,
             m_tranID.asArray().begin());
        offset += TRAN_HASH_SIZE;
        if (m_signature.Deserialize(src, offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init m_signature.");
            return -1;
        }
        offset += TRAN_SIG_SIZE;
        m_version = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        m_nonce = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        copy(src.begin() + offset, src.begin() + offset + ACC_ADDR_SIZE,
             m_toAddr.asArray().begin());
        offset += ACC_ADDR_SIZE;
        if (m_senderPubKey.Deserialize(src, offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init m_senderPubKey.");
            return -1;
        }
        offset += PUB_KEY_SIZE;
        m_amount = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        m_gasPrice = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        m_gasLimit = GetNumber<uint256_t>(src, offset, UINT256_SIZE);
        offset += UINT256_SIZE;
        uint32_t codeSize = GetNumber<uint32_t>(src, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        m_code.clear();
        if (codeSize > 0)
        {
            m_code.resize(codeSize);
            copy(src.begin() + offset, src.begin() + offset + codeSize,
                 m_code.begin());
        }
        offset += codeSize;
        uint32_t dataSize = GetNumber<uint32_t>(src, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        m_data.clear();
        if (dataSize > 0)
        {
            m_data.resize(dataSize);
            copy(src.begin() + offset, src.begin() + offset + dataSize,
                 m_data.begin());
        }
        offset += dataSize;
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

const uint256_t& Transaction::GetVersion() const { return m_version; }

const uint256_t& Transaction::GetNonce() const { return m_nonce; }

const Address& Transaction::GetToAddr() const { return m_toAddr; }

const PubKey& Transaction::GetSenderPubKey() const { return m_senderPubKey; }

const uint256_t& Transaction::GetAmount() const { return m_amount; }

const uint256_t& Transaction::GetGasPrice() const { return m_gasPrice; }

const uint256_t& Transaction::GetGasLimit() const { return m_gasLimit; }

const vector<unsigned char>& Transaction::GetCode() const { return m_code; }

const vector<unsigned char>& Transaction::GetData() const { return m_data; }

const Signature& Transaction::GetSignature() const { return m_signature; }

void Transaction::SetSignature(const Signature& signature)
{
    m_signature = signature;
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
    return TRAN_HASH_SIZE + TRAN_SIG_SIZE + UINT256_SIZE + UINT256_SIZE
        + ACC_ADDR_SIZE + PUB_KEY_SIZE + UINT256_SIZE + UINT256_SIZE
        + UINT256_SIZE + sizeof(uint32_t) + m_code.size() + sizeof(uint32_t)
        + m_data.size();
}

unsigned int Transaction::GetMinSerializedSize()
{
    return TRAN_HASH_SIZE + TRAN_SIG_SIZE + UINT256_SIZE + UINT256_SIZE
        + ACC_ADDR_SIZE + PUB_KEY_SIZE + UINT256_SIZE + UINT256_SIZE
        + UINT256_SIZE;
}

bool Transaction::operator==(const Transaction& tran) const
{
    return ((m_tranID == tran.m_tranID) && (m_signature == tran.m_signature));
}

bool Transaction::operator<(const Transaction& tran) const
{
    return (m_tranID < tran.m_tranID);
}

bool Transaction::operator>(const Transaction& tran) const
{
    return !((*this == tran) || (*this < tran));
}

Transaction& Transaction::operator=(const Transaction& src)
{
    copy(src.m_tranID.asArray().begin(), src.m_tranID.asArray().end(),
         m_tranID.asArray().begin());
    m_signature = src.m_signature;
    m_version = src.m_version;
    m_nonce = src.m_nonce;
    copy(src.m_toAddr.begin(), src.m_toAddr.end(), m_toAddr.asArray().begin());
    m_senderPubKey = src.m_senderPubKey;
    m_amount = src.m_amount;
    m_gasPrice = src.m_gasPrice;
    m_gasLimit = src.m_gasLimit;
    m_code = src.m_code;
    m_data = src.m_data;

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
    if((m_type & ACC_COND) != ACC_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return m_accConAddr;
}

uint8_t Predicate::GetAccConOp() const
{
    if((m_type & ACC_COND) != ACC_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return (m_ops & HIGH_BITS_MASK) >> 4;
}

uint256_t Predicate::GetAccConBalance() const
{
    if((m_type & ACC_COND) != ACC_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return m_accConBalance;
}

const array<unsigned char, ACC_ADDR_SIZE> & Predicate::GetTxConToAddr() const
{
    if((m_type & TX_COND) != TX_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return m_txConToAddr;
}

const array<unsigned char, ACC_ADDR_SIZE> & Predicate::GetTxConFromAddr() const
{
    if((m_type & TX_COND) != TX_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return m_txConFromAddr;
}

uint256_t Predicate::GetTxConAmount() const
{
    if((m_type & TX_COND) != TX_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

    return m_txConAmount;
}

unsigned char Predicate::GetTxConOp() const
{
    if((m_type & TX_COND) != TX_COND)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__
                                         << ": " << __FUNCTION__ << ")");
    }

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
