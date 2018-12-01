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

#ifndef __DSPowSolution_H__
#define __DSPowSolution_H__

#include <array>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

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
  boost::multiprecision::uint128_t m_gasPrice;
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
                const boost::multiprecision::uint128_t& gasPriceInput,
                const Signature& signatureInput);

  /// Constructor for loading DSPowSolution information from a byte stream.
  DSPowSolution(const std::vector<unsigned char>& src, unsigned int offset);

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
  const boost::multiprecision::uint128_t& GetGasPrice() const;

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
