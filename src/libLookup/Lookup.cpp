/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

Lookup::Lookup(Mediator& mediator) : m_mediator(mediator) {
  SetLookupNodes();
  if (LOOKUP_NODE_MODE) {
    SetDSCommitteInfo();
  }
}

Lookup::~Lookup() {}

void Lookup::InitAsNewJoiner() {
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
  auto func = [this]() -> void {
    uint64_t dsBlockNum = 0;
    uint64_t txBlockNum = 0;

    // Hack to allow seed server to be restarted so as to get my newlookup ip
    // and register me with multiplier.
    this_thread::sleep_for(chrono::seconds(NEW_LOOKUP_SYNC_DELAY_IN_SECONDS));

    // Initialize all blockchains and blocklinkchain
    InitAsNewJoiner();

    // Set myself offline
    GetMyLookupOffline();

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
      GetTxBlockFromLookupNodes(txBlockNum, 0);

      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
  };
  DetachedFunction(1, func);
}

void Lookup::SetLookupNodes() {
  LOG_MARKER();

  std::lock_guard<std::mutex> lock(m_mutexLookupNodes);

  m_lookupNodes.clear();
  m_lookupNodesOffline.clear();
  // Populate tree structure pt
  using boost::property_tree::ptree;
  ptree pt;
  read_xml("constants.xml", pt);

  for (const ptree::value_type& v : pt.get_child("node.lookups")) {
    if (v.first == "peer") {
      struct in_addr ip_addr;
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      Peer lookup_node((uint128_t)ip_addr.s_addr,
                       v.second.get<uint32_t>("port"));
      PubKey pubKey(
          DataConversion::HexStrToUint8Vec(v.second.get<std::string>("pubkey")),
          0);
      m_lookupNodes.emplace_back(pubKey, lookup_node);
    }
  }

  // Add myself to lookupnodes
  if (m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    const PubKey& myPubKey = m_mediator.m_selfKey.second;
    if (std::find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                     [&myPubKey](const std::pair<PubKey, Peer>& node) {
                       return node.first == myPubKey;
                     }) == m_lookupNodes.end()) {
      m_lookupNodes.emplace_back(m_mediator.m_selfKey.second,
                                 m_mediator.m_selfPeer);
    }
  }
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

  // std::vector<unsigned char> buf;
  // txn.SerializeWithoutSignature(buf, 0);

  // Signature sig;
  // Schnorr::GetInstance().Sign(buf, fromPrivKey, fromPubKey, sig);

  // vector<unsigned char> sigBuf;
  // sig.Serialize(sigBuf, 0);
  // txn.SetSignature(sigBuf);

  return txn;
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

  for (auto& addrStr : GENESIS_WALLETS) {
    Address addr{DataConversion::HexStrToUint8Vec(addrStr)};

    if (numShards == 0) {
      return false;
    }
    auto txnShard = Transaction::GetShardIndex(addr, numShards);
    txns.clear();

    uint64_t nonce = AccountStore::GetInstance().GetAccount(addr)->GetNonce();

    if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                     num_txn, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns from file");
      return false;
    }

    /*Address receiverAddr = GenOneReceiver();
    unsigned int curr_offset = 0;
    txns.reserve(n);
    for (auto i = 0u; i != n; i++)
    {

        Transaction txn(0, nonce + i + 1, receiverAddr,
                        make_pair(privKey, pubKey), 10 * i + 2, 1, 1, {},
                        {});

        curr_offset = txn.Serialize(txns, curr_offset);
    }*/
    //[Change back here]
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

VectorOfLookupNode Lookup::GetLookupNodes() const {
  LOG_MARKER();
  lock_guard<mutex> lock(m_mutexLookupNodes);
  return m_lookupNodes;
}

bool Lookup::IsLookupNode(const PubKey& pubKey) const {
  VectorOfLookupNode lookups = GetLookupNodes();
  return std::find_if(lookups.begin(), lookups.end(),
                      [&pubKey](const std::pair<PubKey, Peer>& node) {
                        return node.first == pubKey;
                      }) != lookups.end();
}

bool Lookup::IsLookupNode(const Peer& peerInfo) const {
  VectorOfLookupNode lookups = GetLookupNodes();
  return std::find_if(lookups.begin(), lookups.end(),
                      [&peerInfo](const std::pair<PubKey, Peer>& node) {
                        return node.second.GetIpAddress() ==
                               peerInfo.GetIpAddress();
                      }) != lookups.end();
}

void Lookup::SendMessageToLookupNodes(
    const std::vector<unsigned char>& message) const {
  LOG_MARKER();

  // LOG_GENERAL(INFO, "i am here " <<
  // to_string(m_mediator.m_currentEpochNum).c_str())

  // TODO: provide interface in P2PComm instead of repopulating the lookup into
  // vector of Peer
  vector<Peer> allLookupNodes;

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    for (const auto& node : m_lookupNodes) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Sending msg to lookup node "
                    << node.second.GetPrintableIPAddress() << ":"
                    << node.second.m_listenPortHost);

      allLookupNodes.emplace_back(node.second);
    }
  }

  P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void Lookup::SendMessageToLookupNodesSerial(
    const std::vector<unsigned char>& message) const {
  LOG_MARKER();

  // LOG_GENERAL("i am here " <<
  // to_string(m_mediator.m_currentEpochNum).c_str())
  vector<Peer> allLookupNodes;

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    for (const auto& node : m_lookupNodes) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Sending msg to lookup node "
                    << node.second.GetPrintableIPAddress() << ":"
                    << node.second.m_listenPortHost);

      allLookupNodes.emplace_back(node.second);
    }
  }

  P2PComm::GetInstance().SendMessage(allLookupNodes, message);
}

void Lookup::SendMessageToRandomLookupNode(
    const std::vector<unsigned char>& message) const {
  LOG_MARKER();

  // int index = rand() % (NUM_LOOKUP_USE_FOR_SYNC) + m_lookupNodes.size()
  // - NUM_LOOKUP_USE_FOR_SYNC;
  lock_guard<mutex> lock(m_mutexLookupNodes);
  if (0 == m_lookupNodes.size()) {
    LOG_GENERAL(WARNING, "There is no lookup node existed yet!");
    return;
  }

  int index = rand() % m_lookupNodes.size();
  P2PComm::GetInstance().SendMessage(m_lookupNodes[index].second, message);
}

void Lookup::SendMessageToSeedNodes(
    const std::vector<unsigned char>& message) const {
  LOG_MARKER();

  for (auto node : m_seedNodes) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Sending msg to seed node " << node.GetPrintableIPAddress() << ":"
                                          << node.m_listenPortHost);
    P2PComm::GetInstance().SendMessage(node, message);
  }
}

bool Lookup::GetSeedPeersFromLookup() {
  LOG_MARKER();

  vector<unsigned char> getSeedPeersMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETSEEDPEERS};

  if (!Messenger::SetLookupGetSeedPeers(
          getSeedPeersMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetSeedPeers failed.");
    return false;
  }

  SendMessageToRandomLookupNode(getSeedPeersMessage);

  return true;
}

vector<unsigned char> Lookup::ComposeGetDSInfoMessage(bool initialDS) {
  LOG_MARKER();

  vector<unsigned char> getDSNodesMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETDSINFOFROMSEED};

  if (!Messenger::SetLookupGetDSInfoFromSeed(
          getDSNodesMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost, initialDS)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetDSInfoFromSeed failed.");
    return {};
  }

  return getDSNodesMessage;
}

vector<unsigned char> Lookup::ComposeGetStateMessage() {
  LOG_MARKER();

  vector<unsigned char> getStateMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETSTATEFROMSEED};

  if (!Messenger::SetLookupGetStateFromSeed(
          getStateMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetStateFromSeed failed.");
    return {};
  }

  return getStateMessage;
}

bool Lookup::GetDSInfoFromSeedNodes() {
  LOG_MARKER();
  SendMessageToSeedNodes(ComposeGetDSInfoMessage());
  return true;
}

bool Lookup::GetDSInfoFromLookupNodes(bool initialDS) {
  LOG_MARKER();
  SendMessageToRandomLookupNode(ComposeGetDSInfoMessage(initialDS));
  return true;
}

