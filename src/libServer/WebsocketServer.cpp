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
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"

using websocketpp::connection_hdl;

using namespace std;
using namespace dev;

websocketserver WebsocketServer::m_server;

mutex WebsocketServer::m_mutexEqIndex;
EndpointQueryIndex WebsocketServer::m_eqIndex;

mutex WebsocketServer::m_mutexTxBlockSockets;
IpSocketMap WebsocketServer::m_txblock_websockets;

mutex WebsocketServer::m_mutexEventLogSockets;
IpSocketMap WebsocketServer::m_eventlog_websockets;
EventLogSocketTracker WebsocketServer::m_elsockettracker;

mutex WebsocketServer::m_mutexELDataBufferSockets;
std::unordered_map<std::string, std::unordered_map<Address, Json::Value>>
    WebsocketServer::m_eventLogDataBuffer;

bool WebsocketServer::start() {
  LOG_MARKER();
  clean();
  // Initialising websocketserver
  m_server.init_asio();

  // Set custom logger (ostream-based)
  // server.get_alog().set_ostream(&os);
  // server.get_elog().set_ostream(&os);

  // Register the message handlers.
  m_server.set_message_handler(&WebsocketServer::on_message);
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

  try {
    m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(
        &websocketserver::run, &m_server);
  } catch (websocketpp::exception const& e) {
    LOG_GENERAL(WARNING, "websocket run failed, error: " << e.what());
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING, "other exception");
    return false;
  }

  return true;
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

    for (auto& socket : m_txblock_websockets) {
      websocketpp::lib::error_code ec;
      m_server.close(socket.second, websocketpp::close::status::normal,
                     "Terminating connection...", ec);
      if (ec) {
        LOG_GENERAL(WARNING, "websocket stop_listening (1) failed, error: "
                                 << ec.message());
      }
    }
  }

  {
    lock_guard<mutex> g(m_mutexEventLogSockets);

    for (auto& socket : m_eventlog_websockets) {
      websocketpp::lib::error_code ec;
      m_server.close(socket.second, websocketpp::close::status::normal,
                     "Terminating connection...", ec);
      if (ec) {
        LOG_GENERAL(WARNING, "websocket stop_listening (2) failed, error: "
                                 << ec.message());
      }
    }
  }

  try {
    // Stop the end point
    m_server.stop();
    m_thread->join();
  } catch (websocketpp::exception const& e) {
    LOG_GENERAL(WARNING, "websocket stop failed, error: " << e.what());
  } catch (...) {
    LOG_GENERAL(WARNING, "other exception");
  }
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
  {
    lock_guard<mutex> g(m_mutexEqIndex);
    m_eqIndex.clear();
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

bool WebsocketServer::getWebsocket(const string& ip, WEBSOCKETQUERY query,
                                   connection_hdl& hdl) {
  IpSocketMap::iterator it;
  switch (query) {
    case NEWBLOCK:
      it = m_txblock_websockets.find(ip);
      if (it == m_txblock_websockets.end()) {
        return false;
      }
      break;
    case EVENTLOG:
      it = m_eventlog_websockets.find(ip);
      if (it == m_eventlog_websockets.end()) {
        return false;
      }
      break;
  }

  hdl = it->second;
  return true;
}

string GetRemoteIP(string remote) {
  remote.erase(remote.begin());
  vector<string> splits;
  boost::algorithm::split(splits, remote, boost::algorithm::is_any_of("]"));
  return splits.front();
}

void WebsocketServer::removeSocket(const string& ip, WEBSOCKETQUERY q_enum) {
  IpSocketMap::iterator it;
  switch (q_enum) {
    case NEWBLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockSockets);
      it = m_txblock_websockets.find(ip);
      if (it != m_txblock_websockets.end()) {
        m_txblock_websockets.erase(it);
      }
      break;
    }
    case EVENTLOG: {
      {
        lock_guard<mutex> g(m_mutexEventLogSockets);
        m_eventlog_websockets.erase(ip);
        m_elsockettracker.remove(ip);
      }
      {
        lock_guard<mutex> g(m_mutexELDataBufferSockets);
        m_eventLogDataBuffer.erase(ip);
      }
      break;
    }
  }
}

void WebsocketServer::removeSocket(const std::string& remote) {
  WEBSOCKETQUERY q_enum;
  {
    lock_guard<mutex> g(m_mutexEqIndex);
    auto find = m_eqIndex.find(remote);
    if (find == m_eqIndex.end()) {
      LOG_GENERAL(WARNING, "removeSocket for " << remote << " failed");
      return;
    }
    q_enum = find->second;
    m_eqIndex.erase(find);
  }

  string ip = GetRemoteIP(remote);

  removeSocket(ip, q_enum);
}

