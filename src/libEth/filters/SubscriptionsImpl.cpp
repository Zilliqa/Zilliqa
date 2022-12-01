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

#include "SubscriptionsImpl.h"

#include <cassert>

#include "FiltersUtils.h"
#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

namespace {

using Lock = std::lock_guard<std::mutex>;

static const std::string SUBSCR_ID_FOR_NEW_HEADS = "0xe";
static const std::string SUBSCR_ID_FOR_PENDING_TXNS = "0xf";

struct Request {
  enum Action {
    INVALID,
    UNSUBSCRIBE,
    SUBSCR_NEW_HEADS,
    SUBSCR_PENDING_TXNS,
    SUBSCR_EVENTS
  };

  Action action = INVALID;
  Json::Value id;
  EventFilterParams eventFilter;
  std::string subscriptionId;
  std::string error;
  RPCError errorCode = RPCError::OK;
};

bool ParseRequest(const std::string& msg, Request& req,
                  bool& unknownMethodFound) {
  unknownMethodFound = false;

  auto json = JsonRead(msg, req.error);

  if (!req.error.empty()) {
    req.errorCode = RPCError::PARSE_ERROR;
    return false;
  }

  if (!json.isObject()) {
    req.errorCode = RPCError::PARSE_ERROR;
    req.error = "Object expected";
    return false;
  }

  req.id = json.get("id", Json::Value{});
  if (req.id.isNull()) {
    req.errorCode = RPCError::INVALID_REQUEST;
    req.error = "Request id expected";
    return false;
  }

  bool found = false;
  auto method = ExtractStringFromJsonObj(json, "method", req.error, found);

  bool isUnsubscribe = (method == "eth_unsubscribe");
  if (!isUnsubscribe && method != "eth_subscribe") {
    if (!found) {
      req.errorCode = RPCError::METHOD_NOT_FOUND;
      req.error = "Unexpected method: ";
      req.error += method;
      return false;
    } else {
      unknownMethodFound = true;
      return true;
    }
  }

  Json::Value params = json.get("params", Json::Value{});
  if (!params.isArray() || params.empty() || !params[0].isString()) {
    req.errorCode = RPCError::INVALID_PARAMS;
    req.error = "Missing or invalid params";
    return false;
  }

  if (isUnsubscribe) {
    req.action = Request::UNSUBSCRIBE;
    req.subscriptionId = params[0].asString();
    return true;
  }

  auto what = params[0].asString();
  if (what == "newHeads") {
    req.action = Request::SUBSCR_NEW_HEADS;
  } else if (what == "newPendingTransactions") {
    req.action = Request::SUBSCR_PENDING_TXNS;
  } else if (what == "logs") {
    req.action = Request::SUBSCR_EVENTS;
    if (params.size() > 1) {
      if (!InitializeEventFilter(params[1], req.eventFilter, req.error)) {
        req.errorCode = RPCError::INVALID_PARAMS;
        return false;
      }
    }
  } else {
    req.errorCode = RPCError::INVALID_PARAMS;
    req.error = "Unexpected subscribe argument: ";
    req.error += what;
    return false;
  }

  return true;
}

}  // namespace

SubscriptionsImpl::~SubscriptionsImpl() {
  if (m_websocketServer) {
    m_websocketServer->CloseAll();
  }
}

void SubscriptionsImpl::Start(
    std::shared_ptr<rpc::WebsocketServer> websocketServer,
    APICache::BlockByHash blockByHash) {
  assert(websocketServer);
  assert(blockByHash);

  m_websocketServer = std::move(websocketServer);
  m_blockByHash = std::move(blockByHash);

  m_websocketServer->SetOptions(
      [this](Id conn_id, const rpc::WebsocketServer::InMessage& msg,
             bool& methodAccepted) {
        return OnIncomingMessage(conn_id, msg, methodAccepted);
      },
      rpc::WebsocketServer::DEF_MAX_INCOMING_MSG_SIZE);

  m_pendingTxnTemplate["jsonrpc"] = "2.0";
  m_pendingTxnTemplate["method"] = "eth_subscription";
  m_pendingTxnTemplate["params"]["result"] = "";
  m_pendingTxnTemplate["params"]["subscription"] = SUBSCR_ID_FOR_PENDING_TXNS;

  m_newHeadTemplate = m_pendingTxnTemplate;
  m_newHeadTemplate["params"]["subscription"] = SUBSCR_ID_FOR_NEW_HEADS;

  m_eventTemplate = m_newHeadTemplate;
}

