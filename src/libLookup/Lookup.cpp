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
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <exception>
#include <fstream>
#include <random>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "Lookup.h"
#include "common/Messages.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockChainData/BlockChain.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libServer/GetWorkServer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

Lookup::Lookup(Mediator& mediator, SyncType syncType) : m_mediator(mediator) {
  m_syncType.store(SyncType::NO_SYNC);
  vector<SyncType> ignorable_syncTypes = {NO_SYNC, RECOVERY_ALL_SYNC, DB_VERIF};
  if (syncType >= SYNC_TYPE_COUNT) {
    LOG_GENERAL(FATAL, "Invalid SyncType");
  }
  if (find(ignorable_syncTypes.begin(), ignorable_syncTypes.end(), syncType) ==
      ignorable_syncTypes.end()) {
    m_syncType = syncType;
  }
  m_receivedRaiseStartPoW.store(false);
  SetLookupNodes();
  SetAboveLayer();
  if (LOOKUP_NODE_MODE) {
    SetDSCommitteInfo();
  }
}

Lookup::~Lookup() {}

void Lookup::InitAsNewJoiner() {
  LOG_MARKER();
  m_mediator.m_dsBlockChain.Reset();
  m_mediator.m_txBlockChain.Reset();
  m_mediator.m_blocklinkchain.Reset();
  SetLookupNodes();
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    m_mediator.m_DSCommittee->clear();
  }
  AccountStore::GetInstance().Init();

  Synchronizer tempSyncer;
  tempSyncer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                     m_mediator.m_txBlockChain);
  const auto& dsBlock = m_mediator.m_dsBlockChain.GetBlock(0);
  m_mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                           dsBlock.GetBlockHash());
}

void Lookup::InitSync() {
  LOG_MARKER();
  auto func = [this]() -> void {
    uint64_t dsBlockNum = 0;
    uint64_t txBlockNum = 0;

    // Hack to allow seed server to be restarted so as to get my newlookup ip
    // and register me with multiplier.
    this_thread::sleep_for(chrono::seconds(NEW_LOOKUP_SYNC_DELAY_IN_SECONDS));

    if (m_seedNodes.empty()) {
      SetAboveLayer();  // since may have called CleanVariable earlier
    }

    while (GetSyncType() != SyncType::NO_SYNC) {
      if (m_mediator.m_dsBlockChain.GetBlockCount() != 1) {
        dsBlockNum = m_mediator.m_dsBlockChain.GetBlockCount();
      }
      if (m_mediator.m_txBlockChain.GetBlockCount() != 1) {
        txBlockNum = m_mediator.m_txBlockChain.GetBlockCount();
      }
      LOG_GENERAL(INFO,
                  "TxBlockNum " << txBlockNum << " DSBlockNum: " << dsBlockNum);
      ComposeAndSendGetDirectoryBlocksFromSeed(
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1);
      GetTxBlockFromSeedNodes(txBlockNum, 0);

      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
    // Ask for the sharding structure from lookup
    ComposeAndSendGetShardingStructureFromSeed();
    std::unique_lock<std::mutex> cv_lk(m_mutexShardStruct);
    if (cv_shardStruct.wait_for(
            cv_lk,
            std::chrono::seconds(NEW_LOOKUP_GETSHARD_TIMEOUT_IN_SECONDS)) ==
        std::cv_status::timeout) {
      LOG_GENERAL(WARNING, "Didn't receive sharding structure!");
    } else {
      ProcessEntireShardingStructure();
    }
  };
  DetachedFunction(1, func);
}

void Lookup::SetLookupNodes(const VectorOfNode& lookupNodes) {
  // Only used for random testing
  m_lookupNodes = lookupNodes;
  m_lookupNodesStatic = lookupNodes;
}

void Lookup::SetLookupNodes() {
  LOG_MARKER();

  std::lock_guard<std::mutex> lock(m_mutexLookupNodes);

  m_startedTxnBatchThread = false;
  m_lookupNodes.clear();
  m_lookupNodesOffline.clear();
  // Populate tree structure pt
  using boost::property_tree::ptree;
  ptree pt;
  read_xml("constants.xml", pt);

  const vector<string> lookupTypes = {"node.lookups", "node.multipliers",
                                      "node.lower_seed"};

  uint8_t level = 0;
  vector<Peer> levelAbove;
  for (const auto& lookupType : lookupTypes) {
    for (const ptree::value_type& v : pt.get_child(lookupType)) {
      if (v.first == "peer") {
        struct in_addr ip_addr;
        inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
        Peer lookup_node((uint128_t)ip_addr.s_addr,
                         v.second.get<uint32_t>("port"));
        bytes pubkeyBytes;
        if (!DataConversion::HexStrToUint8Vec(
                v.second.get<std::string>("pubkey"), pubkeyBytes)) {
          continue;
        }
        PubKey pubKey(pubkeyBytes, 0);
        if (pubKey == m_mediator.m_selfKey.second) {
          m_level = level;
        }
        if (find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                    [&pubKey](const PairOfNode& x) {
                      return (pubKey == x.first);
                    }) != m_lookupNodes.end()) {
          continue;
        }
        // check for hostname
        if (lookupType == "node.lookups" || lookupType == "node.multipliers") {
          string url = v.second.get<string>("hostname");
          if (!url.empty()) {
            lookup_node.SetHostname(url);
          }
        }
        if (lookupType == "node.multipliers") {
          m_multipliers.emplace_back(pubKey, lookup_node);
        }
        m_lookupNodes.emplace_back(pubKey, lookup_node);
        LOG_GENERAL(INFO, "Added lookup " << lookup_node);
      }
    }
    level++;
  }

  // Add myself to lookupnodes
  if (m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    const PubKey& myPubKey = m_mediator.m_selfKey.second;
    if (std::find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                     [&myPubKey](const PairOfNode& node) {
                       return node.first == myPubKey;
                     }) == m_lookupNodes.end()) {
      m_lookupNodes.emplace_back(m_mediator.m_selfKey.second,
                                 m_mediator.m_selfPeer);
    }
  }

  m_lookupNodesStatic = m_lookupNodes;
}

void Lookup::SetAboveLayer() {
  using boost::property_tree::ptree;
  ptree pt;
  read_xml("constants.xml", pt);
  m_seedNodes.clear();
  for (const ptree::value_type& v : pt.get_child("node.upper_seed")) {
    if (v.first == "peer") {
      struct in_addr ip_addr;
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      Peer lookup_node((uint128_t)ip_addr.s_addr,
                       v.second.get<uint32_t>("port"));
      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(v.second.get<std::string>("pubkey"),
                                            pubkeyBytes)) {
        continue;
      }

      PubKey pubKey(pubkeyBytes, 0);
      string url = v.second.get<string>("hostname");
      if (!url.empty()) {
        lookup_node.SetHostname(url);
      }
      m_seedNodes.emplace_back(pubKey, lookup_node);
    }
  }
}

VectorOfNode Lookup::GetSeedNodes() const {
  lock_guard<mutex> g(m_mutexSeedNodes);

  return m_seedNodes;
}

std::once_flag generateReceiverOnce;

Address GenOneReceiver() {
  static Address receiverAddr;
  std::call_once(generateReceiverOnce, []() {
    auto receiver = Schnorr::GetInstance().GenKeyPair();
    receiverAddr = Account::GetAddressFromPublicKey(receiver.second);
    LOG_GENERAL(INFO, "Generate testing transaction receiver " << receiverAddr);
  });
  return receiverAddr;
}

Transaction CreateValidTestingTransaction(PrivKey& fromPrivKey,
                                          PubKey& fromPubKey,
                                          const Address& toAddr,
                                          uint128_t amount,
                                          uint64_t prevNonce) {
  unsigned int version = 0;
  auto nonce = prevNonce + 1;

  // LOG_GENERAL("fromPrivKey " << fromPrivKey << " / fromPubKey " << fromPubKey
  // << " / toAddr" << toAddr);

  Transaction txn(version, nonce, toAddr, make_pair(fromPrivKey, fromPubKey),
                  amount, 1, 1, {}, {});

  // bytes buf;
  // txn.SerializeWithoutSignature(buf, 0);

  // Signature sig;
  // Schnorr::GetInstance().Sign(buf, fromPrivKey, fromPubKey, sig);

  // bytes sigBuf;
  // sig.Serialize(sigBuf, 0);
  // txn.SetSignature(sigBuf);

  return txn;
}

