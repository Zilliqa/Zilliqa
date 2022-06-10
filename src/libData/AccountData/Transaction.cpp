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
//#include "keccak.h"
//#include "keccak.h"
#include <ethash/keccak.hpp>
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

// todo: delete these after
#include <openssl/ec.h>      // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>   // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h> // for NID_secp192k1
#include <openssl/evp.h> //for all other OpenSSL function calls
#include <openssl/sha.h> //for SHA512_DIGEST_LENGTH

using namespace std;
using namespace boost::multiprecision;

unsigned char HIGH_BITS_MASK = 0xF0;
unsigned char LOW_BITS_MASK = 0x0F;
unsigned char ACC_COND = 0x1;
unsigned char TX_COND = 0x2;

///////////////////////////////////////////////////////////////////////
// https://stackoverflow.com/questions/57385412/ecdsa-do-verify-fails-to-verify-for-some-hash-only
bool verify_signature(const unsigned char* hash, const ECDSA_SIG* signature, EC_KEY* eckey)
{
    cout << "First bytes " << hash[0] << endl;
    cout << "First bytes " << hash[1] << endl;

    int verify_status = ECDSA_do_verify(hash, SHA256_DIGEST_LENGTH, signature, eckey);
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
    //BIGNUM* gy_ptr = gy.get();

    EC_KEY_set_asn1_flag(pKey, OPENSSL_EC_NAMED_CURVE);
    memcpy(cx, sPubKeyString, 64);
    cx[64] = 0;
    int y_chooser_bit = 0;

    if (!BN_hex2bn(&gx_ptr, cx)) {
        std::cout << "***** Error getting to x binary format" << std::endl;
    }

    // Create a new
    EC_GROUP *curve_group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    auto point = EC_POINT_new(curve_group);

    EC_POINT_set_compressed_coordinates_GFp(curve_group, point,
                                                  gx_ptr,
                                                  y_chooser_bit, NULL);

    //cout << "RES " << results << endl;
    //if (!EC_KEY_set_public_key_affine_coordinates(pKey, gx_ptr, nullptr)) {
    //    std::cout << "****** ERROR setting public key attributes" << std::endl;
    //}

    if (!EC_KEY_set_public_key(pKey, point)) {
        std::cout << "****** ERROR! setting public key attributes" << std::endl;
    }

    if (EC_KEY_check_key(pKey) == 1)
    {
        cout << "ec key valid " << endl;
        return true;
    }
    else {
        cout << "ec key INvalid " << endl;
        return false;
    }
}

//bool SetOpensslPublicKey(const std::string& sPublicKeyInHex, EC_KEY* pKey)
//{
//    const char* sPubKeyString = sPublicKeyInHex.c_str();
//
//    char cx[65];
//
//    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gx(NULL, [](BIGNUM* b) { BN_free(b); });
//    std::unique_ptr< BIGNUM, std::function<void(BIGNUM*)>> gy(NULL, [](BIGNUM* b) { BN_free(b); });
//
//    BIGNUM* gx_ptr = gx.get();
//    BIGNUM* gy_ptr = gy.get();
//
//    EC_KEY_set_asn1_flag(pKey, OPENSSL_EC_NAMED_CURVE);
//    memcpy(cx, sPubKeyString, 64);
//    cx[64] = 0;
//
//    if (!BN_hex2bn(&gx_ptr, cx)) {
//        std::cout << "***** Error getting to x binary format" << std::endl;
//    }
//
//    if (!BN_hex2bn(&gy_ptr, &sPubKeyString[64])) {
//        std::cout << "***** Error getting to y binary format" << std::endl;
//    }
//
//    if (!EC_KEY_set_public_key_affine_coordinates(pKey, gx_ptr, nullptr)) {
//        std::cout << "****** ERROR setting public key attributes" << std::endl;
//    }
//
//    if (EC_KEY_check_key(pKey) == 1)
//    {
//        cout << "ec key valid " << endl;
//        return true;
//    }
//    else {
//        cout << "ec key INvalid " << endl;
//        return false;
//    }
//}

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

std::string bytes_to_hex_string(const std::vector<uint8_t>& bytes)
{
  std::ostringstream stream;
  for (uint8_t b : bytes)
  {
    stream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b);
  }
  return stream.str();
}

//perform the SHA3-512 hash
// https://stackoverflow.com/questions/51144505/generate-sha-3-hash-in-c-using-openssl-library
std::string keccak_old(const std::string input)
{
  uint32_t digest_length = SHA256_DIGEST_LENGTH;
  const EVP_MD* algorithm = EVP_sha3_256();
  uint8_t* digest = static_cast<uint8_t*>(OPENSSL_malloc(digest_length));
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  EVP_DigestInit_ex(context, algorithm, nullptr);
  EVP_DigestUpdate(context, input.c_str(), input.size());
  EVP_DigestFinal_ex(context, digest, &digest_length);
  EVP_MD_CTX_destroy(context);
  std::string output = bytes_to_hex_string(std::vector<uint8_t>(digest, digest + digest_length));
  cout << "OUTPUT KEK  " << output << endl;

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    ss << digest[i];
  }
  //return ss.str();

  OPENSSL_free(digest);
  //return output;
  return ss.str();
}

