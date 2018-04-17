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

#include "libCrypto/Schnorr.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"
#include <cstring>

#define BOOST_TEST_MODULE schnorrtest
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(schnorrtest)

BOOST_AUTO_TEST_CASE(test_curve_setup)
{
    INIT_STDOUT_LOGGER();

    Schnorr& schnorr = Schnorr::GetInstance();

    unique_ptr<BIGNUM, void (*)(BIGNUM*)> a(BN_new(), BN_clear_free);
    unique_ptr<BIGNUM, void (*)(BIGNUM*)> b(BN_new(), BN_clear_free);
    unique_ptr<BIGNUM, void (*)(BIGNUM*)> p(BN_new(), BN_clear_free);
    unique_ptr<BIGNUM, void (*)(BIGNUM*)> h(BN_new(), BN_clear_free);

    const char* order_expected
        = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";
    const char* basept_expected
        = "0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798";
    const char* p_expected
        = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F";
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

    if ((a != nullptr) && (b != nullptr) && (p != nullptr) && (h != nullptr))
    {
        BOOST_CHECK_MESSAGE(
            EC_GROUP_get_curve_GFp(schnorr.GetCurve().m_group.get(), p.get(),
                                   a.get(), b.get(), NULL)
                != 0,
            "EC_GROUP_get_curve_GFp failed");
        BOOST_CHECK_MESSAGE(EC_GROUP_get_cofactor(
                                schnorr.GetCurve().m_group.get(), h.get(), NULL)
                                != 0,
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

BOOST_AUTO_TEST_CASE(test_keys)
{
    Schnorr& schnorr = Schnorr::GetInstance();

    unique_ptr<EC_POINT, void (*)(EC_POINT*)> P(
        EC_POINT_new(schnorr.GetCurve().m_group.get()), EC_POINT_clear_free);

    pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();

    BOOST_CHECK_MESSAGE(
        BN_cmp(keypair.first.m_d.get(), schnorr.GetCurve().m_order.get()) == -1,
        "Key generation check #1 failed");
    BOOST_CHECK_MESSAGE(BN_is_zero(keypair.first.m_d.get()) != 1,
                        "Key generation check #2 failed");

    BOOST_CHECK_MESSAGE(EC_POINT_mul(schnorr.GetCurve().m_group.get(), P.get(),
                                     keypair.first.m_d.get(), NULL, NULL, NULL)
                            != 0,
                        "Key generation check #3 failed");
    BOOST_CHECK_MESSAGE(EC_POINT_cmp(schnorr.GetCurve().m_group.get(),
                                     keypair.second.m_P.get(), P.get(), NULL)
                            == 0,
                        "Key generation check #4 failed");
}

BOOST_AUTO_TEST_CASE(test_sign_verif)
{
    Schnorr& schnorr = Schnorr::GetInstance();

    pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();

    // 1 MB message
    const unsigned int message_size = 1048576;
    vector<unsigned char> message_rand(message_size);
    vector<unsigned char> message_1(message_size, 0x01);
    generate(message_rand.begin(), message_rand.end(), std::rand);

    Signature signature;

    // Generate the signature
    BOOST_CHECK_MESSAGE(
        schnorr.Sign(message_rand, keypair.first, keypair.second, signature)
            == true,
        "Signing failed");

    // Check the generated signature
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

    // Verify the signature
    BOOST_CHECK_MESSAGE(schnorr.Verify(message_rand, signature, keypair.second)
                            == true,
                        "Signature verification (correct message) failed");
    BOOST_CHECK_MESSAGE(schnorr.Verify(message_1, signature, keypair.second)
                            == false,
                        "Signature verification (wrong message) failed");
}

BOOST_AUTO_TEST_CASE(test_performance)
{
    Schnorr& schnorr = Schnorr::GetInstance();

    pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();

    const unsigned int message_sizes[]
        = {128 * 1024,      256 * 1024,       512 * 1024,
           1 * 1024 * 1024, 2 * 1024 * 1024,  4 * 1024 * 1024,
           8 * 1024 * 1024, 16 * 1024 * 1024, 32 * 1024 * 1024};
    const char* printable_sizes[] = {"128kB", "256kB", "512kB", "1MB", "2MB",
                                     "4MB",   "8MB",   "16MB",  "32MB"};
    const unsigned int num_messages
        = sizeof(message_sizes) / sizeof(message_sizes[0]);

    for (unsigned int i = 0; i < num_messages; i++)
    {
        vector<unsigned char> message_rand(message_sizes[i]);
        generate(message_rand.begin(), message_rand.end(), std::rand);

        Signature signature;

        // Generate the signature
        auto t = r_timer_start();
        BOOST_CHECK_MESSAGE(
            schnorr.Sign(message_rand, keypair.first, keypair.second, signature)
                == true,
            "Signing failed");
        LOG_GENERAL(INFO, "Message size  = " << printable_sizes[i]);
        LOG_GENERAL(INFO, "Sign (usec)   = " << r_timer_end(t));

        // Check the generated signature
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

        // Verify the signature
        t = r_timer_start();
        BOOST_CHECK_MESSAGE(
            schnorr.Verify(message_rand, signature, keypair.second) == true,
            "Signature verification (correct message) failed");
        LOG_GENERAL(INFO, "Message size  = " << printable_sizes[i]);
        LOG_GENERAL(INFO, "Verify (usec) = " << r_timer_end(t));
    }
}

BOOST_AUTO_TEST_CASE(test_serialization)
{
    Schnorr& schnorr = Schnorr::GetInstance();

    pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();

    // 1 MB message
    const unsigned int message_size = 1048576;
    vector<unsigned char> message(message_size);
    generate(message.begin(), message.end(), std::rand);

    // Generate and verify the signature
    Signature signature;
    BOOST_CHECK_MESSAGE(
        schnorr.Sign(message, keypair.first, keypair.second, signature) == true,
        "Signing failed");
    BOOST_CHECK_MESSAGE(schnorr.Verify(message, signature, keypair.second)
                            == true,
                        "Signature verification failed");

    // Serialize keys and signature
    vector<unsigned char> privkey_bytes, pubkey_bytes, signature_bytes;
    keypair.first.Serialize(privkey_bytes, 0);
    keypair.second.Serialize(pubkey_bytes, 0);
    signature.Serialize(signature_bytes, 0);

    // Deserialize keys and signature using constructor functions
    PrivKey privkey1(privkey_bytes, 0);
    PubKey pubkey1(pubkey_bytes, 0);
    Signature signature1(signature_bytes, 0);
    BOOST_CHECK_MESSAGE(keypair.first == privkey1,
                        "PrivKey serialization check #1 failed");
    BOOST_CHECK_MESSAGE(keypair.second == pubkey1,
                        "PubKey serialization check #1 failed");
    BOOST_CHECK_MESSAGE(signature == signature1,
                        "Signature serialization check #1 failed");

    // Deserialize keys and signature using Deserialize functions (first, initialize the keys and sig with different values)
    pair<PrivKey, PubKey> keypair2 = schnorr.GenKeyPair();
    vector<unsigned char> message_rand(message_size);
    Signature signature2;
    BOOST_CHECK_MESSAGE(
        schnorr.Sign(message_rand, keypair2.first, keypair2.second, signature2)
            == true,
        "Signing failed");
    BOOST_CHECK_MESSAGE(
        schnorr.Verify(message_rand, signature2, keypair2.second) == true,
        "Signature verification failed");

    keypair2.first.Deserialize(privkey_bytes, 0);
    keypair2.second.Deserialize(pubkey_bytes, 0);
    signature2.Deserialize(signature_bytes, 0);
    BOOST_CHECK_MESSAGE(keypair.first == keypair2.first,
                        "PrivKey serialization check #2 failed");
    BOOST_CHECK_MESSAGE(keypair.second == keypair2.second,
                        "PubKey serialization check #2 failed");
    BOOST_CHECK_MESSAGE(signature == signature2,
                        "Signature serialization check #2 failed");
}

BOOST_AUTO_TEST_SUITE_END()