bool Lookup::GenTxnToSend(size_t num_txn, vector<Transaction>& txn) {
  vector<Transaction> txns;
  unsigned int NUM_TXN_TO_DS = num_txn / GENESIS_WALLETS.size();

  for (auto& addrStr : GENESIS_WALLETS) {
    bytes tempAddrBytes;
    if (!DataConversion::HexStrToUint8Vec(addrStr, tempAddrBytes)) {
      continue;
    }
    Address addr{tempAddrBytes};

    txns.clear();

    auto account = AccountStore::GetInstance().GetAccount(addr);

    if (!account) {
      LOG_GENERAL(WARNING, "Failed to get genesis account!");
      return false;
    }

    uint64_t nonce = account->GetNonce();

    if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                     num_txn, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns from file");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(txn));

    LOG_GENERAL(INFO, "[Batching] Last Nonce sent "
                          << nonce + num_txn << " of Addr " << addr.hex());
    txns.clear();

    if (!GetTxnFromFile::GetFromFile(addr,
                                     static_cast<uint32_t>(nonce) + num_txn + 1,
                                     NUM_TXN_TO_DS, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns for DS");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(txn));
  }
  return !txn.empty();
}

bool Lookup::GenTxnToSend(size_t num_txn,
                          map<uint32_t, vector<Transaction>>& mp,
                          uint32_t numShards) {
  LOG_MARKER();
  vector<Transaction> txns;

  if (GENESIS_WALLETS.size() == 0) {
    LOG_GENERAL(WARNING, "No genesis accounts found");
    return false;
  }

  if (!USE_REMOTE_TXN_CREATOR) {
    return false;
  }

  unsigned int NUM_TXN_TO_DS = num_txn / GENESIS_WALLETS.size();

  if (numShards == 0) {
    return false;
  }

  for (auto& addrStr : GENESIS_WALLETS) {
    bytes addrBytes;
    if (!DataConversion::HexStrToUint8Vec(addrStr, addrBytes)) {
      continue;
    }
    Address addr{addrBytes};

    auto txnShard = Transaction::GetShardIndex(addr, numShards);
    txns.clear();

    uint64_t nonce = AccountStore::GetInstance().GetAccount(addr)->GetNonce();

    if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                     num_txn, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns from file");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(mp[txnShard]));

    LOG_GENERAL(INFO, "[Batching] Last Nonce sent "
                          << nonce + num_txn << " of Addr " << addr.hex());
    txns.clear();

    if (!GetTxnFromFile::GetFromFile(addr,
                                     static_cast<uint32_t>(nonce) + num_txn + 1,
                                     NUM_TXN_TO_DS, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns for DS");
    }

    copy(txns.begin(), txns.end(), back_inserter(mp[numShards]));
  }

  return true;
}

VectorOfNode Lookup::GetLookupNodes() const {
  LOG_MARKER();
  lock_guard<mutex> lock(m_mutexLookupNodes);
  return m_lookupNodes;
}

VectorOfNode Lookup::GetLookupNodesStatic() const {
  LOG_MARKER();
  lock_guard<mutex> lock(m_mutexLookupNodes);
  return m_lookupNodesStatic;
}

bool Lookup::IsLookupNode(const PubKey& pubKey) const {
  VectorOfNode lookups = GetLookupNodesStatic();
  return std::find_if(lookups.begin(), lookups.end(),
                      [&pubKey](const PairOfNode& node) {
                        return node.first == pubKey;
                      }) != lookups.end();
}

bool Lookup::IsLookupNode(const Peer& peerInfo) const {
  VectorOfNode lookups = GetLookupNodesStatic();
  return std::find_if(lookups.begin(), lookups.end(),
                      [&peerInfo](const PairOfNode& node) {
                        return node.second.GetIpAddress() ==
                               peerInfo.GetIpAddress();
                      }) != lookups.end();
}

uint128_t Lookup::TryGettingResolvedIP(const Peer& peer) const {
  // try resolving ip from hostname
  string url = peer.GetHostname();
  auto resolved_ip = peer.GetIpAddress();  // existing one
  if (!url.empty()) {
    uint128_t tmpIp;
    if (IPConverter::ResolveDNS(url, peer.GetListenPortHost(), tmpIp)) {
      resolved_ip = tmpIp;  // resolved one
    } else {
      LOG_GENERAL(WARNING, "Unable to resolve DNS for " << url);
    }
  }

  return resolved_ip;
}

void Lookup::SendMessageToLookupNodes(const bytes& message) const {
  LOG_MARKER();

  vector<Peer> allLookupNodes;

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    for (const auto& node : m_lookupNodes) {
      auto resolved_ip = TryGettingResolvedIP(node.second);

      Blacklist::GetInstance().Exclude(
          resolved_ip);  // exclude this lookup ip from blacklisting

      Peer tmp(resolved_ip, node.second.GetListenPortHost());
      LOG_GENERAL(INFO, "Sending to lookup " << tmp);

      allLookupNodes.emplace_back(tmp);
    }
  }

  P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void Lookup::SendMessageToLookupNodesSerial(const bytes& message) const {
  LOG_MARKER();

  vector<Peer> allLookupNodes;

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    for (const auto& node : m_lookupNodes) {
      if (find_if(m_multipliers.begin(), m_multipliers.end(),
                  [&node](const PairOfNode& mult) {
                    return node.second == mult.second;
                  }) != m_multipliers.end()) {
        continue;
      }

      auto resolved_ip = TryGettingResolvedIP(node.second);

      Blacklist::GetInstance().Exclude(
          resolved_ip);  // exclude this lookup ip from blacklisting

      Peer tmp(resolved_ip, node.second.GetListenPortHost());
      LOG_GENERAL(INFO, "Sending to lookup " << tmp);

      allLookupNodes.emplace_back(tmp);
    }
  }

  P2PComm::GetInstance().SendMessage(allLookupNodes, message);
}

void Lookup::SendMessageToRandomLookupNode(const bytes& message) const {
  LOG_MARKER();

  // int index = rand() % (NUM_LOOKUP_USE_FOR_SYNC) + m_lookupNodes.size()
  // - NUM_LOOKUP_USE_FOR_SYNC;
  lock_guard<mutex> lock(m_mutexLookupNodes);
  if (0 == m_lookupNodes.size()) {
    LOG_GENERAL(WARNING, "There is no lookup node existed yet!");
    return;
  }

  // To avoid sending message to multiplier
  VectorOfNode tmp;
  std::copy_if(m_lookupNodes.begin(), m_lookupNodes.end(),
               std::back_inserter(tmp), [this](const PairOfNode& node) {
                 return find_if(m_multipliers.begin(), m_multipliers.end(),
                                [&node](const PairOfNode& mult) {
                                  return node.second == mult.second;
                                }) == m_multipliers.end();
               });

  int index = rand() % tmp.size();

  auto resolved_ip = TryGettingResolvedIP(tmp[index].second);

  Blacklist::GetInstance().Exclude(
      resolved_ip);  // exclude this lookup ip from blacklisting
  Peer tmpPeer(resolved_ip, tmp[index].second.GetListenPortHost());
  LOG_GENERAL(INFO, "Sending to Random lookup: " << tmpPeer);
  P2PComm::GetInstance().SendMessage(tmpPeer, message);
}

void Lookup::SendMessageToSeedNodes(const bytes& message) const {
  LOG_MARKER();

  vector<Peer> seedNodePeer;
  {
    lock_guard<mutex> g(m_mutexSeedNodes);

    for (const auto& node : m_seedNodes) {
      auto resolved_ip = TryGettingResolvedIP(node.second);

      Blacklist::GetInstance().Exclude(
          resolved_ip);  // exclude this lookup ip from blacklisting
      Peer tmpPeer(resolved_ip, node.second.GetListenPortHost());
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Sending msg to seed node " << tmpPeer);
      seedNodePeer.emplace_back(tmpPeer);
    }
  }
  P2PComm::GetInstance().SendMessage(seedNodePeer, message);
}

bytes Lookup::ComposeGetDSInfoMessage(bool initialDS) {
  LOG_MARKER();

  bytes getDSNodesMessage = {MessageType::LOOKUP,
                             LookupInstructionType::GETDSINFOFROMSEED};

  if (!Messenger::SetLookupGetDSInfoFromSeed(
          getDSNodesMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost, initialDS)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetDSInfoFromSeed failed.");
    return {};
  }

  return getDSNodesMessage;
}

bytes Lookup::ComposeGetStateMessage() {
  LOG_MARKER();

  bytes getStateMessage = {MessageType::LOOKUP,
                           LookupInstructionType::GETSTATEFROMSEED};

  if (!Messenger::SetLookupGetStateFromSeed(
          getStateMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetStateFromSeed failed.");
    return {};
  }

  return getStateMessage;
}

bool Lookup::GetDSInfoFromSeedNodes() {
  LOG_MARKER();
  SendMessageToRandomSeedNode(ComposeGetDSInfoMessage());
  return true;
}

bool Lookup::GetDSInfoFromLookupNodes(bool initialDS) {
  LOG_MARKER();
  SendMessageToRandomLookupNode(ComposeGetDSInfoMessage(initialDS));
  return true;
}

bool Lookup::GetStateFromSeedNodes() {
  SendMessageToRandomSeedNode(ComposeGetStateMessage());
  return true;
}

bytes Lookup::ComposeGetDSBlockMessage(uint64_t lowBlockNum,
                                       uint64_t highBlockNum) {
  LOG_MARKER();

  bytes getDSBlockMessage = {MessageType::LOOKUP,
                             LookupInstructionType::GETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupGetDSBlockFromSeed(
          getDSBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetDSBlockFromSeed failed.");
    return {};
  }

  return getDSBlockMessage;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::GetDSBlockFromLookupNodes(uint64_t lowBlockNum,
                                       uint64_t highBlockNum) {
  LOG_MARKER();
  SendMessageToRandomLookupNode(
      ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
  return true;
}

bool Lookup::GetDSBlockFromSeedNodes(uint64_t lowBlockNum,
                                     uint64_t highblocknum) {
  SendMessageToRandomSeedNode(
      ComposeGetDSBlockMessage(lowBlockNum, highblocknum));
  return true;
}

bytes Lookup::ComposeGetTxBlockMessage(uint64_t lowBlockNum,
                                       uint64_t highBlockNum) {
  LOG_MARKER();

  bytes getTxBlockMessage = {MessageType::LOOKUP,
                             LookupInstructionType::GETTXBLOCKFROMSEED};

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetTxBlockMessage for blocks " << lowBlockNum << " to "
                                                   << highBlockNum);

  if (!Messenger::SetLookupGetTxBlockFromSeed(
          getTxBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetTxBlockFromSeed failed.");
    return {};
  }

  return getTxBlockMessage;
}

bytes Lookup::ComposeGetStateDeltaMessage(uint64_t blockNum) {
  LOG_MARKER();

  bytes getStateDeltaMessage = {MessageType::LOOKUP,
                                LookupInstructionType::GETSTATEDELTAFROMSEED};

  if (!Messenger::SetLookupGetStateDeltaFromSeed(
          getStateDeltaMessage, MessageOffset::BODY, blockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetStateDeltaFromSeed failed.");
    return {};
  }

  return getStateDeltaMessage;
}

bytes Lookup::ComposeGetStateDeltasMessage(uint64_t lowBlockNum,
                                           uint64_t highBlockNum) {
  LOG_MARKER();

  bytes getStateDeltasMessage = {MessageType::LOOKUP,
                                 LookupInstructionType::GETSTATEDELTASFROMSEED};

  if (!Messenger::SetLookupGetStateDeltasFromSeed(
          getStateDeltasMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetStateDeltasFromSeed failed.");
    return {};
  }

  return getStateDeltasMessage;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::GetTxBlockFromLookupNodes(uint64_t lowBlockNum,
                                       uint64_t highBlockNum) {
  LOG_MARKER();

  SendMessageToRandomLookupNode(
      ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));

  return true;
}

bool Lookup::GetTxBlockFromSeedNodes(uint64_t lowBlockNum,
                                     uint64_t highBlockNum) {
  LOG_MARKER();

  SendMessageToRandomSeedNode(
      ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));

  return true;
}

bool Lookup::GetStateDeltaFromSeedNodes(const uint64_t& blockNum)

{
  LOG_MARKER();
  SendMessageToRandomSeedNode(ComposeGetStateDeltaMessage(blockNum));
  return true;
}

bool Lookup::GetStateDeltasFromSeedNodes(uint64_t lowBlockNum,
                                         uint64_t highBlockNum)

{
  LOG_MARKER();
  SendMessageToRandomSeedNode(
      ComposeGetStateDeltasMessage(lowBlockNum, highBlockNum));
  return true;
}

bool Lookup::SetDSCommitteInfo(bool replaceMyPeerWithDefault) {
  // Populate tree structure pt

  LOG_MARKER();

  using boost::property_tree::ptree;
  ptree pt;
  read_xml("config.xml", pt);

  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  for (ptree::value_type const& v : pt.get_child("nodes")) {
    if (v.first == "peer") {
      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk"),
                                            pubkeyBytes)) {
        continue;
      }
      PubKey key(pubkeyBytes, 0);

      struct in_addr ip_addr;
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      Peer peer((uint128_t)ip_addr.s_addr, v.second.get<unsigned int>("port"));

      if (replaceMyPeerWithDefault && (key == m_mediator.m_selfKey.second)) {
        m_mediator.m_DSCommittee->emplace_back(make_pair(key, Peer()));
        LOG_GENERAL(INFO, "Added self " << Peer());
      } else {
        m_mediator.m_DSCommittee->emplace_back(make_pair(key, peer));
        LOG_GENERAL(INFO, "Added peer " << peer);
      }
    }
  }

  return true;
}

DequeOfShard Lookup::GetShardPeers() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetShardPeers not expected to be called from "
                "other than the LookUp node.");
    return DequeOfShard();
  }

  lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);
  return m_mediator.m_ds->m_shards;
}

vector<Peer> Lookup::GetNodePeers() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetNodePeers not expected to be called from other "
                "than the LookUp node.");
    return vector<Peer>();
  }

  lock_guard<mutex> g(m_mutexNodesInNetwork);
  return m_nodesInNetwork;
}

bool Lookup::ProcessEntireShardingStructure() {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessEntireShardingStructure not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  LOG_GENERAL(INFO, "[LOOKUP received sharding structure]");

  lock(m_mediator.m_ds->m_mutexShards, m_mutexNodesInNetwork);
  lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards, adopt_lock);
  lock_guard<mutex> h(m_mutexNodesInNetwork, adopt_lock);

  m_nodesInNetwork.clear();
  unordered_set<Peer> t_nodesInNetwork;
  unsigned int totalNodeCount = 0;

  for (unsigned int i = 0; i < m_mediator.m_ds->m_shards.size(); i++) {
    unsigned int index = 0;

    totalNodeCount += m_mediator.m_ds->m_shards.at(i).size();
    LOG_STATE("[SHARD " << to_string(i) << "] Num nodes = "
                        << m_mediator.m_ds->m_shards.at(i).size());

    for (const auto& shardNode : m_mediator.m_ds->m_shards.at(i)) {
      const PubKey& key = std::get<SHARD_NODE_PUBKEY>(shardNode);
      const Peer& peer = std::get<SHARD_NODE_PEER>(shardNode);

      m_nodesInNetwork.emplace_back(peer);
      t_nodesInNetwork.emplace(peer);

      LOG_GENERAL(INFO, "[SHARD " << to_string(i) << "] "
                                  << "[PEER " << to_string(index) << "] "
                                  << string(key) << " " << string(peer));

      index++;
    }
  }

  LOG_STATE("[SHARDS] Total num nodes = " << totalNodeCount);

  for (auto& peer : t_nodesInNetwork) {
    if (!l_nodesInNetwork.erase(peer)) {
      LOG_STATE("[JOINPEER]["
                << std::setw(15) << std::left
                << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                << std::setw(6) << std::left << m_mediator.m_currentEpochNum
                << "][" << std::setw(4) << std::left
                << m_mediator.GetNodeMode(peer) << "]" << string(peer));
    }
  }

  for (auto& peer : l_nodesInNetwork) {
    LOG_STATE("[LOSTPEER]["
              << std::setw(15) << std::left
              << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
              << std::setw(6) << std::left << m_mediator.m_currentEpochNum
              << "][" << std::setw(4) << std::left
              << m_mediator.GetNodeMode(peer) << "]" << string(peer));
  }

  l_nodesInNetwork = t_nodesInNetwork;

  return true;
}

bool Lookup::ProcessGetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetDSInfoFromSeed not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint32_t portNo = 0;
  bool initialDS;

  if (!Messenger::GetLookupGetDSInfoFromSeed(message, offset, portNo,
                                             initialDS)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetDSInfoFromSeed failed.");
    return false;
  }

  bytes dsInfoMessage = {MessageType::LOOKUP,
                         LookupInstructionType::SETDSINFOFROMSEED};

  if (initialDS) {
    LOG_GENERAL(WARNING, "[DSINFOVERIF]"
                             << "Recvd call to send initial ds "
                             << " Unsupported");

  }

  else {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

    for (const auto& ds : *m_mediator.m_DSCommittee) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "IP:" << ds.second.GetPrintableIPAddress());
    }

    if (!Messenger::SetLookupSetDSInfoFromSeed(
            dsInfoMessage, MessageOffset::BODY, m_mediator.m_selfKey,
            DSCOMMITTEE_VERSION, *m_mediator.m_DSCommittee, false)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::SetLookupSetDSInfoFromSeed failed.");
      return false;
    }
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  P2PComm::GetInstance().SendMessage(requestingNode, dsInfoMessage);

  return true;
}

