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

#include "APIThreadPool.h"

#include "libUtils/Logger.h"
#include "libUtils/SetThreadName.h"

namespace rpc {

APIThreadPool::APIThreadPool(boost::asio::io_context& asio, std::string name,
                             size_t numThreads, size_t maxQueueSize,
                             ProcessRequest processRequest,
                             OwnerFeedback ownerFeedback)
    : m_asio(asio),
      m_name(std::move(name)),
      m_processRequest(std::move(processRequest)),
      m_ownerFeedback(std::move(ownerFeedback)),
      m_requestQueue(maxQueueSize) {
  assert(m_processRequest);
  assert(m_ownerFeedback);
  assert(maxQueueSize > 0);

  if (numThreads == 0) numThreads = 1;

  m_threads.reserve(numThreads);
  for (size_t i = 0; i < numThreads; ++i) {
    m_threads.emplace_back([this, i] { WorkerThread(i); });
  }
}

APIThreadPool::~APIThreadPool() {
  Close();
  for (auto& thread : m_threads) {
    thread.join();
  }
}

bool APIThreadPool::PushRequest(JobId id, bool isWebsocket, std::string from,
                                std::string body) {
  if (!m_requestQueue.bounded_push(
          Request{id, isWebsocket, std::move(from), std::move(body)})) {
    Response response;
    response.id = id;
    response.isWebsocket = isWebsocket;
    response.code = 503;
    PushResponse(std::move(response));
    return false;
  }
  return true;
}

void APIThreadPool::Close() {
  m_requestQueue.stop();
  m_responseQueue.stop();
}

namespace {

class StopWatch {
 public:
  void Start() {
    m_elapsed = 0;
    m_started = std::chrono::high_resolution_clock::now();
  }

  void Stop() {
    auto diff = std::chrono::high_resolution_clock::now() - m_started;
    m_elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
  }

  uint64_t Microseconds() const { return m_elapsed; }

 private:
  std::chrono::high_resolution_clock::time_point m_started;
  uint64_t m_elapsed;
};

}  // namespace

void APIThreadPool::WorkerThread(size_t threadNo) {
  auto threadName = m_name + "-" + std::to_string(threadNo + 1);
  utility::SetThreadName(threadName.c_str());
  StopWatch sw;

  Request request;
  size_t queueSize = 0;
  while (m_requestQueue.pop(request, queueSize)) {
    LOG_GENERAL(DEBUG, threadName << " processes job #" << request.id
                                  << ", Q=" << queueSize);
    sw.Start();
    auto response = m_processRequest(request);
    sw.Stop();

    LOG_GENERAL(DEBUG, threadName << ": " << sw.Microseconds()
                                  << " microsec, request=\n"
                                  << request.body << "\nresponse=\n"
                                  << response.body);
    PushResponse(std::move(response));
  }
}

void APIThreadPool::PushResponse(Response&& response) {
  size_t queueSize = 0;
  if (!m_responseQueue.bounded_push(std::move(response), queueSize)) {
    return;
  }
  if (queueSize == 1) {
    m_asio.post([wptr = weak_from_this()] {
      auto self = wptr.lock();
      if (self) {
        self->ProcessResponseQueue();
      }
    });
  }
}

void APIThreadPool::ProcessResponseQueue() {
  Response response;
  while (m_responseQueue.try_pop(response)) {
    m_ownerFeedback(std::move(response));
  }
}

}  // namespace rpc
