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
