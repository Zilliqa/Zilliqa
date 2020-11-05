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
#include <memory>
#include <mutex>
#include <set>

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include "common/Constants.h"
#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block.h"

typedef websocketpp::server<websocketpp::config::asio> websocketserver;

enum WEBSOCKETQUERY : unsigned int { NEWBLOCK, EVENTLOG, TXNLOG, UNSUBSCRIBE };

struct Subscription {
  std::set<WEBSOCKETQUERY> queries;
  std::set<WEBSOCKETQUERY> unsubscribings;

  void subscribe(WEBSOCKETQUERY query) { queries.emplace(query); }

  void unsubscribe_start(WEBSOCKETQUERY query) {
    if (queries.find(query) != queries.end()) {
      unsubscribings.emplace(query);
    }
  }

  bool subscribed(WEBSOCKETQUERY query) {
    return queries.find(query) != queries.end();
  }

  void unsubscribe_finish() {
    for (auto unsubscribing : unsubscribings) {
      queries.erase(unsubscribing);
    }
    unsubscribings.clear();
  }
};

struct EventLogAddrHdlTracker {
  // for updating event log for client subscribed
  std::map<Address, std::set<websocketpp::connection_hdl,
                             std::owner_less<websocketpp::connection_hdl>>>
      m_addr_hdl_map;
  // for removing socket from m_eventlog_hdl_tracker
  std::map<websocketpp::connection_hdl, std::set<Address>,
           std::owner_less<websocketpp::connection_hdl>>
      m_hdl_addr_map;

  void remove(const websocketpp::connection_hdl& hdl) {
    auto iter_hdl_addr = m_hdl_addr_map.find(hdl);
    if (iter_hdl_addr == m_hdl_addr_map.end()) {
      return;
    }
    for (const auto& addr : iter_hdl_addr->second) {
      auto iter_addr_hdl = m_addr_hdl_map.find(addr);
      if (iter_addr_hdl != m_addr_hdl_map.end()) {
        iter_addr_hdl->second.erase(hdl);
      }
      if (iter_addr_hdl->second.empty()) {
        m_addr_hdl_map.erase(iter_addr_hdl);
      }
    }
    m_hdl_addr_map.erase(iter_hdl_addr);
  }

  void update(const websocketpp::connection_hdl& hdl,
              const std::set<Address>& addresses) {
    for (const auto& addr : addresses) {
      m_addr_hdl_map[addr].emplace(hdl);
    }
    m_hdl_addr_map[hdl] = addresses;
  }

  void clear() {
    m_addr_hdl_map.clear();
    m_hdl_addr_map.clear();
  }
};

class WebsocketServer : public Singleton<WebsocketServer> {
  /// websocketpp server instance
  static websocketserver m_server;

  static std::mutex m_mutexSubscriptions;
  static std::map<websocketpp::connection_hdl, Subscription,
                  std::owner_less<websocketpp::connection_hdl>>
      m_subscriptions;

  /// a utility data structure for mapping address and subscriber of EventLog
  /// regarding of new comer or quiting
  static std::mutex m_mutexEventLogAddrHdlTracker;
  static EventLogAddrHdlTracker m_eventLogAddrHdlTracker;

  static std::mutex m_mutexTxnLogAddrHdlTracker;
  static EventLogAddrHdlTracker m_txnLogAddrHdlTracker;

  /// a buffer for keeping the eventlog to send for each subscriber

  static std::mutex m_mutexEventLogDataBuffer;
  static std::map<websocketpp::connection_hdl,
                  std::unordered_map<Address, Json::Value>,
                  std::owner_less<websocketpp::connection_hdl>>
      m_eventLogDataBuffer;

  static std::mutex m_mutexTxnLogDataBuffer;
  static std::map<websocketpp::connection_hdl,
                  std::unordered_map<Address, Json::Value>,
                  std::owner_less<websocketpp::connection_hdl>>
      m_txnLogDataBuffer;

  /// make run() detached in a new thread to avoid blocking
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

  std::mutex m_mutexTxnBlockNTxnHashes;
  Json::Value m_jsonTxnBlockNTxnHashes;

 public:
  /// Returns the singleton AccountStore instance.
  static WebsocketServer& GetInstance() {
    static WebsocketServer ws;
    return ws;
  }

  // /// Public interface for sending TxBlock and TxHashes
  void PrepareTxBlockAndTxHashes(const Json::Value& json_txblock,
                                 const Json::Value& json_txhashes);

  /// Public interface to digest contract event from transaction receipts
  void ParseTxnEventLog(const TransactionWithReceipt& twr);

  //
  void ParseTxn(const TransactionWithReceipt& twr);

  void ParseTxnLog(const TransactionWithReceipt& twr);

  // /// Public interface to send all digested contract events to subscriber
  void SendOutMessages();

 private:
  /// Singleton constructor and start service immediately
  WebsocketServer() {
    m_server.clear_access_channels(websocketpp::log::alevel::all);
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

  /// close a socket from connection_hdl
  static void closeSocket(
      const websocketpp::connection_hdl& hdl, const std::string& reason,
      const websocketpp::close::status::value& close_status);

  /// clean in-memory data structures
  void clean();

  /// Send string data to hdl connection
  bool sendData(const websocketpp::connection_hdl& hdl,
                const std::string& data);

  /// standard callbacks for websocket server instance
  static void on_message(const websocketpp::connection_hdl& hdl,
                         const websocketserver::message_ptr& msg);
  // static void on_fail(const websocketpp::connection_hdl& hdl);
  static void on_close(const websocketpp::connection_hdl& hdl);
  static void on_http(const websocketpp::connection_hdl& hdl);
};

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_