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

#include "DSPowSolution.h"
#include <algorithm>
#include "libCrypto/Sha2.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

DSPowSolution::DSPowSolution() {}

DSPowSolution::DSPowSolution(const DSPowSolution& src)
    : m_blockNumber(src.m_blockNumber),
      m_difficultyLevel(src.m_difficultyLevel),
      m_submitterPeer(src.m_submitterPeer),
      m_submitterKey(src.m_submitterKey),
      m_nonce(src.m_nonce),
      m_resultingHash(src.m_resultingHash),
      m_mixHash(src.m_mixHash),
      m_lookupId(src.m_lookupId),
      m_gasPrice(src.m_gasPrice),
      m_signature(src.m_signature) {}

DSPowSolution::DSPowSolution(
    const uint64_t& blockNumberInput, const uint8_t& difficultyLevelInput,
    const Peer& submitterPeerInput, const PubKey& submitterKeyInput,
    const uint64_t& nonceInput, const std::string& resultingHashInput,
    const std::string& mixHashInput, const uint32_t& lookupIdInput,
    const boost::multiprecision::uint128_t& gasPriceInput,
    const Signature& signatureInput)
    : m_blockNumber(blockNumberInput),
      m_difficultyLevel(difficultyLevelInput),
      m_submitterPeer(submitterPeerInput),
      m_submitterKey(submitterKeyInput),
      m_nonce(nonceInput),
      m_resultingHash(resultingHashInput),
      m_mixHash(mixHashInput),
      m_lookupId(lookupIdInput),
      m_gasPrice(gasPriceInput),
      m_signature(signatureInput) {}

/// Returns the current block number.
const uint64_t& DSPowSolution::GetBlockNumber() const { return m_blockNumber; }

/// Returns the diffculty level.
const uint8_t& DSPowSolution::GetDifficultyLevel() const {
  return m_difficultyLevel;
}

/// Returns the submitter peer
const Peer& DSPowSolution::GetSubmitterPeer() const { return m_submitterPeer; }

//// Returns the public Key.
const PubKey& DSPowSolution::GetSubmitterKey() const { return m_submitterKey; }

/// Returns the nonce
uint64_t DSPowSolution::GetNonce() const { return m_nonce; }

/// Returns resulting hash
const std::string& DSPowSolution::GetResultingHash() const {
  return m_resultingHash;
}

/// Returns mix hash
const std::string& DSPowSolution::GetMixHash() const { return m_mixHash; }

/// Returns lookupid
const uint32_t& DSPowSolution::GetLookupId() const { return m_lookupId; }

/// Returns gas price
const boost::multiprecision::uint128_t& DSPowSolution::GetGasPrice() const {
  return m_gasPrice;
}

const Signature& DSPowSolution::GetSignature() const { return m_signature; }

void DSPowSolution::SetSignature(const Signature& signature) {
  m_signature = signature;
}

bool DSPowSolution::operator==(const DSPowSolution& sol) const {
  return ((m_blockNumber == sol.m_blockNumber) &&
          (m_difficultyLevel == sol.m_difficultyLevel) &&
          (m_submitterPeer == sol.m_submitterPeer) &&
          (m_submitterKey == sol.m_submitterKey) && (m_nonce == sol.m_nonce) &&
          (m_resultingHash == sol.m_resultingHash) &&
          (m_mixHash == sol.m_mixHash) && (m_lookupId == sol.m_lookupId) &&
          (m_gasPrice == sol.m_gasPrice) && (m_signature == sol.m_signature));
}

DSPowSolution& DSPowSolution::operator=(const DSPowSolution& src) {
  m_blockNumber = src.m_blockNumber;
  m_difficultyLevel = src.m_difficultyLevel;
  m_submitterPeer = src.m_submitterPeer;
  m_submitterKey = src.m_submitterKey;
  m_nonce = src.m_nonce;
  m_resultingHash = src.m_resultingHash;
  m_mixHash = src.m_mixHash;
  m_lookupId = src.m_lookupId;
  m_gasPrice = src.m_gasPrice;
  m_signature = src.m_signature;

  return *this;
}