bool Lookup::GetStateFromLookupNodes() {
  LOG_MARKER();
  SendMessageToRandomLookupNode(ComposeGetStateMessage());

  return true;
}

vector<unsigned char> Lookup::ComposeGetDSBlockMessage(uint64_t lowBlockNum,
                                                       uint64_t highBlockNum) {
  LOG_MARKER();

  vector<unsigned char> getDSBlockMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupGetDSBlockFromSeed(
          getDSBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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

vector<unsigned char> Lookup::ComposeGetTxBlockMessage(uint64_t lowBlockNum,
                                                       uint64_t highBlockNum) {
  LOG_MARKER();

  vector<unsigned char> getTxBlockMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETTXBLOCKFROMSEED};

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ComposeGetTxBlockMessage for blocks " << lowBlockNum << " to "
                                                   << highBlockNum);

  if (!Messenger::SetLookupGetTxBlockFromSeed(
          getTxBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetTxBlockFromSeed failed.");
    return {};
  }

  return getTxBlockMessage;
}

vector<unsigned char> Lookup::ComposeGetStateDeltaMessage(uint64_t blockNum) {
  LOG_MARKER();

  vector<unsigned char> getStateDeltaMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETSTATEDELTAFROMSEED};

  if (!Messenger::SetLookupGetStateDeltaFromSeed(
          getStateDeltaMessage, MessageOffset::BODY, blockNum,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetStateDeltaFromSeed failed.");
    return {};
  }

  return getStateDeltaMessage;
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

bool Lookup::GetStateDeltaFromLookupNodes(const uint64_t& blockNum) {
  LOG_MARKER();

  SendMessageToRandomLookupNode(ComposeGetStateDeltaMessage(blockNum));

  return true;
}

bool Lookup::GetTxBodyFromSeedNodes(string txHashStr) {
  LOG_MARKER();

  vector<unsigned char> getTxBodyMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETTXBODYFROMSEED};

  if (!Messenger::SetLookupGetTxBodyFromSeed(
          getTxBodyMessage, MessageOffset::BODY,
          DataConversion::HexStrToUint8Vec(txHashStr),
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetTxBodyFromSeed failed.");
    return false;
  }

  SendMessageToSeedNodes(getTxBodyMessage);

  return true;
}

bool Lookup::SetDSCommitteInfo() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SetDSCommitteInfo not expected to be called from "
                "other than the LookUp node.");
    return true;
  }
  // Populate tree structure pt

  LOG_MARKER();

  using boost::property_tree::ptree;
  ptree pt;
  read_xml("config.xml", pt);

  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  for (ptree::value_type const& v : pt.get_child("nodes")) {
    if (v.first == "peer") {
      PubKey key(DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")),
                 0);

      struct in_addr ip_addr;
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      Peer peer((uint128_t)ip_addr.s_addr, v.second.get<unsigned int>("port"));
      m_mediator.m_DSCommittee->emplace_back(make_pair(key, peer));
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

      LOG_GENERAL(INFO, "[SHARD "
                            << to_string(i) << "] "
                            << "[PEER " << to_string(index) << "] "
                            << "Inserting Pubkey to shard : " << string(key));
      LOG_GENERAL(INFO, "[SHARD " << to_string(i) << "] "
                                  << "[PEER " << to_string(index) << "] "
                                  << "Corresponding peer : " << string(peer));

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

bool Lookup::ProcessGetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetSeedPeersFromLookup not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetSeedPeers(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetSeedPeers failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer peer(ipAddr, portNo);

  lock_guard<mutex> g(m_mutexNodesInNetwork);

  uint32_t numPeersInNetwork = m_nodesInNetwork.size();

  if (numPeersInNetwork < SEED_PEER_LIST_SIZE) {
    LOG_GENERAL(WARNING,
                "[Lookup Node] numPeersInNetwork < SEED_PEER_LIST_SIZE");
    return false;
  }

  // Which of the following two implementations is more efficient and
  // parallelizable?
  // ================================================

  unordered_set<uint32_t> indicesAlreadyAdded;

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, numPeersInNetwork - 1);

  vector<Peer> candidateSeeds;

  for (unsigned int i = 0; i < SEED_PEER_LIST_SIZE; i++) {
    uint32_t index = dis(gen);
    while (indicesAlreadyAdded.find(index) != indicesAlreadyAdded.end()) {
      index = dis(gen);
    }
    indicesAlreadyAdded.insert(index);

    candidateSeeds.emplace_back(m_nodesInNetwork[index]);
  }

  // ================================================

  // auto nodesInNetworkCopy = m_nodesInNetwork;
  // int upperLimit = numPeersInNetwork-1;
  // random_device rd;
  // mt19937 gen(rd());

  // for(unsigned int i = 0; i < SEED_PEER_LIST_SIZE; ++i, --upperLimit)
  // {
  //     uniform_int_distribution<> dis(0, upperLimit);
  //     uint32_t index = dis(gen);

  //     Peer candidateSeed = m_nodesInNetwork[index];
  //     candidateSeed.Serialize(seedPeersMessage, curr_offset);
  //     curr_offset += (IP_SIZE + PORT_SIZE);

  //     swap(nodesInNetworkCopy[index], nodesInNetworkCopy[upperLimit]);
  // }

  // ================================================

  vector<unsigned char> seedPeersMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETSEEDPEERS};

  if (!Messenger::SetLookupSetSeedPeers(seedPeersMessage, MessageOffset::BODY,
                                        m_mediator.m_selfKey, candidateSeeds)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetSeedPeers failed.");
    return false;
  }

  P2PComm::GetInstance().SendMessage(peer, seedPeersMessage);

  return true;
}

bool Lookup::ProcessGetDSInfoFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  //#ifndef IS_LOOKUP_NODE

  LOG_MARKER();

  uint32_t portNo = 0;
  bool initialDS;

  if (!Messenger::GetLookupGetDSInfoFromSeed(message, offset, portNo,
                                             initialDS)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetDSInfoFromSeed failed.");
    return false;
  }

  vector<unsigned char> dsInfoMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETDSINFOFROMSEED};

  if (initialDS) {
    LOG_GENERAL(WARNING, "[DSINFOVERIF]"
                             << "Recvd call to send initial ds "
                             << " Unsupported");

  }

  else {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

    for (const auto& ds : *m_mediator.m_DSCommittee) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "IP:" << ds.second.GetPrintableIPAddress());
    }

    if (!Messenger::SetLookupSetDSInfoFromSeed(
            dsInfoMessage, MessageOffset::BODY, m_mediator.m_selfKey,
            *m_mediator.m_DSCommittee, false)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetLookupSetDSInfoFromSeed failed.");
      return false;
    }
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  P2PComm::GetInstance().SendMessage(requestingNode, dsInfoMessage);

  //#endif // IS_LOOKUP_NODE

  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::ProcessGetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from) {
  //#ifndef IS_LOOKUP_NODE // TODO: remove the comment

  LOG_MARKER();

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetDSBlockFromSeed(message, offset, lowBlockNum,
                                              highBlockNum, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetDSBlockFromSeed failed.");
    return false;
  }

  vector<DSBlock> dsBlocks;
  RetrieveDSBlocks(dsBlocks, lowBlockNum, highBlockNum);
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessGetDSBlockFromSeed requested by " << from << " for blocks "
                                                      << lowBlockNum << " to "
                                                      << highBlockNum);

  vector<unsigned char> dsBlockMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupSetDSBlockFromSeed(
          dsBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfKey, dsBlocks)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetDSBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, dsBlockMessage);

  //#endif // IS_LOOKUP_NODE

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

