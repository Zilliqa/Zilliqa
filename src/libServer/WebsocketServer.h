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

#ifndef ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_

#include <set>
#include <unordered_map>

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"

namespace Websocket {
enum QUERY : unsigned int { NEWBLOCK, EVENTLOG };
struct WebsocketInfo {
  std::string m_host;
  QUERY m_query;

  bool operator==(const WebsocketInfo& info) const {
    return std::tie(m_host, m_query) == std::tie(info.m_host, info.m_query);
  }
};
}  // namespace Websocket

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<Websocket::WebsocketInfo> {
  size_t operator()(Websocket::WebsocketInfo const& info) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, info.m_host);
    boost::hash_combine(seed, to_string(info.m_query));

    return seed;
  }
};
}  // namespace std

namespace Websocket {
struct EventLogSocketTracker {
  // for updating event log for client subscribed
  std::unordered_map<Address, std::set<std::string>> m_addr_host_map;
  // for removing socket from m_eventlog_hdl_tracker
  std::unordered_map<std::string, std::set<Address>> m_host_addr_map;

  void add(const std::string& host, const std::vector<Address>& addresses) {
    for (const auto& addr : addresses) {
      m_addr_host_map[addr].emplace(host);
      m_host_addr_map[host].emplace(addr);
    }
  }

  void remove(const std::string& host) {
    auto iter_ha = m_host_addr_map.find(host);
    if (iter_ha == m_host_addr_map.end()) {
      return;
    }
    for (const auto& addr : iter_ha->second) {
      auto iter_ah = m_addr_host_map.find(addr);
      if (iter_ah != m_addr_host_map.end()) {
        iter_ah->second.erase(host);
      }
      if (iter_ah->second.empty()) {
        m_addr_host_map.erase(iter_ah);
      }
    }
    m_host_addr_map.erase(iter_ha);
  }

  void clean() {
    m_addr_host_map.clear();
    m_host_addr_map.clear();
  }
};

class WebsocketServer {
 public:
  static bool init();
  static void run();
  static void stop();
  static void clean();

  static bool sendData(websocketpp::connection_hdl hdl,
                       const std::string& data);
  static bool closeSocket(websocketpp::connection_hdl hdl);

 private:
  static bool getWebsocket(const WebsocketInfo& id,
                           websocketpp::connection_hdl& hdl);
  static void removeSocket(const std::string& host, const std::string& query);

  static websocketpp::server<websocketpp::config::asio> m_server;
  static pthread_rwlock_t m_websocketsLock;
  static std::unordered_map<WebsocketInfo, websocketpp::connection_hdl>
      m_websockets;
  static EventLogSocketTracker m_elsockettracker;
  // ostream os;

  // callbacks
  static bool on_validate(websocketpp::connection_hdl hdl);
  static void on_fail(websocketpp::connection_hdl hdl);
  static void on_close(websocketpp::connection_hdl hdl);
};
}  // namespace Websocket

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_