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

#include <Schnorr.h>
#include <cstring>
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE schnorrtest
#define BOOST_TEST_DYN_LINK
#include <boost/test/output_test_stream.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(schnorrtest)

/**
 * \brief test_multisig
 *
 * \details Test multisig process and operators
 */
BOOST_AUTO_TEST_CASE(test_curve_setup) {
  INIT_STDOUT_LOGGER();

  Schnorr& schnorr = Schnorr::GetInstance();

  unique_ptr<BIGNUM, void (*)(BIGNUM*)> a(BN_new(), BN_clear_free);
  unique_ptr<BIGNUM, void (*)(BIGNUM*)> b(BN_new(), BN_clear_free);
  unique_ptr<BIGNUM, void (*)(BIGNUM*)> p(BN_new(), BN_clear_free);
  unique_ptr<BIGNUM, void (*)(BIGNUM*)> h(BN_new(), BN_clear_free);

  const char* order_expected =
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";
  const char* basept_expected =
      "0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798";
  const char* p_expected =
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F";
  const char* a_expected = "0";
  const char* b_expected = "07";
  const char* h_expected = "01";

  unique_ptr<char, void (*)(void*)> order_actual(
      BN_bn2hex(schnorr.GetCurve().m_order.get()), free);
  BOOST_CHECK_MESSAGE(strcmp(order_expected, order_actual.get()) == 0,
                      "Wrong order generated");

  unique_ptr<char, void (*)(void*)> basept_actual(
      EC_POINT_point2hex(
          schnorr.GetCurve().m_group.get(),
          EC_GROUP_get0_generator(schnorr.GetCurve().m_group.get()),
          POINT_CONVERSION_COMPRESSED, NULL),
      free);
  BOOST_CHECK_MESSAGE(strcmp(basept_expected, basept_actual.get()) == 0,
                      "Wrong basept generated");

  if ((a != nullptr) && (b != nullptr) && (p != nullptr) && (h != nullptr)) {
    BOOST_CHECK_MESSAGE(
        EC_GROUP_get_curve_GFp(schnorr.GetCurve().m_group.get(), p.get(),
                               a.get(), b.get(), NULL) != 0,
        "EC_GROUP_get_curve_GFp failed");
    BOOST_CHECK_MESSAGE(EC_GROUP_get_cofactor(schnorr.GetCurve().m_group.get(),
                                              h.get(), NULL) != 0,
                        "EC_GROUP_get_cofactor failed");

    unique_ptr<char, void (*)(void*)> p_actual(BN_bn2hex(p.get()), free);
    unique_ptr<char, void (*)(void*)> a_actual(BN_bn2hex(a.get()), free);
    unique_ptr<char, void (*)(void*)> b_actual(BN_bn2hex(b.get()), free);
    unique_ptr<char, void (*)(void*)> h_actual(BN_bn2hex(h.get()), free);

    BOOST_CHECK_MESSAGE(strcmp(p_expected, p_actual.get()) == 0,
                        "Wrong p generated");
    BOOST_CHECK_MESSAGE(strcmp(a_expected, a_actual.get()) == 0,
                        "Wrong a generated");
    BOOST_CHECK_MESSAGE(strcmp(b_expected, b_actual.get()) == 0,
                        "Wrong b generated");
    BOOST_CHECK_MESSAGE(strcmp(h_expected, h_actual.get()) == 0,
                        "Wrong h generated");
  }
}

/**
 * \brief test_multisig
 *
 * \details Test multisig process and operators
 */
BOOST_AUTO_TEST_CASE(test_keys) {
  Schnorr& schnorr = Schnorr::GetInstance();

  unique_ptr<EC_POINT, void (*)(EC_POINT*)> P(
      EC_POINT_new(schnorr.GetCurve().m_group.get()), EC_POINT_clear_free);

  PairOfKey keypair = schnorr.GenKeyPair();

  BOOST_CHECK_MESSAGE(
      BN_cmp(keypair.first.m_d.get(), schnorr.GetCurve().m_order.get()) == -1,
      "Key generation check #1 failed");
  BOOST_CHECK_MESSAGE(BN_is_zero(keypair.first.m_d.get()) != 1,
                      "Key generation check #2 failed");

  BOOST_CHECK_MESSAGE(
      EC_POINT_mul(schnorr.GetCurve().m_group.get(), P.get(),
                   keypair.first.m_d.get(), NULL, NULL, NULL) != 0,
      "Key generation check #3 failed");
  BOOST_CHECK_MESSAGE(
      EC_POINT_cmp(schnorr.GetCurve().m_group.get(), keypair.second.m_P.get(),
                   P.get(), NULL) == 0,
      "Key generation check #4 failed");
}

