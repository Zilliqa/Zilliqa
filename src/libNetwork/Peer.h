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

#ifndef __PEER_H__
#define __PEER_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <cstdint>
#include <functional>

#include "common/Serializable.h"

/// Stores IP information on a single Zilliqa peer.
struct Peer : public Serializable {
  /// Peer IP address (net-encoded)
  boost::multiprecision::uint128_t m_ipAddress;  // net-encoded

  /// Peer listen port (host-encoded)
  uint32_t m_listenPortHost;  // host-encoded

  /// Default constructor.
  Peer();

  /// Constructor with specified IP info.
  Peer(const boost::multiprecision::uint128_t& ip_address,
       uint32_t listen_port_host);

  /// Constructor for loading peer information from a byte stream.
  Peer(const bytes& src, unsigned int offset);

  /// Equality comparison operator.
  bool operator==(const Peer& r) const;

  /// Inequality comparison operator.
  bool operator!=(const Peer& r) const;

  /// Less-than comparison operator.
  bool operator<(const Peer& r) const;

  /// Utility function for printing peer IP info.
  const std::string GetPrintableIPAddress() const;

  /// Utility std::string conversion function for peer IP info.
  explicit operator std::string() const {
    return "<" + GetPrintableIPAddress() + ":" +
           std::to_string(m_listenPortHost) + ">";
  }

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset);

  /// Getters.
  const boost::multiprecision::uint128_t& GetIpAddress() const;
  const uint32_t& GetListenPortHost() const;
};

inline std::ostream& operator<<(std::ostream& os, const Peer& p) {
  os << "<" << std::string(p.GetPrintableIPAddress()) << ":"
     << std::to_string(p.m_listenPortHost) << ">";
  return os;
}

namespace std {
template <>
struct hash<Peer> {
  size_t operator()(const Peer& obj) const {
    bytes s_peer;
    obj.Serialize(s_peer, 0);
    std::string str_peer(s_peer.begin(), s_peer.end());
    return std::hash<string>()(str_peer);
  }
};
}  // namespace std
#endif  // __PEER_H__
