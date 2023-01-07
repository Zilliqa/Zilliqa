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

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <vector>
#include "libNetwork/P2PComm.h"
#include "libUtils/DetachedFunction.h"

using namespace std;
chrono::high_resolution_clock::time_point startTime;

void process_message(std::shared_ptr<zil::p2p::Message> message) {
  LOG_MARKER();

  if (message->msg.size() < 10) {
    LOG_GENERAL(INFO, "Received message '"
                          << (char*)&message->msg.at(0) << "' at port "
                          << message->from.m_listenPortHost << " from address "
                          << message->from.m_ipAddress);
  } else {
    chrono::duration<double, std::milli> time_span =
        chrono::high_resolution_clock::now() - startTime;
    LOG_GENERAL(INFO, "Received " << message->msg.size() / (1024 * 1024)
                                  << " MB message in " << time_span.count()
                                  << " ms");
    LOG_GENERAL(INFO, "Benchmark: " << (1000 * message->msg.size()) /
                                           (time_span.count() * 1024 * 1024)
                                    << " MBps");
  }
}

static bool comparePairSecond(
    const std::pair<zbytes, chrono::time_point<chrono::system_clock>>& a,
    const std::pair<zbytes, chrono::time_point<chrono::system_clock>>& b) {
  return a.second < b.second;
}

void TestRemoveBroadcast() {
  LOG_MARKER();

  static const unsigned int BROADCAST_INTERVAL = 5;
  static const unsigned int BROADCAST_EXPIRY = 10;
  static const unsigned int hashNum = 100000;
  static set<zbytes> broadcastHashes;
  static mutex broadcastHashesMutex;
  static deque<pair<zbytes, chrono::time_point<chrono::system_clock>>>
      broadcastToRemoved;
  static mutex broadcastToRemovedMutex;
  static const chrono::time_point<chrono::system_clock> initTime =
      chrono::system_clock::now();

  LOG_GENERAL(INFO, "Start TestRemoveBroadcast, BROADCAST_INTERVAL = "
                        << BROADCAST_INTERVAL
                        << ", BROADCAST_EXPIRY = " << BROADCAST_EXPIRY
                        << ", hashNum = " << hashNum << ".");

  // Filled in broadcastHashes (hashNum)
  auto FillHash = []() mutable -> void {
    LOG_GENERAL(INFO, "Start to fill broadcastHashes...");

    for (unsigned int i = 0; i < hashNum; ++i) {
      lock_guard<mutex> g(broadcastHashesMutex);
      string hash = to_string(i);
      broadcastHashes.emplace(hash.begin(), hash.end());
    }

    LOG_GENERAL(INFO, "Finished fill " << broadcastHashes.size()
                                       << " broadcastHashes.");
  };

  DetachedFunction(1, FillHash);
  this_thread::sleep_for(chrono::seconds(1));

  // Filled in broadcastToRemoved (hashNum / 2)
  auto FillRemove = []() mutable -> void {
    LOG_GENERAL(INFO, "Start to fill broadcastToRemoved...");
    chrono::time_point<chrono::system_clock> currentTime = initTime;
    for (unsigned int i = 0; i < hashNum; i += 2) {
      lock_guard<mutex> g(broadcastToRemovedMutex);
      string hash = to_string(i);
      zbytes hashS(hash.begin(), hash.end());

      if (i > 0 && 0 == (i % 100)) {
        currentTime += chrono::seconds(1);
      }

      broadcastToRemoved.emplace_back(hashS, currentTime);
    }

    LOG_GENERAL(INFO, "Finished fill " << broadcastToRemoved.size()
                                       << " broadcastToRemoved.");
  };

  DetachedFunction(1, FillRemove);
  this_thread::sleep_for(chrono::seconds(1));

  auto RemoveChecking = []() mutable -> void {
    LOG_GENERAL(INFO, "Start to remove hash, 100 seconds checking...");

    queue<unsigned int> answer;
    answer.push(100000);
    unsigned int cur = 99950;

    for (unsigned int i = 0; i < 19; ++i) {
      answer.push(cur);
      cur -= 250;
    }

    zbytes emptyHash;
    chrono::time_point<chrono::system_clock> currentTime = initTime;

    while (true) {
      this_thread::sleep_for(chrono::seconds(BROADCAST_INTERVAL));
      currentTime += chrono::seconds(BROADCAST_INTERVAL);
      lock(broadcastToRemovedMutex, broadcastHashesMutex);
      lock_guard<mutex> g(broadcastToRemovedMutex, adopt_lock);
      lock_guard<mutex> g2(broadcastHashesMutex, adopt_lock);

      if (broadcastToRemoved.empty() ||
          broadcastToRemoved.front().second >
              currentTime - chrono::seconds(BROADCAST_EXPIRY)) {
        LOG_GENERAL(INFO, "After " << chrono::duration_cast<chrono::seconds>(
                                          currentTime - initTime)
                                          .count()
                                   << " seconds, broadcastHashes size remained "
                                   << broadcastHashes.size());

        LOG_GENERAL(INFO,
                    "Checking " << ((answer.front() == broadcastHashes.size())
                                        ? "PASS!"
                                        : "FAILED!"));
        answer.pop();
        continue;
      }

      auto up = upper_bound(
          broadcastToRemoved.begin(), broadcastToRemoved.end(),
          make_pair(emptyHash, currentTime - chrono::seconds(BROADCAST_EXPIRY)),
          comparePairSecond);

      for (auto it = broadcastToRemoved.begin(); it != up; ++it) {
        broadcastHashes.erase(it->first);
      }

      broadcastToRemoved.erase(broadcastToRemoved.begin(), up);

      LOG_GENERAL(INFO, "After " << chrono::duration_cast<chrono::seconds>(
                                        currentTime - initTime)
                                        .count()
                                 << " seconds, broadcastHashes size reduce to "
                                 << broadcastHashes.size());

      LOG_GENERAL(INFO,
                  "Checking " << ((answer.front() == broadcastHashes.size())
                                      ? "PASS!"
                                      : "FAILED!"));

      answer.pop();
    }
  };

  DetachedFunction(1, RemoveChecking);
  this_thread::sleep_for(chrono::seconds(100));
}

int main() {
  INIT_STDOUT_LOGGER();

  auto func = []() mutable -> void {
    P2PComm::GetInstance().StartMessagePump(process_message);
    P2PComm::GetInstance().EnableListener(33133, false);
  };

  DetachedFunction(1, func);

  this_thread::sleep_for(chrono::seconds(1));  // short delay to prepare socket

  struct in_addr ip_addr {};
  inet_pton(AF_INET, "127.0.0.1", &ip_addr);
  Peer peer = {ip_addr.s_addr, 33133};
  zbytes message1 = {'H', 'e', 'l', 'l', 'o', '\0'};  // Send Hello once

  P2PComm::GetInstance().SendMessage(peer, message1);

  vector<Peer> peers = {peer, peer, peer};
  zbytes message2 = {'W', 'o', 'r', 'l', 'd', '\0'};  // Send World 3x

  P2PComm::GetInstance().SendMessage(peers, message2);

  zbytes longMsg(1024 * 1024 * 1024, 'z');
  longMsg.emplace_back('\0');

  startTime = chrono::high_resolution_clock::now();
  P2PComm::GetInstance().SendMessage(peer, longMsg);

  TestRemoveBroadcast();

  return 0;
}
