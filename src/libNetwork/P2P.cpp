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

#include "P2P.h"

#include "Blacklist.h"
#include "P2PServer.h"
#include "RumorManager.h"
#include "SendJobs.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

namespace zil::p2p {

P2P& GetInstance() {
  static P2P instance;
  return instance;
}

bool VerifyMessage(const zbytes& message, const Signature& toverify,
                   const PubKey& pubKey) {
  bool result = Schnorr::Verify(message, 0, message.size(), toverify, pubKey);

  if (!result) {
    LOG_GENERAL(INFO, "Failed to verify message. Pubkey: " << pubKey);
  }
  return result;
}

void P2P::SetSelfIdentity(const Peer& selfPeer, const PairOfKey& selfKeys) {
  m_selfPeer = selfPeer;
  m_selfKey = selfKeys;
}

std::optional<Signature> P2P::SignMessage(const zbytes& message) {
  Signature signature;
  if (!m_selfKey || !Schnorr::Sign(message, 0, message.size(), m_selfKey->first,
                                   m_selfKey->second, signature)) {
    return std::nullopt;
  }
  return signature;
}

void P2P::StartServer(AsioContext& asio, uint16_t port, uint16_t additionalPort,
                      Dispatcher dispatcher) {
  LOG_MARKER();

  assert((port > 0 || additionalPort > 0) && dispatcher);
  assert(!m_server && !m_additionalServer);

  if (!dispatcher) {
    throw std::runtime_error("P2P::StartServer: dispatcher is null");
  }

  if (m_server || m_additionalServer) {
    throw std::runtime_error("P2P::StartServer: double start");
  }

  m_dispatcher = std::move(dispatcher);

  size_t maxMsgSize =
      std::max(MAX_GOSSIP_MSG_SIZE_IN_BYTES, MAX_READ_WATERMARK_IN_BYTES);
  if (maxMsgSize == 0) {
    maxMsgSize = 1000000;
  }

  auto dispatchFn = [this](const Peer& from,
                           ReadMessageResult& readResult) -> bool {
    return DispatchMessage(from, readResult);
  };

  if (port) {
    m_server = P2PServer::CreateAndStart(asio, port, maxMsgSize, dispatchFn);
  }

  if (additionalPort) {
    m_additionalServer =
        P2PServer::CreateAndStart(asio, additionalPort, maxMsgSize, dispatchFn);
  }

  if (!m_sendJobs) {
    m_sendJobs = SendJobs::Create();
  }
}

void P2P::InitializeRumorManager(const VectorOfNode& peers,
                                 const std::vector<PubKey>& fullNetworkKeys) {
  LOG_MARKER();

  if (!m_selfPeer || !m_selfKey) {
    LOG_GENERAL(FATAL, "Self peer and keys are not set");
    return;
  }

  m_rumorManager->StopRounds();
  if (m_rumorManager->Initialize(peers, m_selfPeer.value(), m_selfKey.value(),
                                 fullNetworkKeys)) {
    if (peers.size() != 0) {
      m_rumorManager->StartRounds();
    }
    // Spread the buffered rumors
    m_rumorManager->SpreadBufferedRumors();
  }
}

namespace {

template <typename PeerList>
void SendMessageImpl(const std::shared_ptr<SendJobs>& sendJobs,
                     const PeerList& peers, const zbytes& message,
                     unsigned char startByteType,
                     bool bAllowSendToRelaxedBlacklist,
                     bool inject_trace_context) {
  if (message.size() <= MessageOffset::BODY) {
    return;
  }

  if (peers.empty()) {
    LOG_GENERAL(WARNING, "Error: empty peer list");
    return;
  }

  if (!sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }

  static const zbytes no_hash;
  auto raw_msg =
      CreateMessage(message, no_hash, startByteType, inject_trace_context);

  for (const auto& peer : peers) {
    sendJobs->SendMessageToPeer(peer, raw_msg, bAllowSendToRelaxedBlacklist);
  }
}

}  // namespace

/// Multicasts message to specified list of peers.
void P2P::SendMessage(const VectorOfPeer& peers, const zbytes& message,
                      unsigned char startByteType, bool inject_trace_context) {
  SendMessageImpl(m_sendJobs, peers, message, startByteType, false,
                  inject_trace_context);
}

/// Multicasts message to specified list of peers.
void P2P::SendMessage(const std::deque<Peer>& peers, const zbytes& message,
                      unsigned char startByteType, bool inject_trace_context,
                      bool bAllowSendToRelaxedBlacklist) {
  SendMessageImpl(m_sendJobs, peers, message, startByteType,
                  bAllowSendToRelaxedBlacklist, inject_trace_context);
}

/// Sends normal message to specified peer.
void P2P::SendMessage(const Peer& peer, const zbytes& message,
                      unsigned char startByteType, bool inject_trace_context) {
  if (!m_sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }
  if (message.size() <= MessageOffset::BODY) {
    return;
  }
  m_sendJobs->SendMessageToPeer(peer, message, startByteType,
                                inject_trace_context);
}

namespace {

template <typename PeerList>
void SendBroadcastMessageImpl(const std::shared_ptr<SendJobs>& sendJobs,
                              const PeerList& peers,
                              const std::optional<Peer>& selfPeer,
                              const zbytes& message, zbytes& hash,
                              bool inject_trace_context) {
  if (message.size() <= MessageOffset::BODY) {
    return;
  }

  if (peers.empty()) {
    return;
  }

  if (!sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }

  SHA256Calculator sha256;
  sha256.Update(message);
  hash = sha256.Finalize();

  auto raw_msg =
      CreateMessage(message, hash, START_BYTE_BROADCAST, inject_trace_context);

  std::string hashStr;
  if (selfPeer) {
    if (!DataConversion::Uint8VecToHexStr(hash, hashStr)) {
      return;
    }
    LOG_STATE("[BROAD][" << std::setw(15) << std::left
                         << selfPeer->GetPrintableIPAddress() << "]["
                         << hashStr.substr(0, 6) << "] DONE");
  }

  for (const auto& peer : peers) {
    sendJobs->SendMessageToPeer(peer, raw_msg, false);
  }
}

}  // namespace

/// Multicasts message of type=broadcast to specified list of peers.
void P2P::SendBroadcastMessage(const VectorOfPeer& peers, const zbytes& message,
                               bool inject_trace_context) {
  LOG_MARKER();

  zbytes hash;
  SendBroadcastMessageImpl(m_sendJobs, peers, m_selfPeer, message, hash,
                           inject_trace_context);

  if (!hash.empty()) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_broadcastHashes.emplace(std::move(hash));
  }
}

