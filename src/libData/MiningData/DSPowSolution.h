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

#ifndef __DSPowSolution_H__
#define __DSPowSolution_H__

#include <array>

#include "common/Constants.h"
#include "libCrypto/Schnorr.h"
#include "libNetwork/Peer.h"

/// Stores information on a single pow solution.
class DSPowSolution {
  uint64_t m_blockNumber;
  uint8_t m_difficultyLevel;
  Peer m_submitterPeer;
  PubKey m_submitterKey;
  uint64_t m_nonce;
  std::string m_resultingHash;
  std::string m_mixHash;
  uint32_t m_lookupId;
  uint128_t m_gasPrice;
  Signature m_signature;

 public:
  /// Default constructor.
  DSPowSolution();

  /// Copy constructor.
  DSPowSolution(const DSPowSolution& src);

  /// Constructor with specified transaction fields.
  DSPowSolution(const uint64_t& blockNumberInput,
                const uint8_t& difficultyLevelInput,
                const Peer& submitterPeerInput, const PubKey& submitterKeyInput,
                const uint64_t& nonceInput,
                const std::string& resultingHashInput,
                const std::string& mixHashInput, const uint32_t& lookupIdInput,
                const uint128_t& gasPriceInput,
                const Signature& signatureInput);

  /// Constructor for loading DSPowSolution information from a byte stream.
  DSPowSolution(const bytes& src, unsigned int offset);

  /// Returns the current block number.
  const uint64_t& GetBlockNumber() const;

  /// Returns the diffculty level.
  const uint8_t& GetDifficultyLevel() const;

  /// Returns the submitter peer
  const Peer& GetSubmitterPeer() const;

  //// Returns the public Key.
  const PubKey& GetSubmitterKey() const;

  /// Returns the nonce
  uint64_t GetNonce() const;

  /// Returns resulting hash
  const std::string& GetResultingHash() const;

  /// Returns mix hash
  const std::string& GetMixHash() const;

  /// Returns lookupid
  const uint32_t& GetLookupId() const;

  /// Returns gas price
  const uint128_t& GetGasPrice() const;

  /// Return signature
  const Signature& GetSignature() const;

  /// Set the signature
  void SetSignature(const Signature& signature);

  /// Equality comparison operator.
  bool operator==(const DSPowSolution& sol) const;

  /// Assignment operator.
  DSPowSolution& operator=(const DSPowSolution& src);
};

#endif  // __DSPowSolution_H__
