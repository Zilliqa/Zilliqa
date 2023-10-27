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

#include "APIServerImpl.h"

#include <boost/asio/signal_set.hpp>
#include <deque>

#include "libUtils/Logger.h"
#include "libUtils/SetThreadName.h"

namespace rpc {

/// HTTP connection from the server perspective
/// TODO write buffer constraint against slow clients or their sabotage
class APIServerImpl::Connection
    : public std::enable_shared_from_this<APIServerImpl::Connection> {
 public:
  /// Incremental IDs needed for long polling
  using ConnectionId = uint64_t;

  /// Ctor, called from the owner on socket accept
  Connection(std::weak_ptr<APIServerImpl> owner, ConnectionId id,
             std::string from, Socket &&socket, size_t inputBodyLimit)
      : m_owner(std::move(owner)),
        m_id(id),
        m_from(std::move(from)),
        m_stream(std::move(socket)),
        m_inputBodyLimit(inputBodyLimit) {}

  /// Initiates async msg read
  void StartReading() {
    m_stream.expires_after(std::chrono::seconds(120));  // TODO option?
    m_parser.emplace();
    if (m_inputBodyLimit > 0) {
      m_parser->body_limit(m_inputBodyLimit);
    }
    m_readBuffer.consume(m_readBuffer.size());
    http::async_read(
        m_stream, m_readBuffer, *m_parser,
        beast::bind_front_handler(&Connection::OnRead, shared_from_this()));
  }

  /// Writes asyn response from thread pool
  void WriteResponse(http::status status, std::string &&body) {
    if (status != http::status::ok) {
      ReplyError(m_clientKeepAlive, m_clientHttpVersion, status,
                 std::move(body));
      return;
    }
    HttpResponse res(status, m_clientHttpVersion);
    res.keep_alive(m_clientKeepAlive);
    res.set(http::field::content_type, "application/json");
    res.set(http::field::access_control_allow_origin, "*");
    res.body() = std::move(body);
    res.prepare_payload();
    m_writeQueue.emplace_back(std::move(res));
    StartWriting();
  }

  /// Explicit close
  void Close() {
    m_owner.reset();
    if (m_writeQueue.size() > 1) {
      m_writeQueue.erase(++m_writeQueue.begin(), m_writeQueue.end());
    } else if (m_writeQueue.empty() && m_clientHttpVersion != 0) {
      // this means that the request is in progess
      ReplyError(false, m_clientHttpVersion,
                 http::status::internal_server_error, "API closed");
    }
  }

 private:
  using HttpResponse = http::response<http::string_body>;

  /// Read callback
  void OnRead(beast::error_code ec, size_t n) {
    auto owner = m_owner.lock();
    if (!owner) {
      // server is closed
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      m_stream.socket().close(ec);
      OnClosed();
      return;
    }

    if (ec || n == 0) {
      if (ec != http::error::end_of_stream) {
        LOG_GENERAL(DEBUG, "Read error: " << ec.message());
        m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      }
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      m_stream.socket().close(ec);
      OnClosed();
      return;
    }

    auto request = m_parser->release();

    if (beast::websocket::is_upgrade(request)) {
      OnWebsocketUpgrade(std::move(request));
      return;
    }

    auto method = request.method();
    if (method == http::verb::post) {
      OnRequest(std::move(request));
    } else if (method == http::verb::options) {
      ReplyOptions(request.keep_alive(), request.version());
    } else {
      ReplyError(request.keep_alive(), request.version(),
                 http::status::method_not_allowed, "Unsupported method");
    }

    StartReading();
  }

  /// Client upgrades to Websocket protocol
  void OnWebsocketUpgrade(HttpRequest &&request) {
    auto owner = m_owner.lock();
    if (owner) {
      owner->OnWebsocketUpgrade(m_id, std::move(m_from),
                                m_stream.release_socket(), std::move(request));
      m_owner.reset();
    }
  }

  /// Client sent API request
  void OnRequest(HttpRequest &&request) {
    static const char *METHOD("method");

    if (request.body().find(METHOD) == std::string::npos) {
      // definitely not a jsonrpc call, don't bother the threadpool
      ReplyError(request.keep_alive(), request.version(),
                 http::status::bad_request, "RPC method missing");
      return;
    }

    auto owner = m_owner.lock();
    if (owner) {
      // save keep_alive and version
      m_clientKeepAlive = request.keep_alive();
      m_clientHttpVersion = request.version();

      // post this to thread pool
      owner->OnRequest(m_id, m_from, std::move(request));
    } else {
      ReplyError(request.keep_alive(), request.version(),
                 http::status::internal_server_error, "API closed");
    }
  }

  /// Client has sent OPTIONS request
  void ReplyOptions(bool keepAlive, unsigned httpVersion) {
    HttpResponse res(http::status::ok, httpVersion);
    res.set(http::field::allow, "POST, OPTIONS");
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_headers,
            "origin, content-type, accept");
    res.set(http::field::dav, "1");
    res.keep_alive(keepAlive);
    res.prepare_payload();
    m_writeQueue.emplace_back(std::move(res));
    StartWriting();
  }