bool Lookup::ProcessGetStateFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from) {
  LOG_MARKER();

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStateFromSeed(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetStateFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  vector<unsigned char> setStateMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETSTATEFROMSEED};

  if (!Messenger::SetLookupSetStateFromSeed(
          setStateMessage, MessageOffset::BODY, m_mediator.m_selfKey,
          AccountStore::GetInstance())) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetStateFromSeed failed.");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, setStateMessage);
  // #endif // IS_LOOKUP_NODE

  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
bool Lookup::ProcessGetTxBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from) {
  // #ifndef IS_LOOKUP_NODE // TODO: remove the comment

  LOG_MARKER();

  uint64_t lowBlockNum = 0;
  uint64_t highBlockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetTxBlockFromSeed(message, offset, lowBlockNum,
                                              highBlockNum, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetTxBlockFromSeed failed.");
    return false;
  }

  vector<TxBlock> txBlocks;
  RetrieveTxBlocks(txBlocks, lowBlockNum, highBlockNum);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessGetTxBlockFromSeed requested by " << from << " for blocks "
                                                      << lowBlockNum << " to "
                                                      << highBlockNum);

  vector<unsigned char> txBlockMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETTXBLOCKFROMSEED};
  if (!Messenger::SetLookupSetTxBlockFromSeed(
          txBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfKey, txBlocks)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetTxBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  P2PComm::GetInstance().SendMessage(requestingNode, txBlockMessage);

  // #endif // IS_LOOKUP_NODE

  return true;
}

// TODO: Refactor the code to remove the following assumption
// lowBlockNum = 1 => Latest block number
// lowBlockNum = 0 => lowBlockNum set to 1
// highBlockNum = 0 => Latest block number
void Lookup::RetrieveTxBlocks(vector<TxBlock>& txBlocks, uint64_t& lowBlockNum,
                              uint64_t& highBlockNum) {
  lock_guard<mutex> g(m_mediator.m_node->m_mutexFinalBlock);

  if (lowBlockNum == 1) {
    // To get block num from dsblockchain instead of txblock chain as node
    // recover from the last ds epoch
    lowBlockNum =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
  } else if (lowBlockNum == 0) {
    // give all the blocks till now in blockchain
    lowBlockNum = 1;
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
      txBlocks.emplace_back(m_mediator.m_txBlockChain.GetBlock(blockNum));
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

bool Lookup::ProcessGetStateDeltaFromSeed(const vector<unsigned char>& message,
                                          unsigned int offset,
                                          const Peer& from) {
  LOG_MARKER();

  uint64_t blockNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStateDeltaFromSeed(message, offset, blockNum,
                                                 portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetStateDeltaFromSeed failed.");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessGetStateDeltaFromSeed requested by "
                << from << " for block " << blockNum);

  vector<unsigned char> stateDelta;

  if (!BlockStorage::GetBlockStorage().GetStateDelta(blockNum, stateDelta)) {
    LOG_GENERAL(INFO, "Block Number "
                          << blockNum
                          << " absent. Didn't include it in response message.");
  }

  vector<unsigned char> stateDeltaMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETSTATEDELTAFROMSEED};

  if (!Messenger::SetLookupSetStateDeltaFromSeed(
          stateDeltaMessage, MessageOffset::BODY, blockNum,
          m_mediator.m_selfKey, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetStateDeltaFromSeed failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, stateDeltaMessage);
  return true;
}

bool Lookup::ProcessGetTxBodyFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  // #ifndef IS_LOOKUP_NODE // TODO: remove the comment

  LOG_MARKER();

  TxnHash tranHash;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetTxBodyFromSeed(message, offset, tranHash,
                                             portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetTxBodyFromSeed failed.");
    return false;
  }

  TxBodySharedPtr tptr;

  BlockStorage::GetBlockStorage().GetTxBody(TxnHash(tranHash), tptr);

  vector<unsigned char> txBodyMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETTXBODYFROMSEED};

  if (!Messenger::SetLookupSetTxBodyFromSeed(txBodyMessage, MessageOffset::BODY,
                                             tranHash, *tptr)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetTxBodyFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  P2PComm::GetInstance().SendMessage(requestingNode, txBodyMessage);

  // #endif // IS_LOOKUP_NODE

  return true;
}

bool Lookup::ProcessGetShardFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from) {
  LOG_MARKER();

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetShardsFromSeed(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetShardsFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  vector<unsigned char> msg = {MessageType::LOOKUP,
                               LookupInstructionType::SETSHARDSFROMSEED};

  lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);

  if (!Messenger::SetLookupSetShardsFromSeed(msg, MessageOffset::BODY,
                                             m_mediator.m_selfKey,
                                             m_mediator.m_ds->m_shards)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetShardsFromSeed failed.");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, msg);

  return true;
}

bool Lookup::ProcessSetShardFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from) {
  LOG_MARKER();

  DequeOfShard shards;
  PubKey lookupPubKey;
  if (!Messenger::GetLookupSetShardsFromSeed(message, offset, lookupPubKey,
                                             shards)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetShardsFromSeed failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
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

  return true;
}

bool Lookup::GetShardFromLookup() {
  LOG_MARKER();

  vector<unsigned char> msg = {MessageType::LOOKUP,
                               LookupInstructionType::GETSHARDSFROMSEED};

  if (!Messenger::SetLookupGetShardsFromSeed(
          msg, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetShardsFromSeed failed.");
    return false;
  }

  SendMessageToRandomLookupNode(msg);

  return true;
}

bool Lookup::ProcessGetNetworkId(const vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from) {
  // #ifndef IS_LOOKUP_NODE
  LOG_MARKER();

  // 4-byte portNo
  uint32_t portNo =
      Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  Peer requestingNode(from.m_ipAddress, portNo);

  vector<unsigned char> networkIdMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETNETWORKIDFROMSEED};
  unsigned int curr_offset = MessageOffset::BODY;

  string networkId = "TESTNET";  // TODO: later convert it to a enum

  copy(networkId.begin(), networkId.end(),
       networkIdMessage.begin() + curr_offset);
  P2PComm::GetInstance().SendMessage(requestingNode, networkIdMessage);

  return true;
  // #endif // IS_LOOKUP_NODE
}

bool Lookup::ProcessSetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetSeedPeersFromLookup not expected to be "
                "called from LookUp node.");
    return true;
  }

  std::vector<Peer> candidateSeeds;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetSeedPeers(message, offset, lookupPubKey,
                                        candidateSeeds)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetSeedPeers failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  m_seedNodes = std::move(candidateSeeds);

  unsigned int i = 0;
  for (const auto& candidateSeed : candidateSeeds) {
    LOG_GENERAL(INFO, "Peer " << i++ << ": " << candidateSeed);
  }

  return true;
}

bool Lookup::AddMicroBlockToStorage(const MicroBlock& microblock) {
  TxBlock txblk =
      m_mediator.m_txBlockChain.GetBlock(microblock.GetHeader().GetEpochNum());
  LOG_GENERAL(INFO, "[SendMB]"
                        << "Add MicroBlock hash: "
                        << microblock.GetBlockHash());
  unsigned int i = 0;

  if (txblk == TxBlock()) {
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

  vector<unsigned char> body;
  microblock.Serialize(body, 0);
  if (!BlockStorage::GetBlockStorage().PutMicroBlock(microblock.GetBlockHash(),
                                                     body)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in body");
    return false;
  }

  return true;
}

bool Lookup::ProcessGetMicroBlockFromLookup(
    const vector<unsigned char>& message, unsigned int offset,
    const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }
  vector<BlockHash> microBlockHashes;
  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetMicroBlockFromLookup(message, offset,
                                                   microBlockHashes, portNo)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return false;
  }

  if (microBlockHashes.size() == 0) {
    LOG_GENERAL(INFO, "No MicroBlock requested");
    return true;
  }

  LOG_GENERAL(INFO, "Reques for " << microBlockHashes.size() << " blocks");

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  vector<MicroBlock> retMicroBlocks;

  for (const auto& mbhash : microBlockHashes) {
    LOG_GENERAL(INFO, "[SendMB]"
                          << "Request for microBlockHash " << mbhash);
    shared_ptr<MicroBlock> mbptr;
    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbhash, mbptr)) {
      LOG_GENERAL(WARNING, "Failed to fetch micro block Hash " << mbhash);
      continue;
    } else {
      retMicroBlocks.push_back(*mbptr);
    }
  }

  vector<unsigned char> retMsg = {
      MessageType::LOOKUP, LookupInstructionType::SETMICROBLOCKFROMLOOKUP};

  if (retMicroBlocks.size() == 0) {
    LOG_GENERAL(WARNING, "return size 0 for microblocks");
    return true;
  }

  if (!Messenger::SetLookupSetMicroBlockFromLookup(
          retMsg, MessageOffset::BODY, m_mediator.m_selfKey, retMicroBlocks)) {
    LOG_GENERAL(WARNING, "Failed to Process ");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, retMsg);
  return true;
}