/// Multicasts message of type=broadcast to specified list of peers.
void P2P::SendBroadcastMessage(const std::deque<Peer>& peers,
                               const zbytes& message,
                               bool inject_trace_context) {
  LOG_MARKER();

  zbytes hash;
  SendBroadcastMessageImpl(m_sendJobs, peers, m_selfPeer, message, hash,
                           inject_trace_context);

  if (!hash.empty()) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_broadcastHashes.emplace(std::move(hash));
  }
}

/// Special case for cmd line utilities only - blocking
void P2P::SendMessageNoQueue(const Peer& peer, const zbytes& message,
                             unsigned char startByteType) {
  if (Blacklist::GetInstance().Exist({peer.m_ipAddress,peer.GetListenPortHost(),peer.GetNodeIndentifier()})) {
    LOG_GENERAL(INFO, "The node "
                          << peer
                          << " is in black list, block all message to it.");
    return;
  }

  if (!m_sendJobs) {
    m_sendJobs = SendJobs::Create();
  }
  m_sendJobs->SendMessageToPeerSynchronous(peer, message, startByteType);
}

bool P2P::SpreadRumor(const zbytes& message) {
  LOG_MARKER();
  return m_rumorManager->AddRumor(message);
}

bool P2P::SpreadForeignRumor(const zbytes& message) {
  LOG_MARKER();
  return m_rumorManager->AddForeignRumor(message);
}

void P2P::SendRumorToForeignPeer(const Peer& foreignPeer,
                                 const zbytes& message) {
  LOG_MARKER();
  m_rumorManager->SendRumorToForeignPeer(foreignPeer, message);
}

void P2P::SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                                  const zbytes& message) {
  LOG_MARKER();
  m_rumorManager->SendRumorToForeignPeers(foreignPeers, message);
}

void P2P::SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                                  const zbytes& message) {
  LOG_MARKER();
  m_rumorManager->SendRumorToForeignPeers(foreignPeers, message);
}

