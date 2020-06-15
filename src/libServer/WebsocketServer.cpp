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

std::mutex WebsocketServer::m_mutexSubscriptions;
std::map<websocketpp::connection_hdl, Subscription,
         std::owner_less<connection_hdl>>
    WebsocketServer::m_subscriptions;

std::mutex WebsocketServer::m_mutexEventLogAddrHdlTracker;
EventLogAddrHdlTracker WebsocketServer::m_eventLogAddrHdlTracker;

std::mutex WebsocketServer::m_mutexEventLogDataBuffer;
std::map<websocketpp::connection_hdl, std::unordered_map<Address, Json::Value>,
         std::owner_less<connection_hdl>>
    WebsocketServer::m_eventLogDataBuffer;

std::mutex WebsocketServer::m_mutexTxnLogDataBuffer;

std::map<websocketpp::connection_hdl, std::unordered_map<Address, Json::Value>,
         std::owner_less<websocketpp::connection_hdl>>
    WebsocketServer::m_txnLogDataBuffer;

EventLogAddrHdlTracker WebsocketServer::m_txnLogAddrHdlTracker;
std::mutex WebsocketServer::m_mutexTxnLogAddrHdlTracker;

bool WebsocketServer::start() {
  LOG_MARKER();
  clean();
  // Initialising websocketserver
  m_server.init_asio();

  // Register the message handlers.
  m_server.set_message_handler(&WebsocketServer::on_message);
  // m_server.set_fail_handler(&WebsocketServer::on_fail);
  m_server.set_close_handler(&WebsocketServer::on_close);

  try {
    m_server.listen(WEBSOCKET_PORT);
  } catch (const websocketpp::exception& e) {
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
  } catch (const websocketpp::exception& e) {
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

  try {
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
      lock_guard<mutex> g(m_mutexSubscriptions);

      for (auto& subscription : m_subscriptions) {
        websocketpp::lib::error_code ec;
        m_server.close(subscription.first, websocketpp::close::status::normal,
                       "Terminating connection...", ec);
        if (ec) {
          LOG_GENERAL(WARNING, "websocket stop_listening (1) failed, error: "
                                   << ec.message());
        }
      }
    }

    // Stop the end point
    m_server.stop();
    m_thread->join();
  } catch (const websocketpp::exception& e) {
    LOG_GENERAL(WARNING, "websocket stop failed, error: " << e.what());
  } catch (...) {
    LOG_GENERAL(WARNING, "other exception");
  }
  clean();
}