bool Lookup::ProcessSetMicroBlockFromLookup(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  //[numberOfMicroBlocks][microblock1][microblock2]...

  vector<MicroBlock> mbs;
  PubKey lookupPubKey;
  if (!Messenger::GetLookupSetMicroBlockFromLookup(message, offset,
                                                   lookupPubKey, mbs)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  for (const auto& mb : mbs) {
    LOG_GENERAL(INFO, "[SendMB]"
                          << " Recvd " << mb.GetHeader().GetEpochNum()
                          << " MBHash:" << mb.GetBlockHash());

    if (ARCHIVAL_NODE) {
      if (!m_mediator.m_archival->RemoveFromFetchMicroBlockInfo(
              mb.GetBlockHash())) {
        LOG_GENERAL(WARNING, "Error in remove fetch micro block");
        continue;
      }
      m_mediator.m_archival->AddToUnFetchedTxn(mb.GetTranHashes());
    }
  }

  return true;
}

void Lookup::SendGetMicroBlockFromLookup(const vector<BlockHash>& mbHashes) {
  vector<unsigned char> msg = {MessageType::LOOKUP,
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

bool Lookup::ProcessSetDSInfoFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  //#ifndef IS_LOOKUP_NODE

  LOG_MARKER();

  bool initialDS = false;

  PubKey senderPubKey;
  std::deque<std::pair<PubKey, Peer>> dsNodes;
  if (!Messenger::GetLookupSetDSInfoFromSeed(message, offset, senderPubKey,
                                             dsNodes, initialDS)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetDSInfoFromSeed failed.");
    return false;
  }

  if (!LOOKUP_NODE_MODE) {
    if (!VerifyLookupNode(GetLookupNodes(), senderPubKey)) {
      LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
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

  else {
    bool isVerif = true;

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
          LOG_GENERAL(WARNING,
                      "The key from ds comm recvd and from file differs "
                          << dsNodes.at(i).first << " "
                          << m_mediator.m_initialDSCommittee->at(i));
        }
      }

      m_mediator.m_blocklinkchain.SetBuiltDSComm(dsNodes);
    }

    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    *m_mediator.m_DSCommittee = move(dsNodes);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetDSInfoFromSeed sent by "
                  << from << " for numPeers "
                  << m_mediator.m_DSCommittee->size());

    unsigned int i = 0;
    for (auto& ds : *m_mediator.m_DSCommittee) {
      if (GetSyncType() == SyncType::DS_SYNC &&
          ds.second == m_mediator.m_selfPeer) {
        ds.second = Peer();
      }
      LOG_EPOCH(
          INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
          "ProcessSetDSInfoFromSeed recvd peer " << i++ << ": " << ds.second);
    }

    if (m_mediator.m_blocklinkchain.GetBuiltDSComm().size() !=
        m_mediator.m_DSCommittee->size()) {
      isVerif = false;
      LOG_GENERAL(WARNING,
                  "Size of "
                      << m_mediator.m_blocklinkchain.GetBuiltDSComm().size()
                      << " " << m_mediator.m_DSCommittee->size()
                      << " does not match");
    }

    for (i = 0; i < m_mediator.m_blocklinkchain.GetBuiltDSComm().size(); i++) {
      if (!(m_mediator.m_DSCommittee->at(i).first ==
            m_mediator.m_blocklinkchain.GetBuiltDSComm().at(i).first)) {
        LOG_GENERAL(WARNING, "Mis-match of ds comm at" << i);
        isVerif = false;
        break;
      }
    }

    if (isVerif) {
      LOG_GENERAL(INFO, "[DSINFOVERIF]"
                            << " Sucess ");
    }
  }

  //    Data::GetInstance().SetDSPeers(dsPeers);
  //#endif // IS_LOOKUP_NODE

  if ((!LOOKUP_NODE_MODE && m_dsInfoWaitingNotifying &&
       (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)) ||
      (LOOKUP_NODE_MODE && m_syncType == SyncType::NEW_LOOKUP_SYNC &&
       m_dsInfoWaitingNotifying)) {
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Notifying ProcessSetStateFromSeed that DSInfo has been received");
    unique_lock<mutex> lock(m_mutexDSInfoUpdation);
    m_fetchedDSInfo = true;
  }
  cv_dsInfoUpdate.notify_all();

  return true;
}

bool Lookup::ProcessSetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset,
                                       [[gnu::unused]] const Peer& from) {
  // #ifndef IS_LOOKUP_NODE TODO: uncomment later

  LOG_MARKER();

  unique_lock<mutex> lock(m_mutexSetDSBlockFromSeed);

  uint64_t lowBlockNum;
  uint64_t highBlockNum;
  PubKey lookupPubKey;
  std::vector<DSBlock> dsBlocks;
  if (!Messenger::GetLookupSetDSBlockFromSeed(
          message, offset, lowBlockNum, highBlockNum, lookupPubKey, dsBlocks)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupGetDSBlockFromSeed failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  uint64_t latestSynBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;

  if (latestSynBlockNum > highBlockNum) {
    // TODO: We should get blocks from n nodes.
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I already have the block");
  } else {
    if (AlreadyJoinedNetwork()) {
      m_fetchedLatestDSBlock = true;
      cv_latestDSBlock.notify_all();
      return true;
    }

    for (const auto& dsblock : dsBlocks) {
      if (!(m_mediator.m_dsBlockChain.GetBlock(
                dsblock.GetHeader().GetBlockNum()) == DSBlock())) {
        continue;
      }
      m_mediator.m_dsBlockChain.AddBlock(dsblock);
      // Store DS Block to disk
      if (!ARCHIVAL_NODE) {
        vector<unsigned char> serializedDSBlock;
        dsblock.Serialize(serializedDSBlock, 0);
        BlockStorage::GetBlockStorage().PutDSBlock(
            dsblock.GetHeader().GetBlockNum(), serializedDSBlock);
      } else {
        m_mediator.m_archDB->InsertDSBlock(dsblock);
      }
    }

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

  return true;
}

bool Lookup::ProcessSetTxBlockFromSeed(const vector<unsigned char>& message,
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetTxBlockFromSeed failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessSetTxBlockFromSeed sent by " << from << " for blocks "
                                                 << lowBlockNum << " to "
                                                 << highBlockNum);

  if (lowBlockNum > highBlockNum) {
    LOG_GENERAL(
        WARNING,
        "The lowBlockNum is higher the highblocknum, maybe DS epoch ongoing");
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
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I already have the block");
    return false;
  } else {
    auto res = m_mediator.m_validator->CheckTxBlocks(
        txBlocks, m_mediator.m_blocklinkchain.GetBuiltDSComm(),
        m_mediator.m_blocklinkchain.GetLatestBlockLink());
    switch (res) {
      case ValidatorBase::TxBlockValidationMsg::VALID:
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

void LogTxBlock(const TxBlock& txBlock, const uint64_t& epochNum) {
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetType(): " << txBlock.GetHeader().GetType());
  LOG_EPOCH(
      INFO, to_string(epochNum).c_str(),
      "txBlock.GetHeader().GetVersion(): " << txBlock.GetHeader().GetVersion());
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetGasLimit(): "
                << txBlock.GetHeader().GetGasLimit());
  LOG_EPOCH(
      INFO, to_string(epochNum).c_str(),
      "txBlock.GetHeader().GetGasUsed(): " << txBlock.GetHeader().GetGasUsed());
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetBlockNum(): "
                << txBlock.GetHeader().GetBlockNum());
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetMicroBlockInfos().size(): "
                << txBlock.GetMicroBlockInfos().size());
  LOG_EPOCH(
      INFO, to_string(epochNum).c_str(),
      "txBlock.GetHeader().GetNumTxs(): " << txBlock.GetHeader().GetNumTxs());
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetMinerPubKey(): "
                << txBlock.GetHeader().GetMinerPubKey());
  LOG_EPOCH(INFO, to_string(epochNum).c_str(),
            "txBlock.GetHeader().GetStateRootHash(): "
                << txBlock.GetHeader().GetStateRootHash());
}

