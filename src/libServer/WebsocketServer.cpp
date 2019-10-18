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

#include "WebsocketServer.h"
#include "LookupServer.h"

#include "AddressChecksum.h"
#include "JSONConversion.h"

#include "libCrypto/Sha2.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"

using websocketpp::connection_hdl;

using namespace std;
using namespace dev;

websocketpp::server<websocketpp::config::asio> WebsocketServer::m_server;

mutex WebsocketServer::m_mutexTxBlockSockets;
HostSocketMap WebsocketServer::m_txblock_websockets;

mutex WebsocketServer::m_mutexEventLogSockets;
HostSocketMap WebsocketServer::m_eventlog_websockets;
EventLogSocketTracker WebsocketServer::m_elsockettracker;

mutex WebsocketServer::m_mutexELDataBufferSockets;
std::unordered_map<std::string, std::unordered_map<Address, Json::Value>>
    WebsocketServer::m_eventLogDataBuffer;

bool WebsocketServer::init() {
  // Initialising websocketserver
  m_server.init_asio();

  // Set custom logger (ostream-based)
  // server.get_alog().set_ostream(&os);
  // server.get_elog().set_ostream(&os);

  // Register the message handlers.
  m_server.set_validate_handler(&WebsocketServer::on_validate);
  m_server.set_fail_handler(&WebsocketServer::on_fail);
  m_server.set_close_handler(&WebsocketServer::on_close);

  try {
    m_server.listen(WEBSOCKET_PORT);
  } catch (websocketpp::exception const& e) {
    // Websocket exception on listen.
    LOG_GENERAL(WARNING, "Websocket listen failed, error: " << e.what());
    return false;
  }

  websocketpp::lib::error_code ec;
  m_server.start_accept(ec);
  if (ec) {
    LOG_GENERAL(WARNING,
                "websocket start_accept failed, error: " << ec.message());
    return false;
  }

  // run, make it in a seperate thread

  return true;
}

void WebsocketServer::run() {
  try {
    m_server.run();
  } catch (websocketpp::exception const& e) {
    LOG_GENERAL(WARNING, "websocket run failed, error: " << e.what());
  }
}

void WebsocketServer::stop() {
  LOG_MARKER();
  // stopping the Websocket listener and closing outstanding connection
  websocketpp::lib::error_code ec;
  m_server.stop_listening(ec);
  if (ec) {
    LOG_GENERAL(WARNING,
                "websocket stop_listening failed, error: " << ec.message());
    return;
  }

  {
    // Close all existing websocket connections.
    lock_guard<mutex> g(m_mutexTxBlockSockets);
    for (auto it = m_txblock_websockets.begin();
         it != m_txblock_websockets.end(); ++it) {
      websocketpp::lib::error_code ec;
      m_server.close(it->second, websocketpp::close::status::normal,
                     "Terminating connection...", ec);
      if (ec) {
        LOG_GENERAL(WARNING, "websocket stop_listening (1) failed, error: "
                                 << ec.message());
      }
    }
  }

  {
    lock_guard<mutex> g(m_mutexEventLogSockets);
    for (auto it = m_eventlog_websockets.begin();
         it != m_eventlog_websockets.end(); ++it) {
      websocketpp::lib::error_code ec;
      m_server.close(it->second, websocketpp::close::status::normal,
                     "Terminating connection...", ec);
      if (ec) {
        LOG_GENERAL(WARNING, "websocket stop_listening (2) failed, error: "
                                 << ec.message());
      }
    }
  }

  // Stop the end point
  m_server.stop();
  clean();
}

void WebsocketServer::clean() {
  {
    lock_guard<mutex> g(m_mutexTxBlockSockets);
    m_txblock_websockets.clear();
  }
  {
    lock_guard<mutex> g(m_mutexEventLogSockets);
    m_eventlog_websockets.clear();
    m_elsockettracker.clean();
  }
  {
    lock_guard<mutex> g(m_mutexELDataBufferSockets);
    m_eventLogDataBuffer.clear();
  }
}

bool GetQueryEnum(const string& query, WEBSOCKETQUERY& q_enum) {
  if (query == "NewBlock") {
    q_enum = NEWBLOCK;
  } else if (query == "EventLog") {
    q_enum = EVENTLOG;
  } else {
    return false;
  }

  return true;
}

