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

#ifndef ZILLIQA_SRC_LIBSERVER_DEDICATEDWEBSOCKETSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_DEDICATEDWEBSOCKETSERVER_H_

#include <memory>

class TransactionWithReceipt;

namespace Json {
class Value;
}

namespace rpc {

class DedicatedWebsocketServer {
 public:
  /// Creates a WS server
  static std::shared_ptr<DedicatedWebsocketServer> Create();

  virtual ~DedicatedWebsocketServer() = default;

  /// starts (only if WEBSOCKET_ENABLED in config)
  virtual void Start() = 0;

  /// stops and joins the thread (if started)
  virtual void Stop() = 0;

  /// Parses tx and receipt, everything will be sent on FinalizeTxBlock
  virtual void ParseTxn(const TransactionWithReceipt& twr) = 0;

  /// Sends out messages related to finalized TX block
  virtual void FinalizeTxBlock(const Json::Value& json_txblock,
                               const Json::Value& json_txhashes) = 0;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_DEDICATEDWEBSOCKETSERVER_H_
