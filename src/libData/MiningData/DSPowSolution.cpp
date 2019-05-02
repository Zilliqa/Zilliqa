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

#include "DSPowSolution.h"
#include <algorithm>
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
    const uint128_t& gasPriceInput, const Signature& signatureInput)
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
const uint128_t& DSPowSolution::GetGasPrice() const { return m_gasPrice; }

/// Returns Signature
const Signature& DSPowSolution::GetSignature() const { return m_signature; }

/// Sets Signature
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