void Lookup::CommitTxBlocks(const vector<TxBlock>& txBlocks) {
  LOG_GENERAL(INFO, "[TxBlockVerif]"
                        << "Success");
  for (const auto& txBlock : txBlocks) {
    LogTxBlock(txBlock, m_mediator.m_currentEpochNum);

    m_mediator.m_node->AddBlock(txBlock);

    // Store Tx Block to disk
    if (!ARCHIVAL_NODE) {
      vector<unsigned char> serializedTxBlock;
      txBlock.Serialize(serializedTxBlock, 0);
      BlockStorage::GetBlockStorage().PutTxBlock(
          txBlock.GetHeader().GetBlockNum(), serializedTxBlock);
    } else {
      for (const auto& info : txBlock.GetMicroBlockInfos()) {
        if (info.m_txnRootHash != TxnHash()) {
          m_mediator.m_archival->AddToFetchMicroBlockInfo(
              info.m_microBlockHash);
        } else {
          LOG_GENERAL(INFO, "MicroBlock of hash " << info.m_microBlockHash.hex()
                                                  << " empty");
        }
      }
      m_mediator.m_archDB->InsertTxBlock(txBlock);
    }
  }

  m_mediator.m_currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;

  m_mediator.m_consensusID =
      m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW;

  m_mediator.UpdateTxBlockRand();

  if ((m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) &&
      !ARCHIVAL_NODE) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "At new DS epoch now, try getting state from lookup");
    GetStateFromLookupNodes();
  } else if (m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New lookup - always try getting state from other lookup");
    GetStateFromLookupNodes();
  }

  cv_setTxBlockFromSeed.notify_all();
}

bool Lookup::ProcessSetStateDeltaFromSeed(const vector<unsigned char>& message,
                                          unsigned int offset,
                                          const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    cv_setStateDeltaFromSeed.notify_all();
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetStateDeltaFromSeed);

  uint64_t blockNum = 0;
  std::vector<unsigned char> stateDelta;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetStateDeltaFromSeed(message, offset, blockNum,
                                                 lookupPubKey, stateDelta)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetStateDeltaFromSeed failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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

bool Lookup::ProcessSetStateFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetState);
  PubKey lookupPubKey;
  vector<unsigned char> accountStoreBytes;
  if (!Messenger::GetLookupSetStateFromSeed(message, offset, lookupPubKey,
                                            accountStoreBytes)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetStateFromSeed failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  if (!AccountStore::GetInstance().Deserialize(accountStoreBytes, 0)) {
    LOG_GENERAL(WARNING, "Deserialize AccountStore Failed");
    return false;
  }

  if (ARCHIVAL_NODE) {
    LOG_GENERAL(INFO, "Succesfull state change");
    return true;
  }

  if (!LOOKUP_NODE_MODE) {
    if (m_syncType == SyncType::NEW_SYNC ||
        m_syncType == SyncType::NORMAL_SYNC) {
      m_dsInfoWaitingNotifying = true;

      GetDSInfoFromLookupNodes();

      {
        unique_lock<mutex> lock(m_mutexDSInfoUpdation);
        while (!m_fetchedDSInfo) {
          LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Waiting for DSInfo");

          if (cv_dsInfoUpdate.wait_for(
                  lock, chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
              std::cv_status::timeout) {
            // timed out
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Timed out waiting for DSInfo");
            m_dsInfoWaitingNotifying = false;
            return false;
          }
          LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Get ProcessDsInfo Notified");
          m_dsInfoWaitingNotifying = false;
        }
        m_fetchedDSInfo = false;
      }

      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "DSInfo received -> Ask lookup to let me know when to "
                "start PoW");

      // Ask lookup to inform me when it's time to do PoW
      vector<unsigned char> getpowsubmission_message = {
          MessageType::LOOKUP, LookupInstructionType::GETSTARTPOWFROMSEED};

      if (!Messenger::SetLookupGetStartPoWFromSeed(
              getpowsubmission_message, MessageOffset::BODY,
              m_mediator.m_selfPeer.m_listenPortHost)) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Messenger::SetLookupGetStartPoWFromSeed failed.");
        return false;
      }

      m_mediator.m_lookup->SendMessageToRandomLookupNode(
          getpowsubmission_message);
    } else if (m_syncType == SyncType::DS_SYNC) {
      if (!m_currDSExpired && m_mediator.m_ds->m_latestActiveDSBlockNum <
                                  m_mediator.m_dsBlockChain.GetLastBlock()
                                      .GetHeader()
                                      .GetBlockNum()) {
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

    GetDSInfoFromLookupNodes();

    {
      unique_lock<mutex> lock(m_mutexDSInfoUpdation);
      while (!m_fetchedDSInfo) {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Waiting for DSInfo");

        if (cv_dsInfoUpdate.wait_for(lock,
                                     chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
            std::cv_status::timeout) {
          // timed out
          LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Timed out waiting for DSInfo");
          m_dsInfoWaitingNotifying = false;
          return false;
        }
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Get ProcessDsInfo Notified");
        m_dsInfoWaitingNotifying = false;
      }
      m_fetchedDSInfo = false;
    }

    if (!m_currDSExpired) {
      if (FinishNewJoinAsLookup()) {
        SetSyncType(SyncType::NO_SYNC);
        m_isFirstLoop = true;
      }
    }
    m_currDSExpired = false;
  }

  return true;
}

bool Lookup::ProcessGetTxnsFromLookup(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  vector<TxnHash> txnhashes;
  txnhashes.clear();

  uint32_t portNo = 0;
  if (!Messenger::GetLookupGetTxnsFromLookup(message, offset, txnhashes,
                                             portNo)) {
    LOG_GENERAL(WARNING, "Failed to Process");
    return false;
  }

  if (txnhashes.size() == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return true;
  }

  vector<TransactionWithReceipt> txnvector;
  for (const auto& txnhash : txnhashes) {
    shared_ptr<TransactionWithReceipt> txn;
    if (!BlockStorage::GetBlockStorage().GetTxBody(txnhash, txn)) {
      LOG_GENERAL(WARNING, "Could not find " << txnhash);
      continue;
    }
    txnvector.emplace_back(*txn);
  }
  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);

  vector<unsigned char> setTxnMsg = {MessageType::LOOKUP,
                                     LookupInstructionType::SETTXNFROMLOOKUP};

  if (!Messenger::SetLookupSetTxnsFromLookup(setTxnMsg, MessageOffset::BODY,
                                             m_mediator.m_selfKey, txnvector)) {
    LOG_GENERAL(WARNING, "Unable to Process");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, setTxnMsg);

  return true;
}

bool Lookup::ProcessSetTxnsFromLookup(const vector<unsigned char>& message,
                                      unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  vector<TransactionWithReceipt> txns;
  PubKey lookupPubKey;

  if (!Messenger::GetLookupSetTxnsFromLookup(message, offset, lookupPubKey,
                                             txns)) {
    LOG_GENERAL(WARNING, "Failed to Process");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  if (ARCHIVAL_NODE) {
    m_mediator.m_archival->AddTxnToDB(txns, *m_mediator.m_archDB);
  }
  return true;
}

void Lookup::SendGetTxnFromLookup(const vector<TxnHash>& txnhashes) {
  vector<unsigned char> msg = {MessageType::LOOKUP,
                               LookupInstructionType::GETTXNFROMLOOKUP};

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

bool Lookup::ProcessSetTxBodyFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (AlreadyJoinedNetwork()) {
    return true;
  }

  unique_lock<mutex> lock(m_mutexSetTxBodyFromSeed);

  TxnHash tranHash;
  TransactionWithReceipt twr;

  if (!Messenger::GetLookupSetTxBodyFromSeed(message, offset, tranHash, twr)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetTxBodyFromSeed failed.");
    return false;
  }

  // if (!AccountStore::GetInstance().UpdateAccounts(
  //         m_mediator.m_currentEpochNum - 1, transaction))
  // {
  //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
  //     return false;
  // }
  vector<unsigned char> serializedTxBody;
  twr.Serialize(serializedTxBody, 0);
  BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);

  return true;
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "New DS epoch check failed");
    return false;
  }

  uint64_t curDsBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  m_mediator.UpdateDSBlockRand();
  auto dsBlockRand = m_mediator.m_dsBlockRand;
  array<unsigned char, 32> txBlockRand{};

  if (CheckStateRoot()) {
    // Attempt PoW
    m_startedPoW = true;
    dsBlockRand = m_mediator.m_dsBlockRand;
    txBlockRand = m_mediator.m_txBlockRand;

    m_mediator.m_node->SetState(Node::POW_SUBMISSION);
    POW::GetInstance().EthashConfigureClient(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
        FULL_DATASET_MINE);

    this_thread::sleep_for(chrono::seconds(NEW_NODE_POW_DELAY));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Starting PoW for new ds block number " << curDsBlockNum + 1);

    m_mediator.m_node->StartPoW(
        curDsBlockNum + 1,
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty(),
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty(),
        dsBlockRand, txBlockRand, lookupIndex);
  } else {
    LOG_GENERAL(WARNING, "State root check failed");
    return false;
  }

  // Check whether is the new node connected to the network. Else, initiate
  // re-sync process again.
  this_thread::sleep_for(chrono::seconds(
      POW_WINDOW_IN_SECONDS + POWPACKETSUBMISSION_WINDOW_IN_SECONDS +
      2 * NEW_NODE_SYNC_INTERVAL + (TX_DISTRIBUTE_TIME_IN_MS / 1000)));
  m_startedPoW = false;
  if (GetSyncType() != SyncType::NO_SYNC) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Not yet connected to network");
    m_mediator.m_node->SetState(Node::SYNC);
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I have successfully join the network");
  }

  return true;
}