bool Verify(const std::string& sRandomNumber, const std::string& sSignature, const std::string& sDevicePubKeyInHex)
{
    std::unique_ptr< ECDSA_SIG, std::function<void(ECDSA_SIG*)>> zSignature(ECDSA_SIG_new(), [](ECDSA_SIG* b) { ECDSA_SIG_free(b); });
    // Set up the signature...
    SetOpensslSignature(sSignature, zSignature.get());

    std::unique_ptr< EC_KEY, std::function<void(EC_KEY*)>> zPublicKey(EC_KEY_new_by_curve_name(NID_secp256k1), [](EC_KEY* b) { EC_KEY_free(b); });
    //std::unique_ptr< EC_KEY, std::function<void(EC_KEY*)>> zPublicKey(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), [](EC_KEY* b) { EC_KEY_free(b); });

    if (!SetOpensslPublicKey(sDevicePubKeyInHex, zPublicKey.get())) {
      std::cout << "Failed to get the public key from the hex input" << std::endl;
    }

    std::string sha256Hash = sha256(sRandomNumber);
    //uint8_t *out = nullptr;
    //auto result = shake256(out, 256, reinterpret_cast<const uint8_t*>(sRandomNumber.c_str()), sRandomNumber.size());

    auto result_empty = ethash::keccak256(reinterpret_cast<const uint8_t*>(sRandomNumber.c_str()), 0);

    uint8_t prelude[] = { 25, 69, 116, 104, 101, 114, 101, 117, 109, 32, 83, 105, 103, 110, 101, 100, 32, 77, 101, 115, 115, 97, 103, 101, 58, 10, 48 };
    auto result = ethash::keccak256(reinterpret_cast<const uint8_t*>(sRandomNumber.c_str()), 27);
    auto result_prelude = ethash::keccak256(prelude, sizeof(prelude));

    cout << result_prelude.bytes[0] << endl;
    cout << sizeof(prelude) << endl;

    std::string sHash = reinterpret_cast<char *>(result.bytes);

    cout << "keccak output empty: " << std::string(reinterpret_cast<char *>(result_empty.bytes)) << endl;
    cout << "keccak output pre: " << sHash << endl;
    cout << "keccak output size pre: " << sHash.size() << endl;
    ///cout << "keccak output XX pre: " << result << endl;


    sHash = "5f35dce98ba4fba25530a026ed80b2cecdaa31091ba4958b99b52ea1d068adad"; // Expected - 64 hex chars = 256 bits
    std::string bytes;

    for (unsigned int i = 0; i < sHash.length(); i += 2) {
      std::string byteString = sHash.substr(i, 2);
      char byte = (char) strtol(byteString.c_str(), NULL, 16);
      bytes.push_back(byte);
    }

    sHash = bytes;

    //sHash = "2d44da53f305ab94b6365837b9803627ab098c41a6013694f9b468bccb9c13e95b3900365eb58924de7158a54467e984efcfdabdbcc9af9a940d49c51455b04c"; // Actual - 128 chars

    cout << "real deal keccak output: " << sHash << endl;
    cout << "real deal keccak output length: " << sHash.size() << endl;

    auto res = verify_signature(result.bytes, zSignature.get(), zPublicKey.get());
    res = verify_signature(result_prelude.bytes, zSignature.get(), zPublicKey.get());
    res = verify_signature(reinterpret_cast<const unsigned char*>(result_prelude.bytes), zSignature.get(), zPublicKey.get());
    res = verify_signature(reinterpret_cast<const unsigned char*>(sHash.c_str()), zSignature.get(), zPublicKey.get());

    cout << "RES " << res << endl;

    return res;
    //return verify_signature((const unsigned char*)sha256Hash.c_str(), zSignature.get(), zPublicKey.get()); // test NID way
}


int VerifyEcdsaSecp256k1(std::string const &sRandomNumber, std::string const &sSignatureInHex, std::string const &sPublicKeyInHex)
{
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
  int prelude[] = { 25, 69, 116, 104, 101, 114, 101, 117, 109, 32, 83, 105, 103, 110, 101, 100, 32, 77, 101, 115, 115, 97, 103, 101, 58, 10, 48 };
  std::string toHash(reinterpret_cast<const char*>(prelude), sizeof(prelude));

  // Example of successful tx sign/verify
  if(false) { // NID verify
    // Successful example given here
    pubKeyStr = "94E62E0C77A2955B1FB3EE98AEAA99AACAD742F20E45B727EACDD10487C2F7D0D8257C6102921880ABE953245D573D7E33EC88A67E2BA930980CB9C3D6722F8A"; // 128 hex = 512
    sigString = "D506D976EC17DD3717C40329E28FD8DB4F32D6A3773454A6427FD12E69728157508086B661D91E07ADF5B57E787EA1EEA526A84500436E430E89B1C1F8532A41"; // 128 hex = 512
    toHash = "65560886818773090201885807838738706912015073749623293202319529"; // irrelevant
  } else {
    // Remove '0x' at beginning of string
    sigString = sigString.substr(2);

    pubKeyStr = "021815bee5679a42f3f38c8b77b99356517407603491c101ee221c7545861d12d4"; // compressed pubkey example
    pubKeyStr = pubKeyStr.substr(2);

    //pubKeyStr = "1815bee5679a42f3f38c8b77b99356517407603491c101ee221c7545861d12d433ff5ead040fd94860a53518509c2e6f3b1582aa1bd724144e99f38ca770ffbc"; // full length pubkey example
  }

  cout << "Verifying transaction with... " << endl << "toHash " << toHash << endl <<  "sig: " << sigString << endl << "pubKey: " << pubKeyStr << endl;
  cout << "Note: size of toHash is " << toHash.size() << endl;

  // Verify the signature
  auto schnorr_result = Schnorr::Verify(txnData, m_signature, m_coreInfo.senderPubKey);

  if (!schnorr_result) {
    bool ecdsa_result = VerifyEcdsaSecp256k1(toHash, sigString, pubKeyStr);
    LOG_GENERAL(WARNING, "*** ECDSA signing result is " << ecdsa_result);
  }

  LOG_GENERAL(WARNING, "*** Schnorr signing result is " << schnorr_result);

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

