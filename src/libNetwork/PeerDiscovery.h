/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBNETWORK_PEERDISCOVERY_H_
#define ZILLIQA_SRC_LIBNETWORK_PEERDISCOVERY_H_

#include "Executable.h"
#include "Peer.h"

#include <chrono>
#include <memory>

#include <Schnorr.h>

namespace boost::asio {
class io_context;
}

namespace zil::p2p {

class P2P;

using Milliseconds = std::chrono::milliseconds;

/// Role of the peer
enum class Role { INDEFINITE, NORMAL, DSGUARD, LOOKUP, SEEDPUB, _ARRAY_SIZE_ };

/// Parses e.g. 'dsguard-2' into { Role::DSGUARD, 2 }
void RoleAndIndexFromString(const std::string& str, Role& role,
                            uint32_t& index);

std::string RoleAndIndexToString(Role role, uint32_t index);

class PeerDiscovery : public Executable {
 public:
  struct Options {
    PrivKey selfFrivateKey;
    PubKey selfPubKey;
    Role selfRole;
    uint32_t selfPeerIndex;
    uint16_t selfPort;
    std::vector<Peer> lookups;
    Milliseconds timerInterval = Milliseconds(5000);
    Milliseconds historyExpiration = Milliseconds(3600000);
  };

  struct PeerInfo {
    PubKey pubKey;
    Peer peer;
    Role role = Role::INDEFINITE;
    uint32_t index = 0;
  };

  using PeerInfoPtr = std::shared_ptr<const PeerInfo>;

  static std::shared_ptr<PeerDiscovery> Create(boost::asio::io_context& asio,
                                               P2P& p2p, Options options);

  virtual ~PeerDiscovery() = default;

  /// Returns peer info by public key. Returns nullptr if not found
  virtual PeerInfoPtr GetByPubkey(const PubKey& pubKey) const = 0;

  /// Returns peers by the role specified. Returns empty vector if none found
  virtual std::vector<PeerInfoPtr> GetByRole(Role role) const = 0;
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_PEERDISCOVERY_H_