/**
 * \brief test_sign_verif
 *
 * \details Test signature verification
 */
BOOST_AUTO_TEST_CASE(test_sign_verif) {
  Schnorr& schnorr = Schnorr::GetInstance();

  PairOfKey keypair = schnorr.GenKeyPair();

  /// 1 MB message
  const unsigned int message_size = 1048576;
  bytes message_rand(message_size);
  bytes message_1(message_size, 0x01);
  generate(message_rand.begin(), message_rand.end(), std::rand);

  Signature signature;

  /// Generate the signature
  BOOST_CHECK_MESSAGE(schnorr.Sign(message_rand, keypair.first, keypair.second,
                                   signature) == true,
                      "Signing failed");

  /// Check the generated signature
  BOOST_CHECK_MESSAGE(
      BN_cmp(signature.m_r.get(), schnorr.GetCurve().m_order.get()) == -1,
      "Signature generation check #1 failed");
  BOOST_CHECK_MESSAGE(BN_is_zero(signature.m_r.get()) != 1,
                      "Signature generation check #2 failed");
  BOOST_CHECK_MESSAGE(
      BN_cmp(signature.m_s.get(), schnorr.GetCurve().m_order.get()) == -1,
      "Signature generation check #3 failed");
  BOOST_CHECK_MESSAGE(BN_is_zero(signature.m_s.get()) != 1,
                      "Signature generation check #4 failed");

  /// Verify the signature
  BOOST_CHECK_MESSAGE(
      schnorr.Verify(message_rand, signature, keypair.second) == true,
      "Signature verification (correct message) failed");
  BOOST_CHECK_MESSAGE(
      schnorr.Verify(message_1, signature, keypair.second) == false,
      "Signature verification (wrong message) failed");
}

/**
 * \brief test_performance
 *
 * \details Test various message sizes
 */
BOOST_AUTO_TEST_CASE(test_performance) {
  Schnorr& schnorr = Schnorr::GetInstance();

  PairOfKey keypair = schnorr.GenKeyPair();

  const unsigned int message_sizes[] = {
      128 * 1024,      256 * 1024,       512 * 1024,
      1 * 1024 * 1024, 2 * 1024 * 1024,  4 * 1024 * 1024,
      8 * 1024 * 1024, 16 * 1024 * 1024, 32 * 1024 * 1024};
  const char* printable_sizes[] = {"128kB", "256kB", "512kB", "1MB", "2MB",
                                   "4MB",   "8MB",   "16MB",  "32MB"};
  const unsigned int num_messages =
      sizeof(message_sizes) / sizeof(message_sizes[0]);

  for (unsigned int i = 0; i < num_messages; i++) {
    bytes message_rand(message_sizes[i]);
    generate(message_rand.begin(), message_rand.end(), std::rand);

    Signature signature;

    /// Generate the signature
    auto t = r_timer_start();
    BOOST_CHECK_MESSAGE(schnorr.Sign(message_rand, keypair.first,
                                     keypair.second, signature) == true,
                        "Signing failed");
    LOG_GENERAL(INFO, "Message size  = " << printable_sizes[i]);
    LOG_GENERAL(INFO, "Sign (usec)   = " << r_timer_end(t));

    /// Check the generated signature
    BOOST_CHECK_MESSAGE(
        BN_cmp(signature.m_r.get(), schnorr.GetCurve().m_order.get()) == -1,
        "Signature generation check #1 failed");
    BOOST_CHECK_MESSAGE(BN_is_zero(signature.m_r.get()) != 1,
                        "Signature generation check #2 failed");
    BOOST_CHECK_MESSAGE(
        BN_cmp(signature.m_s.get(), schnorr.GetCurve().m_order.get()) == -1,
        "Signature generation check #3 failed");
    BOOST_CHECK_MESSAGE(BN_is_zero(signature.m_s.get()) != 1,
                        "Signature generation check #4 failed");

    /// Verify the signature
    t = r_timer_start();
    BOOST_CHECK_MESSAGE(
        schnorr.Verify(message_rand, signature, keypair.second) == true,
        "Signature verification (correct message) failed");
    LOG_GENERAL(INFO, "Message size  = " << printable_sizes[i]);
    LOG_GENERAL(INFO, "Verify (usec) = " << r_timer_end(t));
  }
}

/**
 * \brief test_serialization
 *
 * \details Test serialization both via function and via stream operator
 */