bool Lookup::ProcessSetLookupOffline(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from) {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetLookupOffline not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;

  if (!Messenger::GetLookupSetLookupOffline(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetLookupOffline failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    auto iter =
        std::find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                     [&requestingNode](const std::pair<PubKey, Peer>& node) {
                       return node.second == requestingNode;
                     });
    if (iter != m_lookupNodes.end()) {
      m_lookupNodesOffline.emplace_back(*iter);
      m_lookupNodes.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "The Peer Info is not in m_lookupNodes");
      return false;
    }
  }
  return true;
}

bool Lookup::ProcessSetLookupOnline(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessSetLookupOnline not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;
  PubKey lookupPubKey;
  if (!Messenger::GetLookupSetLookupOnline(message, offset, portNo,
                                           lookupPubKey)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetLookupOnline failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    auto iter =
        std::find_if(m_lookupNodesOffline.cbegin(), m_lookupNodesOffline.cend(),
                     [&requestingNode](const std::pair<PubKey, Peer>& node) {
                       return node.second == requestingNode;
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

bool Lookup::ProcessGetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetOfflineLookups not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetOfflineLookups(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetOfflineLookups failed.");
    return false;
  }

  uint128_t ipAddr = from.m_ipAddress;
  Peer requestingNode(ipAddr, portNo);
  LOG_GENERAL(INFO, requestingNode);

  vector<unsigned char> offlineLookupsMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETOFFLINELOOKUPS};

  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    std::vector<Peer> lookupNodesOffline;
    for (const auto& pairPubKeyPeer : m_lookupNodesOffline)
      lookupNodesOffline.push_back(pairPubKeyPeer.second);

    if (!Messenger::SetLookupSetOfflineLookups(
            offlineLookupsMessage, MessageOffset::BODY, m_mediator.m_selfKey,
            lookupNodesOffline)) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Messenger::SetLookupSetOfflineLookups failed.");
      return false;
    }

    for (const auto& peer : m_lookupNodesOffline) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "IP:" << peer.second.GetPrintableIPAddress());
    }
  }

  P2PComm::GetInstance().SendMessage(requestingNode, offlineLookupsMessage);
  return true;
}

bool Lookup::ProcessSetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from) {
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupSetOfflineLookups failed.");
    return false;
  }

  if (!VerifyLookupNode(GetLookupNodes(), lookupPubKey)) {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessSetOfflineLookups sent by "
                << from << " for numOfflineLookups " << nodes.size());

  unsigned int i = 0;
  for (const auto& peer : nodes) {
    std::lock_guard<std::mutex> lock(m_mutexLookupNodes);
    // Remove selfPeerInfo from m_lookupNodes
    auto iter = std::find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                             [&peer](const std::pair<PubKey, Peer>& node) {
                               return node.second == peer;
                             });
    if (iter != m_lookupNodes.end()) {
      m_lookupNodesOffline.emplace_back(*iter);
      m_lookupNodes.erase(iter);

      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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

bool Lookup::ProcessRaiseStartPoW(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from) {
  // Message = empty

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessRaiseStartPoW not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  // DS leader has informed me that it's time to start PoW
  m_receivedRaiseStartPoW = true;
  cv_startPoWSubmission.notify_all();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Threads running ProcessGetStartPoWFromSeed notified to start PoW");

  // Sleep for a while, then let all remaining threads running
  // ProcessGetStartPoWFromSeed know that it's too late to do PoW Sleep time =
  // time it takes for new node to try getting DSInfo + actual PoW window
  this_thread::sleep_for(
      chrono::seconds(NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
                      POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
  m_receivedRaiseStartPoW = false;
  cv_startPoWSubmission.notify_all();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Threads running ProcessGetStartPoWFromSeed notified it's too "
            "late to start PoW");

  return true;
}

bool Lookup::ProcessGetStartPoWFromSeed(const vector<unsigned char>& message,
                                        unsigned int offset, const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetStartPoWFromSeed not expected to be "
                "called from other than the LookUp node.");
    return true;
  }

  uint32_t portNo = 0;

  if (!Messenger::GetLookupGetStartPoWFromSeed(message, offset, portNo)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetStartPoWFromSeed failed.");
    return false;
  }

  // Normally I'll get this message from new nodes at the vacuous epoch
  // Wait a while if I haven't received RAISESTARTPOW from DS leader yet
  // Wait time = time it takes to finish the vacuous epoch (or at least part of
  // it) + actual PoW window
  if (!m_receivedRaiseStartPoW) {
    std::unique_lock<std::mutex> cv_lk(m_MutexCVStartPoWSubmission);

    if (cv_startPoWSubmission.wait_for(
            cv_lk,
            std::chrono::seconds(POW_WINDOW_IN_SECONDS +
                                 POWPACKETSUBMISSION_WINDOW_IN_SECONDS)) ==
        std::cv_status::timeout) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Timed out waiting for DS leader to raise startPoW");
      return false;
    }

    if (!m_receivedRaiseStartPoW) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "PoW duration already passed");
      return false;
    }
  }

  // Tell the new node that it's time to start PoW
  vector<unsigned char> setstartpow_message = {
      MessageType::LOOKUP, LookupInstructionType::SETSTARTPOWFROMSEED};
  if (!Messenger::SetLookupSetStartPoWFromSeed(
          setstartpow_message, MessageOffset::BODY,
          m_mediator.m_currentEpochNum, m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetStartPoWFromSeed failed.");
    return false;
  }
  P2PComm::GetInstance().SendMessage(Peer(from.m_ipAddress, portNo),
                                     setstartpow_message);

  return true;
}

bool Lookup::ProcessSetStartPoWFromSeed(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from) {
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetStartPoWFromSeed failed.");
    return false;
  }

  auto vecLookupNodes = GetLookupNodes();
  auto it = std::find_if(vecLookupNodes.cbegin(), vecLookupNodes.cend(),
                         [&lookupPubKey](const std::pair<PubKey, Peer>& node) {
                           return node.first == lookupPubKey;
                         });
  uint32_t index;
  if (it != vecLookupNodes.cend()) {
    index = distance(vecLookupNodes.cbegin(), it);
  } else {
    LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
              "The message sender pubkey: "
                  << lookupPubKey << " is not in my lookup node list.");
    return false;
  }

  InitMining(index);

  if (m_syncType == SyncType::DS_SYNC) {
    if (!m_currDSExpired && m_mediator.m_ds->m_latestActiveDSBlockNum <
                                m_mediator.m_dsBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()) {
      m_isFirstLoop = true;
      SetSyncType(SyncType::NO_SYNC);
      m_mediator.m_ds->FinishRejoinAsDS();
    }

    m_currDSExpired = false;
  }

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
    GetDSInfoFromLookupNodes();
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

