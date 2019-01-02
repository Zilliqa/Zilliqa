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
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <iostream>

#include "P2PComm.h"
#include "PeerManager.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool PeerManager::ProcessHello(const bytes& message, unsigned int offset,
                               const Peer& from) {
  LOG_MARKER();

  PubKey key;
  uint32_t listenPort = 0;

  if (!Messenger::GetPMHello(message, offset, key, listenPort)) {
    LOG_GENERAL(WARNING, "Messenger::GetPMHello failed.");
    return false;
  }

  Peer peer(from.m_ipAddress, listenPort);

  PeerStore& ps = PeerStore::GetStore();
  ps.AddPeerPair(key, peer);

  LOG_GENERAL(INFO, "Added peer with port " << peer.m_listenPortHost
                                            << " at address "
                                            << from.GetPrintableIPAddress());

  return true;
}

bool PeerManager::ProcessAddPeer(const bytes& message, unsigned int offset,
                                 [[gnu::unused]] const Peer& from) {
  // Message = [32-byte peer key] [4-byte peer ip address] [4-byte peer listen
  // port]

  LOG_MARKER();

  unsigned int message_size = message.size() - offset;

  if (message_size >= (PUB_KEY_SIZE + UINT128_SIZE + sizeof(uint32_t))) {
    // Get and store peer information

    PubKey key;
    // key.Deserialize(message, offset);
    if (key.Deserialize(message, offset) != 0) {
      LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
      return false;
    }

    Peer peer(
        Serializable::GetNumber<uint128_t>(message, offset + PUB_KEY_SIZE,
                                           UINT128_SIZE),
        Serializable::GetNumber<uint32_t>(
            message, offset + PUB_KEY_SIZE + UINT128_SIZE, sizeof(uint32_t)));

    PeerStore& ps = PeerStore::GetStore();
    ps.AddPeerPair(key, peer);

    LOG_GENERAL(INFO, "Added peer with port " << peer.m_listenPortHost
                                              << " at address "
                                              << peer.GetPrintableIPAddress());

    // Say hello

    bytes hello_message = {MessageType::PEER,
                           PeerManager::InstructionType::HELLO};

    if (!Messenger::SetPMHello(hello_message, MessageOffset::BODY, m_selfKey,
                               m_selfPeer.m_listenPortHost)) {
      LOG_GENERAL(WARNING, "Messenger::SetPMHello failed.");
      return false;
    }

    P2PComm::GetInstance().SendMessage(peer, hello_message);

    return true;
  }

  return false;
}

bool PeerManager::ProcessPing(const bytes& message, unsigned int offset,
                              const Peer& from) {
  // Message = [raw byte stream]

  LOG_MARKER();

  LOG_GENERAL(INFO, "Received ping message at " << from.m_listenPortHost
                                                << " from address "
                                                << from.m_ipAddress);

  bytes ping_message(message.begin() + offset, message.end());
  LOG_PAYLOAD(INFO, "Ping message", ping_message, Logger::MAX_BYTES_TO_DISPLAY);
  return true;
}

bool PeerManager::ProcessPingAll(const bytes& message, unsigned int offset,
                                 [[gnu::unused]] const Peer& from) {
  // Message = [raw byte stream]

  LOG_MARKER();

  bytes ping_message = {MessageType::PEER, PeerManager::InstructionType::PING};

  const unsigned int message_size = min(message.size() - offset, (size_t)1024);

  ping_message.resize(ping_message.size() + message_size);
  copy(message.begin() + offset, message.begin() + offset + message_size,
       ping_message.begin() + MessageOffset::BODY);
  P2PComm::GetInstance().SendMessage(PeerStore::GetStore().GetAllPeers(),
                                     ping_message);

  return true;
}

bool PeerManager::ProcessBroadcast(const bytes& message, unsigned int offset,
                                   [[gnu::unused]] const Peer& from) {
  // Message = [raw byte stream]

  LOG_MARKER();

  bytes broadcast_message(message.size() - offset);
  copy(message.begin() + offset, message.end(), broadcast_message.begin());

  LOG_PAYLOAD(INFO, "Broadcast message", broadcast_message,
              Logger::MAX_BYTES_TO_DISPLAY);
  P2PComm::GetInstance().SendBroadcastMessage(GetBroadcastList(0, m_selfPeer),
                                              broadcast_message);

  return true;
}

PeerManager::PeerManager(const std::pair<PrivKey, PubKey>& key,
                         const Peer& peer, bool loadConfig)
    : m_selfKey(key), m_selfPeer(peer) {
  LOG_MARKER();
  SetupLogLevel();

  if (loadConfig) {
    LOG_GENERAL(INFO, "Loading configuration file");

    // Open config file
    ifstream config("config.xml");

    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml(config, pt);

    // Add all peers in config to peer store
    PeerStore& ps = PeerStore::GetStore();

    BOOST_FOREACH (ptree::value_type const& v, pt.get_child("nodes")) {
      if (v.first == "peer") {
        PubKey key(
            DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")), 0);
        struct in_addr ip_addr;
        inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
        Peer peer((uint128_t)ip_addr.s_addr,
                  v.second.get<unsigned int>("port"));
        if (peer != m_selfPeer) {
          ps.AddPeerPair(key, peer);
          LOG_GENERAL(INFO, "Added peer with port "
                                << peer.m_listenPortHost << " at address "
                                << peer.GetPrintableIPAddress());
        }
      }
    }

    config.close();
  }
}

PeerManager::~PeerManager() {}

bool PeerManager::Execute(const bytes& message, unsigned int offset,
                          const Peer& from) {
  LOG_MARKER();

  bool result = false;

  typedef bool (PeerManager::*InstructionHandler)(const bytes&, unsigned int,
                                                  const Peer&);

  InstructionHandler ins_handlers[] = {
      &PeerManager::ProcessHello,     &PeerManager::ProcessAddPeer,
      &PeerManager::ProcessPing,      &PeerManager::ProcessPingAll,
      &PeerManager::ProcessBroadcast,
  };

  const unsigned char ins_byte = message.at(offset);

  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);

    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_GENERAL(WARNING, "Unknown instruction byte "
                             << std::hex << (unsigned int)ins_byte << " from "
                             << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
  }

  return result;
}

vector<Peer> PeerManager::GetBroadcastList(unsigned char ins_type,
                                           const Peer& broadcast_originator) {
  // LOG_MARKER();
  return Broadcastable::GetBroadcastList(ins_type, broadcast_originator);
}

void PeerManager::SetupLogLevel() {
  LOG_MARKER();
  switch (DEBUG_LEVEL) {
    case 1: {
      LOG_DISPLAY_LEVEL_ABOVE(FATAL);
      break;
    }
    case 2: {
      LOG_DISPLAY_LEVEL_ABOVE(WARNING);
      break;
    }
    case 3: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
    case 4: {
      LOG_DISPLAY_LEVEL_ABOVE(DEBUG);
      break;
    }
    default: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
  }
}
