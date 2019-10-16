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

#include <mutex>
#include <set>
#include <unordered_map>

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block.h"

using HostSocketMap =
    std::unordered_map<std::string, websocketpp::connection_hdl>;

namespace Websocket {
enum QUERY : unsigned int { NEWBLOCK, EVENTLOG };

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

  // external interface
  static bool SendTxBlock(const TxBlock& txblock);
  static void ParseTxnEventLog(const TransactionWithReceipt& twr);
  static void SendOutEventLog();

 private:
  static bool getWebsocket(const std::string& host, QUERY query,
                           websocketpp::connection_hdl& hdl);
  static void removeSocket(const std::string& host, const std::string& query);

  static websocketpp::server<websocketpp::config::asio> m_server;

  static std::mutex m_mutexTxBlockSockets;
  static HostSocketMap m_txblock_websockets;

  static std::mutex m_mutexEventLogSockets;
  static HostSocketMap m_eventlog_websockets;
  static EventLogSocketTracker m_elsockettracker;

  static std::mutex m_mutexELDataBufferSockets;
  static std::unordered_map<std::string,
                            std::unordered_map<Address, Json::Value>>
      m_eventLogDataBuffer;
  // ostream os;

  // callbacks
  static bool on_validate(websocketpp::connection_hdl hdl);
  static void on_fail(websocketpp::connection_hdl hdl);
  static void on_close(websocketpp::connection_hdl hdl);
};
}  // namespace Websocket

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_