void Lookup::SendMessageToRandomSeedNode(const bytes& message) const {
  LOG_MARKER();

  lock_guard<mutex> lock(m_mutexSeedNodes);
  if (0 == m_seedNodes.size()) {
    LOG_GENERAL(WARNING, "Seed nodes are empty");
    return;
  }

  int index = rand() % m_seedNodes.size();
  auto resolved_ip = TryGettingResolvedIP(m_seedNodes[index].second);

  Blacklist::GetInstance().Exclude(
      resolved_ip);  // exclude this lookup ip from blacklisting

  Peer tmpPeer(resolved_ip, m_seedNodes[index].second.GetListenPortHost());
  LOG_GENERAL(INFO, "Sending message to " << tmpPeer);
  P2PComm::GetInstance().SendMessage(tmpPeer, message);
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::ProcessGetDSBlockFromSeed(const bytes& message,
                                       unsigned int offset, const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetDSBlockFromSeed not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetDSBlockFromSeed(message, offset, lowBlockNum,
                                              highBlockNum, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetDSBlockFromSeed failed.");
    return false;
  }

  vector<DSBlock> dsBlocks;
  RetrieveDSBlocks(dsBlocks, lowBlockNum, highBlockNum);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetDSBlockFromSeed requested by " << from << " for blocks "
                                                      << lowBlockNum << " to "
                                                      << highBlockNum);

  bytes dsBlockMessage = {MessageType::LOOKUP,
                          LookupInstructionType::SETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupSetDSBlockFromSeed(
          dsBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfKey, dsBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetDSBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, dsBlockMessage);

  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
void Lookup::RetrieveDSBlocks(vector<DSBlock>& dsBlocks, uint64_t& lowBlockNum,
                              uint64_t& highBlockNum, bool partialRetrieve) {
  lock_guard<mutex> g(m_mediator.m_node->m_mutexDSBlock);

  uint64_t curBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (INIT_BLOCK_NUMBER == curBlockNum) {
    LOG_GENERAL(WARNING,
                "Blockchain is still bootstraping, no ds blocks available.");
    return;
  }

  uint64_t minBlockNum = (curBlockNum > MEAN_GAS_PRICE_DS_NUM)
                             ? (curBlockNum - MEAN_GAS_PRICE_DS_NUM)
                             : 1;

  if (lowBlockNum == 1) {
    lowBlockNum = minBlockNum;
  } else if (lowBlockNum == 0) {
    // give all the blocks in the ds blockchain
    lowBlockNum = 1;
  }

  lowBlockNum = partialRetrieve ? max(minBlockNum, lowBlockNum)
                                : min(minBlockNum, lowBlockNum);

  if (highBlockNum == 0) {
    highBlockNum = curBlockNum;
  }

  uint64_t blockNum;
  for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++) {
    try {
      DSBlock dsblk = m_mediator.m_dsBlockChain.GetBlock(blockNum);
      // TODO
      // Workaround to identify dummy block as == comparator does not work on
      // empty object for DSBlock and DSBlockheader().
      if (dsblk.GetHeader().GetBlockNum() == INIT_BLOCK_NUMBER) {
        LOG_GENERAL(WARNING,
                    "Block Number " << blockNum << " does not exists.");
        break;
      }

      dsBlocks.emplace_back(m_mediator.m_dsBlockChain.GetBlock(blockNum));
    } catch (const char* e) {
      LOG_GENERAL(INFO, "Block Number " << blockNum
                                        << " absent. Didn't include it in "
                                           "response message. Reason: "
                                        << e);
      break;
    }
  }

  // Reset the highBlockNum value if retrieval failed
  if (blockNum != highBlockNum + 1) {
    highBlockNum = blockNum - 1;
  }
}

bool Lookup::ProcessGetStateFromSeed(const bytes& message, unsigned int offset,
                                     const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetStateFromSeed not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStateFromSeed(message, offset, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetStateFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  bytes setStateMessage = {MessageType::LOOKUP,
                           LookupInstructionType::SETSTATEFROMSEED};

  if (!Messenger::SetLookupSetStateFromSeed(
          setStateMessage, MessageOffset::BODY, m_mediator.m_selfKey,
          AccountStore::GetInstance())) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetStateFromSeed failed.");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, setStateMessage);

  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::ProcessGetTxBlockFromSeed(const bytes& message,
                                       unsigned int offset, const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetTxBlockFromSeed not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetTxBlockFromSeed(message, offset, lowBlockNum,
                                              highBlockNum, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetTxBlockFromSeed failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetTxBlockFromSeed requested by " << from << " for blocks "
                                                      << lowBlockNum << " to "
                                                      << highBlockNum);

  vector<TxBlock> txBlocks;
  RetrieveTxBlocks(txBlocks, lowBlockNum, highBlockNum);

  bytes txBlockMessage = {MessageType::LOOKUP,
                          LookupInstructionType::SETTXBLOCKFROMSEED};
  if (!Messenger::SetLookupSetTxBlockFromSeed(
          txBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfKey, txBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetTxBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  P2PComm::GetInstance().SendMessage(requestingNode, txBlockMessage);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Sent Txblks " << lowBlockNum << " - " << highBlockNum);
  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
void Lookup::RetrieveTxBlocks(vector<TxBlock>& txBlocks, uint64_t& lowBlockNum,
                              uint64_t& highBlockNum) {
  lock_guard<mutex> g(m_mediator.m_node->m_mutexFinalBlock);

  if (lowBlockNum == 0) {
    lowBlockNum = 1;
  }

  uint64_t lowestLimitNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
  if (lowBlockNum < lowestLimitNum) {
    LOG_GENERAL(WARNING,
                "Requested number of txBlocks are beyond the current DS epoch "
                "(lowBlockNum :"
                    << lowBlockNum << ", lowestLimitNum : " << lowestLimitNum
                    << ")");
    lowBlockNum = lowestLimitNum;
  }

  if (highBlockNum == 0) {
    highBlockNum =
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  }

  if (INIT_BLOCK_NUMBER == highBlockNum) {
    LOG_GENERAL(WARNING,
                "Blockchain is still bootstraping, no tx blocks available.");
    return;
  }

  uint64_t blockNum;
  for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++) {
    try {
      TxBlock txblk = m_mediator.m_txBlockChain.GetBlock(blockNum);
      // TODO
      // Workaround to identify dummy block as == comparator does not work on
      // empty object for TxBlock and TxBlockheader().
      if (txblk.GetHeader().GetBlockNum() == INIT_BLOCK_NUMBER &&
          txblk.GetHeader().GetDSBlockNum() == INIT_BLOCK_NUMBER) {
        LOG_GENERAL(WARNING,
                    "Block Number " << blockNum << " does not exists.");
        break;
      }
      txBlocks.emplace_back(txblk);
    } catch (const char* e) {
      LOG_GENERAL(INFO, "Block Number " << blockNum
                                        << " absent. Didn't include it in "
                                           "response message. Reason: "
                                        << e);
      break;
    }
  }

  // if serialization got interrupted in between, reset the highBlockNum value
  // in msg
  if (blockNum != highBlockNum + 1) {
    highBlockNum = blockNum - 1;
  }
}

bool Lookup::ProcessGetStateDeltaFromSeed(const bytes& message,
                                          unsigned int offset,
                                          const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessGetStateDeltaFromSeed not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t blockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStateDeltaFromSeed(message, offset, blockNum,
                                                 portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetStateDeltaFromSeed failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetStateDeltaFromSeed requested by "
                << from << " for block " << blockNum);

  bytes stateDelta;

  if (!BlockStorage::GetBlockStorage().GetStateDelta(blockNum, stateDelta)) {
    LOG_GENERAL(INFO, "Block Number "
                          << blockNum
                          << " absent. Didn't include it in response message.");
  }

  bytes stateDeltaMessage = {MessageType::LOOKUP,
                             LookupInstructionType::SETSTATEDELTAFROMSEED};

  if (!Messenger::SetLookupSetStateDeltaFromSeed(
          stateDeltaMessage, MessageOffset::BODY, blockNum,
          m_mediator.m_selfKey, stateDelta)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetStateDeltaFromSeed failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, stateDeltaMessage);
  return true;
}

bool Lookup::ProcessGetStateDeltasFromSeed(const bytes& message,
                                           unsigned int offset,
                                           const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessGetStateDeltasFromSeed not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStateDeltasFromSeed(message, offset, lowBlockNum,
                                                  highBlockNum, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetStateDeltasFromSeed failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetStateDeltasFromSeed requested by "
                << from << " for blocks: " << lowBlockNum << " to "
                << highBlockNum);

  vector<bytes> stateDeltas;
  for (auto i = lowBlockNum; i <= highBlockNum; i++) {
    bytes stateDelta;
    if (!BlockStorage::GetBlockStorage().GetStateDelta(i, stateDelta)) {
      LOG_GENERAL(
          INFO, "Block Number "
                    << i << " absent. Didn't include it in response message.");
    }
    stateDeltas.emplace_back(stateDelta);
  }

  bytes stateDeltasMessage = {MessageType::LOOKUP,
                              LookupInstructionType::SETSTATEDELTASFROMSEED};

  if (!Messenger::SetLookupSetStateDeltasFromSeed(
          stateDeltasMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfKey, stateDeltas)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetStateDeltasFromSeed failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, stateDeltasMessage);
  return true;
}

// Ex-Archival node code
bool Lookup::ProcessGetShardFromSeed([[gnu::unused]] const bytes& message,
                                     [[gnu::unused]] unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetShardsFromSeed(message, offset, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetShardsFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  bytes msg = {MessageType::LOOKUP, LookupInstructionType::SETSHARDSFROMSEED};

  lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);

  if (!Messenger::SetLookupSetShardsFromSeed(
          msg, MessageOffset::BODY, m_mediator.m_selfKey,
          SHARDINGSTRUCTURE_VERSION, m_mediator.m_ds->m_shards)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetShardsFromSeed failed.");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, msg);

  return true;
}

// Ex-Archival node code
bool Lookup::ProcessSetShardFromSeed([[gnu::unused]] const bytes& message,
                                     [[gnu::unused]] unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  DequeOfShard shards;
  PubKey lookupPubKey;
  uint32_t shardingStructureVersion = 0;
  if (!Messenger::GetLookupSetShardsFromSeed(
          message, offset, lookupPubKey, shardingStructureVersion, shards)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetShardsFromSeed failed.");
    return false;
  }

  if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
    LOG_CHECK_FAIL("Sharding structure version", shardingStructureVersion,
                   SHARDINGSTRUCTURE_VERSION);
    return false;
  }

  if (!VerifySenderNode(GetLookupNodesStatic(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_GENERAL(INFO, "Sharding Structure Recvd from " << from);

  uint32_t i = 0;
  for (const auto& shard : shards) {
    LOG_GENERAL(INFO, "Size of shard " << i << " " << shard.size());
    i++;
  }
  lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);

  m_mediator.m_ds->m_shards = move(shards);

  cv_shardStruct.notify_all();

  return true;
}

// UNUSED
bool Lookup::GetShardFromLookup() {
  LOG_MARKER();

  bytes msg = {MessageType::LOOKUP, LookupInstructionType::GETSHARDSFROMSEED};

  if (!Messenger::SetLookupGetShardsFromSeed(
          msg, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetShardsFromSeed failed.");
    return false;
  }

  SendMessageToRandomLookupNode(msg);

  return true;
}

bool Lookup::AddMicroBlockToStorage(const MicroBlock& microblock) {
  TxBlock txblk =
      m_mediator.m_txBlockChain.GetBlock(microblock.GetHeader().GetEpochNum());
  LOG_GENERAL(INFO, "[SendMB]"
                        << "Add MicroBlock hash: "
                        << microblock.GetBlockHash());
  unsigned int i = 0;

  // TODO
  // Workaround to identify dummy block as == comparator does not work on
  // empty object for TxBlock and TxBlockheader().
  // if (txblk == TxBlock()) {
  if (txblk.GetHeader().GetBlockNum() == INIT_BLOCK_NUMBER &&
      txblk.GetHeader().GetDSBlockNum() == INIT_BLOCK_NUMBER) {
    LOG_GENERAL(WARNING, "Failed to fetch Txblock");
    return false;
  }
  for (i = 0; i < txblk.GetMicroBlockInfos().size(); i++) {
    if (txblk.GetMicroBlockInfos().at(i).m_microBlockHash ==
        microblock.GetBlockHash()) {
      break;
    }
  }
  if (i == txblk.GetMicroBlockInfos().size()) {
    LOG_GENERAL(WARNING, "Failed to find mbHash " << microblock.GetBlockHash());
    return false;
  }

  bytes body;
  microblock.Serialize(body, 0);
  if (!BlockStorage::GetBlockStorage().PutMicroBlock(microblock.GetBlockHash(),
                                                     body)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in body");
    return false;
  }

  return true;
}

// Unused code
bool Lookup::ProcessGetMicroBlockFromLookup(
    [[gnu::unused]] const bytes& message, [[gnu::unused]] unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_GENERAL(WARNING, "Function not in used");
  return false;

  // LOG_MARKER();

  // if (!LOOKUP_NODE_MODE) {
  //   LOG_GENERAL(WARNING,
  //               "Function not expected to be called from non-lookup node");
  //   return false;
  // }
  // vector<BlockHash> microBlockHashes;
  // uint32_t portNo = 0;

  // if (!Messenger::GetLookupGetMicroBlockFromLookup(message, offset,
  //                                                  microBlockHashes, portNo))
  //                                                  {
  //   LOG_GENERAL(WARNING, "Failed to process");
  //   return false;
  // }

  // if (microBlockHashes.size() == 0) {
  //   LOG_GENERAL(INFO, "No MicroBlock requested");
  //   return true;
  // }

  // LOG_GENERAL(INFO, "Reques for " << microBlockHashes.size() << " blocks");

  // uint128_t ipAddr = from.m_ipAddress;
  // Peer requestingNode(ipAddr, portNo);
  // vector<MicroBlock> retMicroBlocks;

  // for (const auto& mbhash : microBlockHashes) {
  //   LOG_GENERAL(INFO, "[SendMB]"
  //                         << "Request for microBlockHash " << mbhash);
  //   shared_ptr<MicroBlock> mbptr;
  //   if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbhash, mbptr)) {
  //     LOG_GENERAL(WARNING, "Failed to fetch micro block Hash " << mbhash);
  //     continue;
  //   } else {
  //     retMicroBlocks.push_back(*mbptr);
  //   }
  // }

  // bytes retMsg = {MessageType::LOOKUP,
  //                 LookupInstructionType::SETMICROBLOCKFROMLOOKUP};

  // if (retMicroBlocks.size() == 0) {
  //   LOG_GENERAL(WARNING, "return size 0 for microblocks");
  //   return true;
  // }

  // if (!Messenger::SetLookupSetMicroBlockFromLookup(
  //         retMsg, MessageOffset::BODY, m_mediator.m_selfKey, retMicroBlocks))
  //         {
  //   LOG_GENERAL(WARNING, "Failed to Process ");
  //   return false;
  // }

  // P2PComm::GetInstance().SendMessage(requestingNode, retMsg);
  // return true;
}

// Unused code
bool Lookup::ProcessSetMicroBlockFromLookup(
    [[gnu::unused]] const bytes& message, [[gnu::unused]] unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  //[numberOfMicroBlocks][microblock1][microblock2]...
  LOG_GENERAL(WARNING, "Function not in used");
  return false;

  // vector<MicroBlock> mbs;
  // PubKey lookupPubKey;
  // if (!Messenger::GetLookupSetMicroBlockFromLookup(message, offset,
  //                                                  lookupPubKey, mbs)) {
  //   LOG_GENERAL(WARNING, "Failed to process");
  //   return false;
  // }

  // if (!VerifySenderNode(GetLookupNodes(), lookupPubKey)) {
  //   LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
  //             "The message sender pubkey: "
  //                 << lookupPubKey << " is not in my lookup node list.");
  //   return false;
  // }

  // for (const auto& mb : mbs) {
  //   LOG_GENERAL(INFO, "[SendMB]"
  //                         << " Recvd " << mb.GetHeader().GetEpochNum()
  //                         << " MBHash:" << mb.GetBlockHash());
  // }

  // return true;
}

// UNUSED
void Lookup::SendGetMicroBlockFromLookup(const vector<BlockHash>& mbHashes) {
  bytes msg = {MessageType::LOOKUP,
               LookupInstructionType::GETMICROBLOCKFROMLOOKUP};

  if (mbHashes.size() == 0) {
    LOG_GENERAL(INFO, "No microBlock requested");
    return;
  }

  if (!Messenger::SetLookupGetMicroBlockFromLookup(
          msg, MessageOffset::BODY, mbHashes,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return;
  }

  SendMessageToRandomLookupNode(msg);
}

bool Lookup::ProcessSetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  LOG_MARKER();

  bool initialDS = false;

  PubKey senderPubKey;
  DequeOfNode dsNodes;
  uint32_t dsCommitteeVersion = 0;
  if (!Messenger::GetLookupSetDSInfoFromSeed(message, offset, senderPubKey,
                                             dsCommitteeVersion, dsNodes,
                                             initialDS)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetDSInfoFromSeed failed.");
    return false;
  }

  if (dsCommitteeVersion != DSCOMMITTEE_VERSION) {
    LOG_CHECK_FAIL("DS committee version", dsCommitteeVersion,
                   DSCOMMITTEE_VERSION);
    return false;
  }

  // If first epoch and I'm a lookup
  if ((m_mediator.m_currentEpochNum <= 1) && LOOKUP_NODE_MODE) {
    // Sender must be a DS guard (if in guard mode)
    if (GUARD_MODE && !Guard::GetInstance().IsNodeInDSGuardList(senderPubKey)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "First epoch, and message sender pubkey: "
                    << senderPubKey << " is not in DS guard list.");
      return false;
    }
  }
  // If not first epoch or I'm not a lookup
  else {
    // Sender must be a lookup node
    if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "The message sender pubkey: "
                    << senderPubKey << " is not in my lookup node list.");
      return false;
    }
  }

  if (initialDS && !LOOKUP_NODE_MODE) {
    LOG_GENERAL(INFO, "[DSINFOVERIF]"
                          << "Recvd inital ds config "
                          << "Call Unsupported");
    return false;
  }

  if (m_mediator.m_currentEpochNum == 1 && LOOKUP_NODE_MODE) {
    lock_guard<mutex> h(m_mediator.m_mutexInitialDSCommittee);
    LOG_GENERAL(INFO, "[DSINFOVERIF]"
                          << "Recvd initial ds config");
    if (dsNodes.size() != m_mediator.m_initialDSCommittee->size()) {
      LOG_GENERAL(WARNING, "The initial ds comm recvd and from file differs "
                               << dsNodes.size() << " "
                               << m_mediator.m_initialDSCommittee->size());
    }
    for (unsigned int i = 0; i < dsNodes.size(); i++) {
      if (!(m_mediator.m_initialDSCommittee->at(i) == dsNodes.at(i).first)) {
        LOG_GENERAL(WARNING, "The key from ds comm recvd and from file differs "
                                 << dsNodes.at(i).first << " "
                                 << m_mediator.m_initialDSCommittee->at(i));
      }
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(dsNodes);
  }

  LOG_EPOCH(
      INFO, m_mediator.m_currentEpochNum,
      "SetDSInfoFromSeed from " << from << " for numPeers " << dsNodes.size());

  unsigned int i = 0;
  for (auto& ds : dsNodes) {
    if ((GetSyncType() == SyncType::DS_SYNC ||
         GetSyncType() == SyncType::GUARD_DS_SYNC) &&
        ds.second == m_mediator.m_selfPeer) {
      ds.second = Peer();
    }
    LOG_GENERAL(INFO, "[" << PAD(i++, 3, ' ') << "] " << ds.second);
  }

  if (m_mediator.m_blocklinkchain.GetBuiltDSComm().size() != dsNodes.size()) {
    LOG_GENERAL(
        WARNING, "Size of "
                     << m_mediator.m_blocklinkchain.GetBuiltDSComm().size()
                     << " " << dsNodes.size() << " does not match");
    return false;
  }

  bool isVerif = true;

  for (i = 0; i < m_mediator.m_blocklinkchain.GetBuiltDSComm().size(); i++) {
    if (!(dsNodes.at(i).first ==
          m_mediator.m_blocklinkchain.GetBuiltDSComm().at(i).first)) {
      LOG_GENERAL(WARNING, "Mis-match of ds comm at index " << i);
      isVerif = false;
      break;
    }
  }

  if (isVerif) {
    LOG_GENERAL(INFO, "[DSINFOVERIF] Success");
  }

  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
  *m_mediator.m_DSCommittee = move(dsNodes);

  // Add ds guard to exclude list for lookup at bootstrap
  Guard::GetInstance().AddDSGuardToBlacklistExcludeList(
      *m_mediator.m_DSCommittee);

  //    Data::GetInstance().SetDSPeers(dsPeers);
  //#endif // IS_LOOKUP_NODE

  if ((!LOOKUP_NODE_MODE && m_dsInfoWaitingNotifying &&
       (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)) ||
      (LOOKUP_NODE_MODE && m_syncType == SyncType::NEW_LOOKUP_SYNC &&
       m_dsInfoWaitingNotifying)) {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "Notifying ProcessSetStateFromSeed that DSInfo has been received");
    unique_lock<mutex> lock(m_mutexDSInfoUpdation);
    m_fetchedDSInfo = true;
  }
  cv_dsInfoUpdate.notify_all();
  return true;
}

bool Lookup::ProcessSetDSBlockFromSeed(const bytes& message,
                                       unsigned int offset,
                                       [[gnu::unused]] const Peer& from) {
  // #ifndef IS_LOOKUP_NODE TODO: uncomment later

  LOG_MARKER();

  lock(m_mutexSetDSBlockFromSeed, m_mutexCheckDirBlocks);

  unique_lock<mutex> lock(m_mutexSetDSBlockFromSeed, adopt_lock);
  lock_guard<mutex> g(m_mutexCheckDirBlocks, adopt_lock);

  uint64_t lowBlockNum;
  uint64_t highBlockNum;
  PubKey lookupPubKey;
  std::vector<DSBlock> dsBlocks;
  if (!Messenger::GetLookupSetDSBlockFromSeed(
          message, offset, lowBlockNum, highBlockNum, lookupPubKey, dsBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetDSBlockFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  uint64_t latestSynBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;

  if (latestSynBlockNum > highBlockNum) {
    // TODO: We should get blocks from n nodes.
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I already have the block. latestSynBlockNum="
                  << latestSynBlockNum << " highBlockNum=" << highBlockNum);
  } else {
    if (AlreadyJoinedNetwork()) {
      m_fetchedLatestDSBlock = true;
      cv_latestDSBlock.notify_all();
      return true;
    }
    vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
        dirBlocks;
    for (const auto& dsblock : dsBlocks) {
      if (dsblock.GetHeader().GetBlockNum() < latestSynBlockNum) {
        // skip as already I have them
        continue;
      }
      dirBlocks.emplace_back(dsblock);
    }
    if (m_mediator.m_blocklinkchain.GetBuiltDSComm().size() == 0) {
      LOG_GENERAL(WARNING, "Initial DS comm size 0, it is unset")
      return true;
    }
    uint64_t dsblocknumbefore =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    uint64_t index_num = m_mediator.m_blocklinkchain.GetLatestIndex() + 1;

    DequeOfNode newDScomm;
    if (!m_mediator.m_validator->CheckDirBlocks(
            dirBlocks, m_mediator.m_blocklinkchain.GetBuiltDSComm(), index_num,
            newDScomm)) {
      LOG_GENERAL(WARNING, "Could not verify all DS blocks");
    }
    m_mediator.m_blocklinkchain.SetBuiltDSComm(newDScomm);
    uint64_t dsblocknumafter =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

    LOG_GENERAL(INFO, "DS epoch before" << dsblocknumbefore + 1
                                        << " DS epoch now "
                                        << dsblocknumafter + 1);

    if (dsblocknumbefore < dsblocknumafter) {
      if (m_syncType == SyncType::DS_SYNC ||
          m_syncType == SyncType::LOOKUP_SYNC) {
        if (!m_isFirstLoop) {
          m_currDSExpired = true;
        } else {
          m_isFirstLoop = false;
        }
      }
      m_mediator.UpdateDSBlockRand();
    }
  }

  return true;
}

bool Lookup::ProcessSetTxBlockFromSeed(const bytes& message,
                                       unsigned int offset, const Peer& from) {
  //#ifndef IS_LOOKUP_NODE
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    cv_setTxBlockFromSeed.notify_all();
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetTxBlockFromSeed);

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  std::vector<TxBlock> txBlocks;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetTxBlockFromSeed(
          message, offset, lowBlockNum, highBlockNum, lookupPubKey, txBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetTxBlockFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessSetTxBlockFromSeed sent by " << from << " for blocks "
                                                 << lowBlockNum << " to "
                                                 << highBlockNum);

  // Update GetWork Server info for new nodes not in shards
  if (GETWORK_SERVER_MINE) {
    // roughly calc how many seconds to next PoW
    auto cur_block = std::max(lowBlockNum, highBlockNum);
    auto num_block =
        NUM_FINAL_BLOCK_PER_POW - (cur_block % NUM_FINAL_BLOCK_PER_POW);
    num_block = num_block % NUM_FINAL_BLOCK_PER_POW;
    auto now = std::chrono::system_clock::now();
    auto wait_seconds = chrono::seconds(
        ((TX_DISTRIBUTE_TIME_IN_MS + ANNOUNCEMENT_DELAY_IN_MS) / 1000) *
        num_block);

    GetWorkServer::GetInstance().SetNextPoWTime(now + wait_seconds);
  }

  if (lowBlockNum > highBlockNum) {
    LOG_GENERAL(
        WARNING,
        "The lowBlockNum is higher than highblocknum, maybe DS epoch ongoing");
    cv_setTxBlockFromSeed.notify_all();
    return false;
  }

  if (txBlocks.empty()) {
    LOG_GENERAL(WARNING, "No block actually sent");
    cv_setTxBlockFromSeed.notify_all();
    return false;
  }

  uint64_t latestSynBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;

  if (latestSynBlockNum > highBlockNum) {
    // TODO: We should get blocks from n nodes.
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I already have the block. latestSynBlockNum="
                  << latestSynBlockNum << " highBlockNum=" << highBlockNum);
    return false;
  } else {
    auto res = m_mediator.m_validator->CheckTxBlocks(
        txBlocks, m_mediator.m_blocklinkchain.GetBuiltDSComm(),
        m_mediator.m_blocklinkchain.GetLatestBlockLink());
    switch (res) {
      case ValidatorBase::TxBlockValidationMsg::VALID:
#ifdef SJ_TEST_SJ_TXNBLKS_PROCESS_SLOW
        if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP) {
          LOG_GENERAL(INFO,
                      "Processing txnblks recvd from lookup is slow "
                      "(SJ_TEST_SJ_TXNBLKS_PROCESS_SLOW)");
          this_thread::sleep_for(chrono::seconds(10));
        }
#endif  // SJ_TEST_SJ_TXNBLKS_PROCESS_SLOW
        CommitTxBlocks(txBlocks);
        break;
      case ValidatorBase::TxBlockValidationMsg::INVALID:
        LOG_GENERAL(INFO, "[TxBlockVerif]"
                              << "Invalid blocks");
        break;
      case ValidatorBase::TxBlockValidationMsg::STALEDSINFO:
        LOG_GENERAL(INFO, "[TxBlockVerif]"
                              << "Saved to buffer");
        m_txBlockBuffer.clear();
        for (const auto& txBlock : txBlocks) {
          m_txBlockBuffer.emplace_back(txBlock);
        }
        break;
      default:;
    }
  }

  return true;
}

bool Lookup::GetDSInfo() {
  LOG_MARKER();
  m_dsInfoWaitingNotifying = true;

  GetDSInfoFromSeedNodes();

  {
    unique_lock<mutex> lock(m_mutexDSInfoUpdation);
    while (!m_fetchedDSInfo) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Waiting for DSInfo");

      if (cv_dsInfoUpdate.wait_for(lock,
                                   chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
          std::cv_status::timeout) {
        // timed out
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Timed out waiting for DSInfo");
        m_dsInfoWaitingNotifying = false;
        return false;
      }
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Get ProcessDsInfo Notified");
      m_dsInfoWaitingNotifying = false;
    }
    m_fetchedDSInfo = false;
  }
  return true;
}

void Lookup::PrepareForStartPow() {
  LOG_MARKER();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "At new DS epoch now, already have state. Getting ready to "
            "know for pow");

  if (!GetDSInfo()) {
    return;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "DSInfo received -> Ask lookup to let me know when to "
            "start PoW");

  // Ask lookup to inform me when it's time to do PoW
  bytes getpowsubmission_message = {MessageType::LOOKUP,
                                    LookupInstructionType::GETSTARTPOWFROMSEED};

  if (!Messenger::SetLookupGetStartPoWFromSeed(
          getpowsubmission_message, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost,
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
          m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetStartPoWFromSeed failed.");
    return;
  }

  m_mediator.m_lookup->SendMessageToRandomSeedNode(getpowsubmission_message);
}

void Lookup::CommitTxBlocks(const vector<TxBlock>& txBlocks) {
  LOG_GENERAL(INFO, "[TxBlockVerif]"
                        << "Success");
  uint64_t lowBlockNum = txBlocks.front().GetHeader().GetBlockNum();
  uint64_t highBlockNum = txBlocks.back().GetHeader().GetBlockNum();

  if (m_syncType != SyncType::RECOVERY_ALL_SYNC) {
    unsigned int retry = 1;
    while (retry <= RETRY_GETSTATEDELTAS_COUNT) {
      // Get the state-delta for all txBlocks from random lookup nodes
      GetStateDeltasFromSeedNodes(lowBlockNum, highBlockNum);
      std::unique_lock<std::mutex> cv_lk(m_mutexSetStateDeltaFromSeed);
      if (cv_setStateDeltasFromSeed.wait_for(
              cv_lk, std::chrono::seconds(GETSTATEDELTAS_TIMEOUT_IN_SECONDS)) ==
          std::cv_status::timeout) {
        LOG_GENERAL(
            WARNING,
            "[Retry: " << retry
                       << "] Didn't receive statedeltas! Will try again");
        retry++;
      } else {
        break;
      }
    }
    if (retry > RETRY_GETSTATEDELTAS_COUNT) {
      LOG_GENERAL(WARNING, "Failed to receive state-deltas for txBlks: "
                               << lowBlockNum << "-" << highBlockNum);
      cv_setTxBlockFromSeed.notify_all();
      cv_waitJoined.notify_all();
      return;
    }

    // Check StateRootHash and One in last TxBlk
    if (m_prevStateRootHashTemp !=
        txBlocks.back().GetHeader().GetStateRootHash()) {
      LOG_CHECK_FAIL("State root hash",
                     txBlocks.back().GetHeader().GetStateRootHash(),
                     m_prevStateRootHashTemp);
      return;
    }
  }

  for (const auto& txBlock : txBlocks) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, txBlock);

    m_mediator.m_node->AddBlock(txBlock);
    // Store Tx Block to disk
    bytes serializedTxBlock;
    txBlock.Serialize(serializedTxBlock, 0);
    if (!BlockStorage::GetBlockStorage().PutTxBlock(
            txBlock.GetHeader().GetBlockNum(), serializedTxBlock)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
      return;
    }
  }

  m_mediator.m_currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  // To trigger m_isVacuousEpoch calculation
  m_mediator.IncreaseEpochNum();

  m_mediator.m_consensusID =
      m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW;

  m_mediator.UpdateTxBlockRand();

  if ((m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) &&
      (m_syncType != SyncType::NEW_LOOKUP_SYNC)) {
    if (m_syncType == SyncType::RECOVERY_ALL_SYNC) {
      LOG_EPOCH(
          INFO, m_mediator.m_currentEpochNum,
          "New node - At new DS epoch now, try getting state from lookup");
      GetStateFromSeedNodes();
    } else if (m_syncType == SyncType::NEW_SYNC ||
               m_syncType == SyncType::NORMAL_SYNC) {
      PrepareForStartPow();
    } else if (m_syncType == SyncType::DS_SYNC ||
               m_syncType == SyncType::GUARD_DS_SYNC) {
      if (!m_currDSExpired &&
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum() <
              m_mediator.m_currentEpochNum) {
        m_isFirstLoop = true;
        SetSyncType(SyncType::NO_SYNC);
        m_mediator.m_ds->FinishRejoinAsDS();
      }
      m_currDSExpired = false;
    }
  } else if (m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "New lookup node - Already should have latest state by now.");
    if (GetDSInfo()) {
      if (!m_currDSExpired) {
        SetSyncType(SyncType::NO_SYNC);
        m_isFirstLoop = true;
      }
      m_currDSExpired = false;
    }
  }

  cv_setTxBlockFromSeed.notify_all();
  cv_waitJoined.notify_all();
}