Peer Lookup::GetLookupPeerToRsync() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetLookupPeerToRsync not expected to be called "
                "from other than the LookUp node.");
    return Peer();
  }

  LOG_MARKER();

  std::vector<Peer> t_Peers;
  {
    lock_guard<mutex> lock(m_mutexLookupNodes);
    for (const auto& p : m_lookupNodes) {
      if (p.second != m_mediator.m_selfPeer) {
        t_Peers.emplace_back(p.second);
      }
    }
  }

  int index = rand() % t_Peers.size();

  return t_Peers[index];
}

std::vector<unsigned char> Lookup::ComposeGetLookupOfflineMessage() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetLookupOfflineMessage not expected to be "
                "called from other than the LookUp node.");
    return std::vector<unsigned char>();
  }

  LOG_MARKER();

  vector<unsigned char> getLookupOfflineMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETLOOKUPOFFLINE};

  if (!Messenger::SetLookupSetLookupOffline(
          getLookupOfflineMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetLookupSetLookupOffline failed.");
    return {};
  }

  return getLookupOfflineMessage;
}

std::vector<unsigned char> Lookup::ComposeGetLookupOnlineMessage() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetLookupOnlineMessage not expected to be "
                "called from other than the LookUp node.");
    return std::vector<unsigned char>();
  }

  LOG_MARKER();

  vector<unsigned char> getLookupOnlineMessage = {
      MessageType::LOOKUP, LookupInstructionType::SETLOOKUPONLINE};

  if (!Messenger::SetLookupSetLookupOnline(
          getLookupOnlineMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost,
          m_mediator.m_selfKey.second)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
    auto iter = std::find_if(m_lookupNodes.begin(), m_lookupNodes.end(),
                             [&selfPeer](const std::pair<PubKey, Peer>& node) {
                               return node.second == selfPeer;
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

bool Lookup::GetMyLookupOnline() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::GetMyLookupOnline not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  LOG_MARKER();
  bool found = false;
  {
    std::lock_guard<std::mutex> lock(m_mutexLookupNodes);
    auto selfPeer(m_mediator.m_selfPeer);
    auto iter =
        std::find_if(m_lookupNodesOffline.begin(), m_lookupNodesOffline.end(),
                     [&selfPeer](const std::pair<PubKey, Peer>& node) {
                       return node.second == selfPeer;
                     });
    if (iter != m_lookupNodesOffline.end()) {
      found = true;
      m_lookupNodes.emplace_back(*iter);
      m_lookupNodesOffline.erase(iter);
    } else {
      LOG_GENERAL(WARNING, "My Peer Info is not in m_lookupNodesOffline");
      return false;
    }
  }

  if (found) {
    SendMessageToLookupNodesSerial(ComposeGetLookupOnlineMessage());
  }
  return true;
}

bool Lookup::RsyncTxBodies() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::RsyncTxBodies not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  LOG_MARKER();
  const Peer& p = GetLookupPeerToRsync();
  string ipAddr = std::string(p.GetPrintableIPAddress());
  string port = std::to_string(p.m_listenPortHost);
  string dbNameStr =
      BlockStorage::GetBlockStorage().GetDBName(BlockStorage::TX_BODY)[0];
  string cmdStr;
  if (ipAddr == "127.0.0.1" || ipAddr == "localhost") {
    string indexStr = port;
    indexStr.erase(indexStr.begin());
    cmdStr = "rsync -iraz --size-only ../node_0" + indexStr + "/" +
             PERSISTENCE_PATH + "/" + dbNameStr + "/* " + PERSISTENCE_PATH +
             "/" + dbNameStr + "/";
  } else {
    cmdStr =
        "rsync -iraz --size-only -e \"ssh -o "
        "StrictHostKeyChecking=no\" ubuntu@" +
        ipAddr + ":" + REMOTE_TEST_DIR + "/" + PERSISTENCE_PATH + "/" +
        dbNameStr + "/* " + PERSISTENCE_PATH + "/" + dbNameStr + "/";
  }
  LOG_GENERAL(INFO, cmdStr);

  string output;
  if (!SysCommand::ExecuteCmdWithOutput(cmdStr, output)) {
    return false;
  }
  LOG_GENERAL(INFO, "RunRsync: " << output);
  return true;
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

bool Lookup::FinishNewJoinAsLookup() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::FinishNewJoinAsLookup not expected to be called "
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
          ins_byte != LookupInstructionType::SETDIRBLOCKSFROMSEED);
}

std::vector<unsigned char> Lookup::ComposeGetOfflineLookupNodes() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ComposeGetOfflineLookupNodes not expected to be "
                "called from the LookUp node.");
    return std::vector<unsigned char>();
  }

  LOG_MARKER();

  vector<unsigned char> getCurrLookupsMessage = {
      MessageType::LOOKUP, LookupInstructionType::GETOFFLINELOOKUPS};

  if (!Messenger::SetLookupGetOfflineLookups(
          getCurrLookupsMessage, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
  SendMessageToLookupNodesSerial(ComposeGetOfflineLookupNodes());
  return true;
}

bool Lookup::ProcessGetDirectoryBlocksFromSeed(
    const vector<unsigned char>& message, unsigned int offset,
    const Peer& from) {
  uint64_t index_num;
  uint32_t portNo;

  LOG_MARKER();

  if (!Messenger::GetLookupGetDirectoryBlocksFromSeed(message, offset, portNo,
                                                      index_num)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupGetDirectoryBlocksFromSeed failed");
    return false;
  }

  vector<unsigned char> msg = {MessageType::LOOKUP,
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

  if (!Messenger::SetLookupSetDirectoryBlocksFromSeed(msg, MessageOffset::BODY,
                                                      dirBlocks, index_num)) {
    LOG_GENERAL(WARNING,
                "Messenger::SetLookupSetDirectoryBlocksFromSeed failed");
    return false;
  }

  P2PComm::GetInstance().SendMessage(peer, msg);

  return true;
}

bool Lookup::ProcessSetDirectoryBlocksFromSeed(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
      dirBlocks;
  uint64_t index_num;

  if (!Messenger::GetLookupSetDirectoryBlocksFromSeed(message, offset,
                                                      dirBlocks, index_num)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupSetDirectoryBlocksFromSeed failed");
    return false;
  }

  if (dirBlocks.empty()) {
    LOG_GENERAL(WARNING, "No Directory blocks sent/ I have the latest blocks");
    return false;
  }

  if (m_mediator.m_blocklinkchain.GetLatestIndex() >= index_num) {
    LOG_GENERAL(INFO, "Already have dir blocks");
    return true;
  }

  deque<pair<PubKey, Peer>> newDScomm;

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
        m_syncType == SyncType::NEW_LOOKUP_SYNC) {
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
  unique_lock<mutex> lock(m_mutexSetTxBlockFromSeed);

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

void Lookup::ComposeAndSendGetDirectoryBlocksFromSeed(
    const uint64_t& index_num) {
  LOG_MARKER();
  vector<unsigned char> message = {MessageType::LOOKUP,
                                   LookupInstructionType::GETDIRBLOCKSFROMSEED};

  if (!Messenger::SetLookupGetDirectoryBlocksFromSeed(
          message, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost,
          index_num)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetDirectoryBlocksFromSeed");
    return;
  }

  SendMessageToRandomLookupNode(message);
}

bool Lookup::Execute(const vector<unsigned char>& message, unsigned int offset,
                     const Peer& from) {
  LOG_MARKER();

  bool result = true;

  typedef bool (Lookup::*InstructionHandler)(const vector<unsigned char>&,
                                             unsigned int, const Peer&);

  InstructionHandler ins_handlers[] = {
      &Lookup::ProcessGetSeedPeersFromLookup,
      &Lookup::ProcessSetSeedPeersFromLookup,
      &Lookup::ProcessGetDSInfoFromSeed,
      &Lookup::ProcessSetDSInfoFromSeed,
      &Lookup::ProcessGetDSBlockFromSeed,
      &Lookup::ProcessSetDSBlockFromSeed,
      &Lookup::ProcessGetTxBlockFromSeed,
      &Lookup::ProcessSetTxBlockFromSeed,
      &Lookup::ProcessGetTxBodyFromSeed,
      &Lookup::ProcessSetTxBodyFromSeed,
      &Lookup::ProcessGetNetworkId,
      &Lookup::ProcessGetNetworkId,
      &Lookup::ProcessGetStateFromSeed,
      &Lookup::ProcessSetStateFromSeed,
      &Lookup::ProcessSetLookupOffline,
      &Lookup::ProcessSetLookupOnline,
      &Lookup::ProcessGetOfflineLookups,
      &Lookup::ProcessSetOfflineLookups,
      &Lookup::ProcessRaiseStartPoW,
      &Lookup::ProcessGetStartPoWFromSeed,
      &Lookup::ProcessSetStartPoWFromSeed,
      &Lookup::ProcessGetShardFromSeed,
      &Lookup::ProcessSetShardFromSeed,
      &Lookup::ProcessGetMicroBlockFromLookup,
      &Lookup::ProcessSetMicroBlockFromLookup,
      &Lookup::ProcessGetTxnsFromLookup,
      &Lookup::ProcessSetTxnsFromLookup,
      &Lookup::ProcessGetDirectoryBlocksFromSeed,
      &Lookup::ProcessSetDirectoryBlocksFromSeed,
      &Lookup::ProcessGetStateDeltaFromSeed,
      &Lookup::ProcessSetStateDeltaFromSeed,
      &Lookup::ProcessVCGetLatestDSTxBlockFromSeed};
  const unsigned char ins_byte = message.at(offset);
  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  if (LOOKUP_NODE_MODE) {
    if (ToBlockMessage(ins_byte)) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Ignore lookup message");
      return false;
    }
  }

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);
    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_GENERAL(WARNING,
                "Unknown instruction byte " << hex << (unsigned int)ins_byte);
  }

  return result;
}

