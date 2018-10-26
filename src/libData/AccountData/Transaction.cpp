/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "Transaction.h"
#include <algorithm>
#include "Account.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned char HIGH_BITS_MASK = 0xF0;
unsigned char LOW_BITS_MASK = 0x0F;
unsigned char ACC_COND = 0x1;
unsigned char TX_COND = 0x2;

unsigned int Transaction::SerializeCoreFields(std::vector<unsigned char>& dst,
                                              unsigned int offset) const {
  unsigned int size_needed =
      UINT256_SIZE                                     /*m_version*/
      + UINT256_SIZE /*m_nonce*/ + ACC_ADDR_SIZE       /*m_toAddr*/
      + PUB_KEY_SIZE /*m_senderPubKey*/ + UINT256_SIZE /*m_amount*/
      + UINT256_SIZE /*m_gasPrice*/ + UINT256_SIZE     /*m_gasLimit*/
      + sizeof(uint32_t) + m_code.size()               /*m_code*/
      + sizeof(uint32_t) + m_data.size();              /*m_data*/

  if (dst.size() < size_needed + offset) {
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
  offset += m_data.size();

  return size_needed;
}

Transaction::Transaction() {}

Transaction::Transaction(const Transaction& src)
    : m_tranID(src.m_tranID),
      m_version(src.m_version),
      m_nonce(src.m_nonce),
      m_toAddr(src.m_toAddr),
      m_senderPubKey(src.m_senderPubKey),
      m_amount(src.m_amount),
      m_gasPrice(src.m_gasPrice),
      m_gasLimit(src.m_gasLimit),
      m_code(src.m_code),
      m_data(src.m_data),
      m_signature(src.m_signature) {}

Transaction::Transaction(const vector<unsigned char>& src,
                         unsigned int offset) {
  Deserialize(src, offset);
}

Transaction::Transaction(const uint256_t& version, const uint256_t& nonce,
                         const Address& toAddr, const KeyPair& senderKeyPair,
                         const uint256_t& amount, const uint256_t& gasPrice,
                         const uint256_t& gasLimit,
                         const vector<unsigned char>& code,
                         const vector<unsigned char>& data)
    : m_version(version),
      m_nonce(nonce),
      m_toAddr(toAddr),
      m_senderPubKey(senderKeyPair.second),
      m_amount(amount),
      m_gasPrice(gasPrice),
      m_gasLimit(gasLimit),
      m_code(code),
      m_data(data) {
  vector<unsigned char> txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const vector<unsigned char>& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Generate the signature
  if (!Schnorr::GetInstance().Sign(txnData, senderKeyPair.first, m_senderPubKey,
                                   m_signature)) {
    LOG_GENERAL(WARNING, "We failed to generate m_signature.");
  }
}

Transaction::Transaction(const TxnHash& tranID, const uint256_t& version,
                         const uint256_t& nonce, const Address& toAddr,
                         const PubKey& senderPubKey, const uint256_t& amount,
                         const uint256_t& gasPrice, const uint256_t& gasLimit,
                         const std::vector<unsigned char>& code,
                         const std::vector<unsigned char>& data,
                         const Signature& signature)
    : m_tranID(tranID),
      m_version(version),
      m_nonce(nonce),
      m_toAddr(toAddr),
      m_senderPubKey(senderPubKey),
      m_amount(amount),
      m_gasPrice(gasPrice),
      m_gasLimit(gasLimit),
      m_code(code),
      m_data(data),
      m_signature(signature) {}

Transaction::Transaction(const uint256_t& version, const uint256_t& nonce,
                         const Address& toAddr, const PubKey& senderPubKey,
                         const uint256_t& amount, const uint256_t& gasPrice,
                         const uint256_t& gasLimit,
                         const std::vector<unsigned char>& code,
                         const std::vector<unsigned char>& data,
                         const Signature& signature)
    : m_version(version),
      m_nonce(nonce),
      m_toAddr(toAddr),
      m_senderPubKey(senderPubKey),
      m_amount(amount),
      m_gasPrice(gasPrice),
      m_gasLimit(gasLimit),
      m_code(code),
      m_data(data),
      m_signature(signature) {
  vector<unsigned char> txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const vector<unsigned char>& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Verify the signature
  if (!Schnorr::GetInstance().Verify(txnData, m_signature, m_senderPubKey)) {
    LOG_GENERAL(WARNING, "We failed to verify the input signature.");
  }
}

bool Transaction::Serialize(vector<unsigned char>& dst,
                            unsigned int offset) const {
  if (!Messenger::SetTransaction(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransaction failed.");
    return false;
  }

  return true;
}

bool Transaction::Deserialize(const vector<unsigned char>& src,
                              unsigned int offset) {
  if (!Messenger::GetTransaction(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransaction failed.");
    return false;
  }

  return true;
}

const TxnHash& Transaction::GetTranID() const { return m_tranID; }

const uint256_t& Transaction::GetVersion() const { return m_version; }

const uint256_t& Transaction::GetNonce() const { return m_nonce; }

const Address& Transaction::GetToAddr() const { return m_toAddr; }

const PubKey& Transaction::GetSenderPubKey() const { return m_senderPubKey; }

Address Transaction::GetSenderAddr() const {
  return Account::GetAddressFromPublicKey(GetSenderPubKey());
}

const uint256_t& Transaction::GetAmount() const { return m_amount; }

const uint256_t& Transaction::GetGasPrice() const { return m_gasPrice; }

const uint256_t& Transaction::GetGasLimit() const { return m_gasLimit; }

const vector<unsigned char>& Transaction::GetCode() const { return m_code; }

const vector<unsigned char>& Transaction::GetData() const { return m_data; }

const Signature& Transaction::GetSignature() const { return m_signature; }

void Transaction::SetSignature(const Signature& signature) {
  m_signature = signature;
}

unsigned int Transaction::GetShardIndex(const Address& fromAddr,
                                        unsigned int numShards) {
  unsigned int target_shard = 0;
  unsigned int numbits = log2(numShards);
  unsigned int numbytes = numbits / 8;
  unsigned int extrabits = numbits % 8;

  if (extrabits > 0) {
    unsigned char msb_mask = 0;
    for (unsigned int i = 0; i < extrabits; i++) {
      msb_mask |= 1 << i;
    }
    target_shard =
        fromAddr.asArray().at(ACC_ADDR_SIZE - numbytes - 1) & msb_mask;
  }

  for (unsigned int i = ACC_ADDR_SIZE - numbytes; i < ACC_ADDR_SIZE; i++) {
    target_shard = (target_shard << 8) + fromAddr.asArray().at(i);
  }

  return target_shard;
}

bool Transaction::operator==(const Transaction& tran) const {
  return ((m_tranID == tran.m_tranID) && (m_signature == tran.m_signature));
}

bool Transaction::operator<(const Transaction& tran) const {
  return (m_tranID < tran.m_tranID);
}

bool Transaction::operator>(const Transaction& tran) const {
  return !((*this == tran) || (*this < tran));
}

Transaction& Transaction::operator=(const Transaction& src) {
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