bool WebsocketServer::getWebsocket(const string& host, WEBSOCKETQUERY query,
                                   connection_hdl& hdl) {
  HostSocketMap::iterator it;
  switch (query) {
    case NEWBLOCK:
      it = m_txblock_websockets.find(host);
      if (it == m_txblock_websockets.end()) {
        return false;
      }
      break;
    case EVENTLOG:
      it = m_eventlog_websockets.find(host);
      if (it == m_eventlog_websockets.end()) {
        return false;
      }
      break;
  }

  hdl = it->second;
  return true;
}

void WebsocketServer::removeSocket(const std::string& host,
                                   const std::string& query) {
  LOG_GENERAL(INFO, "remove conn: " << host << "(" << query << ")");
  WEBSOCKETQUERY q_enum;
  if (!GetQueryEnum(query, q_enum)) {
    LOG_GENERAL(WARNING, "GetQueryEnum failed");
    return;
  }

  HostSocketMap::iterator it;
  switch (q_enum) {
    case NEWBLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockSockets);
      it = m_txblock_websockets.find(host);
      if (it != m_txblock_websockets.end()) {
        m_txblock_websockets.erase(it);
      }
      break;
    }
    case EVENTLOG: {
      {
        lock_guard<mutex> g(m_mutexEventLogSockets);
        m_eventlog_websockets.erase(host);
        m_elsockettracker.remove(host);
      }
      {
        lock_guard<mutex> g(m_mutexELDataBufferSockets);
        m_eventLogDataBuffer.erase(host);
      }
      break;
    }
  }
}

bool WebsocketServer::on_validate(connection_hdl hdl) {
  LOG_MARKER();
  websocketpp::server<websocketpp::config::asio>::connection_ptr con =
      m_server.get_con_from_hdl(hdl);
  websocketpp::uri_ptr uri = con->get_uri();
  string host = uri->get_host();
  string query = uri->get_query();
  if (query.empty()) {
    return false;
  }

  Json::Value j_query;
  if (!JSONUtils::GetInstance().convertStrtoJson(query, j_query)) {
    return false;
  }

  if (!j_query.isObject()) {
    return false;
  }

  if (!j_query.isMember("query")) {
    return false;
  }

  if (!j_query["query"].isString()) {
    return false;
  }

  vector<Address> el_addresses;

  if (j_query["query"].asString() == "NewBlock") {
  } else if (j_query["query"].asString() == "EventLog") {
    if (!j_query.isMember("addresses")) {
      return false;
    }
    if (!j_query["addresses"].isArray()) {
      return false;
    }
    if (j_query["addresses"].empty()) {
      return false;
    }
    for (const auto& address : j_query["addresses"]) {
      string lower_case_addr;
      if (!AddressChecksum::VerifyChecksumAddress(address.asString(),
                                                  lower_case_addr)) {
        LOG_GENERAL(INFO, "To Address checksum wrong " << address.asString());
        return false;
      }
      bytes addr_ser;
      if (!DataConversion::HexStrToUint8Vec(lower_case_addr, addr_ser)) {
        LOG_GENERAL(WARNING, "json containing invalid hex str for addresses");
      }
      Address addr(addr_ser);
      el_addresses.emplace_back(addr);
    }
  } else {
    return false;
  }

  WEBSOCKETQUERY q_enum;
  if (!GetQueryEnum(query, q_enum)) {
    LOG_GENERAL(WARNING, "GetQueryEnum failed");
    return false;
  }

  switch (q_enum) {
    case NEWBLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockSockets);
      m_txblock_websockets.emplace(host, hdl);
      break;
    }
    case EVENTLOG: {
      lock_guard<mutex> g(m_mutexEventLogSockets);
      m_eventlog_websockets.emplace(host, hdl);
      m_elsockettracker.add(host, el_addresses);
      break;
    }
  }

  return true;
}

void WebsocketServer::on_fail(connection_hdl hdl) {
  LOG_MARKER();
  websocketpp::server<websocketpp::config::asio>::connection_ptr con =
      m_server.get_con_from_hdl(hdl);
  websocketpp::lib::error_code ec = con->get_ec();
  LOG_GENERAL(WARNING, "websocket connection failed, error: " << ec.message());
  websocketpp::uri_ptr uri = con->get_uri();
  string host = uri->get_host();
  string query = uri->get_query();
  removeSocket(host, query);
}

