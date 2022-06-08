/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Transaction.h"
#include <algorithm>
#include "Account.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

// todo: delete these after
#include <openssl/ec.h>      // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>   // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h> // for NID_secp192k1

using namespace std;
using namespace boost::multiprecision;

unsigned char HIGH_BITS_MASK = 0xF0;
unsigned char LOW_BITS_MASK = 0x0F;
unsigned char ACC_COND = 0x1;
unsigned char TX_COND = 0x2;

///////////////////////////////////////////////////////////////////////
bool verify_signature(const unsigned char* hash, const ECDSA_SIG* signature, EC_KEY* eckey)
{
    int verify_status = ECDSA_do_verify(hash, strlen((const char*)hash), signature, eckey);
    if (1 != verify_status)
    {
        printf("Failed to verify EC Signature\n");
        return false;
    }

    printf("Verifed EC Signature\n");

    return true;
}



void SetOpensslSignature(const std::string& sSignatureInHex, ECDSA_SIG* pSign)
{
    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> rr(NULL, [](BIGNUM* b) { BN_free(b); });
    BIGNUM* r_ptr = rr.get();
    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> ss(NULL, [](BIGNUM* b) { BN_free(b); });
    BIGNUM* s_ptr = ss.get();

    std::string sSignatureR = sSignatureInHex.substr(0, sSignatureInHex.size() / 2);
    std::string sSignatureS = sSignatureInHex.substr(sSignatureInHex.size() / 2);

    BN_hex2bn(&r_ptr, sSignatureR.c_str());
    BN_hex2bn(&s_ptr, sSignatureS.c_str());

    ECDSA_SIG_set0(pSign, r_ptr, s_ptr);

    return;
}

bool SetOpensslPublicKey(const std::string& sPublicKeyInHex, EC_KEY* pKey)
{
    const char* sPubKeyString = sPublicKeyInHex.c_str();

    char cx[65];

    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gx(NULL, [](BIGNUM* b) { BN_free(b); });
    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gy(NULL, [](BIGNUM* b) { BN_free(b); });

    BIGNUM* gx_ptr = gx.get();
    BIGNUM* gy_ptr = gy.get();

    EC_KEY_set_asn1_flag(pKey, OPENSSL_EC_NAMED_CURVE);
    memcpy(cx, sPubKeyString, 64);
    cx[64] = 0;

    if (!BN_hex2bn(&gx_ptr, cx)) {
        std::cout << "Error getting to binary format" << std::endl;
    }

    if (!BN_hex2bn(&gy_ptr, &sPubKeyString[64])) {
        std::cout << "Error getting to binary format" << std::endl;
    }

    if (!EC_KEY_set_public_key_affine_coordinates(pKey, gx_ptr, gy_ptr)) {
        std::cout << "setting public key attributes" << std::endl;
    }

    if (EC_KEY_check_key(pKey) == 1)
    {
        printf("EC Key valid.\n");
        return true;
    }
    else {
        printf("EC Key Invalid!\n");
        return false;
    }
}

std::string sha256(const std::string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hash[i];
    }
    return ss.str();
}


bool Verify(const std::string& sRandomNumber, const std::string& sSignature, const std::string& sDevicePubKeyInHex)
{
    std::unique_ptr< ECDSA_SIG, std::function<void(ECDSA_SIG*)>> zSignature(ECDSA_SIG_new(), [](ECDSA_SIG* b) { ECDSA_SIG_free(b); });
    // Set up the signature...
    SetOpensslSignature(sSignature, zSignature.get());

    std::unique_ptr< EC_KEY, std::function<void(EC_KEY*)>> zPublicKey(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), [](EC_KEY* b) { EC_KEY_free(b); });
    if (!SetOpensslPublicKey(sDevicePubKeyInHex, zPublicKey.get()))
        std::cout << "Failed to get the public key from the hex input" << std::endl;

    std::string sHash = sha256(sRandomNumber);

    return verify_signature((const unsigned char*)sHash.c_str(), zSignature.get(), zPublicKey.get());
}