void WebsocketServer::clean() {
  {
    lock_guard<mutex> g(m_mutexSubscriptions);
    m_subscriptions.clear();
  }
  {
    lock_guard<mutex> g(m_mutexEventLogAddrHdlTracker);
    m_eventLogAddrHdlTracker.clear();
  }
  {
    lock_guard<mutex> g(m_mutexEventLogDataBuffer);
    m_eventLogDataBuffer.clear();
  }
  {
    lock_guard<mutex> g(m_mutexTxnBlockNTxnHashes);
    m_jsonTxnBlockNTxnHashes.clear();
  }
  {
    lock_guard<mutex> g(m_mutexTxnLogDataBuffer);
    m_txnLogDataBuffer.clear();
  }
  {
    lock_guard<mutex> g(m_mutexTxnLogAddrHdlTracker);
    m_txnLogAddrHdlTracker.clear();
  }
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

bool GetQueryEnum(const string& query, WEBSOCKETQUERY& q_enum) {
  if (query == "NewBlock") {
    q_enum = NEWBLOCK;
  } else if (query == "EventLog") {
    q_enum = EVENTLOG;
  } else if (query == "Unsubscribe") {
    q_enum = UNSUBSCRIBE;
  } else if (query == "TxnLog") {
    q_enum = TXNLOG;
  }

  return true;
}

string GetQueryString(const WEBSOCKETQUERY& q_enum) {
  string ret;
  switch (q_enum) {
    case NEWBLOCK:
      ret = "NewBlock";
      break;
    case EVENTLOG:
      ret = "EventLog";
      break;
    case TXNLOG:
      ret = "TxnLog";
      break;
    case UNSUBSCRIBE:
      ret = "Unsubscribe";
      break;
    default:
      break;
  }
  return ret;
}

void WebsocketServer::on_message(const connection_hdl& hdl,
                                 const websocketserver::message_ptr& msg) {
  LOG_MARKER();

  websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
  string query = msg->get_payload();
  LOG_GENERAL(INFO, "hdl: " << hdl.lock().get() << endl << "query: " << query);

  Json::Value j_query;
  WEBSOCKETQUERY q_enum;

  string response{};

  if (!query.empty() &&
      JSONUtils::GetInstance().convertStrtoJson(query, j_query) &&
      j_query.isObject() && j_query.isMember("query") &&
      j_query["query"].isString() &&
      GetQueryEnum(j_query["query"].asString(), q_enum)) {
    switch (q_enum) {
      case NEWBLOCK: {
        lock_guard<mutex> g(m_mutexSubscriptions);
        m_subscriptions[hdl].subscribe(NEWBLOCK);
        break;
      }
      case EVENTLOG: {
        if (j_query.isMember("addresses") && j_query["addresses"].isArray() &&
            !j_query["addresses"].empty()) {
          set<Address> el_addresses;
          for (const auto& address : j_query["addresses"]) {
            try {
              const auto& addr_str = address.asString();
              if (!JSONConversion::checkStringAddress(addr_str)) {
                response = "Invalid hex address";
                break;
              }
              Address addr(addr_str);
              Account* acc = AccountStore::GetInstance().GetAccount(addr);
              if (acc == nullptr || !acc->isContract()) {
                continue;
              }
              el_addresses.emplace(addr);
            } catch (...) {
              response = "invalid address";
              break;
            }
          }
          if (el_addresses.empty()) {
            response = "no contract found in list";
          } else {
            {
              lock_guard<mutex> g(m_mutexSubscriptions);
              m_subscriptions[hdl].subscribe(EVENTLOG);
            }
            {
              lock_guard<mutex> g(m_mutexEventLogAddrHdlTracker);
              m_eventLogAddrHdlTracker.update(hdl, el_addresses);
            }
          }
        } else {
          response = "invalid addresses field";
        }
        break;
      }
      case UNSUBSCRIBE: {
        WEBSOCKETQUERY t_enum;
        if (j_query.isMember("type") &&
            GetQueryEnum(j_query["type"].asString(), t_enum) &&
            t_enum != UNSUBSCRIBE) {
          lock_guard<mutex> g(m_mutexSubscriptions);
          m_subscriptions[hdl].unsubscribe_start(t_enum);
        } else {
          response = "invalid type field";
        }
        break;
      }
      case TXNLOG: {
        set<Address> track_addresses;
        if (j_query.isMember("addresses") && j_query["addresses"].isArray() &&
            !j_query["addresses"].empty()) {
          for (const auto& addr_json : j_query["addresses"]) {
            try {
              const auto& addr_str = addr_json.asString();
              if (!JSONConversion::checkStringAddress(addr_str)) {
                response = "invalid hex address";
                break;
              }
              Address addr(addr_str);
              track_addresses.emplace(addr);
            } catch (...) {
              response = "invalid address";
              break;
            }
          }

          if (!track_addresses.empty()) {
            {
              lock_guard<mutex> g(m_mutexSubscriptions);
              m_subscriptions[hdl].subscribe(TXNLOG);
            }
            {
              lock_guard<mutex> h(m_mutexTxnLogDataBuffer);
              m_txnLogAddrHdlTracker.update(hdl, track_addresses);
            }
          } else {
            response = "no valid address found";
          }
        } else {
          response = "invalid addresses field";
        }
        break;
      }
      default:
        break;
    }
    if (response.empty()) {
      response = query;
    }
  } else {
    response = "invalid query field";
  }

  websocketpp::lib::error_code ec;
  m_server.send(hdl, response, websocketpp::frame::opcode::text, ec);
  if (ec) {
    LOG_GENERAL(WARNING, "websocket send failed, error: " << ec.message());
    return;
  }
}

void WebsocketServer::on_close(const connection_hdl& hdl) {
  closeSocket(hdl, "connection closed", websocketpp::close::status::going_away);
}

void WebsocketServer::PrepareTxBlockAndTxHashes(
    const Json::Value& json_txblock, const Json::Value& json_txhashes) {
  // LOG_MARKER();

  lock_guard<mutex> g(m_mutexTxnBlockNTxnHashes);
  m_jsonTxnBlockNTxnHashes.clear();
  m_jsonTxnBlockNTxnHashes["TxBlock"] = json_txblock;
  m_jsonTxnBlockNTxnHashes["TxHashes"] = json_txhashes;
}

Json::Value CreateReturnAddressJson(const TransactionWithReceipt& twr) {
  Json::Value _json;

  _json["toAddr"] = twr.GetTransaction().GetToAddr().hex();
  _json["fromAddr"] = twr.GetTransaction().GetSenderAddr().hex();
  _json["amount"] = twr.GetTransaction().GetAmount().str();
  _json["success"] = twr.GetTransactionReceipt().GetJsonValue()["success"];
  _json["ID"] = twr.GetTransaction().GetTranID().hex();

  return _json;
}

void WebsocketServer::ParseTxnLog(const TransactionWithReceipt& twr) {
  LOG_MARKER();

  const auto& txn_to_addr = twr.GetTransaction().GetToAddr();

  const auto txn_from_addr = twr.GetTransaction().GetSenderAddr();

  lock_guard<mutex> g(m_mutexTxnLogAddrHdlTracker);

  auto addr_confirmed = txn_to_addr;

  auto find_addr = m_txnLogAddrHdlTracker.m_addr_hdl_map.find(txn_to_addr);

  if (find_addr == m_txnLogAddrHdlTracker.m_addr_hdl_map.end()) {
    find_addr = m_txnLogAddrHdlTracker.m_addr_hdl_map.find(txn_from_addr);

    if (find_addr == m_txnLogAddrHdlTracker.m_addr_hdl_map.end()) {
      return;
    }
    addr_confirmed = txn_from_addr;
  }
  const auto log_json = CreateReturnAddressJson(twr);

  lock_guard<mutex> h(m_mutexTxnLogDataBuffer);
  for (const connection_hdl& hdl : find_addr->second) {
    m_txnLogDataBuffer[hdl][addr_confirmed].append(log_json);
  }
}

void WebsocketServer::ParseTxn(const TransactionWithReceipt& twr) {
  ParseTxnEventLog(twr);

  ParseTxnLog(twr);
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

    try {
      Address addr(log["address"].asString());
      lock_guard<mutex> g(m_mutexEventLogAddrHdlTracker);
      auto find = m_eventLogAddrHdlTracker.m_addr_hdl_map.find(addr);
      if (find == m_eventLogAddrHdlTracker.m_addr_hdl_map.end()) {
        continue;
      }
      Json::Value j_eventlog;
      j_eventlog["_eventname"] = log["_eventname"];
      j_eventlog["params"] = log["params"];
      for (const connection_hdl& hdl : find->second) {
        lock_guard<mutex> g(m_mutexEventLogDataBuffer);
        m_eventLogDataBuffer[hdl][addr].append(j_eventlog);
      }
    } catch (...) {
      continue;
    }
  }
}

