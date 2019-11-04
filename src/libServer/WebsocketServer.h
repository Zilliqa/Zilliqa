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

#include <json/json.h>
#include <mutex>
#include <set>
#include <unordered_map>

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include "common/Constants.h"
#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block.h"

typedef websocketpp::server<websocketpp::config::asio> websocketserver;

using IpSocketMap =
    std::unordered_map<std::string, websocketpp::connection_hdl>;

enum WEBSOCKETQUERY : unsigned int { NEWBLOCK, EVENTLOG };

using EndpointQueryIndex = std::unordered_map<std::string, WEBSOCKETQUERY>;

struct EventLogSocketTracker {
  // for updating event log for client subscribed
  std::unordered_map<Address, std::set<std::string>> m_addr_ip_map;
  // for removing socket from m_eventlog_hdl_tracker
  std::unordered_map<std::string, std::set<Address>> m_ip_addr_map;

  void remove(const std::string& ip) {
    auto iter_ha = m_ip_addr_map.find(ip);
    if (iter_ha == m_ip_addr_map.end()) {
      return;
    }
    for (const auto& addr : iter_ha->second) {
      auto iter_ah = m_addr_ip_map.find(addr);
      if (iter_ah != m_addr_ip_map.end()) {
        iter_ah->second.erase(ip);
      }
      if (iter_ah->second.empty()) {
        m_addr_ip_map.erase(iter_ah);
      }
    }
    m_ip_addr_map.erase(iter_ha);
  }

  void update(const std::string& ip, const std::set<Address>& addresses) {
    for (const auto& addr : addresses) {
      m_addr_ip_map[addr].emplace(ip);
    }
    m_ip_addr_map[ip] = addresses;
  }

  void clean() {
    m_addr_ip_map.clear();
    m_ip_addr_map.clear();
  }
};

class WebsocketServer : public Singleton<WebsocketServer> {
 public:
  /// Returns the singleton AccountStore instance.
  static WebsocketServer& GetInstance() {
    static WebsocketServer ws;
    return ws;
  }

  /// clean in-memory data structures
  void clean();

  /// Send string data to hdl connection
  bool sendData(const websocketpp::connection_hdl& hdl,
                const std::string& data);

  /// Public interface for sending TxBlock and TxHashes
  bool SendTxBlockAndTxHashes(const Json::Value& json_txblock,
                              const Json::Value& json_txhashes);

  /// Public interface to digest contract event from transaction receipts
  void ParseTxnEventLog(const TransactionWithReceipt& twr);

  /// Public interface to send all digested contract events to subscriber
  void SendOutEventLog();

 private:
  /// Singleton constructor and start service immediately
  WebsocketServer() {
    if (!start()) {
      LOG_GENERAL(FATAL, "WebsocketServer start failed");
      ENABLE_WEBSOCKET = false;
      stop();
      return;
    }
  }

  /// Singleton desctructor and stop service
  ~WebsocketServer() { stop(); }

  /// Start websocket server
  bool start();

  /// Stop websocket server
  void stop();

  /// get connection_hdl with ip and WEBSOCKETQUERY
  bool getWebsocket(const std::string& ip, WEBSOCKETQUERY query,
                    websocketpp::connection_hdl& hdl);

  /// remove a connection_hdl in memory via remote endpoint string
  static void removeSocket(const std::string& remote);

  /// remove a connection_hdl in memory via endpoint ip and WEBSOCKETQUERY
  static void removeSocket(const std::string& ip, WEBSOCKETQUERY q_enum);

  /// close a socket from connection_hdl
  static bool closeSocket(const websocketpp::connection_hdl& hdl);

  /// websocketpp server instance
  static websocketserver m_server;

  /// mapping endpoint to the WEBSOCKETQUERY
  static std::mutex m_mutexEqIndex;
  static EndpointQueryIndex m_eqIndex;

  /// containing the connection_hdl for TxBlock subscriber
  static std::mutex m_mutexTxBlockSockets;
  static IpSocketMap m_txblock_websockets;

  /// containing the connection_hdl for EventLog subscriber
  static std::mutex m_mutexEventLogSockets;
  static IpSocketMap m_eventlog_websockets;
  /// a utility data structure for mapping address and subscriber of EventLog
  /// regarding of new comer or quiting
  static EventLogSocketTracker m_elsockettracker;

  /// a buffer for keeping the eventlog to send for each subscriber
  static std::mutex m_mutexELDataBufferSockets;
  static std::unordered_map<std::string,
                            std::unordered_map<Address, Json::Value>>
      m_eventLogDataBuffer;

  /// make run() detached in a new thread to avoid blocking
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

  /// standard callbacks for websocket server instance
  static void on_message(const websocketpp::connection_hdl& hdl,
                         const websocketserver::message_ptr& msg);
  static void on_fail(const websocketpp::connection_hdl& hdl);
  static void on_close(const websocketpp::connection_hdl& hdl);
};

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_