bool Lookup::AlreadyJoinedNetwork() {
  if (ARCHIVAL_NODE) {
    return false;
  }
  return m_syncType == SyncType::NO_SYNC;
}

bool Lookup::AddToTxnShardMap(const Transaction& tx, uint32_t shardId) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::AddToTxnShardMap not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  lock_guard<mutex> g(m_txnShardMapMutex);

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

void Lookup::SenderTxnBatchThread() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SenderTxnBatchThread not expected to be called from "
                "other than the LookUp node.");
    return;
  }
  LOG_MARKER();

  auto main_func = [this]() mutable -> void {
    uint32_t numShards = 0;
    while (true) {
      if (!m_mediator.GetIsVacuousEpoch()) {
        {
          lock_guard<mutex> g(m_mediator.m_ds->m_mutexShards);
          numShards = m_mediator.m_ds->m_shards.size();
        }
        if (numShards == 0) {
          this_thread::sleep_for(chrono::milliseconds(1000));
          continue;
        }
        SendTxnPacketToNodes(numShards);
      }
      break;
    }
  };
  DetachedFunction(1, main_func);
}

void Lookup::SendTxnPacketToNodes(uint32_t numShards) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::SendTxnPacketToNodes not expected to be called from "
                "other than the LookUp node.");
    return;
  }

  map<uint32_t, vector<Transaction>> mp;

  if (!GenTxnToSend(NUM_TXN_TO_SEND_PER_ACCOUNT, mp, numShards)) {
    LOG_GENERAL(WARNING, "GenTxnToSend failed");
    // return;
  }

  // allow receving nodes to be ready with latest DS block ( Only** for first
  // txn epoch of every ds epoch )
  if ((m_mediator.m_currentEpochNum == 1) ||
      (m_mediator.m_currentEpochNum > 1 &&
       m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting for " << LOOKUP_DELAY_SEND_TXNPACKET_IN_MS
                             << " ms before sending txn packets to shards");
    this_thread::sleep_for(
        chrono::milliseconds(LOOKUP_DELAY_SEND_TXNPACKET_IN_MS));
  }

  for (unsigned int i = 0; i < numShards + 1; i++) {
    vector<unsigned char> msg = {MessageType::NODE,
                                 NodeInstructionType::FORWARDTXNPACKET};
    bool result = false;

    {
      lock_guard<mutex> g(m_txnShardMapMutex);
      auto transactionNumber = mp[i].size();

      LOG_GENERAL(INFO, "Transaction number generated: " << transactionNumber);

      if (m_txnShardMap[i].empty() && mp[i].empty()) {
        LOG_GENERAL(INFO, "No txns to send to shard " << i);
        continue;
      }

      result = Messenger::SetNodeForwardTxnBlock(
          msg, MessageOffset::BODY, m_mediator.m_currentEpochNum,
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(), i,
          m_mediator.m_selfKey, m_txnShardMap[i], mp[i]);
    }

    if (!result) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
        uint32_t leader_id =
            lastBlockHash % m_mediator.m_ds->m_shards.at(i).size();
        LOG_GENERAL(INFO, "Shard leader id " << leader_id);

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
        Peer dsLeaderPeer;
        if (Node::GetDSLeaderPeer(
                m_mediator.m_blocklinkchain.GetLatestBlockLink(),
                m_mediator.m_dsBlockChain.GetLastBlock(),
                *m_mediator.m_DSCommittee, m_mediator.m_currentEpochNum,
                dsLeaderPeer)) {
          toSend.push_back(dsLeaderPeer);
        }

        for (auto const& i : *m_mediator.m_DSCommittee) {
          if (toSend.size() < NUM_NODES_TO_SEND_LOOKUP &&
              i.second != dsLeaderPeer) {
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

bool Lookup::VerifyLookupNode(const VectorOfLookupNode& vecLookupNodes,
                              const PubKey& pubKeyToVerify) {
  auto iter =
      std::find_if(vecLookupNodes.cbegin(), vecLookupNodes.cend(),
                   [&pubKeyToVerify](const std::pair<PubKey, Peer>& node) {
                     return node.first == pubKeyToVerify;
                   });
  return vecLookupNodes.cend() != iter;
}

bool Lookup::ProcessVCGetLatestDSTxBlockFromSeed(
    const vector<unsigned char>& message, unsigned int offset,
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetLookupGetSeedPeers failed.");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessVCGetLatestDSTxBlockFromSeed (pre) requested by "
                << from << " for ds blocks " << dsLowBlockNum << " to "
                << dsHighBlockNum << " and tx blocks " << txLowBlockNum
                << " to " << txHighBlockNum << " with receiving port "
                << listenPort);

  vector<DSBlock> dsBlocks;
  RetrieveDSBlocks(dsBlocks, dsLowBlockNum, dsHighBlockNum, true);

  vector<TxBlock> txBlocks;
  RetrieveTxBlocks(txBlocks, txLowBlockNum, txHighBlockNum);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "ProcessVCGetLatestDSTxBlockFromSeed (final) requested by "
                << from << " for ds blocks " << dsLowBlockNum << " to "
                << dsHighBlockNum << " and tx blocks " << txLowBlockNum
                << " to " << txHighBlockNum << " with receiving port "
                << listenPort);

  vector<unsigned char> dsTxBlocksMessage = {
      MessageType::DIRECTORY, DSInstructionType::VCPUSHLATESTDSTXBLOCK};

  if (!Messenger::SetVCNodeSetDSTxBlockFromSeed(
          dsTxBlocksMessage, MessageOffset::BODY, m_mediator.m_selfKey,
          dsBlocks, txBlocks)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetVCNodeSetDSTxBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, listenPort);
  P2PComm::GetInstance().SendMessage(requestingNode, dsTxBlocksMessage);
  return true;
}

void Lookup::SetSyncType(SyncType syncType) {
  m_syncType = syncType;
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Set sync type to " << syncType);
}
