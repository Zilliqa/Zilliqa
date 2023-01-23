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

#ifndef ZILLIQA_SRC_LIBSERVER_APITHREADPOOL_H_
#define ZILLIQA_SRC_LIBSERVER_APITHREADPOOL_H_

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "libUtils/Queue.h"

namespace rpc {

class APIThreadPool : public std::enable_shared_from_this<APIThreadPool> {
 public:
  /// Job ID (is effectively connection's id)
  using JobId = uint64_t;

  /// OK response code
  static constexpr int OK_RESPONSE_CODE = 200;

  struct Request {
    /// Job ID
    JobId id = 0;

    /// If true than the response will be dispatched to websocket connections
    bool isWebsocket = false;

    /// Remote peer (for logging)
    std::string from;

    /// Request body (json rpc 2.0 format expected)
    std::string body;
  };

  struct Response {
    /// Job ID
    JobId id = 0;

    /// If true than the response will be dispatched to websocket connections
    bool isWebsocket = false;

    /// HTTP response code
    int code = OK_RESPONSE_CODE;

    /// Response body
    std::string body;
  };

  /// Process request callback
  using ProcessRequest = std::function<Response(const Request& request)>;

  /// Feedback to the owner
  using OwnerFeedback = std::function<void(Response&& response)>;

  /// Spawns threads
  APIThreadPool(boost::asio::io_context& asio, std::string name,
                size_t numThreads, size_t maxQueueSize,
                ProcessRequest processRequest, OwnerFeedback ownerFeedback);

  /// Joins the threads
  ~APIThreadPool();

  /// A metric
  size_t GetQueueSize() const { return m_requestQueue.size(); }

  /// Owner pushes a new request
  bool PushRequest(JobId id, bool isWebsocket, std::string from,
                   std::string body);

  /// Resets queues
  void Reset();

 private:
  /// Worker thread processes request while not stopped
  void WorkerThread(size_t threadNo);

  /// Threads push a response for the main thread to send to the wire
  void PushResponse(Response&& response);

  /// Main thread processes response queue (TODO maybe revise its logic).
  void ProcessResponseQueue();

  /// Asio context is needed here to send replies to the network thread
  boost::asio::io_context& m_asio;

  /// Threadpool (short) name for logs
  std::string m_name;

  /// Process request callback
  ProcessRequest m_processRequest;

  /// Feedback to the owner
  OwnerFeedback m_ownerFeedback;

  /// Threads
  std::vector<std::thread> m_threads;

  /// Request queue (main thread->pool)
  utility::Queue<Request> m_requestQueue;

  /// Response queue (pool->main thread)
  utility::Queue<Response> m_responseQueue;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_APITHREADPOOL_H_
