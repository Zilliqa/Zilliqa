/*
 * Copyright (C) 2023 Zilliqa
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
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

#include <arpa/inet.h>
#include <cpr/cpr.h>
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/program_options.hpp>
#include "libCrypto/Sha2.h"
#include "libMetrics/Tracing.h"
#include "libNetwork/P2P.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"
#include "libNetwork/P2PMessage.h"

using namespace zil::p2p;
std::chrono::high_resolution_clock::time_point startTime;

#define PB_SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_HARDWARE_SPEC_MISMATCH_EXCEPTION -2
#define ERROR_UNHANDLED_EXCEPTION -3
#define ERROR_IN_CONSTANTS -4

using VectorOfPeer = std::vector<Peer>;

namespace beast = boost::beast;
namespace http = beast::http;

namespace zil::multiplier::utils {

std::vector<std::string> split(const std::string& s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

std::vector<std::string> removeEmptyAddr(
    const std::vector<std::string>& addresses) {
  std::vector<std::string> result;
  for (const std::string& address : addresses) {
    if (!address.empty()) {
      result.push_back(address);
    }
  }
  return result;
}

std::set<std::string> reportDifference(
    const std::vector<std::string>& newAddresses,
    const std::vector<std::string>& oldAddresses,
    const std::set<std::string>& addressStore) {
  std::set<std::string> difference;
  for (const std::string& address : newAddresses) {
    if (std::find(oldAddresses.begin(), oldAddresses.end(), address) ==
            oldAddresses.end() &&
        addressStore.find(address) == addressStore.end()) {
      difference.insert(address);
    }
  }
  return difference;
}

bool fetchDownstreams(const std::string downstreamURL,
                      std::vector<std::string>& mirrorAddresses,
                      std::set<std::string>& addressStore) {
  cpr::Response r = cpr::Get(cpr::Url{downstreamURL});

  if (r.status_code == 200) {
    std::string contents = r.text;
    std::vector<std::string> oldAddresses = mirrorAddresses;
    std::vector<std::string> newAddresses = removeEmptyAddr(split(contents, '\n'));
    std::set<std::string> diffAddresses = reportDifference(newAddresses, oldAddresses, addressStore);
    for (const std::string& address : diffAddresses) {
      mirrorAddresses.push_back(address);
    }
  } else {
    LOG_GENERAL(INFO,"DownstreamURL " << downstreamURL
              << " may not be available at this moment" );
    return false;
  }
  return true;
}

};  // namespace zil::multiplier::utils

class registeredPeers {
 public:
  const VectorOfPeer& getPeers() { return m_peers; }
  void setPeers(std::vector<Peer> newPeers) { m_peers = newPeers; }
  void addPeer(Peer newPeer) { m_peers.push_back(newPeer); }
  void removePeer(Peer oldPeer) {
    for (size_t i = 0; i < m_peers.size(); i++) {
      if (m_peers[i].m_ipAddress == oldPeer.m_ipAddress &&
          m_peers[i].m_listenPortHost == oldPeer.m_listenPortHost) {
        m_peers.erase(m_peers.begin() + i);
      }
    }
  }
  void removePeer(int index) { m_peers.erase(m_peers.begin() + index); }
  void clearPeers() { m_peers.clear(); }
  int size() { return m_peers.size(); }
  Peer getPeer(int index) { return m_peers[index]; }
  void setPeer(int index, Peer newPeer) { m_peers[index] = newPeer; }
  void printPeers() {
    for (size_t i = 0; i < m_peers.size(); i++) {
      std::cout << "Peer " << i << ": " << m_peers[i].m_ipAddress << std::endl;
    }
  }
  std::vector<Peer> m_peers;
};

void process_message(std::shared_ptr<zil::p2p::Message> message,
                     registeredPeers& peers) {
  LOG_MARKER();

  if (message->msg.size() < 10) {
    LOG_GENERAL(INFO, "Received message '"
                          << (char*)&message->msg.at(0) << "' at port "
                          << message->from.m_listenPortHost << " from address "
                          << message->from.m_ipAddress);
  } else {
    std::chrono::duration<double, std::milli> time_span =
        std::chrono::high_resolution_clock::now() - startTime;
    LOG_GENERAL(INFO, "Received " << message->msg.size() / (1024 * 1024)
                                  << " MB message in " << time_span.count()
                                  << " ms");
    LOG_GENERAL(INFO, "Benchmark: " << (1000 * message->msg.size()) /
                                           (time_span.count() * 1024 * 1024)
                                    << " MBps");
  }
  zil::p2p::GetInstance().SendBroadcastMessage(peers.getPeers(), message->msg,false);
}

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  using namespace zil::multiplier::utils;
  Peer my_network_info;

  std::string url;
  std::string logpath(std::filesystem::absolute("./").string());
  registeredPeers our_peers{};
  std::set<std::string> addressStore;
  std::vector<std::string> mirrorAddresses;
  int port;
  std::atomic<bool> execution_continues{true };
  std::mutex lock_addressStore;

  po::options_description desc("Options");
  desc.add_options()("help,h", "Print help messages")(
      "listen,l", po::value<int>(&port)->required()->default_value(30300),
      "Specifies port to bind to")(
      "url,s", po::value<std::string>(&url)->required(),
      "url of list of nodes to poll for connections")(
      "version,v", "Displays the Zilliqa Multiplier version information");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      SWInfo::LogBrandBugReport();
      return PB_SUCCESS;
    }

    if (vm.count("version")) {
      SWInfo::LogBrandBugReport();
      return PB_SUCCESS;
    }

    po::notify(vm);

    if ((port < 0) || (port > 65535)) {
      SWInfo::LogBrandBugReport();
      LOG_GENERAL( INFO, "ERROR: Invalid port" );
      return ERROR_IN_COMMAND_LINE;
    }
    if (url.empty()) {
      SWInfo::LogBrandBugReport();
      LOG_GENERAL( INFO, "ERROR: url empty" );
      return ERROR_IN_COMMAND_LINE;
    }
  } catch (boost::program_options::required_option& e) {
    SWInfo::LogBrandBugReport();
    LOG_GENERAL( INFO, "ERROR: " << e.what() );
    LOG_GENERAL( INFO, "ERROR: " << desc );
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    SWInfo::LogBrandBugReport();
    LOG_GENERAL( INFO, "ERROR: " << e.what() );
    return ERROR_IN_COMMAND_LINE;
  }

  INIT_FILE_LOGGER("asio_multiplier", std::filesystem::current_path());
  LOG_DISPLAY_LEVEL_ABOVE(INFO);


  auto func = [&execution_continues, &our_peers,&port,&lock_addressStore]() mutable -> void {
    boost::asio::io_context ctx(1);
    boost::asio::signal_set sig(ctx, SIGINT, SIGTERM);
    sig.async_wait([&](const boost::system::error_code&, int) {
      ctx.stop();
      execution_continues.store(false);
    });
    auto dispatcher = [&our_peers,&lock_addressStore](std::shared_ptr<zil::p2p::Message> message) {
      lock_addressStore.lock();
      process_message(std::move(message), our_peers);
      lock_addressStore.unlock();
    };
    zil::p2p::GetInstance().StartServer(ctx, port, 0, std::move(dispatcher));
    ctx.run();
  };

  DetachedFunction(1, func);

  while (execution_continues.load()) {
    if (fetchDownstreams(url, mirrorAddresses, addressStore)) {
      for (const std::string& address : mirrorAddresses) {
        std::vector<std::string> address_pair;
        address_pair = split(address, ':');
        if (address_pair.size() != 2) {
          LOG_GENERAL(INFO, "Invalid address: " << address );
          continue;
        }

        if (addressStore.find(address) == addressStore.end()) {
          addressStore.insert(address);
          struct in_addr ip_addr {};
          inet_pton(AF_INET, address_pair[0].c_str(), &ip_addr);
          {
            LOG_GENERAL( INFO, "Updating downstream Addresses: " );
            lock_addressStore.lock();
            our_peers.addPeer({ip_addr.s_addr, static_cast<u_int32_t>(std::stoi(
                                                   address_pair[1]))});
            lock_addressStore.unlock();
          }
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
  return 0;
}