  /// Reply with HTTP error
  void ReplyError(bool keepAlive, unsigned httpVersion, http::status status,
                  std::string description) {
    HttpResponse res(status, httpVersion);
    res.keep_alive(keepAlive);
    if (!description.empty()) {
      res.set(http::field::content_type, "text/plain");
      res.set(http::field::access_control_allow_origin, "*");
      res.body() = std::move(description);
    }
    res.prepare_payload();
    m_writeQueue.emplace_back(std::move(res));
    StartWriting();
  }

  /// Detach from the owner
  void OnClosed() {
    auto owner = m_owner.lock();
    if (owner) {
      owner->OnClosed(m_id);
      m_owner.reset();
    }
  }

  /// Initiate async write operation
  void StartWriting() {
    auto sz = m_writeQueue.size();

    if (sz == 0 && m_owner.expired()) {
      beast::error_code ec;
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      m_stream.socket().close(ec);
      OnClosed();
      return;
    }

    if (sz != 1) {
      // only one active write operation is allowed
      return;
    }

    auto &msg = m_writeQueue.front();

    http::async_write(
        m_stream, msg,
        [self = shared_from_this()](beast::error_code ec, size_t n) {
          self->OnWrite(ec, n);
        });
  }

  /// Write callback
  void OnWrite(beast::error_code ec, size_t) {
    if (ec) {
      OnClosed();
      return;
    }

    if (m_writeQueue.empty()) {
      // Something corrupted - we should never get here
      OnClosed();
      return;
    }

    m_writeQueue.pop_front();
    StartWriting();
  }

  /// The owner. If not expired then it accepts callbacks
  std::weak_ptr<APIServerImpl> m_owner;

  /// Id of connection, for dispatching
  const ConnectionId m_id;

  /// Peer address
  std::string m_from;

  /// Internal http stream with socket inside
  beast::tcp_stream m_stream;

  /// Limit for body size of input messages, security reason
  size_t m_inputBodyLimit;

  /// Internal read buffer
  beast::flat_buffer m_readBuffer;

  /// Write queue, we support keep-alive
  std::deque<HttpResponse> m_writeQueue;

  /// Internal HTTP parser, is being reset before every message read
  boost::optional<http::request_parser<http::string_body>> m_parser;

  /// Save kee-alive parameter of the last request here
  bool m_clientKeepAlive = false;

  /// Save client's http version here
  unsigned m_clientHttpVersion = 0;
};

std::shared_ptr<APIServer> APIServer::CreateAndStart(APIServer::Options options,
                                                     bool startImmediately) {
  auto server = std::make_shared<APIServerImpl>(std::move(options));
  if (startImmediately && !server->Start()) {
    return {};
  }
  return server;
}

APIServerImpl::APIServerImpl(Options options) : m_options(std::move(options)) {
  if (m_options.numThreads == 0) {
    m_options.numThreads = 1;
  }
  if (m_options.maxQueueSize == 0) {
    m_options.maxQueueSize = std::numeric_limits<size_t>::max();
  }

  if (!m_options.asio) {
    m_options.asio = std::make_shared<AsioCtx>(1);
    m_ownEventLoop = true;
  }

  m_threadPool = std::make_shared<APIThreadPool>(
      *m_options.asio, m_options.threadPoolName, m_options.numThreads,
      m_options.maxQueueSize,
      [this](const APIThreadPool::Request &req) -> APIThreadPool::Response {
        return ProcessRequestInThreadPool(req);
      },
      [this](APIThreadPool::Response &&res) {
        OnResponseFromThreadPool(std::move(res));
      });

  m_websocket = WebsocketServerBackend::CreateWithThreadPool(*m_options.asio,
                                                             m_threadPool);
}

bool APIServerImpl::Start() {
  if (m_active) {
    LOG_GENERAL(WARNING, "Double start ignored");
    return false;
  }

  if (!m_started) {
    if (!DoListen()) {
      return false;
    }

    m_started = true;

    if (m_ownEventLoop) {
      m_eventLoopThread.emplace([this] { EventLoopThread(); });
    }
  } else {
    m_options.asio->post(
        [self = shared_from_this()] { std::ignore = self->DoListen(); });
  }

  return true;
}