int VerifyEcdsaSecp256k1(std::string const &sRandomNumber, std::string const &sSignatureInHex, std::string const &sPublicKeyInHex)
{
    //std::string sSignatureInHex = "228B756444CFF74453ABA22BF1FD052965682FDFDC915647F8B07068636BE6827938ED61B6C388551A6D4CCF3397858E14F5EA648FE13454C13292364BB40C1C";
    //std::string sPublicKeyInHex = "94E62E0C77A2955B1FB3EE98AEAA99AACAD742F20E45B727EACDD10487C2F7D0D8257C6102921880ABE953245D573D7E33EC88A67E2BA930980CB9C3D6722F8A";
    //std::string sRandomNumber = "65560886818773090201885807838738706912015073749623293202319529";

    //sRandomNumber = "65560886818773090201885807838738706912015073749623293202319529";
    //sSignatureInHex = "D506D976EC17DD3717C40329E28FD8DB4F32D6A3773454A6427FD12E69728157508086B661D91E07ADF5B57E787EA1EEA526A84500436E430E89B1C1F8532A41";

    int ret = 0;

    if (!Verify(sRandomNumber, sSignatureInHex, sPublicKeyInHex))
        std::cout << "Verification failed." << std::endl;
    else {
      std::cout << "Verification succeeded" << std::endl;
      ret = 1;
    }

    return ret;
}
///////////////////////////////////////////////////////////////////////

bool Transaction::SerializeCoreFields(bytes& dst, unsigned int offset) const {
  return Messenger::SetTransactionCoreInfo(dst, offset, m_coreInfo);
}

Transaction::Transaction() {}

Transaction::Transaction(const bytes& src, unsigned int offset) {
  Deserialize(src, offset);
}

Transaction::Transaction(const uint32_t& version, const uint64_t& nonce,
                         const Address& toAddr, const PairOfKey& senderKeyPair,
                         const uint128_t& amount, const uint128_t& gasPrice,
                         const uint64_t& gasLimit, const bytes& code,
                         const bytes& data)
    : m_coreInfo(version, nonce, toAddr, senderKeyPair.second, amount, gasPrice,
                 gasLimit, code, data) {
  bytes txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Generate the signature
  if (!Schnorr::Sign(txnData, senderKeyPair.first, m_coreInfo.senderPubKey,
                     m_signature)) {
    LOG_GENERAL(WARNING, "We failed to generate m_signature.");
  }
}

Transaction::Transaction(const TxnHash& tranID, const uint32_t& version,
                         const uint64_t& nonce, const Address& toAddr,
                         const PubKey& senderPubKey, const uint128_t& amount,
                         const uint128_t& gasPrice, const uint64_t& gasLimit,
                         const bytes& code, const bytes& data,
                         const Signature& signature)
    : m_tranID(tranID),
      m_coreInfo(version, nonce, toAddr, senderPubKey, amount, gasPrice,
                 gasLimit, code, data),
      m_signature(signature) {}

Transaction::Transaction(const uint32_t& version, const uint64_t& nonce,
                         const Address& toAddr, const PubKey& senderPubKey,
                         const uint128_t& amount, const uint128_t& gasPrice,
                         const uint64_t& gasLimit, const bytes& code,
                         const bytes& data, const Signature& signature)
    : m_coreInfo(version, nonce, toAddr, senderPubKey, amount, gasPrice,
                 gasLimit, code, data),
      m_signature(signature) {
  bytes txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& output = sha2.Finalize();
  if (output.size() != TRAN_HASH_SIZE) {
    LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
    return;
  }
  copy(output.begin(), output.end(), m_tranID.asArray().begin());

  // Verify the signature
  if (!Schnorr::Verify(txnData, m_signature, m_coreInfo.senderPubKey)) {
    LOG_GENERAL(WARNING, "We failed to verify the input signature! Just a warning...");
    LOG_GENERAL(WARNING, m_signature.operator std::string());
  }
}

Transaction::Transaction(const TxnHash& tranID,
                         const TransactionCoreInfo& coreInfo,
                         const Signature& signature)
    : m_tranID(tranID), m_coreInfo(coreInfo), m_signature(signature) {}

bool Transaction::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTransaction(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransaction failed.");
    return false;
  }

  return true;
}