void SubscriptionsImpl::OnNewHead(const std::string& blockHash) {
  Lock lk(m_mutex);

  if (m_subscribedToNewHeads.empty()) {
    return;
  }

  m_newHeadTemplate["params"]["result"] = m_blockByHash(blockHash);
  auto msg = std::make_shared<std::string>(JsonWrite(m_newHeadTemplate));

  assert(m_websocketServer);

  for (auto& conn : m_subscribedToNewHeads) {
    m_websocketServer->SendMessage(conn->id, msg);
  }
}

void SubscriptionsImpl::OnPendingTransaction(const std::string& hash) {
  Lock lk(m_mutex);

  if (m_subscribedToPendingTxns.empty()) {
    return;
  }

  m_pendingTxnTemplate["params"]["result"] = hash;
  auto msg = std::make_shared<std::string>(JsonWrite(m_pendingTxnTemplate));

  assert(m_websocketServer);

  for (auto& conn : m_subscribedToPendingTxns) {
    m_websocketServer->SendMessage(conn->id, msg);
  }
}

void SubscriptionsImpl::OnEventLog(const Address& address,
                                   const std::vector<Quantity>& topics,
                                   const Json::Value& log_response) {
  Lock lk(m_mutex);

  Json::Value& json = m_eventTemplate["params"];
  bool prepared = false;

  for (const auto& conn : m_subscribedToLogs) {
    for (const auto& pair : conn->eventFilters) {
      if (Match(pair.second, address, topics)) {
        if (!prepared) {
          json["params"] = Json::Value{};
          json["params"]["result"] = log_response;
          json["params"]["subscription"] = pair.first;
          prepared = true;
        }
        json["subscription"] = pair.first;
        json["method"] = "eth_subscription";
        m_websocketServer->SendMessage(
            conn->id, std::make_shared<std::string>(JsonWrite(json)));

        // Don't send the same message to the same connection
        break;
      }
    }
  }
}

bool SubscriptionsImpl::OnIncomingMessage(
    Id conn_id, const rpc::WebsocketServer::InMessage& msg,
    bool& unknownMethodFound) {
  assert(m_websocketServer);

  if (msg.empty()) {
    Lock lk(m_mutex);
    OnSessionDisconnected(conn_id);
    return false;
  }

  Request req;
  if (!ParseRequest(msg, req, unknownMethodFound)) {
    LOG_GENERAL(INFO, "Request parse error: " << req.error);
    ReplyError(conn_id, std::move(req.id), req.errorCode, std::move(req.error));
    return true;
  }

  if (unknownMethodFound) {
    return true;
  }

  Lock lk(m_mutex);

  auto it = m_connections.find(conn_id);
  if (it == m_connections.end()) {
    auto newConn = std::make_shared<Connection>();
    newConn->id = conn_id;
    it =
        m_connections.insert(std::make_pair(conn_id, std::move(newConn))).first;
  }

  const auto& conn = it->second;

  OutMessage response;
  switch (req.action) {
    case Request::UNSUBSCRIBE:
      response =
          OnUnsubscribe(conn, std::move(req.id), std::move(req.subscriptionId));
      break;
    case Request::SUBSCR_NEW_HEADS:
      response = OnSubscribeToNewHeads(conn, std::move(req.id));
      break;
    case Request::SUBSCR_PENDING_TXNS:
      response = OnSubscribeToPendingTxns(conn, std::move(req.id));
      break;
    case Request::SUBSCR_EVENTS:
      response = OnSubscribeToEvents(conn, std::move(req.id),
                                     std::move(req.eventFilter));
      break;
    default:
      ReplyError(conn_id, std::move(req.id), RPCError::INTERNAL_ERROR,
                 "Should not get here");
      return true;
  }

  m_websocketServer->SendMessage(conn_id, std::move(response));
  return true;
}