const vector<Transaction>& Lookup::GetTxnFromShardMap(uint32_t index) {
  return m_txnShardMap[index];
}

bool Lookup::ProcessSetStateDeltaFromSeed(const bytes& message,
                                          unsigned int offset,
                                          const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    cv_setStateDeltaFromSeed.notify_all();
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetStateDeltaFromSeed);

  uint64_t blockNum = 0;
  bytes stateDelta;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetStateDeltaFromSeed(message, offset, blockNum,
                                                 lookupPubKey, stateDelta)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetStateDeltaFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessSetStateDeltaFromSeed sent by " << from << " for block "
                                                    << blockNum);

  if (!AccountStore::GetInstance().DeserializeDelta(stateDelta, 0)) {
    LOG_GENERAL(WARNING, "AccountStore::GetInstance().DeserializeDelta failed");
    return false;
  }

  m_mediator.m_ds->SaveCoinbase(
      m_mediator.m_txBlockChain.GetLastBlock().GetB1(),
      m_mediator.m_txBlockChain.GetLastBlock().GetB2(),
      CoinbaseReward::FINALBLOCK_REWARD, m_mediator.m_currentEpochNum);
  cv_setStateDeltaFromSeed.notify_all();
  return true;
}

bool Lookup::ProcessSetStateDeltasFromSeed(const bytes& message,
                                           unsigned int offset,
                                           const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    cv_setStateDeltasFromSeed.notify_all();
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetStateDeltasFromSeed);

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  vector<bytes> stateDeltas;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetStateDeltasFromSeed(message, offset, lowBlockNum,
                                                  highBlockNum, lookupPubKey,
                                                  stateDeltas)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetStateDeltasFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessSetStateDeltasFromSeed sent by "
                << from << " for blocks: " << lowBlockNum << " to "
                << highBlockNum);

  if (stateDeltas.size() != (highBlockNum - lowBlockNum + 1)) {
    LOG_GENERAL(WARNING,
                "StateDeltas recvd:" << stateDeltas.size() << " , Expected: "
                                     << highBlockNum - lowBlockNum + 1);
    return false;
  }

  int txBlkNum = lowBlockNum;
  bytes tmp;
  for (const auto& delta : stateDeltas) {
    // TBD - To verify state delta hash against one from TxBlk.
    // But not crucial right now since we do verify sender i.e lookup and trust
    // it.

    if (!BlockStorage::GetBlockStorage().GetStateDelta(txBlkNum, tmp)) {
      if (!AccountStore::GetInstance().DeserializeDelta(delta, 0)) {
        LOG_GENERAL(WARNING,
                    "AccountStore::GetInstance().DeserializeDelta failed");
        return false;
      }
      if (!BlockStorage::GetBlockStorage().PutStateDelta(txBlkNum, delta)) {
        LOG_GENERAL(WARNING, "BlockStorage::PutStateDelta failed");
        return false;
      }
      m_prevStateRootHashTemp = AccountStore::GetInstance().GetStateRootHash();
    }
    if ((txBlkNum + 1) % NUM_FINAL_BLOCK_PER_POW == 0) {
      if (ENABLE_REPOPULATE && ((txBlkNum + 1) % (NUM_FINAL_BLOCK_PER_POW *
                                                  REPOPULATE_STATE_PER_N_DS) ==
                                REPOPULATE_STATE_IN_DS)) {
        if (!AccountStore::GetInstance().MoveUpdatesToDisk(true)) {
          LOG_GENERAL(WARNING, "AccountStore::MoveUpdatesToDisk(true) failed");
          return false;
        }
      } else if (txBlkNum + NUM_FINAL_BLOCK_PER_POW > highBlockNum) {
        if (!AccountStore::GetInstance().MoveUpdatesToDisk(false)) {
          LOG_GENERAL(WARNING, "AccountStore::MoveUpdatesToDisk(false) failed");
          return false;
        }
      }
    }
    txBlkNum++;
  }

  cv_setStateDeltasFromSeed.notify_all();
  return true;
}

