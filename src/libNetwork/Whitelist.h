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

/*
 * This should only be used in testnet release  only. This is to ensure the
 * stability of testnet.
 * Mainnet will not require this function and nodes will be incentivise to
 * perform the role as member of DS committee.
 */

#ifndef __WHITELIST_H__
#define __WHITELIST_H__

#include <boost/multiprecision/cpp_int.hpp>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Peer.h"
#include "libCrypto/Schnorr.h"

class Whitelist {
  Whitelist();
  ~Whitelist();

  // Singleton should not implement these
  Whitelist(Whitelist const&) = delete;
  void operator=(Whitelist const&) = delete;

  // DS whitelist
  std::mutex m_mutexDSWhiteList;
  std::unordered_map<Peer, PubKey> m_DSWhiteList;

  // Shard whitelist
  std::mutex m_mutexShardWhiteList;
  std::vector<PubKey> m_ShardWhiteList;

  // IPFilter
  std::mutex m_mutexIPExclusion;
  std::vector<std::pair<boost::multiprecision::uint128_t,
                        boost::multiprecision::uint128_t>>
      m_IPExclusionRange;

 public:
  /// Returns the singleton Whitelist instance.
  static Whitelist& GetInstance();
  void UpdateDSWhitelist();
  void UpdateShardWhitelist();

  void AddToDSWhitelist(const Peer& whiteListPeer,
                        const PubKey& whiteListPubKey);
  bool IsNodeInDSWhiteList(const Peer& nodeNetworkInfo,
                           const PubKey& nodePubKey);
  bool IsPubkeyInShardWhiteList(const PubKey& nodePubKey);

  // To check if IP is a valid v4 IP and not belongs to exclusion list
  bool IsValidIP(const boost::multiprecision::uint128_t& ip_addr);

  // To add limits to the exclusion list
  void AddToExclusionList(const boost::multiprecision::uint128_t& ft,
                          const boost::multiprecision::uint128_t& sd);
  void AddToExclusionList(const std::string& ft, const std::string& sd);
  // Intialize
  void Init();
};

#endif  // __WHITELIST_H__
