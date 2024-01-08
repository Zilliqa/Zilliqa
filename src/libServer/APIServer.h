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

#ifndef ZILLIQA_SRC_LIBSERVER_APISERVER_H_
#define ZILLIQA_SRC_LIBSERVER_APISERVER_H_

#include <memory>
#include <string>
#include "common/Constants.h"

namespace boost {
namespace asio {
class io_context;
}
}  // namespace boost

namespace jsonrpc {
class AbstractServerConnector;
}

namespace rpc {

class WebsocketServer;

/// API server: includes HTTP backend for JSON RPC (with thread pool) and
/// websocket server backend
class APIServer {
 public:
  /// Server start parameters
  struct Options {
    /// External Asio context. If nullptr, the server will run event loop in its
    /// private dedicated thread
    std::shared_ptr<boost::asio::io_context> asio;

    /// Listen port
    uint16_t port = 8080;

    /// If true, then the listening socket is bound to 127.0.0.1 only
    bool bindToLocalhost = false;

    /// Limit in bytes for POST bodies of incoming requests (security)
    size_t inputBodyLimitBytes = 5 * 1024 * 1024;

    /// Prefix for thread names in threadpool
    std::string threadPoolName;

    /// Number of threads in thread pool
    size_t numThreads = REQUEST_PROCESSING_THREADS;

    /// Max size of unhandled requests queue
    size_t maxQueueSize = REQUEST_QUEUE_SIZE;

    // TODO enable TLS later
    // std::string tlsCertificateFileName;
    // std::string tlsKeyFileName;
  };

  /// Creates and starts API server instance
  /// \param asio Boost.asio context
  /// \param options Server options
  /// \param startListening Start listening immediately
  /// \return APIServer instance on success, empty shared_ptr on failure
  static std::shared_ptr<APIServer> CreateAndStart(Options options,
                                                   bool startListening = true);

  virtual ~APIServer() = default;

  /// Returns the backend needed for LookupServer (or IsolatedServer)
  virtual jsonrpc::AbstractServerConnector& GetRPCServerBackend() = 0;

  /// Returns Websocket backend
  virtual std::shared_ptr<WebsocketServer> GetWebsocketServer() = 0;

  /// Explicit close because of shared_ptr usage
  virtual void Close() = 0;

  virtual void Pause(bool value) = 0;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_APISERVER_H_