BOOST_AUTO_TEST_CASE(test_serialization) {
  Schnorr& schnorr = Schnorr::GetInstance();

  PairOfKey keypair = schnorr.GenKeyPair();

  /// 1 MB message
  const unsigned int message_size = 1048576;
  bytes message(message_size);
  generate(message.begin(), message.end(), std::rand);

  /// Generate and verify the signature
  Signature signature;
  BOOST_CHECK_MESSAGE(
      schnorr.Sign(message, keypair.first, keypair.second, signature) == true,
      "Signing failed");
  BOOST_CHECK_MESSAGE(
      schnorr.Verify(message, signature, keypair.second) == true,
      "Signature verification failed");

  /// Serialize keys and signature
  bytes privkey_bytes, pubkey_bytes, signature_bytes;
  keypair.first.Serialize(privkey_bytes, 0);
  keypair.second.Serialize(pubkey_bytes, 0);
  signature.Serialize(signature_bytes, 0);

  /// Deserialize keys and signature using constructor functions
  PrivKey privkey1(privkey_bytes, 0);
  PubKey pubkey1(pubkey_bytes, 0);
  Signature signature1(signature_bytes, 0);
  BOOST_CHECK_MESSAGE(keypair.first == privkey1,
                      "PrivKey serialization check #1 failed");
  BOOST_CHECK_MESSAGE(keypair.second == pubkey1,
                      "PubKey serialization check #1 failed");
  BOOST_CHECK_MESSAGE(signature == signature1,
                      "Signature serialization check #1 failed");

  /// Check PrivKey operator =
  PrivKey privkey2;
  privkey2 = privkey1;

  /// Check PubKey operator >
  PubKey pubkey2;
  pubkey2 = pubkey1;
  BOOST_CHECK_MESSAGE(!(pubkey2 > pubkey1), "Pubkey operator > failed");

  /// Deserialize keys and signature using Deserialize functions (first,
  /// initialize the keys and sig with different values)
  PairOfKey keypair2 = schnorr.GenKeyPair();
  bytes message_rand(message_size);
  Signature signature2;
  BOOST_CHECK_MESSAGE(schnorr.Sign(message_rand, keypair2.first,
                                   keypair2.second, signature2) == true,
                      "Signing failed");
  BOOST_CHECK_MESSAGE(
      schnorr.Verify(message_rand, signature2, keypair2.second) == true,
      "Signature verification failed");

  keypair2.first.Deserialize(privkey_bytes, 0);
  keypair2.second.Deserialize(pubkey_bytes, 0);
  signature2.Deserialize(signature_bytes, 0);
  BOOST_CHECK_MESSAGE(keypair.first == keypair2.first,
                      "PrivKey serialization check #2 failed");
  boost::test_tools::output_test_stream PrivKeyOutput;
  PrivKeyOutput << keypair.first;
  BOOST_CHECK(!PrivKeyOutput.is_empty(false));
  BOOST_CHECK_MESSAGE(keypair.second == keypair2.second,
                      "PubKey serialization check #2 failed");
  boost::test_tools::output_test_stream PubKeyOutput;
  PubKeyOutput << keypair.second;
  BOOST_CHECK(!PubKeyOutput.is_empty(false));
  BOOST_CHECK_MESSAGE(signature == signature2,
                      "Signature serialization check #2 failed");
  boost::test_tools::output_test_stream SignatureOutput;
  SignatureOutput << signature2;
  BOOST_CHECK(!SignatureOutput.is_empty(false));
}

/**
 * \brief test_error_deserialization_pubkey
 *
 * \details Test failure in deserialization of public key
 */
BOOST_AUTO_TEST_CASE(test_error_deserialization_pubkey) {
  PubKey pubkey;
  bytes pubkey_bytes_empty;
  int returnValue = pubkey.Deserialize(pubkey_bytes_empty, 0);
  BOOST_CHECK_MESSAGE(returnValue == -1,
                      "Expected: -1 Obtained: " << returnValue);
}

/**
 * \brief test_error_deserialization_privkey
 *
 * \details Test failure in deserialization of private key
 */
BOOST_AUTO_TEST_CASE(test_error_deserialization_privkey) {
  PrivKey privkey;
  bytes privkey_bytes_empty;
  int returnValue = privkey.Deserialize(privkey_bytes_empty, 0);
  BOOST_CHECK_MESSAGE(returnValue == -1,
                      "Expected: -1 Obtained: " << returnValue);
}

/**
 * \brief test_error_deserialization_signature
 *
 * \details Test failure in deserialization of signature
 */
BOOST_AUTO_TEST_CASE(test_error_deserialization_signature) {
  Signature signature;
  bytes sig_bytes_empty;
  int returnValue = signature.Deserialize(sig_bytes_empty, 0);
  BOOST_CHECK_MESSAGE(returnValue == -1,
                      "Expected: -1 Obtained: " << returnValue);
}

BOOST_AUTO_TEST_SUITE_END()
