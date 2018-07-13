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

#include "libCrypto/MultiSig.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE multisigtest
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(multisigtest)

BOOST_AUTO_TEST_CASE(test_multisig)
{
    INIT_STDOUT_LOGGER();

    Schnorr& schnorr = Schnorr::GetInstance();

    // Generate key pairs
    const unsigned int nbsigners = 2000;
    vector<PrivKey> privkeys;
    vector<PubKey> pubkeys;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();
        privkeys.emplace_back(keypair.first);
        pubkeys.emplace_back(keypair.second);
    }

    // 1 MB message
    const unsigned int message_size = 1048576;
    vector<unsigned char> message_rand(message_size);
    vector<unsigned char> message_1(message_size, 0x01);
    generate(message_rand.begin(), message_rand.end(), std::rand);

    // Aggregate public keys
    shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubkeys);
    BOOST_CHECK_MESSAGE(aggregatedPubkey != nullptr, "AggregatePubKeys failed");

    // Generate individual commitments
    vector<CommitSecret> secrets(nbsigners);
    vector<CommitPoint> points;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        points.emplace_back(secrets.at(i));
    }

    // Aggregate commits
    shared_ptr<CommitPoint> aggregatedCommit
        = MultiSig::AggregateCommits(points);
    BOOST_CHECK_MESSAGE(aggregatedCommit != nullptr, "AggregateCommits failed");

    // Generate challenge
    Challenge challenge(*aggregatedCommit, *aggregatedPubkey, message_rand);
    BOOST_CHECK_MESSAGE(challenge.Initialized() == true,
                        "Challenge generation failed");

    // Generate responses
    vector<Response> responses;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        responses.emplace_back(secrets.at(i), challenge, privkeys.at(i));
        BOOST_CHECK_MESSAGE(responses.back().Initialized() == true,
                            "Response generation failed");
    }

    // Aggregate responses
    shared_ptr<Response> aggregatedResponse
        = MultiSig::AggregateResponses(responses);
    BOOST_CHECK_MESSAGE(aggregatedResponse != nullptr,
                        "AggregateResponses failed");

    // Generate the aggregated signature
    shared_ptr<Signature> signature
        = MultiSig::AggregateSign(challenge, *aggregatedResponse);
    BOOST_CHECK_MESSAGE(signature != nullptr, "AggregateSign failed");

    // Verify the signature
    BOOST_CHECK_MESSAGE(
        schnorr.Verify(message_rand, *signature, *aggregatedPubkey) == true,
        "Signature verification (correct message) failed");
    BOOST_CHECK_MESSAGE(schnorr.Verify(message_1, *signature, *aggregatedPubkey)
                            == false,
                        "Signature verification (wrong message) failed");

    CommitPoint cp_copy;
    cp_copy = *aggregatedCommit;
    BOOST_CHECK_MESSAGE(cp_copy == *aggregatedCommit,
                        "CommitPoint operator= failed");

    Challenge challenge_copy;
    challenge_copy = challenge;
    BOOST_CHECK_MESSAGE(challenge_copy == challenge,
                        "Challenge operator= failed");

    Response response_copy;
    response_copy = *aggregatedResponse;
    BOOST_CHECK_MESSAGE(response_copy == *aggregatedResponse,
                        "Response operator= failed");
}

BOOST_AUTO_TEST_CASE(test_serialization)
{
    Schnorr& schnorr = Schnorr::GetInstance();

    // Generate key pairs
    const unsigned int nbsigners = 80;
    vector<PrivKey> privkeys;
    vector<PubKey> pubkeys;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        pair<PrivKey, PubKey> keypair = schnorr.GenKeyPair();
        privkeys.emplace_back(keypair.first);
        pubkeys.emplace_back(keypair.second);
    }

    // 1 MB message
    const unsigned int message_size = 1048576;
    vector<unsigned char> message_rand(message_size);
    vector<unsigned char> message_1(message_size, 0x01);
    generate(message_rand.begin(), message_rand.end(), std::rand);

    // Aggregate public keys
    shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubkeys);
    BOOST_CHECK_MESSAGE(aggregatedPubkey != nullptr, "AggregatePubKeys failed");

    // Generate individual commitments
    vector<CommitSecret> secrets(nbsigners);
    vector<CommitPoint> points;
    vector<CommitSecret> secrets1;
    vector<CommitPoint> points1;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        vector<unsigned char> tmp1, tmp2;
        secrets.at(i).Serialize(tmp1, 0);
        secrets1.emplace_back(tmp1, 0);
        points.emplace_back(secrets.at(i));
        points.back().Serialize(tmp2, 0);
        points1.emplace_back(tmp2, 0);
    }

    // Aggregate commits
    shared_ptr<CommitPoint> aggregatedCommit
        = MultiSig::AggregateCommits(points);
    BOOST_CHECK_MESSAGE(aggregatedCommit != nullptr, "AggregateCommits failed");
    shared_ptr<CommitPoint> aggregatedCommit1
        = MultiSig::AggregateCommits(points1);
    BOOST_CHECK_MESSAGE(*aggregatedCommit == *aggregatedCommit1,
                        "Commit serialization failed");

    // Generate challenge
    Challenge challenge(*aggregatedCommit, *aggregatedPubkey, message_rand);
    BOOST_CHECK_MESSAGE(challenge.Initialized() == true,
                        "Challenge generation failed");
    vector<unsigned char> tmp;
    challenge.Serialize(tmp, 0);
    Challenge challenge2(tmp, 0);
    BOOST_CHECK_MESSAGE(challenge == challenge2,
                        "Challenge serialization failed");
    tmp.clear();

    // Generate responses
    vector<Response> responses;
    vector<Response> responses1;
    for (unsigned int i = 0; i < nbsigners; i++)
    {
        responses.emplace_back(secrets.at(i), challenge, privkeys.at(i));
        BOOST_CHECK_MESSAGE(responses.back().Initialized() == true,
                            "Response generation failed");
        vector<unsigned char> tmp;
        responses.back().Serialize(tmp, 0);
        responses1.emplace_back(tmp, 0);
        // Verify response
        BOOST_CHECK_MESSAGE(MultiSig::VerifyResponse(responses.at(i), challenge,
                                                     pubkeys.at(i),
                                                     points.at(i))
                                == true,
                            "Verify response failed");
    }

    // Aggregate responses
    shared_ptr<Response> aggregatedResponse
        = MultiSig::AggregateResponses(responses);
    BOOST_CHECK_MESSAGE(aggregatedResponse != nullptr,
                        "AggregateResponses failed");
    shared_ptr<Response> aggregatedResponse1
        = MultiSig::AggregateResponses(responses1);
    BOOST_CHECK_MESSAGE(*aggregatedResponse == *aggregatedResponse1,
                        "Response serialization failed");

    // Generate the aggregated signature
    shared_ptr<Signature> signature
        = MultiSig::AggregateSign(challenge, *aggregatedResponse);
    BOOST_CHECK_MESSAGE(signature != nullptr, "AggregateSign failed");

    // Verify the signature
    BOOST_CHECK_MESSAGE(
        schnorr.Verify(message_rand, *signature, *aggregatedPubkey) == true,
        "Signature verification (correct message) failed");
    BOOST_CHECK_MESSAGE(schnorr.Verify(message_1, *signature, *aggregatedPubkey)
                            == false,
                        "Signature verification (wrong message) failed");
}

BOOST_AUTO_TEST_SUITE_END()
