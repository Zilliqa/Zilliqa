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

#include "AddressChecksum.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"

using websocketpp::connection_hdl;

using namespace std;
using namespace dev;

namespace Websocket {

pthread_rwlock_t WebsocketServer::m_websocketsLock = PTHREAD_RWLOCK_INITIALIZER;

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
  // stopping the Websocket listener and closing outstanding connection
  websocketpp::lib::error_code ec;
  m_server.stop_listening(ec);
  if (ec) {
    LOG_GENERAL(WARNING,
                "websocket stop_listening failed, error: " << ec.message());
    return;
  }

  // Close all existing websocket connections.
  for (auto it = m_websockets.begin(); it != m_websockets.end(); ++it) {
    websocketpp::lib::error_code ec;
    m_server.close(it->second, websocketpp::close::status::normal,
                   "Terminating connection...", ec);
    if (ec) {
      LOG_GENERAL(WARNING,
                  "websocket stop_listening failed, error: " << ec.message());
    }
  }

  // Stop the end point
  m_server.stop();
  clean();
}

void WebsocketServer::clean() {
  m_websockets.clear();
  m_elsockettracker.clean();
}

bool GetQueryEnum(const string& query, QUERY& q_enum) {
  if (query == "NewBlock") {
    q_enum = NEWBLOCK;
  } else if (query == "EventLog") {
    q_enum = EVENTLOG;
  } else {
    return false;
  }

  return true;
}

bool WebsocketServer::on_validate(connection_hdl hdl) {
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

  if (pthread_rwlock_wrlock(&m_websocketsLock) != 0) {
    LOG_GENERAL(WARNING, "Failed to write-lock websocketLock");
    return false;
  }

  QUERY q_enum;
  if (!GetQueryEnum(query, q_enum)) {
    LOG_GENERAL(WARNING, "GetQueryEnum failed");
    return false;
  }

  m_websockets.insert(
      std::pair<WebsocketInfo, connection_hdl>({host, q_enum}, hdl));
  if (!el_addresses.empty()) {
    m_elsockettracker.add(host, el_addresses);
  }

  if (pthread_rwlock_unlock(&m_websocketsLock) != 0) {
    LOG_GENERAL(WARNING, "Failed to unlock websocketsLock");
    return false;
  }

  return true;
}

void WebsocketServer::removeSocket(const std::string& host,
                                   const std::string& query) {
  QUERY q_enum;
  if (!GetQueryEnum(query, q_enum)) {
    LOG_GENERAL(WARNING, "GetQueryEnum failed");
    return;
  }

  WebsocketInfo t_wi{host, q_enum};
  auto find = m_websockets.find(t_wi);
  if (find != m_websockets.end()) {
    LOG_GENERAL(INFO, "remove conn: " << find->first.m_host << "("
                                      << to_string(find->first.m_query) << ")");
    if (find->first.m_query == EVENTLOG) {
      m_elsockettracker.remove(host);
    }
    m_websockets.erase(find);
  }
}

void WebsocketServer::on_fail(connection_hdl hdl) {
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
  websocketpp::server<websocketpp::config::asio>::connection_ptr con =
      m_server.get_con_from_hdl(hdl);
  websocketpp::uri_ptr uri = con->get_uri();
  string host = uri->get_host();
  string query = uri->get_query();
  removeSocket(host, query);
}

bool WebsocketServer::sendData(connection_hdl hdl, const string& data) {
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

}  // namespace Websocket