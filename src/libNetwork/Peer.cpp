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

#include "Peer.h"
#include <arpa/inet.h>
#include "common/Constants.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

Peer::Peer() : m_ipAddress(0), m_listenPortHost(0) {}

Peer::Peer(const uint128_t& ip_address, uint32_t listen_port_host, const std::string& nodeIndentifier)
    : m_ipAddress(ip_address), m_listenPortHost(listen_port_host), m_nodeIndentifier(nodeIndentifier) {
  if (m_ipAddress == 0) {
        LOG_GENERAL(WARNING, "We tried to init Peer with zero IP address.");
  } else if  (IPConverter::ToStrFromNumericalIP(m_ipAddress) == "127.0.0.1") {
        LOG_GENERAL(WARNING, "We tried to init Peer with 127.0.0.1 port.");
  }
}

Peer::Peer(const zbytes& src, unsigned int offset)
    : m_ipAddress(0), m_listenPortHost(0) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init Peer.");
  }
}

bool Peer::operator==(const Peer& r) const {
  return (m_ipAddress == r.m_ipAddress) &&
         (m_listenPortHost == r.m_listenPortHost);
}

bool Peer::operator!=(const Peer& r) const {
  return (m_ipAddress != r.m_ipAddress) ||
         (m_listenPortHost != r.m_listenPortHost);
}

bool Peer::operator<(const Peer& r) const {
  return (m_ipAddress < r.m_ipAddress) ||
         ((m_ipAddress == r.m_ipAddress) &&
          (m_listenPortHost < r.m_listenPortHost));
}

const string Peer::GetPrintableIPAddress() const {
  return IPConverter::ToStrFromNumericalIP(m_ipAddress);
}

unsigned int Peer::Serialize(zbytes& dst, unsigned int offset) const {
  Serializable::SetNumber<uint128_t>(dst, offset, m_ipAddress, UINT128_SIZE);
  Serializable::SetNumber<uint32_t>(dst, offset + UINT128_SIZE,
                                    m_listenPortHost, sizeof(uint32_t));

  return UINT128_SIZE + sizeof(uint32_t);
}

int Peer::Deserialize(const zbytes& src, unsigned int offset) {
  try {
    m_ipAddress = Serializable::GetNumber<uint128_t>(src, offset, UINT128_SIZE);
    m_listenPortHost = Serializable::GetNumber<uint32_t>(
        src, offset + UINT128_SIZE, sizeof(uint32_t));
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with Peer::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

void Peer::SetHostname(const std::string& hostname) { m_hostname = hostname; }
const uint32_t& Peer::GetListenPortHost() const { return m_listenPortHost; }
const uint128_t& Peer::GetIpAddress() const { return m_ipAddress; }
const std::string Peer::GetHostname() const { return m_hostname; }
const std::string Peer::GetNodeIndentifier() const { return m_nodeIndentifier; }
