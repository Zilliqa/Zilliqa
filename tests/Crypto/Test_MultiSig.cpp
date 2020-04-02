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

#include <MultiSig.h>
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE multisigtest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(multisigtest)

/**
 * \brief test_multisig
 *
 * \details Test multisig process and operators
 */
BOOST_AUTO_TEST_CASE(test_multisig) {
  INIT_STDOUT_LOGGER();

  Schnorr& schnorr = Schnorr::GetInstance();
  MultiSig& multisig = MultiSig::GetInstance();

  /// Generate key pairs
  const unsigned int nbsigners = 2000;
  vector<PrivKey> privkeys;
  vector<PubKey> pubkeys;
  for (unsigned int i = 0; i < nbsigners; i++) {
    PairOfKey keypair = schnorr.GenKeyPair();
    privkeys.emplace_back(keypair.first);
    pubkeys.emplace_back(keypair.second);
  }

  /// 1 MB message
  const unsigned int message_size = 1048576;
  bytes message_rand(message_size);
  bytes message_1(message_size, 0x01);
  generate(message_rand.begin(), message_rand.end(), std::rand);

  /// Aggregate public keys
  shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubkeys);
  BOOST_CHECK_MESSAGE(aggregatedPubkey != nullptr, "AggregatePubKeys failed");

  /// Generate individual commitments
  vector<CommitSecret> secrets(nbsigners);
  vector<CommitPoint> points;
  for (unsigned int i = 0; i < nbsigners; i++) {
    points.emplace_back(secrets.at(i));
  }

  /// Aggregate commits
  shared_ptr<CommitPoint> aggregatedCommit = MultiSig::AggregateCommits(points);
  BOOST_CHECK_MESSAGE(aggregatedCommit != nullptr, "AggregateCommits failed");

  /// Generate challenge
  Challenge challenge(*aggregatedCommit, *aggregatedPubkey, message_rand);
  BOOST_CHECK_MESSAGE(challenge.Initialized() == true,
                      "Challenge generation failed");

  /// Check Challenge copy constructor
  Challenge challengeCopy(challenge);
  BOOST_CHECK_MESSAGE(challenge == challengeCopy,
                      "Challenge copy constructor failed");

  /// Generate responses
  vector<Response> responses;
  for (unsigned int i = 0; i < nbsigners; i++) {
    responses.emplace_back(secrets.at(i), challenge, privkeys.at(i));
    BOOST_CHECK_MESSAGE(responses.back().Initialized() == true,
                        "Response generation failed");
  }

  /// Aggregate responses
  shared_ptr<Response> aggregatedResponse =
      MultiSig::AggregateResponses(responses);
  BOOST_CHECK_MESSAGE(aggregatedResponse != nullptr,
                      "AggregateResponses failed");

  /// Generate the aggregated signature
  shared_ptr<Signature> signature =
      MultiSig::AggregateSign(challenge, *aggregatedResponse);
  BOOST_CHECK_MESSAGE(signature != nullptr, "AggregateSign failed");

  /// Verify the signature
  BOOST_CHECK_MESSAGE(multisig.MultiSigVerify(message_rand, *signature,
                                              *aggregatedPubkey) == true,
                      "Signature verification (correct message) failed");
  BOOST_CHECK_MESSAGE(multisig.MultiSigVerify(message_1, *signature,
                                              *aggregatedPubkey) == false,
                      "Signature verification (wrong message) failed");

  /// Check CommitPoint operator =
  CommitPoint cp_copy;
  cp_copy = *aggregatedCommit;
  BOOST_CHECK_MESSAGE(cp_copy == *aggregatedCommit,
                      "CommitPoint operator= failed");

  /// Check Challenge operator =
  Challenge challenge_copy;
  challenge_copy = challenge;
  BOOST_CHECK_MESSAGE(challenge_copy == challenge,
                      "Challenge operator= failed");

  /// Check Response operator =
  Response response_copy;
  response_copy = *aggregatedResponse;
  BOOST_CHECK_MESSAGE(response_copy == *aggregatedResponse,
                      "Response operator= failed");
}

/**
 * \brief test_serialization
 *
 * \details Test Response serialization
 */