void P2P::UpdatePeerInfoInRumorManager(const Peer& peer, const PubKey& pubKey) {
  LOG_MARKER();
  m_rumorManager->UpdatePeerInfo(peer, pubKey);
}

void P2P::BroadcastCleanupJob() {
  auto interval = std::chrono::seconds(std::max(1u, BROADCAST_INTERVAL));
  auto expiryTime = std::chrono::seconds(std::max(1u, BROADCAST_EXPIRY));
  const std::string emptyHash;

  while (!m_stopped) {
    std::unique_lock lk(m_mutex);
    m_condition.wait_for(lk, std::chrono::seconds(interval));
    if (m_stopped) {
      break;
    }

    if (m_broadcastToRemove.empty() ||
        m_broadcastToRemove.front().second >
            std::chrono::steady_clock::now() - expiryTime) {
      continue;
    }

    auto up = upper_bound(
        m_broadcastToRemove.begin(), m_broadcastToRemove.end(),
        make_pair(emptyHash, std::chrono::steady_clock::now() - expiryTime),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    for (auto it = m_broadcastToRemove.begin(); it != up; ++it) {
      m_broadcastHashes.erase(it->first);
    }

    m_broadcastToRemove.erase(m_broadcastToRemove.begin(), up);
  }
}

namespace {

inline std::shared_ptr<Message> MakeMsg(zbytes msg, Peer peer,
                                        uint8_t startByte,
                                        std::string& traceContext) {
  auto r = std::make_shared<Message>();
  r->msg = std::move(msg);
  r->traceContext = std::move(traceContext);
  r->from = std::move(peer);
  r->startByte = startByte;
  return r;
}

constexpr unsigned GOSSIP_MSGTYPE_LEN = 1;
constexpr unsigned GOSSIP_ROUND_LEN = 4;
constexpr unsigned GOSSIP_SNDR_LISTNR_PORT_LEN = 4;

}  // namespace

bool P2P::DispatchMessage(const Peer& from, ReadMessageResult& result) {
  if (result.startByte == START_BYTE_BROADCAST) {
    LOG_PAYLOAD(INFO, "Incoming broadcast " << from, result.message,
                Logger::MAX_BYTES_TO_DISPLAY);

    if (result.hash.empty()) {
      LOG_GENERAL(WARNING,
                  "Hash missing or empty broadcast message (messageLength = "
                      << result.message.size() << ")");
      Blacklist::GetInstance().Add({from.GetIpAddress(),from.GetListenPortHost(),from.GetNodeIndentifier()});
      return false;
    }

    ProcessBroadCastMsg(result.message, result.hash, from, result.traceInfo);
  } else if (result.startByte == START_BYTE_NORMAL) {
    LOG_PAYLOAD(INFO, "Incoming normal " << from, result.message,
                Logger::MAX_BYTES_TO_DISPLAY);

    // Queue the message
    m_dispatcher(MakeMsg(std::move(result.message), from, START_BYTE_NORMAL,
                         result.traceInfo));
  } else if (result.startByte == START_BYTE_GOSSIP) {
    // Check for the maximum gossiped-message size
    if (result.message.size() >= MAX_GOSSIP_MSG_SIZE_IN_BYTES) {
      LOG_GENERAL(WARNING,
                  "Gossip message received [Size:"
                      << result.message.size() << "] is unexpectedly large [ >"
                      << MAX_GOSSIP_MSG_SIZE_IN_BYTES
                      << " ]. Will be strictly blacklisting the sender");
      Blacklist::GetInstance().Add({from.GetIpAddress(),from.GetListenPortHost(),from.GetNodeIndentifier()});  // so we don't spend cost sending any data to this
                              // sender as well.
      return false;
    }
    if (result.message.size() <
        GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + GOSSIP_SNDR_LISTNR_PORT_LEN) {
      LOG_GENERAL(
          WARNING,
          "Gossip Msg Type and/or Gossip Round and/or SNDR LISTNR is missing "
          "(messageLength = "
              << result.message.size() << ")");
      Blacklist::GetInstance().Add({from.GetIpAddress(),from.GetListenPortHost(),from.GetNodeIndentifier()});
      return false;
    }

    ProcessGossipMsg(result.message, from, result.traceInfo);
  } else {
    // Unexpected start byte. Drop this message
    LOG_GENERAL(WARNING, "Incorrect start byte " << result.startByte);
    Blacklist::GetInstance().Add({from.GetIpAddress(),from.GetListenPortHost(),from.GetNodeIndentifier()});
    return false;
  }

  return true;
}

void P2P::ProcessBroadCastMsg(zbytes& message, zbytes& hash, const Peer& from,
                              std::string& traceInfo) {
  // Check if this message has been received before
  bool found = false;
  {
    std::lock_guard<std::mutex> guard(m_mutex);

    found = (m_broadcastHashes.find(hash) != m_broadcastHashes.end());
    // While we have the lock, we should quickly add the hash
    if (!found) {
      SHA256Calculator sha256;
      sha256.Update(message);
      zbytes this_msg_hash = sha256.Finalize();

      if (this_msg_hash == hash) {
        m_broadcastHashes.insert(this_msg_hash);
      } else {
        LOG_GENERAL(WARNING, "Incorrect message hash. Blacklisting peer "
                                 << from.GetPrintableIPAddress());
        Blacklist::GetInstance().Add({from.GetIpAddress(),from.GetListenPortHost(),from.GetNodeIndentifier()});
        return;
      }
    }
  }

  if (found) {
    // We already sent and/or received this message before -> discard
    LOG_GENERAL(DEBUG, "Discarding duplicate");
    return;
  }

  {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_broadcastToRemove.emplace_back(hash, std::chrono::steady_clock::now());
  }

  std::string msgHashStr;
  if (!DataConversion::Uint8VecToHexStr(hash, msgHashStr)) {
    LOG_GENERAL(FATAL, ".");
    return;
  }

  LOG_STATE("[BROAD][" << std::setw(15) << std::left
                       << (m_selfPeer ? m_selfPeer.value() : Peer()) << "]["
                       << msgHashStr.substr(0, 6) << "] RECV");

  // Queue the message
  m_dispatcher(
      MakeMsg(std::move(message), from, START_BYTE_BROADCAST, traceInfo));
}

void P2P::ProcessGossipMsg(zbytes& message, const Peer& from,
                           std::string& traceInfo) {
  unsigned char gossipMsgTyp = message.at(0);

  const uint32_t gossipMsgRound =
      ReadU32BE(message.data() + GOSSIP_MSGTYPE_LEN);

  const uint32_t gossipSenderPort =
      ReadU32BE(message.data() + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN);

  Peer remoteListener(from);
  remoteListener.m_listenPortHost = gossipSenderPort;

  RumorManager::RawBytes rumor_message(message.begin() + GOSSIP_MSGTYPE_LEN +
                                           GOSSIP_ROUND_LEN +
                                           GOSSIP_SNDR_LISTNR_PORT_LEN,
                                       message.end());

  if (gossipMsgTyp == (uint8_t)RRS::Message::Type::FORWARD) {
    LOG_GENERAL(INFO, "Gossip type FORWARD from " << remoteListener);

    if (SpreadForeignRumor(rumor_message)) {
      // skip the keys and signature.
      zbytes tmp(rumor_message.begin() + PUB_KEY_SIZE +
                     SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE,
                 rumor_message.end());

      LOG_GENERAL(INFO, "Rumor size: " << tmp.size());

      // Queue the message
      m_dispatcher(MakeMsg(std::move(tmp), remoteListener, START_BYTE_GOSSIP,
                           traceInfo));
    }
  } else {
    auto resp = m_rumorManager->RumorReceived((unsigned int)gossipMsgTyp,
                                              gossipMsgRound, rumor_message,
                                              remoteListener);
    if (resp.first) {
      LOG_GENERAL(INFO, "Rumor size: " << rumor_message.size());

      // Queue the message
      m_dispatcher(MakeMsg(std::move(resp.second), remoteListener,
                           START_BYTE_GOSSIP, traceInfo));
    }
  }
}

P2P::P2P()
    : m_rumorManager(std::make_shared<RumorManager>()),
      m_broadcastCleanupThread([this] { BroadcastCleanupJob(); }) {}

P2P::~P2P() {
  {
    std::lock_guard lk(m_mutex);
    m_stopped = true;
  }
  m_condition.notify_all();
  m_broadcastCleanupThread.join();
}

}  // namespace zil::p2p
