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

namespace evmproj {

APIThreadPool::APIThreadPool(boost::asio::io_context& asio, size_t numThreads,
                             size_t maxQueueSize, ProcessRequest processRequest,
                             OwnerFeedback ownerFeedback)
    : m_asio(asio),
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

bool APIThreadPool::PushRequest(JobId id, std::string from, std::string body) {
  if (!m_requestQueue.bounded_push(
          Request{id, std::move(from), std::move(body)})) {
    Response response;
    response.id = id;
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

void APIThreadPool::WorkerThread(size_t threadNo) {
  auto threadName = std::string("APIWorker-") + std::to_string(threadNo + 1);
  SetThreadName(threadName.c_str());

  Request request;
  size_t queueSize = 0;
  while (m_requestQueue.pop(request, queueSize)) {
    LOG_GENERAL(INFO, threadName << " processes job #" << request.id
                                 << ", Q=" << queueSize);
    PushResponse(m_processRequest(request));
  }
}

void APIThreadPool::PushResponse(Response response) {
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

}  // namespace evmproj