bool Lookup::ProcessSetStateFromSeed(const bytes& message, unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetState);
  PubKey lookupPubKey;
  bytes accountStoreBytes;
  if (!Messenger::GetLookupSetStateFromSeed(message, offset, lookupPubKey,
                                            accountStoreBytes)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetStateFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  if (!AccountStore::GetInstance().Deserialize(accountStoreBytes, 0)) {
    LOG_GENERAL(WARNING, "Deserialize AccountStore Failed");
    return false;
  }

  if (!LOOKUP_NODE_MODE) {
    if (m_syncType == SyncType::NEW_SYNC ||
        m_syncType == SyncType::NORMAL_SYNC) {
      m_dsInfoWaitingNotifying = true;

      GetDSInfoFromSeedNodes();

      {
        unique_lock<mutex> lock(m_mutexDSInfoUpdation);
        while (!m_fetchedDSInfo) {
          LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Waiting for DSInfo");

          if (cv_dsInfoUpdate.wait_for(
                  lock, chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
              std::cv_status::timeout) {
            // timed out
            LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                      "Timed out waiting for DSInfo");
            m_dsInfoWaitingNotifying = false;
            return false;
          }
          LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                    "Get ProcessDsInfo Notified");
          m_dsInfoWaitingNotifying = false;
        }
        m_fetchedDSInfo = false;
      }

      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "DSInfo received -> Ask lookup to let me know when to "
                "start PoW");

      // Ask lookup to inform me when it's time to do PoW
      bytes getpowsubmission_message = {
          MessageType::LOOKUP, LookupInstructionType::GETSTARTPOWFROMSEED};

      if (!Messenger::SetLookupGetStartPoWFromSeed(
              getpowsubmission_message, MessageOffset::BODY,
              m_mediator.m_selfPeer.m_listenPortHost,
              m_mediator.m_dsBlockChain.GetLastBlock()
                  .GetHeader()
                  .GetBlockNum(),
              m_mediator.m_selfKey)) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Messenger::SetLookupGetStartPoWFromSeed failed.");
        return false;
      }

      m_mediator.m_lookup->SendMessageToRandomSeedNode(
          getpowsubmission_message);
    } else if (m_syncType == SyncType::DS_SYNC ||
               m_syncType == SyncType::GUARD_DS_SYNC) {
      if (!m_currDSExpired &&
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum() <
              m_mediator.m_currentEpochNum) {
        m_isFirstLoop = true;
        SetSyncType(SyncType::NO_SYNC);
        m_mediator.m_ds->FinishRejoinAsDS();
      }

      m_currDSExpired = false;
    }
  } else if (m_syncType == SyncType::LOOKUP_SYNC) {
    if (!m_currDSExpired) {
      if (FinishRejoinAsLookup()) {
        SetSyncType(SyncType::NO_SYNC);
      }
    }
    m_currDSExpired = false;
  } else if (LOOKUP_NODE_MODE && m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    m_dsInfoWaitingNotifying = true;

    GetDSInfoFromSeedNodes();

    {
      unique_lock<mutex> lock(m_mutexDSInfoUpdation);
      while (!m_fetchedDSInfo) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Waiting for DSInfo");

        if (cv_dsInfoUpdate.wait_for(lock,
                                     chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
            std::cv_status::timeout) {
          // timed out
          LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                    "Timed out waiting for DSInfo");
          m_dsInfoWaitingNotifying = false;
          return false;
        }
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Get ProcessDsInfo Notified");
        m_dsInfoWaitingNotifying = false;
      }
      m_fetchedDSInfo = false;
    }

    if (!m_currDSExpired) {
      SetSyncType(SyncType::NO_SYNC);
      m_isFirstLoop = true;
    }
    m_currDSExpired = false;
  }

  return true;
}

// Ex-Archival code
bool Lookup::ProcessGetTxnsFromLookup([[gnu::unused]] const bytes& message,
                                      [[gnu::unused]] unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_GENERAL(WARNING, "Function not in used");
  return false;

  // vector<TxnHash> txnhashes;
  // txnhashes.clear();

  // uint32_t portNo = 0;
  // if (!Messenger::GetLookupGetTxnsFromLookup(message, offset, txnhashes,
  //                                            portNo)) {
  //   LOG_GENERAL(WARNING, "Failed to Process");
  //   return false;
  // }

  // if (txnhashes.size() == 0) {
  //   LOG_GENERAL(INFO, "No txn requested");
  //   return true;
  // }

  // vector<TransactionWithReceipt> txnvector;
  // for (const auto& txnhash : txnhashes) {
  //   shared_ptr<TransactionWithReceipt> txn;
  //   if (!BlockStorage::GetBlockStorage().GetTxBody(txnhash, txn)) {
  //     LOG_GENERAL(WARNING, "Could not find " << txnhash);
  //     continue;
  //   }
  //   txnvector.emplace_back(*txn);
  // }
  // uint128_t ipAddr = from.m_ipAddress;
  // Peer requestingNode(ipAddr, portNo);

  // bytes setTxnMsg = {MessageType::LOOKUP,
  //                    LookupInstructionType::SETTXNFROMLOOKUP};

  // if (!Messenger::SetLookupSetTxnsFromLookup(setTxnMsg, MessageOffset::BODY,
  //                                            m_mediator.m_selfKey,
  //                                            txnvector)) {
  //   LOG_GENERAL(WARNING, "Unable to Process");
  //   return false;
  // }

  // P2PComm::GetInstance().SendMessage(requestingNode, setTxnMsg);

  // return true;
}

// Ex archival code
bool Lookup::ProcessSetTxnsFromLookup([[gnu::unused]] const bytes& message,
                                      [[gnu::unused]] unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_GENERAL(WARNING, "Function not in used");
  return false;
  // vector<TransactionWithReceipt> txns;
  // PubKey lookupPubKey;

  // if (!Messenger::GetLookupSetTxnsFromLookup(message, offset, lookupPubKey,
  //                                            txns)) {
  //   LOG_GENERAL(WARNING, "Failed to Process");
  //   return false;
  // }

  // if (!VerifySenderNode(GetLookupNodes(), lookupPubKey)) {
  //   LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
  //             "The message sender pubkey: "
  //                 << lookupPubKey << " is not in my lookup node list.");
  //   return false;
  // }

  // return true;
}

// UNUSED
void Lookup::SendGetTxnFromLookup(const vector<TxnHash>& txnhashes) {
  bytes msg = {MessageType::LOOKUP, LookupInstructionType::GETTXNFROMLOOKUP};

  if (txnhashes.size() == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return;
  }

  if (!Messenger::SetLookupGetTxnsFromLookup(
          msg, MessageOffset::BODY, txnhashes,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return;
  }

  SendMessageToRandomLookupNode(msg);
}

bool Lookup::CheckStateRoot() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::CheckStateRoot not expected to be called from "
                "LookUp node.");
    return true;
  }

  StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();
  StateHash rootInFinalBlock =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetStateRootHash();

  if (stateRoot == rootInFinalBlock) {
    LOG_GENERAL(INFO, "CheckStateRoot match");
    return true;
  } else {
    LOG_GENERAL(WARNING, "State root doesn't match. Calculated = "
                             << stateRoot << ". "
                             << "StoredInBlock = " << rootInFinalBlock);

    return false;
  }
}

bool Lookup::InitMining(uint32_t lookupIndex) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::InitMining not expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  // General check
  if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW != 0) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "New DS epoch check failed");
    return false;
  }

  uint64_t curDsBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  m_mediator.UpdateDSBlockRand();
  auto dsBlockRand = m_mediator.m_dsBlockRand;
  array<unsigned char, 32> txBlockRand{};

  // state root could be changed after repopulating states. so check is moved
  // before repopulating state in CommitTxBlocks. if (CheckStateRoot()) {
  // Attempt PoW
  m_startedPoW = true;
  dsBlockRand = m_mediator.m_dsBlockRand;
  txBlockRand = m_mediator.m_txBlockRand;

  m_mediator.m_node->SetState(Node::POW_SUBMISSION);
  POW::GetInstance().EthashConfigureClient(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
      FULL_DATASET_MINE);

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Starting PoW for new ds block number " << curDsBlockNum + 1);

  m_mediator.m_node->StartPoW(
      curDsBlockNum + 1,
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty(),
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty(),
      dsBlockRand, txBlockRand, lookupIndex);
  //} else {
  //  LOG_GENERAL(WARNING, "State root check failed");
  //  return false;
  //}

  uint64_t lastTxBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  unique_lock<mutex> lk(m_mutexCVJoined);
  cv_waitJoined.wait(lk);

  m_startedPoW = false;

  if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() >
      lastTxBlockNum) {
    if (GetSyncType() != SyncType::NO_SYNC) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Not yet connected to network");

      m_mediator.m_node->SetState(Node::SYNC);
    }
  } else {
    Guard::GetInstance().AddDSGuardToBlacklistExcludeList(
        *m_mediator.m_DSCommittee);
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "I have successfully join the network");
  }

  return true;
}

bool Lookup::ProcessSetLookupOffline(const bytes& message, unsigned int offset,
                                     const Peer& from) {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetLookupOffline not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  uint8_t msgType = 0;
  uint32_t portNo = 0;
  PubKey lookuppubkey;
  if (!Messenger::GetLookupSetLookupOffline(message, offset, msgType, portNo,
                                            lookuppubkey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetLookupOffline failed.");
    return false;
  }

  if ((unsigned char)msgType != LookupInstructionType::SETLOOKUPOFFLINE) {
    LOG_GENERAL(WARNING,
                "Current message does not belong to this instrunction handler. "
                "There might be replay attack.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    auto iter = std::find_if(
        m_lookupNodes.begin(), m_lookupNodes.end(),
        [&lookuppubkey, &requestingNode](const PairOfNode& node) {
          return (node.first == lookuppubkey && node.second == requestingNode);
        });
    if (iter != m_lookupNodes.end()) {
      m_lookupNodesOffline.emplace_back(*iter);
      m_lookupNodes.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "The Peer Info or pubkey is not in m_lookupNodes");
      return false;
    }
  }
  return true;
}