void WebsocketServer::closeSocket(
    const connection_hdl& hdl, const std::string& reason,
    const websocketpp::close::status::value& close_status) {
  string data = "Terminating connection due to " + reason;
  websocketpp::lib::error_code ec;
  m_server.close(hdl, close_status, data, ec);
  if (ec) {
    LOG_GENERAL(WARNING, "websocket close failed, error: " << ec.message());
  }

  lock_guard<mutex> g1(m_mutexSubscriptions);

  // remove element if subscribed EVENTLOG
  auto find = m_subscriptions.find(hdl);
  if (find != m_subscriptions.end()) {
    if (find->second.subscribed(EVENTLOG)) {
      lock_guard<mutex> g3(m_mutexEventLogDataBuffer);
      m_eventLogAddrHdlTracker.remove(hdl);
    }
    m_subscriptions.erase(find);
  }
}

void WebsocketServer::SendOutMessages() {
  LOG_MARKER();

  vector<std::pair<connection_hdl, string>> hdlToRemove;

  {
    lock_guard<mutex> g1(m_mutexSubscriptions);

    if (m_subscriptions.empty()) {
      {
        lock_guard<mutex> g(m_mutexEventLogDataBuffer);
        m_eventLogDataBuffer.clear();
      }
      {
        lock_guard<mutex> h(m_mutexTxnLogDataBuffer);
        m_txnLogDataBuffer.clear();
      }
      return;
    }
    lock(m_mutexTxnLogDataBuffer, m_mutexEventLogDataBuffer,
         m_mutexTxnBlockNTxnHashes);
    lock_guard<mutex> g2(m_mutexTxnBlockNTxnHashes, adopt_lock);
    lock_guard<mutex> g3(m_mutexEventLogDataBuffer, adopt_lock);
    lock_guard<mutex> g4(m_mutexTxnLogDataBuffer, adopt_lock);

    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
      if (it->second.queries.empty()) {
        hdlToRemove.push_back({it->first, "no subscription"});
      } else {
        Json::Value notification;
        notification["type"] = "Notification";

        // SUBSCRIBE
        for (const auto& query : it->second.queries) {
          Json::Value value;
          value["query"] = GetQueryString(query);
          bool valid_query = true;
          switch (query) {
            case NEWBLOCK: {
              value["value"] = m_jsonTxnBlockNTxnHashes;
              break;
            }
            case EVENTLOG: {
              Json::Value j_eventlogs;
              auto buffer = m_eventLogDataBuffer.find(it->first);
              if (buffer != m_eventLogDataBuffer.end()) {
                for (const auto& entry : buffer->second) {
                  Json::Value j_contract;
                  j_contract["address"] = entry.first.hex();
                  j_contract["event_logs"] = entry.second;
                  j_eventlogs.append(j_contract);
                }
                value["value"] = std::move(j_eventlogs);
              }
              break;
            }
            case TXNLOG: {
              Json::Value j_txnlogs;
              auto buffer = m_txnLogDataBuffer.find(it->first);
              if (buffer != m_txnLogDataBuffer.end()) {
                for (const auto& entry : buffer->second) {
                  Json::Value _json;
                  _json["address"] = entry.first.hex();
                  _json["log"] = entry.second;
                  j_txnlogs.append(_json);
                }
                value["value"] = move(j_txnlogs);
              }
              break;
            }
            default:
              valid_query = false;
              break;
          }
          if (!valid_query) {
            continue;
          }
          notification["values"].append(value);
        }

        // UNSUBSCRIBE
        if (!it->second.unsubscribings.empty()) {
          Json::Value value;
          value["query"] = GetQueryString(UNSUBSCRIBE);
          Json::Value j_unsubscripings;
          for (const auto& unsubscriping : it->second.unsubscribings) {
            j_unsubscripings.append(GetQueryString(unsubscriping));
          }
          value["value"] = std::move(j_unsubscripings);
          notification["values"].append(value);

          it->second.unsubscribe_finish();
        }

        if (!sendData(it->first, JSONUtils::GetInstance().convertJsontoStr(
                                     notification))) {
          hdlToRemove.push_back({it->first, "unable to send data"});
        }
      }

      ++it;
    }

    m_eventLogDataBuffer.clear();
    m_txnLogDataBuffer.clear();
  }

  for (const auto& pair : hdlToRemove) {
    closeSocket(pair.first, pair.second, websocketpp::close::status::normal);
  }
}