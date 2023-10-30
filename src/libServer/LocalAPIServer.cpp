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

#include "LocalAPIServer.h"

#include "libUtils/Logger.h"

#include <jsonrpccpp/common/sharedconstants.h>
#include <boost/asio.hpp>

namespace rpc {

namespace {

constexpr size_t MAX_READ_BUFFER_SIZE = 128 * 1024;

}

LocalAPIServer::~LocalAPIServer() { DoStop(); }

bool LocalAPIServer::StartListening() {
  if (m_started) {
    return false;
  }

  try {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(m_ip),
                                            m_port);
    m_acceptor.emplace(m_asio);

    boost::system::error_code ec;

#define CHECK_EC()                                                     \
  if (ec) {                                                            \
    LOG_GENERAL(WARNING, "Cannot start API server: " << ec.message() << " port =" << m_port); \
    return false;                                                      \
  }

    m_acceptor->open(endpoint.protocol(), ec);
    CHECK_EC();
    m_acceptor->set_option(boost::asio::socket_base::reuse_address(true), ec);
    CHECK_EC();
    m_acceptor->bind(endpoint, ec);
    CHECK_EC();
    m_acceptor->listen(boost::asio::socket_base::max_listen_connections, ec);
    CHECK_EC();

#undef CHECK_EC

    m_ip += ":";
    m_ip += std::to_string(m_port);

  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Start listening to " << m_ip << " failed: " << e.what());
    m_acceptor.reset();
    return false;
  }

  m_started = true;
  m_thread.emplace([this] { WorkerThread(); });

  return true;
}

bool LocalAPIServer::StopListening() { return DoStop(); }

namespace {

void WakeAcceptor(const boost::asio::ip::tcp::endpoint& endpoint) {
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  boost::system::error_code ec;
  socket.connect(endpoint, ec);
  // And close immediately
}

}  // namespace

bool LocalAPIServer::DoStop() {
  if (!m_started) {
    return false;
  }

  assert(m_acceptor.has_value());
  assert(m_thread.has_value());

  m_started = false;
  WakeAcceptor(m_endpoint);
  m_thread->join();
  m_acceptor.reset();
  return true;
}

void LocalAPIServer::WorkerThread() {
  try {
    std::string readBuffer;
    std::string response;

    while (m_started) {
      boost::asio::ip::tcp::socket socket(m_asio);

      boost::system::error_code ec;
      m_acceptor->accept(socket, ec);
      if (ec && m_started) {
        socket.close(ec);
        throw std::runtime_error(ec.message());
      }

      if (!m_started) {
        socket.close(ec);
        break;
      }

      readBuffer.clear();

      auto n = boost::asio::read_until(
          socket, boost::asio::dynamic_buffer(readBuffer, MAX_READ_BUFFER_SIZE),
          DEFAULT_DELIMITER_CHAR, ec);
      if (ec || n <= 1) {
        LOG_GENERAL(WARNING, "Read (" << m_ip << ") failed: " << ec.message());
        socket.close(ec);
        continue;
      }

      if (!m_started) {
        socket.close(ec);
        break;
      }

      readBuffer.erase(n);
      response.clear();

      try {
        ProcessRequest(readBuffer, response);
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING, "Unexpected exception: " << e.what());
      } catch (...) {
        LOG_GENERAL(WARNING, "Unexpected unhandled exception");
      }

      if (!m_started) {
        socket.close(ec);
        break;
      }

      if (response.empty()) {
        response =
            R"({"jsonrpc":"2.0","id":null,"error":{"code":-32603,"message":"Internal error","data":null}"})";
      } else {
        std::replace(response.begin(), response.end(), DEFAULT_DELIMITER_CHAR,
                     ' ');
      }

      response += DEFAULT_DELIMITER_CHAR;

      boost::asio::write(socket, boost::asio::buffer(response), ec);
      if (ec) {
        LOG_GENERAL(WARNING, "Write (" << m_ip << ") failed: " << ec.message());
        socket.close(ec);
        continue;
      }

      socket.shutdown(
          boost::asio::local::stream_protocol::socket::shutdown_both, ec);
      if (ec) {
        LOG_GENERAL(WARNING, "Shutdown failed: " << ec.message());
      }
      socket.close(ec);
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Listening to " << m_ip << " failed: " << e.what());
  }
}

}  // namespace rpc