bool Lookup::ProcessSetLookupOnline(const bytes& message, unsigned int offset,
                                    const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetLookupOnline not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  uint8_t msgType = 0;
  uint32_t portNo = 0;
  PubKey lookupPubKey;
  if (!Messenger::GetLookupSetLookupOnline(message, offset, msgType, portNo,
                                           lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetLookupOnline failed.");
    return false;
  }

  if ((unsigned char)msgType != LookupInstructionType::SETLOOKUPONLINE) {
    LOG_GENERAL(WARNING,
                "Current message does not belong to this instrunction handler. "
                "There might be replay attack.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    auto iter = std::find_if(
        m_lookupNodesOffline.cbegin(), m_lookupNodesOffline.cend(),
        [&lookupPubKey, &requestingNode](const PairOfNode& node) {
          return (node.first == lookupPubKey && node.second == requestingNode);
        });
    if (iter != m_lookupNodesOffline.end()) {
      m_lookupNodes.emplace_back(*iter);
      m_lookupNodesOffline.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "The Peer Info is not in m_lookupNodesOffline");
      return false;
    }
  }
  return true;
}

bool Lookup::ProcessGetOfflineLookups(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetOfflineLookups not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetOfflineLookups(message, offset, portNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetOfflineLookups failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  LOG_GENERAL(INFO, requestingNode);

  bytes offlineLookupsMessage = {MessageType::LOOKUP,
                                 LookupInstructionType::SETOFFLINELOOKUPS};

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    std::vector<Peer> lookupNodesOffline;
    for (const auto& pairPubKeyPeer : m_lookupNodesOffline)
      lookupNodesOffline.push_back(pairPubKeyPeer.second);

    if (!Messenger::SetLookupSetOfflineLookups(
            offlineLookupsMessage, MessageOffset::BODY, m_mediator.m_selfKey,
            lookupNodesOffline)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::SetLookupSetOfflineLookups failed.");
      return false;
    }

    for (const auto& peer : m_lookupNodesOffline) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "IP:" << peer.second.GetPrintableIPAddress());
    }
  }

  P2PComm::GetInstance().SendMessage(requestingNode, offlineLookupsMessage);
  return true;
}

bool Lookup::ProcessSetOfflineLookups(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetOfflineLookups not expected to be "
                "called from the LookUp node.");
    return true;
  }

  vector<Peer> nodes;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetOfflineLookups(message, offset, lookupPubKey,
                                             nodes)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetOfflineLookups failed.");
    return false;
  }

  if (!VerifySenderNode(GetLookupNodesStatic(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessSetOfflineLookups sent by "
                << from << " for numOfflineLookups " << nodes.size());

  unsigned int i = 0;
  for (const auto& peer : nodes) {
    std::lock_guard<std::mutex> lock(m_mutexLookupNodes);
    // Remove selfPeerInfo from m_lookupNodes
    auto iter = std::find_if(
        m_lookupNodes.begin(), m_lookupNodes.end(),
        [&peer](const PairOfNode& node) { return node.second == peer; });
    if (iter != m_lookupNodes.end()) {
      m_lookupNodesOffline.emplace_back(*iter);
      m_lookupNodes.erase(iter);

      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "ProcessSetOfflineLookups recvd offline lookup " << i << ": "
                                                                 << peer);
    }

    i++;
  }

  {
    unique_lock<mutex> lock(m_mutexOfflineLookupsUpdation);
    m_fetchedOfflineLookups = true;
    cv_offlineLookups.notify_all();
  }
  return true;
}

bool Lookup::ProcessRaiseStartPoW(const bytes& message, unsigned int offset,
                                  [[gnu::unused]] const Peer& from) {
  // Message = empty

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessRaiseStartPoW not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  if (m_receivedRaiseStartPoW.load()) {
    LOG_GENERAL(WARNING, "Already raised start pow");
    return false;
  }

  uint8_t msgType;
  uint64_t blockNumber;
  PubKey dspubkey;
  if (!Messenger::GetLookupSetRaiseStartPoW(message, offset, msgType,
                                            blockNumber, dspubkey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetRaiseStartPoW failed.");
    return false;
  }

  if ((unsigned char)msgType != LookupInstructionType::RAISESTARTPOW) {
    LOG_GENERAL(WARNING,
                "Current message does not belong to this instrunction handler. "
                "There might be replay attack.");
    return false;
  }

  if (blockNumber != m_mediator.m_currentEpochNum &&
      blockNumber != m_mediator.m_currentEpochNum + 1) {
    LOG_GENERAL(WARNING, "block num is not within the current epoch.");
    return false;
  }

  PairOfNode expectedDSLeader;
  if (!Node::GetDSLeader(m_mediator.m_blocklinkchain.GetLatestBlockLink(),
                         m_mediator.m_dsBlockChain.GetLastBlock(),
                         *m_mediator.m_DSCommittee, expectedDSLeader)) {
    LOG_GENERAL(WARNING, "Does not know expected ds leader");
    return false;
  }

  if (!(expectedDSLeader.first == dspubkey)) {
    LOG_GENERAL(WARNING, "Message does not comes from DS leader");
    return false;
  }

  // DS leader has informed me that it's time to start PoW
  m_receivedRaiseStartPoW.store(true);
  cv_startPoWSubmission.notify_all();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Threads running ProcessGetStartPoWFromSeed notified to start PoW");

  // Sleep for a while, then let all remaining threads running
  // ProcessGetStartPoWFromSeed know that it's too late to do PoW Sleep time =
  // time it takes for new node to try getting DSInfo + actual PoW window
  this_thread::sleep_for(
      chrono::seconds(NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                      POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
  m_receivedRaiseStartPoW.store(false);
  cv_startPoWSubmission.notify_all();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Threads running ProcessGetStartPoWFromSeed notified it's too "
            "late to start PoW");

  return true;
}

bool Lookup::ProcessGetStartPoWFromSeed(const bytes& message,
                                        unsigned int offset, const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetStartPoWFromSeed not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;
  uint64_t blockNumber = 0;

  if (!Messenger::GetLookupGetStartPoWFromSeed(message, offset, portNo,
                                               blockNumber)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetStartPoWFromSeed failed.");
    return false;
  }

  if (blockNumber !=
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "DS block " << blockNumber
                    << " in GetStartPoWFromSeed not equal to current DS block "
                    << m_mediator.m_dsBlockChain.GetLastBlock()
                           .GetHeader()
                           .GetBlockNum());
    return false;
  }

  // Normally I'll get this message from new nodes at the vacuous epoch
  // Wait a while if I haven't received RAISESTARTPOW from DS leader yet
  // Wait time = time it takes to finish the vacuous epoch (or at least part of
  // it) + actual PoW window
  if (!m_receivedRaiseStartPoW.load()) {
    std::unique_lock<std::mutex> cv_lk(m_MutexCVStartPoWSubmission);

    if (cv_startPoWSubmission.wait_for(
            cv_lk,
            std::chrono::seconds(POW_WINDOW_IN_SECONDS +
                                 POWPACKETSUBMISSION_WINDOW_IN_SECONDS)) ==
        std::cv_status::timeout) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Timed out waiting for DS leader to raise startPoW");
      return false;
    }

    if (!m_receivedRaiseStartPoW.load()) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "PoW duration already passed");
      return false;
    }
  }

  // Tell the new node that it's time to start PoW
  bytes setstartpow_message = {MessageType::LOOKUP,
                               LookupInstructionType::SETSTARTPOWFROMSEED};
  if (!Messenger::SetLookupSetStartPoWFromSeed(
          setstartpow_message, MessageOffset::BODY,
          m_mediator.m_currentEpochNum, m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetStartPoWFromSeed failed.");
    return false;
  }
  P2PComm::GetInstance().SendMessage(Peer(from.m_ipAddress, portNo),
                                     setstartpow_message);

  return true;
}

bool Lookup::ProcessSetStartPoWFromSeed([[gnu::unused]] const bytes& message,
                                        [[gnu::unused]] unsigned int offset,
                                        [[gnu::unused]] const Peer& from) {
  // Message = empty

  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetStartPoWFromSeed not expected to be "
                "called from the LookUp node.");
    return true;
  }

  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetStartPoWFromSeed(message, offset, lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetStartPoWFromSeed failed.");
    return false;
  }

  auto vecLookupNodes = GetSeedNodes();
  auto it = std::find_if(vecLookupNodes.cbegin(), vecLookupNodes.cend(),
                         [&lookupPubKey](const PairOfNode& node) {
                           return node.first == lookupPubKey;
                         });
  uint32_t index;
  if (it != vecLookupNodes.cend()) {
    index = distance(vecLookupNodes.cbegin(), it);
  } else {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  InitMining(index);
  return true;
}

void Lookup::StartSynchronization() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::StartSynchronization not expected to be called "
                "from other than the LookUp node.");
    return;
  }

  LOG_MARKER();

  this->CleanVariables();

  auto func = [this]() -> void {
    GetMyLookupOffline();
    GetDSInfoFromLookupNodes();
    while (GetSyncType() != SyncType::NO_SYNC) {
      GetDSBlockFromLookupNodes(m_mediator.m_dsBlockChain.GetBlockCount(), 0);
      GetTxBlockFromLookupNodes(m_mediator.m_txBlockChain.GetBlockCount(), 0);
      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
  };
  DetachedFunction(1, func);
}

bool Lookup::GetDSInfoLoop() {
  unsigned int counter = 0;
  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (m_mediator.m_DSCommittee->size() > 0) {
      LOG_GENERAL(WARNING,
                  "DS comm already set, make sure you cleaned variables");
      return false;
    }
  }

  while (counter <= FETCH_LOOKUP_MSG_MAX_RETRY) {
    GetDSInfoFromSeedNodes();
    unique_lock<mutex> lk(m_mutexDSInfoUpdation);
    if (cv_dsInfoUpdate.wait_for(lk, chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
        cv_status::timeout) {
      counter++;

    } else {
      break;
    }
  }
  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (m_mediator.m_DSCommittee->size() > 0) {
      return true;
    } else {
      LOG_GENERAL(WARNING, "ds committee still unset");
      return false;
    }
  }

  LOG_GENERAL(WARNING, "Exceeded max tries " << counter << "/"
                                             << FETCH_LOOKUP_MSG_MAX_RETRY);
  return false;
}

bytes Lookup::ComposeGetLookupOfflineMessage() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetLookupOfflineMessage not expected to be "
                "called from other than the LookUp node.");
    return bytes();
  }

  LOG_MARKER();

  bytes getLookupOfflineMessage = {MessageType::LOOKUP,
                                   LookupInstructionType::SETLOOKUPOFFLINE};

  if (!Messenger::SetLookupSetLookupOffline(
          getLookupOfflineMessage, MessageOffset::BODY,
          (uint8_t)LookupInstructionType::SETLOOKUPOFFLINE,
          m_mediator.m_selfPeer.m_listenPortHost, m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetLookupOffline failed.");
    return {};
  }

  return getLookupOfflineMessage;
}

bytes Lookup::ComposeGetLookupOnlineMessage() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetLookupOnlineMessage not expected to be "
                "called from other than the LookUp node.");
    return bytes();
  }

  LOG_MARKER();

  bytes getLookupOnlineMessage = {MessageType::LOOKUP,
                                  LookupInstructionType::SETLOOKUPONLINE};

  if (!Messenger::SetLookupSetLookupOnline(
          getLookupOnlineMessage, MessageOffset::BODY,
          (uint8_t)LookupInstructionType::SETLOOKUPONLINE,
          m_mediator.m_selfPeer.m_listenPortHost, m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetLookupOnline failed.");
    return {};
  }

  return getLookupOnlineMessage;
}

bool Lookup::GetMyLookupOffline() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetMyLookupOffline not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  {
    std::lock_guard<std::mutex> lock(m_mutexLookupNodes);
    // Remove selfPeerInfo from m_lookupNodes
    auto selfPeer(m_mediator.m_selfPeer);
    auto selfpubkey(m_mediator.m_selfKey.second);
    auto iter = std::find_if(
        m_lookupNodes.begin(), m_lookupNodes.end(),
        [&selfPeer, &selfpubkey](const PairOfNode& node) {
          return (node.first == selfpubkey && node.second == selfPeer);
        });
    if (iter != m_lookupNodes.end()) {
      m_lookupNodesOffline.emplace_back(*iter);
      m_lookupNodes.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "My Peer Info is not in m_lookupNodes");
      return false;
    }
  }

  SendMessageToLookupNodesSerial(ComposeGetLookupOfflineMessage());
  return true;
}

bool Lookup::GetMyLookupOnline(bool fromRecovery) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetMyLookupOnline not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  LOG_MARKER();
  bool found = false;

  if (!fromRecovery) {
    std::lock_guard<std::mutex> lock(m_mutexLookupNodes);
    auto selfPeer(m_mediator.m_selfPeer);
    auto selfPubkey(m_mediator.m_selfKey.second);
    auto iter = std::find_if(
        m_lookupNodesOffline.begin(), m_lookupNodesOffline.end(),
        [&selfPeer, &selfPubkey](const PairOfNode& node) {
          return (node.first == selfPubkey && node.second == selfPeer);
        });
    if (iter != m_lookupNodesOffline.end()) {
      found = true;
      m_lookupNodes.emplace_back(*iter);
      m_lookupNodesOffline.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "My Peer Info is not in m_lookupNodesOffline");
      return false;
    }
  } else {
    // If recovering a lookup, we don't expect it to be in the offline list, so
    // just set found to true here
    found = true;
  }

  if (found) {
    bytes lookupOnlineMsg = ComposeGetLookupOnlineMessage();
    if (lookupOnlineMsg.size() != 0) {
      SendMessageToLookupNodesSerial(lookupOnlineMsg);
    } else {
      return false;
    }
  }
  return true;
}

void Lookup::RejoinAsNewLookup() {
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP) {
    LOG_GENERAL(WARNING,
                "Lookup::RejoinAsNewLookup not expected to be called from "
                "other than the NewLookup node.");
    return;
  }

  LOG_MARKER();
  if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
    auto func = [this]() mutable -> void {
      while (true) {
        m_mediator.m_lookup->SetSyncType(SyncType::NEW_LOOKUP_SYNC);
        this->CleanVariables();
        while (!m_mediator.m_node->DownloadPersistenceFromS3()) {
          LOG_GENERAL(
              WARNING,
              "Downloading persistence from S3 has failed. Will try again!");
          this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
        }
        if (!BlockStorage::GetBlockStorage().RefreshAll()) {
          LOG_GENERAL(WARNING, "BlockStorage::RefreshAll failed");
          return;
        }
        if (!AccountStore::GetInstance().RefreshDB()) {
          LOG_GENERAL(WARNING, "BlockStorage::RefreshDB failed");
          return;
        }
        if (m_mediator.m_node->Install(SyncType::NEW_LOOKUP_SYNC, true)) {
          break;
        };
        this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
      }
      InitSync();
    };
    DetachedFunction(1, func);
  }
}