bool APIServerImpl::DoListen() {
  if (m_active) {
    return false;
  }

  auto address = m_options.bindToLocalhost
                     ? boost::asio::ip::address_v4::loopback()
                     : boost::asio::ip::address_v4::any();
  tcp::endpoint endpoint(address, m_options.port);

  m_acceptor.emplace(*m_options.asio);

  beast::error_code ec;

#define CHECK_EC()                                                   \
  if (ec) {                                                          \
    LOG_GENERAL(FATAL, "Cannot start API server: " << ec.message()); \
    return false;                                                    \
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

  m_active = true;

  AcceptNext();

  m_metrics.SetCallback([this](auto &&result) {
    if (m_metrics.Enabled()) {
      result.Set(m_connections.size(), {{"server", m_options.threadPoolName},
                                        {"counter", "TotalConnections"}});

      result.Set(m_threadPool->GetQueueSize(),
                 {{"server", m_options.threadPoolName},
                  {"counter", "ThreadPoolQueueSize"}});
    }
  });

  return true;
}

void APIServerImpl::OnWebsocketUpgrade(ConnectionId id, std::string &&from,
                                       Socket &&socket, HttpRequest &&request) {
  m_connections.erase(id);
  m_websocket->NewConnection(std::move(from), std::move(socket),
                             std::move(request));
}

void APIServerImpl::OnRequest(ConnectionId id, std::string from,
                              HttpRequest &&request) {
  if (!m_threadPool->PushRequest(id, false, std::move(from),
                                 std::move(request.body()))) {
    LOG_GENERAL(WARNING, "Request queue is full");
  }
}

void APIServerImpl::OnClosed(ConnectionId id) { m_connections.erase(id); }

std::shared_ptr<WebsocketServer> APIServerImpl::GetWebsocketServer() {
  return m_websocket;
}

jsonrpc::AbstractServerConnector &APIServerImpl::GetRPCServerBackend() {
  return *this;
}

void APIServerImpl::Close() {
  std::ignore = StopListening();

  if (m_started) {
    if (m_eventLoopThread.has_value()) {
      // after connections closed
      m_options.asio->post(
          [self = shared_from_this()] { self->m_options.asio->stop(); });
      m_eventLoopThread->join();
      m_eventLoopThread.reset();
    }
    m_started = false;
  }
}

bool APIServerImpl::StartListening() {
  if (!m_active) {
    return Start();
  }
  return true;
}

bool APIServerImpl::StopListening() {
  if (m_active) {
    m_active = false;

    m_threadPool->Reset();

    assert(m_websocket);
    m_websocket->CloseAll();

    m_options.asio->post([self = shared_from_this()] {
      self->m_acceptor.reset();
      for (auto &conn : self->m_connections) {
        conn.second->Close();
      }
      self->m_connections.clear();
    });
  }
  return true;
}

void APIServerImpl::AcceptNext() {
  assert(m_acceptor);

  m_acceptor->async_accept(
      beast::bind_front_handler(&APIServerImpl::OnAccept, shared_from_this()));
}

void APIServerImpl::OnAccept(beast::error_code ec, tcp::socket socket) {
  if (ec || !m_active || !m_started || !socket.is_open()) {
    // stopped, ignore
    return;
  }

  socket.set_option(asio::socket_base::keep_alive(true), ec);

  auto ep = socket.remote_endpoint(ec);
  auto from = ep.address().to_string() + ":" + std::to_string(ep.port());

  ++m_counter;
  auto conn = std::make_shared<Connection>(weak_from_this(), m_counter, from,
                                           std::move(socket),
                                           m_options.inputBodyLimitBytes);
  conn->StartReading();
  m_connections[m_counter] = std::move(conn);

  LOG_GENERAL(DEBUG, "Connection #" << m_counter << " from " << from
                                    << ", total=" << m_connections.size());

  AcceptNext();
}

APIThreadPool::Response APIServerImpl::ProcessRequestInThreadPool(
    const APIThreadPool::Request &request) {
  APIThreadPool::Response response;
  bool error = false;
  try {
    // Calls connection handler from AbstractServerConnector
    ProcessRequest(request.body, response.body);

    // Connection handler was not installed - internal error
    error = response.body.empty();
  } catch (const std::exception &e) {
    LOG_GENERAL(WARNING,
                "Unhandled exception in API thread pool: " << e.what());
    error = true;
  } catch (...) {
    LOG_GENERAL(WARNING, "Unhandled exception in API thread pool.");
    error = true;
  }

  if (error) {
    response.code = 500;
    response.body = "Error processing request";
  }
  response.id = request.id;
  response.isWebsocket = request.isWebsocket;
  return response;
}

void APIServerImpl::OnResponseFromThreadPool(
    APIThreadPool::Response &&response) {
  if (!m_active) {
    // ignoring response if not active
    return;
  }

  if (response.isWebsocket) {
    // API response to be dispatched to websocket connection
    if (m_websocket) {
      m_websocket->SendMessage(
          response.id, std::make_shared<std::string>(std::move(response.body)));
    } else {
      LOG_GENERAL(WARNING, "Websocket server expected");
    }
    return;
  }

  auto it = m_connections.find(response.id);
  if (it == m_connections.end()) {
    // ignoring closed connection
    return;
  }
  it->second->WriteResponse(static_cast<http::status>(response.code),
                            std::move(response.body));
}

void APIServerImpl::EventLoopThread() {
  LOG_MARKER();

  if (!m_options.threadPoolName.empty()) {
    utility::SetThreadName(m_options.threadPoolName.c_str());
  }

  boost::asio::signal_set sig(*m_options.asio, SIGABRT);
  sig.async_wait([](const boost::system::error_code &, int) {});

  m_options.asio->run();
}

}  // namespace rpc