void WebsocketServer::on_close(connection_hdl hdl) {
  LOG_MARKER();
  websocketpp::server<websocketpp::config::asio>::connection_ptr con =
      m_server.get_con_from_hdl(hdl);
  websocketpp::uri_ptr uri = con->get_uri();
  string host = uri->get_host();
  string query = uri->get_query();
  removeSocket(host, query);
}

bool WebsocketServer::sendData(connection_hdl hdl, const string& data) {
  LOG_MARKER();
  websocketpp::lib::error_code ec;
  m_server.send(hdl, data, websocketpp::frame::opcode::text, ec);
  if (ec) {
    LOG_GENERAL(WARNING, "websocket send failed, error: " << ec.message());
    return false;
  }

  return true;
}

bool WebsocketServer::closeSocket(connection_hdl hdl) {
  string data = "Terminating connection...";
  websocketpp::lib::error_code ec;
  m_server.close(hdl, websocketpp::close::status::normal, data, ec);
  if (ec) {
    LOG_GENERAL(WARNING, "websocket close failed, error: " << ec.message());
    return false;
  }

  return true;
}

bool WebsocketServer::SendTxBlockAndTxHashes(const Json::Value& json_txblock,
                                             const Json::Value& json_txhashes) {
  LOG_MARKER();
  Json::Value json_msg;
  json_msg["TxBlock"] = json_txblock;
  json_msg["TxHashes"] = json_txhashes;

  {
    lock_guard<mutex> g(m_mutexTxBlockSockets);
    for (auto it = m_txblock_websockets.begin();
         it != m_txblock_websockets.end(); ++it) {
      if (!sendData(it->second,
                    JSONUtils::GetInstance().convertJsontoStr(json_msg))) {
        LOG_GENERAL(WARNING, "sendData (txblock) failed for " << it->first);
        return false;
      }
    }
  }

  return true;
}

void WebsocketServer::ParseTxnEventLog(const TransactionWithReceipt& twr) {
  LOG_MARKER();
  if (Transaction::GetTransactionType(twr.GetTransaction()) !=
      Transaction::CONTRACT_CALL) {
    return;
  }

  const auto& j_receipt = twr.GetTransactionReceipt().GetJsonValue();
  if (!j_receipt["success"].asBool()) {
    return;
  }

  if (!j_receipt.isMember("event_logs")) {
    return;
  }

  if (j_receipt["event_logs"].type() != Json::arrayValue) {
    return;
  }

  for (const auto& log : j_receipt["event_logs"]) {
    if (!(log.isMember("_eventname") && log.isMember("address") &&
          log.isMember("params"))) {
      continue;
    }
    if (!(log["_eventname"].type() == Json::stringValue &&
          log["address"].type() == Json::stringValue &&
          log["params"].type() == Json::arrayValue)) {
      continue;
    }
    Address addr(log["address"].asString());

    auto find = m_elsockettracker.m_addr_host_map.find(addr);
    if (find == m_elsockettracker.m_addr_host_map.end()) {
      continue;
    }
    Json::Value j_eventlog;
    j_eventlog["_eventname"] = log["_eventname"];
    j_eventlog["params"] = log["params"];
    for (const string& id : find->second) {
      lock_guard<mutex> g(m_mutexELDataBufferSockets);
      m_eventLogDataBuffer[id][addr].append(j_eventlog);
    }
  }
}

void WebsocketServer::SendOutEventLog() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexELDataBufferSockets);
  for (auto it = m_eventLogDataBuffer.begin(); it != m_eventLogDataBuffer.end();
       ++it) {
    connection_hdl hdl;
    if (!getWebsocket(it->first, EVENTLOG, hdl)) {
      continue;
    }
    Json::Value j_data;
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it) {
      Json::Value j_contract;
      j_contract["address"] = it2->first.hex();
      j_contract["event_logs"] = it2->second;
      j_data.append(j_contract);
    }
    if (!sendData(hdl, JSONUtils::GetInstance().convertJsontoStr(j_data))) {
      continue;
    }
  }
  m_eventLogDataBuffer.clear();
}