void Lookup::RejoinAsLookup() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::RejoinAsLookup not expected to be called from "
                "other than the LookUp node.");
    return;
  }

  LOG_MARKER();
  if (m_syncType == SyncType::NO_SYNC) {
    auto func = [this]() mutable -> void {
      SetSyncType(SyncType::LOOKUP_SYNC);
      AccountStore::GetInstance().InitSoft();
      m_mediator.m_node->Install(SyncType::LOOKUP_SYNC);
      this->StartSynchronization();
    };
    DetachedFunction(1, func);
  }
}

bool Lookup::FinishRejoinAsLookup() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::FinishRejoinAsLookup not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  return GetMyLookupOnline();
}

bool Lookup::CleanVariables() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::CleanVariables not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  m_seedNodes.clear();
  m_currDSExpired = false;
  m_startedTxnBatchThread = false;
  m_isFirstLoop = true;
  {
    std::lock_guard<mutex> lock(m_mediator.m_ds->m_mutexShards);
    m_mediator.m_ds->m_shards.clear();
  }
  {
    std::lock_guard<mutex> lock(m_mutexNodesInNetwork);
    m_nodesInNetwork.clear();
    l_nodesInNetwork.clear();
  }

  return true;
}

bool Lookup::ToBlockMessage(unsigned char ins_byte) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ToBlockMessage not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  return m_syncType != SyncType::NO_SYNC &&
         (ins_byte != LookupInstructionType::SETDSBLOCKFROMSEED &&
          ins_byte != LookupInstructionType::SETDSINFOFROMSEED &&
          ins_byte != LookupInstructionType::SETTXBLOCKFROMSEED &&
          ins_byte != LookupInstructionType::SETSTATEFROMSEED &&
          ins_byte != LookupInstructionType::SETLOOKUPOFFLINE &&
          ins_byte != LookupInstructionType::SETLOOKUPONLINE &&
          ins_byte != LookupInstructionType::SETSTATEDELTAFROMSEED &&
          ins_byte != LookupInstructionType::SETSTATEDELTASFROMSEED &&
          ins_byte != LookupInstructionType::SETDIRBLOCKSFROMSEED);
}

bytes Lookup::ComposeGetOfflineLookupNodes() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetOfflineLookupNodes not expected to be "
                "called from the LookUp node.");
    return bytes();
  }

  LOG_MARKER();

  bytes getCurrLookupsMessage = {MessageType::LOOKUP,
                                 LookupInstructionType::GETOFFLINELOOKUPS};

  if (!Messenger::SetLookupGetOfflineLookups(
          getCurrLookupsMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetOfflineLookups failed.");
    return {};
  }

  return getCurrLookupsMessage;
}

bool Lookup::GetOfflineLookupNodes() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetOfflineLookupNodes not expected to be called "
                "from the LookUp node.");
    return true;
  }

  LOG_MARKER();
  // Reset m_lookupNodes/m_lookupNodesOffline
  SetLookupNodes();
  bytes OfflineLookupNodesMsg = ComposeGetOfflineLookupNodes();
  if (OfflineLookupNodesMsg.size() != 0) {
    SendMessageToLookupNodesSerial(OfflineLookupNodesMsg);
  } else {
    return false;
  }
  return true;
}

bool Lookup::ProcessGetDirectoryBlocksFromSeed(const bytes& message,
                                               unsigned int offset,
                                               const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessGetDirectoryBlocksFromSeed not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t index_num;
  uint32_t portNo;
  if (!Messenger::GetLookupGetDirectoryBlocksFromSeed(message, offset, portNo,
                                                      index_num)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupGetDirectoryBlocksFromSeed failed");
    return false;
  }

  bytes msg = {MessageType::LOOKUP,
               LookupInstructionType::SETDIRBLOCKSFROMSEED};

  vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
      dirBlocks;

  for (uint64_t i = index_num;
       i <= m_mediator.m_blocklinkchain.GetLatestIndex(); i++) {
    BlockLink b = m_mediator.m_blocklinkchain.GetBlockLink(i);

    if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::DS) {
      dirBlocks.emplace_back(
          m_mediator.m_dsBlockChain.GetBlock(get<BlockLinkIndex::DSINDEX>(b)));
    } else if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::VC) {
      VCBlockSharedPtr vcblockptr;
      if (!BlockStorage::GetBlockStorage().GetVCBlock(
              get<BlockLinkIndex::BLOCKHASH>(b), vcblockptr)) {
        LOG_GENERAL(WARNING, "could not get vc block "
                                 << get<BlockLinkIndex::BLOCKHASH>(b));
        continue;
      }
      dirBlocks.emplace_back(*vcblockptr);
    } else if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::FB) {
      FallbackBlockSharedPtr fallbackwsharding;
      if (!BlockStorage::GetBlockStorage().GetFallbackBlock(
              get<BlockLinkIndex::BLOCKHASH>(b), fallbackwsharding)) {
        LOG_GENERAL(WARNING, "could not get fb block "
                                 << get<BlockLinkIndex::BLOCKHASH>(b));
        continue;
      }
      dirBlocks.emplace_back(*fallbackwsharding);
    }
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer peer(ipAddr, portNo);

  if (!Messenger::SetLookupSetDirectoryBlocksFromSeed(
          msg, MessageOffset::BODY, SHARDINGSTRUCTURE_VERSION, dirBlocks,
          index_num, m_mediator.m_selfKey)) {
    LOG_GENERAL(WARNING,
                "Messenger::SetLookupSetDirectoryBlocksFromSeed failed");
    return false;
  }

  P2PComm::GetInstance().SendMessage(peer, msg);

  return true;
}

bool Lookup::ProcessSetDirectoryBlocksFromSeed(
    const bytes& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
      dirBlocks;
  uint64_t index_num;
  uint32_t shardingStructureVersion = 0;
  PubKey lookupPubKey;

  lock(m_mutexCheckDirBlocks, m_mutexSetTxBlockFromSeed);

  lock_guard<mutex> g(m_mutexCheckDirBlocks, adopt_lock);
  lock_guard<mutex> lock(m_mutexSetTxBlockFromSeed, adopt_lock);
  if (!Messenger::GetLookupSetDirectoryBlocksFromSeed(
          message, offset, shardingStructureVersion, dirBlocks, index_num,
          lookupPubKey)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupSetDirectoryBlocksFromSeed failed");
    return false;
  }

  if (!Lookup::VerifySenderNode(GetSeedNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  // Not all calls to GetLookupSetDirectoryBlocksFromSeed set
  // shardingStructureVersion

  // if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
  //   LOG_GENERAL(WARNING, "Sharding structure version check failed. Expected:
  //   "
  //                            << SHARDINGSTRUCTURE_VERSION
  //                            << " Actual: " << shardingStructureVersion);
  //   return false;
  // }

  if (dirBlocks.empty()) {
    LOG_GENERAL(WARNING, "No Directory blocks sent/ I have the latest blocks");
    return false;
  }

  if (m_mediator.m_blocklinkchain.GetLatestIndex() >= index_num) {
    LOG_GENERAL(INFO, "Already have dir blocks");
    return true;
  }

  DequeOfNode newDScomm;

  uint64_t dsblocknumbefore =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  LOG_GENERAL(INFO, "[DSINFOVERIF]"
                        << "Recvd " << dirBlocks.size() << " from lookup");
  {
    if (m_mediator.m_blocklinkchain.GetBuiltDSComm().size() == 0) {
      LOG_GENERAL(WARNING, "Initial DS comm size 0, it is unset")
      return true;
    }

    if (!m_mediator.m_validator->CheckDirBlocks(
            dirBlocks, m_mediator.m_blocklinkchain.GetBuiltDSComm(), index_num,
            newDScomm)) {
      LOG_GENERAL(WARNING, "Verification of ds information failed");
    } else {
      LOG_GENERAL(INFO, "[DSINFOVERIF]"
                            << "Verified successfully");
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(newDScomm);
  }
  uint64_t dsblocknumafter =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (dsblocknumafter > dsblocknumbefore) {
    if (m_syncType == SyncType::NO_SYNC &&
        m_mediator.m_node->m_stillMiningPrimary) {
      m_fetchedLatestDSBlock = true;
      cv_latestDSBlock.notify_all();
      return true;
    }

    if (m_syncType == SyncType::DS_SYNC ||
        m_syncType == SyncType::LOOKUP_SYNC ||
        m_syncType == SyncType::NEW_LOOKUP_SYNC ||
        m_syncType == SyncType::GUARD_DS_SYNC) {
      if (!m_isFirstLoop) {
        m_currDSExpired = true;
      } else {
        m_isFirstLoop = false;
      }
    }
    m_mediator.UpdateDSBlockRand();
  }

  CheckBufferTxBlocks();
  return true;
}

void Lookup::CheckBufferTxBlocks() {
  if (!m_txBlockBuffer.empty()) {
    ValidatorBase::TxBlockValidationMsg res =
        m_mediator.m_validator->CheckTxBlocks(
            m_txBlockBuffer, m_mediator.m_blocklinkchain.GetBuiltDSComm(),
            m_mediator.m_blocklinkchain.GetLatestBlockLink());

    switch (res) {
      case ValidatorBase::TxBlockValidationMsg::VALID:
        CommitTxBlocks(m_txBlockBuffer);
        m_txBlockBuffer.clear();
        break;
      case ValidatorBase::TxBlockValidationMsg::STALEDSINFO:
        LOG_GENERAL(
            WARNING,
            "Even after the recving latest ds info, the information is stale ");
        break;
      case ValidatorBase::TxBlockValidationMsg::INVALID:
        LOG_GENERAL(WARNING, "The blocks in buffer are invalid ");
        m_txBlockBuffer.clear();
        break;
      default:
        LOG_GENERAL(WARNING,
                    "The return value of ValidatorBase::CheckTxBlocks does not "
                    "match any type");
    }
  }
}

void Lookup::ComposeAndSendGetDirectoryBlocksFromSeed(const uint64_t& index_num,
                                                      bool toSendSeed) {
  LOG_MARKER();
  bytes message = {MessageType::LOOKUP,
                   LookupInstructionType::GETDIRBLOCKSFROMSEED};

  if (!Messenger::SetLookupGetDirectoryBlocksFromSeed(
          message, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost,
          index_num)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetDirectoryBlocksFromSeed");
    return;
  }

  if (!toSendSeed) {
    SendMessageToRandomLookupNode(message);
  } else {
    SendMessageToRandomSeedNode(message);
  }
}

void Lookup::ComposeAndSendGetShardingStructureFromSeed() {
  LOG_MARKER();
  bytes message = {MessageType::LOOKUP,
                   LookupInstructionType::GETSHARDSFROMSEED};

  if (!Messenger::SetLookupGetShardsFromSeed(
          message, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetShardsFromSeed");
    return;
  }

  SendMessageToRandomSeedNode(message);
}

bool Lookup::Execute(const bytes& message, unsigned int offset,
                     const Peer& from) {
  LOG_MARKER();

  bool result = true;

  typedef bool (Lookup::*InstructionHandler)(const bytes&, unsigned int,
                                             const Peer&);

  InstructionHandler ins_handlers[] = {
      &Lookup::ProcessGetDSInfoFromSeed,
      &Lookup::ProcessSetDSInfoFromSeed,
      &Lookup::ProcessGetDSBlockFromSeed,
      &Lookup::ProcessSetDSBlockFromSeed,
      &Lookup::ProcessGetTxBlockFromSeed,
      &Lookup::ProcessSetTxBlockFromSeed,
      &Lookup::ProcessGetStateFromSeed,
      &Lookup::ProcessSetStateFromSeed,
      &Lookup::ProcessSetLookupOffline,
      &Lookup::ProcessSetLookupOnline,
      &Lookup::ProcessGetOfflineLookups,
      &Lookup::ProcessSetOfflineLookups,
      &Lookup::ProcessRaiseStartPoW,
      &Lookup::ProcessGetStartPoWFromSeed,
      &Lookup::ProcessSetStartPoWFromSeed,
      &Lookup::ProcessGetShardFromSeed,         // UNUSED
      &Lookup::ProcessSetShardFromSeed,         // UNUSED
      &Lookup::ProcessGetMicroBlockFromLookup,  // UNUSED
      &Lookup::ProcessSetMicroBlockFromLookup,  // UNUSED
      &Lookup::ProcessGetTxnsFromLookup,        // UNUSED
      &Lookup::ProcessSetTxnsFromLookup,        // UNUSED
      &Lookup::ProcessGetDirectoryBlocksFromSeed,
      &Lookup::ProcessSetDirectoryBlocksFromSeed,
      &Lookup::ProcessGetStateDeltaFromSeed,
      &Lookup::ProcessGetStateDeltasFromSeed,
      &Lookup::ProcessSetStateDeltaFromSeed,
      &Lookup::ProcessSetStateDeltasFromSeed,
      &Lookup::ProcessVCGetLatestDSTxBlockFromSeed,
      &Lookup::ProcessForwardTxn,
      &Lookup::ProcessGetDSGuardNetworkInfo,
      &Lookup::ProcessSetHistoricalDB};

  const unsigned char ins_byte = message.at(offset);
  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  if (LOOKUP_NODE_MODE) {
    if (ToBlockMessage(ins_byte)) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "Ignore lookup message");
      return false;
    }
  }

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);
    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_GENERAL(WARNING, "Unknown instruction byte "
                             << hex << (unsigned int)ins_byte << " from "
                             << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
  }

  return result;
}

bool Lookup::AlreadyJoinedNetwork() { return m_syncType == SyncType::NO_SYNC; }

bool Lookup::AddToTxnShardMap(const Transaction& tx, uint32_t shardId) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::AddToTxnShardMap not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  lock_guard<mutex> g(m_txnShardMapMutex);

  uint32_t size = 0;

  for (const auto& x : m_txnShardMap) {
    size += x.second.size();
  }

  if (size >= TXN_STORAGE_LIMIT) {
    LOG_GENERAL(INFO, "Number of txns exceeded limit");
    return false;
  }

  // case where txn already exist
  if (find_if(m_txnShardMap[shardId].begin(), m_txnShardMap[shardId].end(),
              [tx](const Transaction& txn) {
                return tx.GetTranID() == txn.GetTranID();
              }) != m_txnShardMap[shardId].end()) {
    LOG_GENERAL(WARNING, "Same hash present " << tx.GetTranID());
    return false;
  }

  m_txnShardMap[shardId].push_back(tx);

  return true;
}