void SubscriptionsImpl::ReplyError(Id conn_id, Json::Value&& request_id,
                                   RPCError errorCode, std::string&& error) {
  Json::Value json;
  json["jsonrpc"] = "2.0";
  json["id"] = std::move(request_id);
  auto& err = json["error"];
  err["code"] = static_cast<int>(errorCode);
  err["message"] = std::move(error);
  m_websocketServer->SendMessage(
      conn_id, std::make_shared<std::string>(JsonWrite(json)));
}

void SubscriptionsImpl::OnSessionDisconnected(Id conn_id) {
  auto it = m_connections.find(conn_id);
  if (it == m_connections.end()) {
    return;
  }
  const auto& conn = it->second;
  m_subscribedToPendingTxns.erase(conn);
  m_subscribedToNewHeads.erase(conn);
  m_subscribedToLogs.erase(conn);
  m_connections.erase(it);
}

SubscriptionsImpl::OutMessage SubscriptionsImpl::OnUnsubscribe(
    const ConnectionPtr& conn, Json::Value&& request_id,
    std::string&& subscription_id) {
  bool result = false;
  if (subscription_id == SUBSCR_ID_FOR_PENDING_TXNS) {
    if (conn->subscribedToPendingTxns) {
      m_subscribedToPendingTxns.erase(conn);
      conn->subscribedToPendingTxns = false;
      result = true;
    }
  } else if (subscription_id == SUBSCR_ID_FOR_NEW_HEADS) {
    if (conn->subscribedToNewHeads) {
      m_subscribedToNewHeads.erase(conn);
      conn->subscribedToNewHeads = false;
      result = true;
    }
  } else {
    auto& filters = conn->eventFilters;
    auto it = filters.find(subscription_id);
    if (it != filters.end()) {
      filters.erase(it);
      if (filters.empty()) {
        m_subscribedToLogs.erase(conn);
      }
      result = true;
    }
  }

  Json::Value json;
  json["jsonrpc"] = "2.0";
  json["id"] = std::move(request_id);
  json["result"] = result;
  return std::make_shared<std::string>(JsonWrite(json));
}

SubscriptionsImpl::OutMessage SubscriptionsImpl::OnSubscribeToNewHeads(
    const ConnectionPtr& conn, Json::Value&& request_id) {
  if (!conn->subscribedToNewHeads) {
    conn->subscribedToNewHeads = true;
    m_subscribedToNewHeads.insert(conn);
  }

  Json::Value json;
  json["jsonrpc"] = "2.0";
  json["id"] = std::move(request_id);
  json["result"] = SUBSCR_ID_FOR_NEW_HEADS;
  return std::make_shared<std::string>(JsonWrite(json));
}

SubscriptionsImpl::OutMessage SubscriptionsImpl::OnSubscribeToPendingTxns(
    const ConnectionPtr& conn, Json::Value&& request_id) {
  if (!conn->subscribedToPendingTxns) {
    conn->subscribedToPendingTxns = true;
    m_subscribedToPendingTxns.insert(conn);
  }

  Json::Value json;
  json["jsonrpc"] = "2.0";
  json["id"] = std::move(request_id);
  json["result"] = SUBSCR_ID_FOR_PENDING_TXNS;
  return std::make_shared<std::string>(JsonWrite(json));
}

SubscriptionsImpl::OutMessage SubscriptionsImpl::OnSubscribeToEvents(
    const ConnectionPtr& conn, Json::Value&& request_id,
    EventFilterParams&& filter) {
  auto subscriptionId = NumberAsString(++m_eventSubscriptionCounter);
  conn->eventFilters[subscriptionId] = std::move(filter);
  if (conn->eventFilters.size() == 1) {
    m_subscribedToLogs.insert(conn);
  }

  Json::Value json;
  json["jsonrpc"] = "2.0";
  json["id"] = std::move(request_id);
  json["result"] = std::move(subscriptionId);
  return std::make_shared<std::string>(JsonWrite(json));
}

}  // namespace filters
}  // namespace evmproj