bool Transaction::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetTransaction(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransaction failed.");
    return false;
  }

  return true;
}

bool Transaction::Deserialize(const string& src, unsigned int offset) {
  if (!Messenger::GetTransaction(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransaction failed.");
    return false;
  }

  return true;
}

const TxnHash& Transaction::GetTranID() const { return m_tranID; }

const TransactionCoreInfo& Transaction::GetCoreInfo() const {
  return m_coreInfo;
}

const uint32_t& Transaction::GetVersion() const { return m_coreInfo.version; }

const uint64_t& Transaction::GetNonce() const { return m_coreInfo.nonce; }

const Address& Transaction::GetToAddr() const { return m_coreInfo.toAddr; }

const PubKey& Transaction::GetSenderPubKey() const {
  return m_coreInfo.senderPubKey;
}

Address Transaction::GetSenderAddr() const {
  return Account::GetAddressFromPublicKey(GetSenderPubKey());
}

const uint128_t& Transaction::GetAmount() const { return m_coreInfo.amount; }

const uint128_t& Transaction::GetGasPrice() const {
  return m_coreInfo.gasPrice;
}

const uint64_t& Transaction::GetGasLimit() const { return m_coreInfo.gasLimit; }

const bytes& Transaction::GetCode() const { return m_coreInfo.code; }

const bytes& Transaction::GetData() const { return m_coreInfo.data; }

const Signature& Transaction::GetSignature() const { return m_signature; }

bool Transaction::IsSigned() const {
  bytes txnData;
  SerializeCoreFields(txnData, 0);

  // Generate the transaction ID
  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  //const bytes& output = sha2.Finalize();
  //if (output.size() != TRAN_HASH_SIZE) {
  //  LOG_GENERAL(WARNING, "We failed to generate m_tranID.");
  //  return false;
  //}
  //
  //copy(output.begin(), output.end(), m_tranID.asArray().begin());
  std::string res;
  boost::algorithm::hex(txnData.begin(), txnData.end(), back_inserter(res));

  //std::stringstream sstream;
  //sstream << std::hex << setfill('0') << setw(2);

  //for(const auto &item: txnData) {
  //  sstream << item;
  //}
  //std::string result = sstream.str();

  std::string pubKeyStr = std::string(m_coreInfo.senderPubKey);
  std::string sigString = std::string(m_signature);

  cout << "Verifying transaction with... " << endl << res << endl <<  m_signature << endl << pubKeyStr << endl;

  // Verify the signature
  auto schnorr_result = Schnorr::Verify(txnData, m_signature, m_coreInfo.senderPubKey);

  std::string const toHash = "0";

  bool ecdsa_result = VerifyEcdsaSecp256k1(toHash, sigString, pubKeyStr);

  LOG_GENERAL(WARNING, "*** Schnorr signing result is " << schnorr_result);
  LOG_GENERAL(WARNING, "*** ECDSA signing result is " << ecdsa_result);

  return true;
}

void Transaction::SetSignature(const Signature& signature) {
  m_signature = signature;
}

unsigned int Transaction::GetShardIndex(const Address& fromAddr,
                                        unsigned int numShards) {
  uint32_t x = 0;

  if (numShards == 0) {
    LOG_GENERAL(WARNING, "numShards is 0 and trying to calculate shard index");
    return 0;
  }

  // Take the last four bytes of the address
  for (unsigned int i = 0; i < 4; i++) {
    x = (x << 8) | fromAddr.asArray().at(ACC_ADDR_SIZE - 4 + i);
  }

  return x % numShards;
}

unsigned int Transaction::GetShardIndex(unsigned int numShards) const {
  const auto& fromAddr = GetSenderAddr();

  return GetShardIndex(fromAddr, numShards);
}

bool Transaction::operator==(const Transaction& tran) const {
  return ((m_tranID == tran.m_tranID) && (m_signature == tran.m_signature));
}

bool Transaction::operator<(const Transaction& tran) const {
  return tran.m_tranID > m_tranID;
}

bool Transaction::operator>(const Transaction& tran) const {
  return tran < *this;
}