bool Lookup::DeleteTxnShardMap(uint32_t shardId) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::DeleteTxnShardMap not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  lock_guard<mutex> g(m_txnShardMapMutex);

  m_txnShardMap[shardId].clear();

  return true;
}

void Lookup::SenderTxnBatchThread(const uint32_t oldNumShards) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SenderTxnBatchThread not expected to be called from "
                "other than the LookUp node.");
    return;
  }
  LOG_MARKER();

  if (m_startedTxnBatchThread) {
    LOG_GENERAL(WARNING,
                "The last TxnBatchThread hasn't finished, discard this time");
    return;
  }

  auto main_func = [this, oldNumShards]() mutable -> void {
    m_startedTxnBatchThread = true;
    uint32_t numShards = 0;
    while (true) {
      if (!m_mediator.GetIsVacuousEpoch()) {
        numShards = m_mediator.m_ds->GetNumShards();
        if (numShards == 0) {
          this_thread::sleep_for(chrono::milliseconds(1000));
          continue;
        }
        SendTxnPacketToNodes(oldNumShards, numShards);
      }
      break;
    }
    m_startedTxnBatchThread = false;
  };
  DetachedFunction(1, main_func);
}

void Lookup::RectifyTxnShardMap(const uint32_t oldNumShards,
                                const uint32_t newNumShards) {
  LOG_MARKER();

  auto t_start = std::chrono::high_resolution_clock::now();

  map<uint, vector<Transaction>> tempTxnShardMap;

  lock_guard<mutex> g(m_txnShardMapMutex);

  LOG_GENERAL(INFO, "Shard dropped or gained, shuffling txn shard map");
  LOG_GENERAL(INFO, "New Shard Size: " << newNumShards
                                       << "  Old Shard Size: " << oldNumShards);
  for (const auto& shard : m_txnShardMap) {
    if (shard.first == oldNumShards) {
      // ds txns
      continue;
    }
    for (const auto& tx : shard.second) {
      unsigned int fromShard = tx.GetShardIndex(newNumShards);

      if (Transaction::GetTransactionType(tx) == Transaction::CONTRACT_CALL) {
        // if shard do not match directly send to ds
        unsigned int toShard =
            Transaction::GetShardIndex(tx.GetToAddr(), newNumShards);
        if (toShard != fromShard) {
          // later would be placed in the new ds shard
          m_txnShardMap[oldNumShards].emplace_back(move(tx));
          continue;
        }
      }

      tempTxnShardMap[fromShard].emplace_back(move(tx));
    }
  }
  tempTxnShardMap[newNumShards] = move(m_txnShardMap[oldNumShards]);

  m_txnShardMap.clear();

  m_txnShardMap = move(tempTxnShardMap);

  auto t_end = std::chrono::high_resolution_clock::now();

  double elaspedTimeMs =
      std::chrono::duration<double, std::milli>(t_end - t_start).count();

  LOG_GENERAL(INFO, "Elapsed time for exchange " << elaspedTimeMs);
}

void Lookup::SendTxnPacketToNodes(const uint32_t oldNumShards,
                                  const uint32_t newNumShards) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SendTxnPacketToNodes not expected to be called from "
                "other than the LookUp node.");
    return;
  }

  const uint32_t numShards = newNumShards;

  map<uint32_t, vector<Transaction>> mp;

  if (!GenTxnToSend(NUM_TXN_TO_SEND_PER_ACCOUNT, mp, numShards)) {
    LOG_GENERAL(WARNING, "GenTxnToSend failed");
    // return;
  }

  if (oldNumShards != newNumShards) {
    auto rectifyFunc = [this, oldNumShards, newNumShards]() mutable -> void {
      RectifyTxnShardMap(oldNumShards, newNumShards);
    };
    DetachedFunction(1, rectifyFunc);
  }

  this_thread::sleep_for(
      chrono::milliseconds(LOOKUP_DELAY_SEND_TXNPACKET_IN_MS));

  for (unsigned int i = 0; i < numShards + 1; i++) {
    bytes msg = {MessageType::NODE, NodeInstructionType::FORWARDTXNPACKET};
    bool result = false;

    {
      lock_guard<mutex> g(m_txnShardMapMutex);
      auto transactionNumber = mp[i].size();

      LOG_GENERAL(INFO, "Txn number generated: " << transactionNumber);

      if (GetTxnFromShardMap(i).empty() && mp[i].empty()) {
        LOG_GENERAL(INFO, "No txns to send to shard " << i);
        continue;
      }

      result = Messenger::SetNodeForwardTxnBlock(
          msg, MessageOffset::BODY, m_mediator.m_currentEpochNum,
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(), i,
          m_mediator.m_selfKey, GetTxnFromShardMap(i), mp[i]);
    }

    if (!result) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::SetNodeForwardTxnBlock failed.");
      LOG_GENERAL(WARNING, "Cannot create packet for " << i << " shard");
      continue;
    }
    vector<Peer> toSend;
    if (i < numShards) {
      {
        lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);
        uint16_t lastBlockHash = DataConversion::charArrTo16Bits(
            m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
        uint32_t leader_id = m_mediator.m_node->CalculateShardLeaderFromShard(
            lastBlockHash, m_mediator.m_ds->m_shards.at(i).size(),
            m_mediator.m_ds->m_shards.at(i));
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Shard leader id " << leader_id);

        auto it = m_mediator.m_ds->m_shards.at(i).begin();
        // Lookup sends to NUM_NODES_TO_SEND_LOOKUP + Leader
        unsigned int num_node_to_send = NUM_NODES_TO_SEND_LOOKUP;
        for (unsigned int j = 0; j < num_node_to_send &&
                                 it != m_mediator.m_ds->m_shards.at(i).end();
             j++, it++) {
          if (distance(m_mediator.m_ds->m_shards.at(i).begin(), it) ==
              leader_id) {
            num_node_to_send++;
          } else {
            toSend.push_back(std::get<SHARD_NODE_PEER>(*it));
            LOG_GENERAL(INFO, "Sent to node " << get<SHARD_NODE_PEER>(*it));
          }
        }
        if (m_mediator.m_ds->m_shards.at(i).empty()) {
          continue;
        }
      }

      P2PComm::GetInstance().SendBroadcastMessage(toSend, msg);

      DeleteTxnShardMap(i);
    } else if (i == numShards) {
      // To send DS
      {
        lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

        if (m_mediator.m_DSCommittee->empty()) {
          continue;
        }

        // Send to NUM_NODES_TO_SEND_LOOKUP which including DS leader
        PairOfNode dsLeader;
        if (Node::GetDSLeader(m_mediator.m_blocklinkchain.GetLatestBlockLink(),
                              m_mediator.m_dsBlockChain.GetLastBlock(),
                              *m_mediator.m_DSCommittee, dsLeader)) {
          toSend.push_back(dsLeader.second);
        }

        for (auto const& i : *m_mediator.m_DSCommittee) {
          if (toSend.size() < NUM_NODES_TO_SEND_LOOKUP &&
              i.second != dsLeader.second) {
            toSend.push_back(i.second);
          }

          if (toSend.size() >= NUM_NODES_TO_SEND_LOOKUP) {
            break;
          }
        }
      }

      P2PComm::GetInstance().SendBroadcastMessage(toSend, msg);

      LOG_GENERAL(INFO, "[DSMB]"
                            << " Sent DS the txns");

      DeleteTxnShardMap(i);
    }
  }
}

void Lookup::SetServerTrue() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SetServerTrue not expected to be called from "
                "other than the LookUp node.");
    return;
  }

  m_isServer = true;
}

bool Lookup::GetIsServer() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetIsServer not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  return m_isServer;
}

bool Lookup::VerifySenderNode(const VectorOfNode& vecLookupNodes,
                              const PubKey& pubKeyToVerify) {
  auto iter = std::find_if(vecLookupNodes.cbegin(), vecLookupNodes.cend(),
                           [&pubKeyToVerify](const PairOfNode& node) {
                             return node.first == pubKeyToVerify;
                           });
  return vecLookupNodes.cend() != iter;
}

bool Lookup::ProcessForwardTxn(const bytes& message, unsigned int offset,
                               const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessForwardTxn not expected to be called from "
                "non-lookup node");
  }

  vector<Transaction> txnsShard;
  vector<Transaction> txnsDS;

  if (!Messenger::GetForwardTxnBlockFromSeed(message, offset, txnsShard,
                                             txnsDS)) {
    LOG_GENERAL(WARNING, "Failed to Messenger::GetForwardTxnBlockFromSeed");
    return false;
  }

  LOG_GENERAL(INFO, "Recvd from " << from);

  if (!ARCHIVAL_LOOKUP) {
    uint32_t shard_size = m_mediator.m_ds->GetNumShards();

    if (shard_size == 0) {
      LOG_GENERAL(WARNING, "Shard size 0");
      return false;
    }

    for (const auto& txn : txnsShard) {
      unsigned int shard = txn.GetShardIndex(shard_size);
      AddToTxnShardMap(txn, shard);
    }
    for (const auto& txn : txnsDS) {
      AddToTxnShardMap(txn, shard_size);
    }
  } else {
    for (const auto& txn : txnsShard) {
      AddToTxnShardMap(txn, SEND_TYPE::ARCHIVAL_SEND_SHARD);
    }
    for (const auto& txn : txnsDS) {
      AddToTxnShardMap(txn, SEND_TYPE::ARCHIVAL_SEND_DS);
    }
  }

  return true;
}

bool Lookup::ProcessVCGetLatestDSTxBlockFromSeed(const bytes& message,
                                                 unsigned int offset,
                                                 const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessVCGetLatestDSTxBlockFromSeed not expected to be "
        "called from other than the LookUp node.");
    return true;
  }

  uint64_t dsLowBlockNum = 0;
  uint64_t dsHighBlockNum = 0;
  uint64_t txLowBlockNum = 0;
  uint64_t txHighBlockNum = 0;
  uint32_t listenPort = 0;

  if (!Messenger::GetLookupGetDSTxBlockFromSeed(message, offset, dsLowBlockNum,
                                                dsHighBlockNum, txLowBlockNum,
                                                txHighBlockNum, listenPort)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetSeedPeers failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessVCGetLatestDSTxBlockFromSeed (pre) requested by "
                << from << " for ds blocks " << dsLowBlockNum << " to "
                << dsHighBlockNum << " and tx blocks " << txLowBlockNum
                << " to " << txHighBlockNum << " with receiving port "
                << listenPort);

  vector<DSBlock> dsBlocks;
  RetrieveDSBlocks(dsBlocks, dsLowBlockNum, dsHighBlockNum, true);

  vector<TxBlock> txBlocks;
  RetrieveTxBlocks(txBlocks, txLowBlockNum, txHighBlockNum);

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessVCGetLatestDSTxBlockFromSeed (final) requested by "
                << from << " for ds blocks " << dsLowBlockNum << " to "
                << dsHighBlockNum << " and tx blocks " << txLowBlockNum
                << " to " << txHighBlockNum << " with receiving port "
                << listenPort);

  bytes dsTxBlocksMessage = {MessageType::DIRECTORY,
                             DSInstructionType::VCPUSHLATESTDSTXBLOCK};

  if (!Messenger::SetVCNodeSetDSTxBlockFromSeed(
          dsTxBlocksMessage, MessageOffset::BODY, m_mediator.m_selfKey,
          dsBlocks, txBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetVCNodeSetDSTxBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, listenPort);
  P2PComm::GetInstance().SendMessage(requestingNode, dsTxBlocksMessage);
  return true;
}

void Lookup::SetSyncType(SyncType syncType) {
  m_syncType.store(syncType);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Set sync type to " << syncType);
}

bool Lookup::ProcessGetDSGuardNetworkInfo(const bytes& message,
                                          unsigned int offset,
                                          const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetDSGuardNetworkInfo not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING,
                "Not in guard mode. Unable to process request to update ds "
                "guard network info.");
    return false;
  }

  LOG_MARKER();

  uint32_t portNo = 0;
  uint64_t dsEpochNo = 0;

  if (!Messenger::GetLookupGetNewDSGuardNetworkInfoFromLookup(
          message, offset, portNo, dsEpochNo)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetNewDSGuardNetworkInfoFromLookup failed.");
    return false;
  }

  if (m_mediator.m_ds->m_lookupStoreForGuardNodeUpdate.find(dsEpochNo) ==
      m_mediator.m_ds->m_lookupStoreForGuardNodeUpdate.end()) {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "No record found for guard ds update. No update needed. dsEpochNo: "
            << dsEpochNo);
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  bytes setNewDSGuardNetworkInfo = {
      MessageType::NODE, NodeInstructionType::DSGUARDNODENETWORKINFOUPDATE};

  if (!Messenger::SetNodeSetNewDSGuardNetworkInfo(
          setNewDSGuardNetworkInfo, MessageOffset::BODY,
          m_mediator.m_ds->m_lookupStoreForGuardNodeUpdate.at(dsEpochNo),
          m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeSetNewDSGuardNetworkInfo failed.");
    return false;
  }

  LOG_GENERAL(INFO, "[update ds guard] Sending guard node update info to "
                        << requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, setNewDSGuardNetworkInfo);
  return true;
}

bool Lookup::ProcessSetHistoricalDB(const bytes& message, unsigned int offset,
                                    [[gnu::unused]] const Peer& from) {
  string path = "";
  uint32_t code = 0;
  PubKey archPubkey;

  if (!Messenger::GetSeedNodeHistoricalDB(message, offset, archPubkey, code,
                                          path)) {
    LOG_GENERAL(WARNING, "GetSeedNodeHistoricalDB failed");
    return false;
  }

  bytes verifierPubkeyBytes;
  if (!DataConversion::HexStrToUint8Vec(VERIFIER_PUBKEY, verifierPubkeyBytes)) {
    LOG_GENERAL(WARNING, "VERIFIER_PUBKEY is not a hex str");
    return false;
  }

  if (!(archPubkey == PubKey(verifierPubkeyBytes, 0))) {
    LOG_GENERAL(WARNING, "PubKey not of verifier");
    return false;
  }

  if (code == 1) {
    if (!BlockStorage::GetBlockStorage().InitiateHistoricalDB(VERIFIER_PATH +
                                                              "/" + path)) {
      LOG_GENERAL(WARNING,
                  "BlockStorage::InitiateHistoricalDB failed, path: " << path);
      return false;
    }

    m_historicalDB = true;
  } else {
    LOG_GENERAL(WARNING, "Code is errored " << code);
    return false;
  }

  LOG_GENERAL(INFO, "HistDB Success");
  return true;
}
