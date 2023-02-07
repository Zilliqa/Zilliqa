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

#ifndef ZILLIQA_SRC_LIBNETWORK_PEER_H_
#define ZILLIQA_SRC_LIBNETWORK_PEER_H_

#include "common/Serializable.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

/// Stores IP information on a single Zilliqa peer.
struct Peer : public Serializable {
  /// Peer IP address (net-encoded)
  uint128_t m_ipAddress;  // net-encoded

  /// Peer listen port (host-encoded)
  uint32_t m_listenPortHost;  // host-encoded

  /// Peer hostname
  std::string m_hostname;  // optional

  /// Constructor with specified IP info.
  Peer(const uint128_t& ip_address = 0, uint32_t listen_port_host = 0)
      : m_ipAddress(ip_address), m_listenPortHost(listen_port_host) {}

  /// Constructor for loading peer information from a byte stream.
  Peer(const zbytes& src, unsigned int offset)
      : m_ipAddress(0), m_listenPortHost(0) {
    if (Deserialize(src, offset) != 0) {
      LOG_GENERAL(WARNING, "We failed to init Peer.");
    }
  }

  /// Equality comparison operator.
  bool operator==(const Peer& r) const {
    return (m_ipAddress == r.m_ipAddress) &&
           (m_listenPortHost == r.m_listenPortHost);
  }

  /// Inequality comparison operator.
  bool operator!=(const Peer& r) const {
    return (m_ipAddress != r.m_ipAddress) ||
           (m_listenPortHost != r.m_listenPortHost);
  }

  /// Less-than comparison operator.
  bool operator<(const Peer& r) const {
    return (m_ipAddress < r.m_ipAddress) ||
           ((m_ipAddress == r.m_ipAddress) &&
            (m_listenPortHost < r.m_listenPortHost));
  }

  /// Utility function for printing peer IP info.
  std::string GetPrintableIPAddress() const {
    return IPConverter::ToStrFromNumericalIP(m_ipAddress);
  }

  /// Utility std::string conversion function for peer IP info.
  explicit operator std::string() const {
    return "<" + GetPrintableIPAddress() + ":" +
           std::to_string(m_listenPortHost) + ">";
  }

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(zbytes& dst, unsigned int offset) const {
    Serializable::SetNumber<uint128_t>(dst, offset, m_ipAddress, UINT128_SIZE);
    Serializable::SetNumber<uint32_t>(dst, offset + UINT128_SIZE,
                                      m_listenPortHost, sizeof(uint32_t));

    return UINT128_SIZE + sizeof(uint32_t);
  }

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const zbytes& src, unsigned int offset) {
    try {
      m_ipAddress =
          Serializable::GetNumber<uint128_t>(src, offset, UINT128_SIZE);
      m_listenPortHost = Serializable::GetNumber<uint32_t>(
          src, offset + UINT128_SIZE, sizeof(uint32_t));
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Error with Peer::Deserialize." << ' ' << e.what());
      return -1;
    }
    return 0;
  }

  /// Setter
  void SetHostname(const std::string& hostname) { m_hostname = hostname; }

  /// Getters.
  const uint128_t& GetIpAddress() const { return m_ipAddress; }
  uint32_t GetListenPortHost() const { return m_listenPortHost; }
  std::string GetHostname() const { return m_hostname; }
};
namespace IPCHECK {
static inline bool IsPortValid(const uint32_t listenPort) {
  return (listenPort <= 65535);
}
}  // namespace IPCHECK

inline std::ostream& operator<<(std::ostream& os, const Peer& p) {
  os << "<" << std::string(p.GetPrintableIPAddress()) << ":"
     << std::to_string(p.m_listenPortHost) << ">";
  return os;
}

namespace std {
template <>
struct hash<Peer> {
  size_t operator()(const Peer& obj) const {
    zbytes s_peer;
    obj.Serialize(s_peer, 0);
    std::string str_peer(s_peer.begin(), s_peer.end());
    return std::hash<string>()(str_peer);
  }
};
}  // namespace std
#endif  // ZILLIQA_SRC_LIBNETWORK_PEER_H_