BOOST_AUTO_TEST_CASE(test_serialization) {
  Schnorr& schnorr = Schnorr::GetInstance();
  MultiSig& multisig = MultiSig::GetInstance();

  /// Generate key pairs
  const unsigned int nbsigners = 80;
  vector<PrivKey> privkeys;
  vector<PubKey> pubkeys;
  for (unsigned int i = 0; i < nbsigners; i++) {
    PairOfKey keypair = schnorr.GenKeyPair();
    privkeys.emplace_back(keypair.first);
    pubkeys.emplace_back(keypair.second);
  }

  /// 1 MB message
  const unsigned int message_size = 1048576;
  bytes message_rand(message_size);
  bytes message_1(message_size, 0x01);
  generate(message_rand.begin(), message_rand.end(), std::rand);

  /// Aggregate public keys
  shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubkeys);
  BOOST_CHECK_MESSAGE(aggregatedPubkey != nullptr, "AggregatePubKeys failed");

  /// Generate individual commitments
  vector<CommitSecret> secrets(nbsigners);
  vector<CommitPoint> points;
  vector<CommitSecret> secrets1;
  vector<CommitPoint> points1;
  for (unsigned int i = 0; i < nbsigners; i++) {
    bytes tmp1, tmp2;
    secrets.at(i).Serialize(tmp1, 0);
    secrets1.emplace_back(tmp1, 0);
    points.emplace_back(secrets.at(i));
    points.back().Serialize(tmp2, 0);
    points1.emplace_back(tmp2, 0);
  }

  /// Check PrintPoint function
  schnorr.PrintPoint(aggregatedPubkey->m_P.get());

  /// Check CommitSecret operator =
  CommitSecret dummy_secret;
  dummy_secret = secrets.at(0);
  BOOST_CHECK_MESSAGE(dummy_secret == secrets.at(0),
                      "The operator = failed for CommitSecret");

  /// Aggregate commits
  shared_ptr<CommitPoint> aggregatedCommit = MultiSig::AggregateCommits(points);
  BOOST_CHECK_MESSAGE(aggregatedCommit != nullptr, "AggregateCommits failed");
  shared_ptr<CommitPoint> aggregatedCommit1 =
      MultiSig::AggregateCommits(points1);
  BOOST_CHECK_MESSAGE(*aggregatedCommit == *aggregatedCommit1,
                      "Commit serialization failed");

  /// Generate challenge
  Challenge challenge(*aggregatedCommit, *aggregatedPubkey, message_rand);
  BOOST_CHECK_MESSAGE(challenge.Initialized() == true,
                      "Challenge generation failed");
  bytes tmp;
  challenge.Serialize(tmp, 0);
  Challenge challenge2(tmp, 0);
  BOOST_CHECK_MESSAGE(challenge == challenge2,
                      "Challenge serialization failed");
  tmp.clear();

  /// Generate responses
  vector<Response> responses;
  vector<Response> responses1;
  for (unsigned int i = 0; i < nbsigners; i++) {
    responses.emplace_back(secrets.at(i), challenge, privkeys.at(i));
    BOOST_CHECK_MESSAGE(responses.back().Initialized() == true,
                        "Response generation failed");
    bytes tmp;
    responses.back().Serialize(tmp, 0);
    responses1.emplace_back(tmp, 0);
    // Verify response
    BOOST_CHECK_MESSAGE(
        MultiSig::VerifyResponse(responses.at(i), challenge, pubkeys.at(i),
                                 points.at(i)) == true,
        "Verify response failed");
  }

  /// Aggregate responses
  shared_ptr<Response> aggregatedResponse =
      MultiSig::AggregateResponses(responses);
  BOOST_CHECK_MESSAGE(aggregatedResponse != nullptr,
                      "AggregateResponses failed");
  shared_ptr<Response> aggregatedResponse1 =
      MultiSig::AggregateResponses(responses1);
  BOOST_CHECK_MESSAGE(*aggregatedResponse == *aggregatedResponse1,
                      "Response serialization failed");

  /// Generate the aggregated signature
  shared_ptr<Signature> signature =
      MultiSig::AggregateSign(challenge, *aggregatedResponse);
  BOOST_CHECK_MESSAGE(signature != nullptr, "AggregateSign failed");

  /// Verify the signature
  BOOST_CHECK_MESSAGE(multisig.MultiSigVerify(message_rand, *signature,
                                              *aggregatedPubkey) == true,
                      "Signature verification (correct message) failed");
  BOOST_CHECK_MESSAGE(multisig.MultiSigVerify(message_1, *signature,
                                              *aggregatedPubkey) == false,
                      "Signature verification (wrong message) failed");
}

BOOST_AUTO_TEST_SUITE_END()