void WebsocketServer::on_message(const connection_hdl& hdl,
                                 const websocketserver::message_ptr& msg) {
  LOG_MARKER();
  websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
  string remote = con->get_remote_endpoint();
  string ip = GetRemoteIP(remote);
  string query = msg->get_payload();
  LOG_GENERAL(INFO, "remote endpoint: " << remote << endl
                                        << "query: " << query);
  if (query.empty()) {
    closeSocket(hdl);
    return;
  }

  Json::Value j_query;
  if (!JSONUtils::GetInstance().convertStrtoJson(query, j_query)) {
    closeSocket(hdl);
    return;
  }

  if (!j_query.isObject()) {
    closeSocket(hdl);
    return;
  }

  if (!j_query.isMember("query")) {
    closeSocket(hdl);
    return;
  }

  if (!j_query["query"].isString()) {
    closeSocket(hdl);
    return;
  }

  WEBSOCKETQUERY q_enum;
  if (!GetQueryEnum(j_query["query"].asString(), q_enum)) {
    closeSocket(hdl);
    return;
  }

  switch (q_enum) {
    case NEWBLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockSockets);
      m_txblock_websockets[ip] = hdl;
      break;
    }
    case EVENTLOG: {
      set<Address> el_addresses;
      if (!j_query.isMember("addresses")) {
        closeSocket(hdl);
        return;
      }
      if (!j_query["addresses"].isArray()) {
        closeSocket(hdl);
        return;
      }
      if (j_query["addresses"].empty()) {
        closeSocket(hdl);
        return;
      }
      for (const auto& address : j_query["addresses"]) {
        Address addr(address.asString());
        Account* acc = AccountStore::GetInstance().GetAccount(addr);
        if (acc == nullptr || !acc->isContract()) {
          continue;
        }
        el_addresses.emplace(addr);
      }
      if (el_addresses.empty()) {
        closeSocket(hdl);
        return;
      }
      {
        lock_guard<mutex> g2(m_mutexEventLogSockets);
        m_eventlog_websockets[ip] = hdl;
      }
      {
        lock_guard<mutex> g(m_mutexEventLogSockets);
        m_elsockettracker.update(ip, el_addresses);
      }
      break;
    }
  }

  {
    lock_guard<mutex> g(m_mutexEqIndex);
    m_eqIndex[remote] = q_enum;
  }
}

void WebsocketServer::on_fail(const connection_hdl& hdl) {
  LOG_MARKER();
  websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
  websocketpp::lib::error_code ec = con->get_ec();
  LOG_GENERAL(WARNING, "websocket connection failed, error: " << ec.message());
  string remote = con->get_remote_endpoint();
  if (remote == "Unknown") {
    return;
  }
  removeSocket(remote);
}

void WebsocketServer::on_close(const connection_hdl& hdl) {
  LOG_MARKER();
  websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
  string remote = con->get_remote_endpoint();
  if (remote == "Unknown") {
    return;
  }
  removeSocket(remote);
}

bool WebsocketServer::sendData(const connection_hdl& hdl, const string& data) {
  LOG_MARKER();
  websocketpp::lib::error_code ec;
  m_server.send(hdl, data, websocketpp::frame::opcode::text, ec);
  if (ec) {
    LOG_GENERAL(WARNING, "websocket send failed, error: " << ec.message());
    return false;
  }

  return true;
}

bool WebsocketServer::closeSocket(const connection_hdl& hdl) {
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

  vector<string> ipToRemove;

  {
    lock_guard<mutex> g(m_mutexTxBlockSockets);

    for (auto& socket : m_txblock_websockets) {
      if (!sendData(socket.second,
                    JSONUtils::GetInstance().convertJsontoStr(json_msg))) {
        LOG_GENERAL(WARNING, "sendData (txblock) failed for " << socket.first);
        ipToRemove.emplace_back(socket.first);
        return false;
      }
    }
  }

  for (const auto& ip : ipToRemove) {
    removeSocket(ip, NEWBLOCK);
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

    auto find = m_elsockettracker.m_addr_ip_map.find(addr);
    if (find == m_elsockettracker.m_addr_ip_map.end()) {
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
  vector<string> ipToRemove;
  {
    lock_guard<mutex> g1(m_mutexELDataBufferSockets);
    lock_guard<mutex> g2(m_mutexEventLogSockets);
    for (auto& buffer : m_eventLogDataBuffer) {
      connection_hdl hdl;
      if (!getWebsocket(buffer.first, EVENTLOG, hdl)) {
        continue;
      }
      Json::Value j_data;
      for (auto& entry : buffer.second) {
        Json::Value j_contract;
        j_contract["address"] = entry.first.hex();
        j_contract["event_logs"] = entry.second;
        j_data.append(j_contract);
      }
      if (!sendData(hdl, JSONUtils::GetInstance().convertJsontoStr(j_data))) {
        ipToRemove.emplace_back(buffer.first);
        continue;
      }
    }
    m_eventLogDataBuffer.clear();
  }

  for (const auto& ip : ipToRemove) {
    removeSocket(ip, EVENTLOG);
  }
}