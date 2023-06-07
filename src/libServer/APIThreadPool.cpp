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
#include <time.h>

namespace rpc {

APIThreadPool::APIThreadPool(boost::asio::io_context& asio, std::string name,
                             size_t numThreads, size_t maxQueueSize,
                             ProcessRequest processRequest,
                             OwnerFeedback ownerFeedback)
    : m_asio(asio),
      m_name(std::move(name)),
      m_processRequest(std::move(processRequest)),
      m_ownerFeedback(std::move(ownerFeedback)),
      m_occupied(),
      m_last_occupied_log(0),
      m_requestQueue(maxQueueSize) {
  LOG_MARKER();
  assert(m_processRequest);
  assert(m_ownerFeedback);
  assert(maxQueueSize > 0);

  if (numThreads == 0) numThreads = 1;

  m_threads.reserve(numThreads);
  for (size_t i =0;i < numThreads; ++i) {
    m_occupied.push_back('I');
  }
  LOG_GENERAL(INFO, "maxQueueSize = "<<maxQueueSize<< " num threads = "<< numThreads);
  for (size_t i = 0; i < numThreads; ++i) {
    m_threads.emplace_back([this, i] { WorkerThread(i); });
  }
}

APIThreadPool::~APIThreadPool() {
  LOG_MARKER();
  m_requestQueue.stop();
  m_responseQueue.stop();
  for (auto& thread : m_threads) {
    thread.join();
  }
}

bool APIThreadPool::PushRequest(JobId id, bool isWebsocket, std::string from,
                                std::string body) {
  LOG_MARKER();
  {
    size_t ourSize = m_requestQueue.size();
    struct timespec now = { 0, 0 };
    if (ourSize > m_requestQueueHWM) {
      m_requestQueueHWM = ourSize;
    }
    clock_gettime(CLOCK_MONOTONIC, & now);
    if (now.tv_sec > m_lastSeconds+2) {
      LOG_GENERAL(INFO, "Queue HWM " << m_requestQueueHWM << " size " << ourSize);
      m_lastSeconds = now.tv_sec;
      m_requestQueueHWM = 0;
    }
  }

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

void APIThreadPool::Reset() {
  LOG_MARKER();
  m_requestQueue.reset();
  m_responseQueue.reset();
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

// TODO : good place to add a gate to stop processing request - SW

void APIThreadPool::WorkerThread(size_t threadNo) {
  auto threadName = m_name + "-" + std::to_string(threadNo + 1);
  utility::SetThreadName(threadName.c_str());
  StopWatch sw;

  Request request;
  size_t queueSize = 0;
  this->SetThreadStatus(threadNo, 'w');
  while (m_requestQueue.pop(request, queueSize)) {
    this->SetThreadStatus(threadNo, '<');
    LOG_GENERAL(INFO, threadName << " processes job #" << request.id
                                  << ", Q=" << queueSize);
    sw.Start();
    auto response = m_processRequest(request);
    sw.Stop();

    LOG_GENERAL(INFO, threadName << ": " << sw.Microseconds() << " microsec");
    this->SetThreadStatus(threadNo, '>');
    PushResponse(std::move(response));
    this->SetThreadStatus(threadNo, 'w');
  }
  this->SetThreadStatus(threadNo, 'x');
}

void APIThreadPool::SetThreadStatus(size_t which, char what) {
  std::lock_guard<std::mutex> my_lock(m_occupied_mutex);
  m_occupied[which] = what;
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  if (tv.tv_sec > m_last_occupied_log+3) {
    std::string occupation(m_occupied.begin(), m_occupied.end());
    LOG_GENERAL(INFO, "T: " << occupation);
    m_last_occupied_log = tv.tv_sec;
  }
}

void APIThreadPool::PushResponse(Response&& response) {
  LOG_MARKER();
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
  LOG_MARKER();
  Response response;
  while (m_responseQueue.try_pop(response)) {
    m_ownerFeedback(std::move(response));
  }
}

}  // namespace rpc
