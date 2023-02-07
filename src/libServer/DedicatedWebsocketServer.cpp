/*
 * Copyright (C) 2022 Zilliqa
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

#include "DedicatedWebsocketServer.h"

#include <mutex>
#include <set>
#include <unordered_map>

#include <boost/asio/signal_set.hpp>

#include "AddressChecksum.h"
#include "JSONConversion.h"
#include "WebsocketServerBackend.h"
#include "common/Constants.h"
#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/AccountStore.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SetThreadName.h"

namespace rpc {

using ConnectionId = WebsocketServer::ConnectionId;

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
  std::unordered_map<Address, std::set<ConnectionId>> m_addr_hdl_map;
  // for removing socket from m_eventlog_hdl_tracker
  std::unordered_map<ConnectionId, std::set<Address>> m_hdl_addr_map;

  void remove(ConnectionId hdl) {
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

  void update(const ConnectionId hdl, const std::set<Address>& addresses) {
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

class DedicatedWSImpl : public DedicatedWebsocketServer,
                        public std::enable_shared_from_this<DedicatedWSImpl> {
  /// server backend
  std::shared_ptr<WebsocketServerBackend> m_backend;

  std::mutex m_mutex;

  std::unordered_map<ConnectionId, Subscription> m_subscriptions;

  /// a utility data structure for mapping address and subscriber of EventLog
  /// regarding of new comer or quiting
  EventLogAddrHdlTracker m_eventLogAddrHdlTracker;

  EventLogAddrHdlTracker m_txnLogAddrHdlTracker;

  /// a buffer for keeping the eventlog to send for each subscriber
  std::unordered_map<ConnectionId, std::unordered_map<Address, Json::Value>>
      m_eventLogDataBuffer;

  std::unordered_map<ConnectionId, std::unordered_map<Address, Json::Value>>
      m_txnLogDataBuffer;

  Json::Value m_jsonTxnBlockNTxnHashes;

  /// Started flag
  std::atomic<bool> m_started{};

  /// Asio context
  std::optional<AsioCtx> m_asio;

  /// Websocket server
  std::shared_ptr<WebsocketServerBackend> m_websocket;

  /// Listening socket
  std::optional<tcp::acceptor> m_acceptor;

  /// Event loop thread (if internal loop enabled)
  std::optional<std::thread> m_eventLoopThread;

  /// starts (only if WEBSOCKET_ENABLED in config)
  void Start() override;

  /// stops and joins the thread (if started)
  void Stop() override;

  /// Parses tx and receipt, everything will be sent on FinalizeTxBlock
  void ParseTxn(const TransactionWithReceipt& twr) override;

  /// Sends out messages related to finalized TX block
  void FinalizeTxBlock(const Json::Value& json_txblock,
                       const Json::Value& json_txhashes) override;

  /// Initiates accept async operation
  void AcceptNext();

  /// Socket accept callback
  void OnAccept(beast::error_code ec, tcp::socket socket);

  /// Event loop thread
  void EventLoopThread();

  void ParseTxnEventLog(const TransactionWithReceipt& twr);

  void ParseTxnLog(const TransactionWithReceipt& twr);

  /// Sends all digested contract events to subscriber
  void SendOutMessages();

  /// NVI for virtual Stop()
  void DoStop();

  /// clean in-memory data structures
  void Clean();

  /// Message handler. Returns false to close the connection with protocol_error
  /// code, true to proceed with the connection
  bool OnMessage(ConnectionId hdl, const std::string& query);

  void CloseConnection(ConnectionId hdl);

 public:
  DedicatedWSImpl() = default;

  ~DedicatedWSImpl() override { DoStop(); }
};

std::shared_ptr<DedicatedWebsocketServer> DedicatedWebsocketServer::Create() {
  return std::make_shared<DedicatedWSImpl>();
}

void DedicatedWSImpl::Start() {
  if (m_started) {
    LOG_GENERAL(WARNING, "Websocket server already started");
    return;
  }

  if (!ENABLE_WEBSOCKET) {
    LOG_GENERAL(WARNING, "Websocket server disabled");
    return;
  }

  LOG_MARKER();

  Clean();

  std::lock_guard<std::mutex> g(m_mutex);

  m_asio.emplace(1);

  m_websocket = WebsocketServerBackend::Create(*m_asio);

  // just a guard for sanity
  static constexpr size_t MAX_INCOMING_MSG_SIZE = 2048;
  m_websocket->SetOptions(
      [wptr = weak_from_this()](ConnectionId hdl, const std::string& msg,
                                bool& unknownMethodFound) -> bool {
        unknownMethodFound = false;
        auto self = wptr.lock();
        if (!self) {
          return false;
        }
        return self->OnMessage(hdl, msg);
      },
      MAX_INCOMING_MSG_SIZE);

  tcp::endpoint endpoint(boost::asio::ip::address_v4::any(), WEBSOCKET_PORT);
  m_acceptor.emplace(*m_asio);

  beast::error_code ec;

#define CHECK_EC()                                                  \
  if (ec) {                                                         \
    LOG_GENERAL(FATAL, "Cannot start WS server: " << ec.message()); \
    return;                                                         \
  }

  m_acceptor->open(endpoint.protocol(), ec);
  CHECK_EC();
  m_acceptor->set_option(asio::socket_base::reuse_address(true), ec);
  CHECK_EC();
  m_acceptor->bind(endpoint, ec);
  CHECK_EC();
  m_acceptor->listen(asio::socket_base::max_listen_connections, ec);
  CHECK_EC();

#undef CHECK_EC

  AcceptNext();

  m_started = true;

  m_eventLoopThread.emplace([this] { EventLoopThread(); });
}

void DedicatedWSImpl::AcceptNext() {
  assert(m_acceptor);

  m_acceptor->async_accept(beast::bind_front_handler(&DedicatedWSImpl::OnAccept,
                                                     shared_from_this()));
}

void DedicatedWSImpl::OnAccept(beast::error_code ec, tcp::socket socket) {
  if (ec || !m_started || !socket.is_open()) {
    // stopped, ignore
    return;
  }

  socket.set_option(asio::socket_base::keep_alive(true), ec);

  assert(m_websocket);

  auto ep = socket.remote_endpoint(ec);
  auto from = ep.address().to_string() + ":" + std::to_string(ep.port());

  m_websocket->NewConnection(std::move(from), std::move(socket));

  AcceptNext();
}

void DedicatedWSImpl::EventLoopThread() {
  utility::SetThreadName("Websocket");

  boost::asio::signal_set sig(*m_asio, SIGABRT);
  sig.async_wait([](const boost::system::error_code&, int) {});

  m_asio->run();
}

void DedicatedWSImpl::Stop() { DoStop(); }

void DedicatedWSImpl::DoStop() {
  if (!m_started) {
    return;
  }

  LOG_MARKER();

  std::lock_guard<std::mutex> g(m_mutex);

  m_started = false;

  m_acceptor.reset();

  assert(m_websocket);
  m_websocket->CloseAll();

  // after connections closed
  m_asio->post([self = shared_from_this()] { self->m_asio->stop(); });
  m_eventLoopThread->join();
  m_eventLoopThread.reset();

  m_websocket.reset();

  Clean();
}

void DedicatedWSImpl::Clean() {
  std::lock_guard<std::mutex> g(m_mutex);
  m_subscriptions.clear();
  m_eventLogAddrHdlTracker.clear();
  m_eventLogDataBuffer.clear();
  m_jsonTxnBlockNTxnHashes.clear();
  m_txnLogDataBuffer.clear();
  m_txnLogAddrHdlTracker.clear();
}

bool GetQueryEnum(const std::string& query, WEBSOCKETQUERY& q_enum) {
  if (query == "NewBlock") {
    q_enum = NEWBLOCK;
  } else if (query == "EventLog") {
    q_enum = EVENTLOG;
  } else if (query == "Unsubscribe") {
    q_enum = UNSUBSCRIBE;
  } else if (query == "TxnLog") {
    q_enum = TXNLOG;
  } else {
    return false;
  }
  return true;
}

std::string GetQueryString(const WEBSOCKETQUERY& q_enum) {
  std::string ret;
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

bool DedicatedWSImpl::OnMessage(ConnectionId hdl, const std::string& query) {
  assert(m_websocket);

  if (!m_started) {
    return false;
  }

  if (query.empty()) {
    LOG_GENERAL(DEBUG, "EOF: " << hdl);
    CloseConnection(hdl);
    return false;
  }

  LOG_GENERAL(DEBUG, "conn: " << hdl << " query: " << query);

  Json::Value j_query;
  WEBSOCKETQUERY q_enum;

  auto response = std::make_shared<std::string>();

  if (!query.empty() &&
      JSONUtils::GetInstance().convertStrtoJson(query, j_query) &&
      j_query.isObject() && j_query.isMember("query") &&
      j_query["query"].isString() &&
      GetQueryEnum(j_query["query"].asString(), q_enum)) {
    switch (q_enum) {
      case NEWBLOCK: {
        std::lock_guard<std::mutex> g(m_mutex);
        m_subscriptions[hdl].subscribe(NEWBLOCK);
        break;
      }
      case EVENTLOG: {
        if (j_query.isMember("addresses") && j_query["addresses"].isArray() &&
            !j_query["addresses"].empty()) {
          std::set<Address> el_addresses;

          {
            std::shared_lock<std::shared_timed_mutex> lock(
                AccountStore::GetInstance().GetPrimaryMutex());
            AccountStore::GetInstance().GetPrimaryWriteAccessCond().wait(
                lock, [] {
                  return AccountStore::GetInstance().GetPrimaryWriteAccess();
                });
            for (const auto& address : j_query["addresses"]) {
              try {
                const auto& addr_str = address.asString();
                if (!JSONConversion::checkStringAddress(addr_str)) {
                  *response = "Invalid hex address";
                  break;
                }
                Address addr(addr_str);
                Account* acc =
                    AccountStore::GetInstance().GetAccount(addr, true);
                if (acc == nullptr || !acc->isContract()) {
                  continue;
                }
                el_addresses.emplace(addr);
              } catch (...) {
                *response = "invalid address";
                break;
              }
            }
          }
          if (el_addresses.empty()) {
            *response = "no contract found in list";
          } else {
            std::lock_guard<std::mutex> g(m_mutex);
            m_subscriptions[hdl].subscribe(EVENTLOG);
            m_eventLogAddrHdlTracker.update(hdl, el_addresses);
          }
        } else {
          *response = "invalid addresses field";
        }
        break;
      }
      case UNSUBSCRIBE: {
        WEBSOCKETQUERY t_enum;
        if (j_query.isMember("type") &&
            GetQueryEnum(j_query["type"].asString(), t_enum) &&
            t_enum != UNSUBSCRIBE) {
          std::lock_guard<std::mutex> g(m_mutex);
          m_subscriptions[hdl].unsubscribe_start(t_enum);
        } else {
          *response = "invalid type field";
        }
        break;
      }
      case TXNLOG: {
        std::set<Address> track_addresses;
        if (j_query.isMember("addresses") && j_query["addresses"].isArray() &&
            !j_query["addresses"].empty()) {
          for (const auto& addr_json : j_query["addresses"]) {
            try {
              const auto& addr_str = addr_json.asString();
              if (!JSONConversion::checkStringAddress(addr_str)) {
                *response = "invalid hex address";
                break;
              }
              Address addr(addr_str);
              track_addresses.emplace(addr);
            } catch (...) {
              *response = "invalid address";
              break;
            }
          }

          if (!track_addresses.empty()) {
            std::lock_guard<std::mutex> g(m_mutex);
            m_subscriptions[hdl].subscribe(TXNLOG);
            m_txnLogAddrHdlTracker.update(hdl, track_addresses);
          } else {
            *response = "no valid address found";
          }
        } else {
          *response = "invalid addresses field";
        }
        break;
      }
      default:
        break;
    }
    if (response->empty()) {
      *response = query;
    }
  } else {
    *response = "invalid query field";
  }

  m_websocket->SendMessage(hdl, std::move(response));

  return true;
}

void DedicatedWSImpl::FinalizeTxBlock(const Json::Value& json_txblock,
                                      const Json::Value& json_txhashes) {
  if (!m_started) {
    return;
  }
  std::lock_guard<std::mutex> g(m_mutex);
  m_jsonTxnBlockNTxnHashes.clear();
  m_jsonTxnBlockNTxnHashes["TxBlock"] = json_txblock;
  m_jsonTxnBlockNTxnHashes["TxHashes"] = json_txhashes;
  SendOutMessages();
}

Json::Value CreateReturnAddressJson(const TransactionWithReceipt& twr) {
  Json::Value _json;

  _json["toAddr"] = twr.GetTransaction().GetToAddr().hex();
  _json["fromAddr"] = twr.GetTransaction().GetSenderAddr().hex();
  _json["amount"] = twr.GetTransaction().GetAmountQa().str();
  _json["success"] = twr.GetTransactionReceipt().GetJsonValue()["success"];
  _json["ID"] = twr.GetTransaction().GetTranID().hex();

  return _json;
}

void DedicatedWSImpl::ParseTxn(const TransactionWithReceipt& twr) {
  if (m_started) {
    ParseTxnEventLog(twr);
    ParseTxnLog(twr);
  }
}

void DedicatedWSImpl::ParseTxnEventLog(const TransactionWithReceipt& twr) {
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
      std::lock_guard<std::mutex> g(m_mutex);
      auto find = m_eventLogAddrHdlTracker.m_addr_hdl_map.find(addr);
      if (find == m_eventLogAddrHdlTracker.m_addr_hdl_map.end()) {
        continue;
      }
      Json::Value j_eventlog;
      j_eventlog["_eventname"] = log["_eventname"];
      j_eventlog["params"] = log["params"];
      for (auto hdl : find->second) {
        m_eventLogDataBuffer[hdl][addr].append(j_eventlog);
      }
    } catch (...) {
      continue;
    }
  }
}

void DedicatedWSImpl::ParseTxnLog(const TransactionWithReceipt& twr) {
  LOG_MARKER();

  const auto& txn_to_addr = twr.GetTransaction().GetToAddr();

  const auto txn_from_addr = twr.GetTransaction().GetSenderAddr();

  std::lock_guard<std::mutex> g(m_mutex);

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

  for (auto hdl : find_addr->second) {
    m_txnLogDataBuffer[hdl][addr_confirmed].append(log_json);
  }
}

void DedicatedWSImpl::CloseConnection(ConnectionId hdl) {
  assert(m_websocket);
  m_websocket->CloseConnection(hdl);

  std::lock_guard<std::mutex> g(m_mutex);

  // remove element if subscribed EVENTLOG
  auto find = m_subscriptions.find(hdl);
  if (find != m_subscriptions.end()) {
    if (find->second.subscribed(EVENTLOG)) {
      m_eventLogAddrHdlTracker.remove(hdl);
    }
    m_subscriptions.erase(find);
  }
}

void DedicatedWSImpl::SendOutMessages() {
  if (m_subscriptions.empty()) {
    m_eventLogDataBuffer.clear();
    m_txnLogDataBuffer.clear();
    return;
  }

  LOG_MARKER();

  for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();) {
    if (!it->second.queries.empty()) {
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
              value["value"] = std::move(j_txnlogs);
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

      m_websocket->SendMessage(
          it->first,
          std::make_shared<const std::string>(
              JSONUtils::GetInstance().convertJsontoStr(notification)));
    }

    ++it;
  }

  m_eventLogDataBuffer.clear();
  m_txnLogDataBuffer.clear();
}

}  // namespace rpc
