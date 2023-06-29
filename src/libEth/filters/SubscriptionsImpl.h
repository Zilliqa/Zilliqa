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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_SUBSCRIPTIONSIMPL_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_SUBSCRIPTIONSIMPL_H_

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "Common.h"
#include "libServer/WebsocketServer.h"


namespace evmproj {
namespace filters {

enum class RPCError {
  OK = 0,
  PARSE_ERROR = -32700,
  INVALID_REQUEST = -32600,
  METHOD_NOT_FOUND = -32601,
  INVALID_PARAMS = -32602,
  INTERNAL_ERROR = -32603
};

/// Backend for eth_subscribe/eth_unsubscribe API
class SubscriptionsImpl {
 public:
  ~SubscriptionsImpl();

  /// Attaches websocket server
  void Start(std::shared_ptr<rpc::WebsocketServer> websocketServer,
             APICache::BlockByHash blockByHash);

  /// Broadcasts new head
  void OnNewHead(const std::string& blockHash);

  /// Broadcasts pending tx to subscriptions
  void OnPendingTransaction(const std::string& hash);

  /// Applies event logs to filters
  void OnEventLog(const Address& address, const std::vector<Quantity>& topics,
                  const Json::Value& log_response);

 private:
  /// Websocket backend
  std::shared_ptr<rpc::WebsocketServer> m_websocketServer;

  /// Callback which gets JSON message for newHeads subscription out of block
  /// hash
  APICache::BlockByHash m_blockByHash;

  using Id = rpc::WebsocketServer::ConnectionId;

  struct Connection {
    /// Id for websocket dispatch
    Id id = 0;

    // populated if this conn subscribed to pending txns.
    // since a single connection could have multiple subscriptions, we need to
    // keep a set of subscription IDs here. They are non-overlapping
    // with subscribedToNewHeads
    //bool subscribedToPendingTxns = false;
    std::unordered_set<uint64_t> subscribedToPendingTxn;

    /// populated if this conn subscribed to new heads
    std::unordered_set<uint64_t> subscribedToNewHeads;

    /// Event subscription -> filter
    std::unordered_map<std::string, EventFilterParams> eventFilters;

    uint64_t index = 0;

    uint64_t AddHeadSubscription() {
      subscribedToNewHeads.insert(index);
      index++;
      return index - 1;
    }

    uint64_t AddPendingTxnSubscription() {
      subscribedToPendingTxn.insert(index);
      index++;
      return index - 1;
    }
  };

  using ConnectionPtr = std::shared_ptr<Connection>;

  /// Incoming message from websocket server
  bool OnIncomingMessage(Id conn_id, const rpc::WebsocketServer::InMessage& msg,
                         bool& methodAccepted);

  /// Connection closed
  void OnSessionDisconnected(Id conn_id);

  /// Sends error reply to the connection
  void ReplyError(Id conn_id, Json::Value&& request_id, RPCError errorCode,
                  std::string&& error);

  using OutMessage = rpc::WebsocketServer::OutMessage;

  /// eth_unsubscribe handler
  OutMessage OnUnsubscribe(const ConnectionPtr& conn, Json::Value&& request_id,
                           std::string&& subscription_id);

  /// eth_subscribe to "newHeads" handler
  OutMessage OnSubscribeToNewHeads(const ConnectionPtr& conn,
                                   Json::Value&& request_id);

  /// eth_subscribe to "newPendingTransactions" handler
  OutMessage OnSubscribeToPendingTxns(const ConnectionPtr& conn,
                                      Json::Value&& request_id);

  /// eth_subscribe to "logs" handler
  OutMessage OnSubscribeToEvents(const ConnectionPtr& conn,
                                 Json::Value&& request_id,
                                 EventFilterParams&& filter);

  /// All active connections
  std::unordered_map<Id, ConnectionPtr> m_connections;

  /// Connections who subscribed to event logs
  std::unordered_set<ConnectionPtr> m_subscribedToLogs;

  /// Template for pending txn message
  Json::Value m_pendingTxnTemplate;

  /// Template for new head message
  Json::Value m_newHeadTemplate;

  /// Template for event log message
  Json::Value m_eventTemplate;

  /// Incremental counter for event logs subscriptions (not starting from 1
  /// because there are special values for other types of subscriptions)
  uint64_t m_eventSubscriptionCounter = 100;

  /// Mutex
  std::mutex m_mutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_SUBSCRIPTIONSIMPL_H_
