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
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libRemoteStorageDB/RemoteStorageDB.h"
#include "libServer/GetWorkServer.h"
#include "libServer/LookupServer.h"
#include "libServer/StakingServer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/RandomGenerator.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

const int32_t MAX_FETCH_BLOCK_RETRIES = 5;

Lookup::Lookup(Mediator& mediator, SyncType syncType, bool multiplierSyncMode,
               PairOfKey extSeedKey)
    : m_mediator(mediator) {
  m_syncType.store(SyncType::NO_SYNC);
  MULTIPLIER_SYNC_MODE = multiplierSyncMode;
  LOG_GENERAL(INFO, "MULTIPLIER_SYNC_MODE is set to " << MULTIPLIER_SYNC_MODE);
  vector<SyncType> ignorable_syncTypes = {NO_SYNC, DB_VERIF};
  if (syncType >= SYNC_TYPE_COUNT) {
    LOG_GENERAL(FATAL, "Invalid SyncType");
  }
  if (find(ignorable_syncTypes.begin(), ignorable_syncTypes.end(), syncType) ==
      ignorable_syncTypes.end()) {
    m_syncType = syncType;
  }
  SetLookupNodes();
  SetAboveLayer(m_seedNodes, "node.upper_seed");
  if (!MULTIPLIER_SYNC_MODE) {
    SetAboveLayer(m_l2lDataProviders, "node.l2l_data_providers");
  }
  if (LOOKUP_NODE_MODE) {
    SetDSCommitteInfo();
  }
  m_sendSCCallsToDS = false;

  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    m_extSeedKey = std::move(extSeedKey);
  }
}

Lookup::~Lookup() {}

void Lookup::InitSync() {
  LOG_MARKER();
  auto func = [this]() -> void {
    uint64_t dsBlockNum = 0;
    uint64_t txBlockNum = 0;

    // Hack to allow seed server to be restarted so as to get my newlookup ip
    // and register me with multiplier.
    this_thread::sleep_for(chrono::seconds(NEW_LOOKUP_SYNC_DELAY_IN_SECONDS));

    if (m_seedNodes.empty()) {
      SetAboveLayer(
          m_seedNodes,
          "node.upper_seed");  // since may have called CleanVariable earlier
    }

    if (!MULTIPLIER_SYNC_MODE && m_l2lDataProviders.empty()) {
      SetAboveLayer(m_l2lDataProviders, "node.l2l_data_providers");
    }

    // Send whitelist request to seeds, in case it was blacklisted if was
    // restarted.
    if (m_mediator.m_node->ComposeAndSendRemoveNodeFromBlacklist(
            Node::LOOKUP)) {
      this_thread::sleep_for(
          chrono::seconds(REMOVENODEFROMBLACKLIST_DELAY_IN_SECONDS));
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
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1, true,
          LOOKUP_NODE_MODE);
      GetTxBlockFromSeedNodes(txBlockNum, 0);

      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
    // Ask for the sharding structure from lookup
    ComposeAndSendGetShardingStructureFromSeed();
    std::unique_lock<std::mutex> cv_lk(m_mutexShardStruct);
    if (cv_shardStruct.wait_for(
            cv_lk, std::chrono::seconds(GETSHARD_TIMEOUT_IN_SECONDS)) ==
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
        struct in_addr ip_addr {};
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

  m_lookupNodesStatic = m_lookupNodes;

  /*
    For testing with gentxn, distribute the genesis accounts evenly across the
    number of dispatching lookups. Then for each lookup's accounts, split the
    list evenly in two so that the lookup can continuously send transactions
    across alternating epochs. Finally, to achieve even distribution across
    shards, each epoch's list of accounts should have an equal number of
    addresses that belong to a shard. For example: assuming 3 lookups and 3
    shards: <accounts> <account></account> -> for lookup 0 shard 0 set 1
      <account></account> -> for lookup 0 shard 1 set 1
      <account></account> -> for lookup 0 shard 2 set 1
      <account></account> -> for lookup 0 shard 0 set 2
      <account></account> -> for lookup 0 shard 1 set 2
      <account></account> -> for lookup 0 shard 2 set 2
      ...
      <account></account> -> for lookup 2 shard 0 set 2
      <account></account> -> for lookup 2 shard 1 set 2
      <account></account> -> for lookup 2 shard 2 set 2
  */
  if (USE_REMOTE_TXN_CREATOR && GENESIS_WALLETS.size() > 0) {
    const unsigned int myLookupIndex = std::distance(
        m_lookupNodesStatic.begin(),
        find_if(m_lookupNodesStatic.begin(), m_lookupNodesStatic.end(),
                [&](const PairOfNode& x) {
                  return (m_mediator.m_selfKey.second == x.first);
                }));
    const unsigned int indexBeg =
        myLookupIndex * (GENESIS_WALLETS.size() / NUM_DISPATCHERS);
    const unsigned int indexMid =
        indexBeg + (GENESIS_WALLETS.size() / NUM_DISPATCHERS) / 2;
    const unsigned int indexEnd =
        (myLookupIndex + 1) * (GENESIS_WALLETS.size() / NUM_DISPATCHERS);

    LOG_GENERAL(INFO, "I am dispatcher number " << (myLookupIndex + 1) << " of "
                                                << NUM_DISPATCHERS);

    for (unsigned int i = indexBeg; i < indexMid; i++) {
      const auto& addrStr = GENESIS_WALLETS.at(i);
      bytes addrBytes;
      if (!DataConversion::HexStrToUint8Vec(addrStr, addrBytes)) {
        continue;
      }
      m_myGenesisAccounts1.emplace_back(Address(addrBytes));
    }

    for (unsigned int i = indexMid; i < indexEnd; i++) {
      const auto& addrStr = GENESIS_WALLETS.at(i);
      bytes addrBytes;
      if (!DataConversion::HexStrToUint8Vec(addrStr, addrBytes)) {
        continue;
      }
      m_myGenesisAccounts2.emplace_back(Address(addrBytes));
    }
  }
}

void Lookup::SetAboveLayer(VectorOfNode& aboveLayer, const string& xml_node) {
  using boost::property_tree::ptree;
  ptree pt;
  read_xml("constants.xml", pt);
  aboveLayer.clear();
  for (const ptree::value_type& v : pt.get_child(xml_node)) {
    if (v.first == "peer") {
      struct in_addr ip_addr {};
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      Peer node((uint128_t)ip_addr.s_addr, v.second.get<uint32_t>("port"));
      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(v.second.get<std::string>("pubkey"),
                                            pubkeyBytes)) {
        continue;
      }

      PubKey pubKey(pubkeyBytes, 0);
      string url = v.second.get<string>("hostname");
      if (!url.empty()) {
        node.SetHostname(url);
      }
      aboveLayer.emplace_back(pubKey, node);
    }
  }
}

bool Lookup::AddToWhitelistExtSeed(const PubKey& pubKey) {
  lock_guard<mutex> g(m_mutexExtSeedWhitelisted);
  if (m_extSeedWhitelisted.emplace(pubKey).second) {
    return BlockStorage::GetBlockStorage().PutExtSeedPubKey(pubKey);
  }
  return false;
}

bool Lookup::RemoveFromWhitelistExtSeed(const PubKey& pubKey) {
  lock_guard<mutex> g(m_mutexExtSeedWhitelisted);
  if (m_extSeedWhitelisted.erase(pubKey) > 0) {
    return BlockStorage::GetBlockStorage().DeleteExtSeedPubKey(pubKey);
  }
  return false;
}

VectorOfNode Lookup::GetSeedNodes() const {
  if (!MULTIPLIER_SYNC_MODE) {
    lock_guard<mutex> g(m_mutexL2lDataProviders);
    return m_l2lDataProviders;
  } else {
    lock_guard<mutex> g(m_mutexSeedNodes);
    return m_seedNodes;
  }
}

std::once_flag generateReceiverOnce;

Address GenOneReceiver() {
  static Address receiverAddr;
  std::call_once(generateReceiverOnce, []() {
    auto receiver = Schnorr::GenKeyPair();
    receiverAddr = Account::GetAddressFromPublicKey(receiver.second);
    LOG_GENERAL(INFO, "Generate testing transaction receiver " << receiverAddr);
  });
  return receiverAddr;
}

Transaction CreateValidTestingTransaction(PrivKey& fromPrivKey,
                                          PubKey& fromPubKey,
                                          const Address& toAddr,
                                          const uint128_t& amount,
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
  // Schnorr::Sign(buf, fromPrivKey, fromPubKey, sig);

  // bytes sigBuf;
  // sig.Serialize(sigBuf, 0);
  // txn.SetSignature(sigBuf);

  return txn;
}

bool Lookup::GenTxnToSend(size_t num_txn, vector<Transaction>& shardTxn,
                          vector<Transaction>& DSTxn) {
  vector<Transaction> txns;
  unsigned int NUM_TXN_TO_DS = num_txn / GENESIS_WALLETS.size();

  for (auto& addrStr : GENESIS_WALLETS) {
    bytes tempAddrBytes;
    if (!DataConversion::HexStrToUint8Vec(addrStr, tempAddrBytes)) {
      continue;
    }
    Address addr{tempAddrBytes};

    txns.clear();

    uint64_t nonce;

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      auto account = AccountStore::GetInstance().GetAccount(addr);

      if (!account) {
        LOG_GENERAL(WARNING, "Failed to get genesis account!");
        return false;
      }

      nonce = account->GetNonce();
    }

    if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                     num_txn, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns from file");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(shardTxn));

    LOG_GENERAL(INFO, "[Batching] Last Nonce sent "
                          << nonce + num_txn << " of Addr " << addr.hex());
    txns.clear();

    if (!GetTxnFromFile::GetFromFile(addr,
                                     static_cast<uint32_t>(nonce) + num_txn + 1,
                                     NUM_TXN_TO_DS, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns for DS");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(DSTxn));
  }
  return !(shardTxn.empty() || DSTxn.empty());
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

  const vector<Address>& myGenesisAccounts = m_mediator.m_currentEpochNum % 2
                                                 ? m_myGenesisAccounts1
                                                 : m_myGenesisAccounts2;

  if (myGenesisAccounts.empty()) {
    return false;
  }
  const unsigned int NUM_TXN_TO_DS_PER_ACCOUNT =
      num_txn / myGenesisAccounts.size();

  for (unsigned int i = 0; i < myGenesisAccounts.size(); i++) {
    const auto& addr = myGenesisAccounts.at(i);
    auto txnShard = Transaction::GetShardIndex(addr, numShards);
    txns.clear();

    uint64_t nonce;

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());
      nonce = AccountStore::GetInstance().GetAccount(addr)->GetNonce();
    }

    if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                     num_txn, txns)) {
      LOG_GENERAL(WARNING, "Failed to get txns from file");
      continue;
    }

    copy(txns.begin(), txns.end(), back_inserter(mp[txnShard]));

    LOG_GENERAL(INFO, "[Batching] Last Nonce sent "
                          << nonce + num_txn << " of Addr " << addr.hex());
    txns.clear();

    if (!GetTxnFromFile::GetFromFile(
            addr, static_cast<uint32_t>(nonce) + num_txn + 1,
            NUM_TXN_TO_DS_PER_ACCOUNT +
                (i != myGenesisAccounts.size() - 1
                     ? 0
                     : num_txn % myGenesisAccounts.size()),
            txns)) {
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

      Blacklist::GetInstance().Whitelist(
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

      Blacklist::GetInstance().Whitelist(
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

  // To avoid sending message to multiplier and himself
  VectorOfNode tmp;
  std::copy_if(m_lookupNodes.begin(), m_lookupNodes.end(),
               std::back_inserter(tmp), [this](const PairOfNode& node) {
                 return (find_if(m_multipliers.begin(), m_multipliers.end(),
                                 [&node](const PairOfNode& mult) {
                                   return node.second == mult.second;
                                 }) == m_multipliers.end()) &&
                        (node.second != m_mediator.m_selfPeer);
               });

  if (tmp.empty()) {
    LOG_GENERAL(WARNING, "No other lookup to send message to!");
    return;
  }

  int index = RandomGenerator::GetRandomInt(tmp.size());
  auto resolved_ip = TryGettingResolvedIP(tmp[index].second);

  Blacklist::GetInstance().Whitelist(
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

      Blacklist::GetInstance().Whitelist(
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

bool Lookup::GetDSInfoFromSeedNodes() {
  LOG_MARKER();
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    SendMessageToRandomL2lDataProvider(ComposeGetDSInfoMessage());
  } else {
    SendMessageToRandomSeedNode(ComposeGetDSInfoMessage());
  }
  return true;
}

bool Lookup::GetDSInfoFromLookupNodes(bool initialDS) {
  LOG_MARKER();
  SendMessageToRandomLookupNode(ComposeGetDSInfoMessage(initialDS));
  return true;
}

bytes Lookup::ComposeGetDSBlockMessage(uint64_t lowBlockNum,
                                       uint64_t highBlockNum,
                                       const bool includeMinerInfo) {
  LOG_MARKER();

  bytes getDSBlockMessage = {MessageType::LOOKUP,
                             LookupInstructionType::GETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupGetDSBlockFromSeed(
          getDSBlockMessage, MessageOffset::BODY, lowBlockNum, highBlockNum,
          m_mediator.m_selfPeer.m_listenPortHost, includeMinerInfo)) {
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
                                       uint64_t highBlockNum,
                                       const bool includeMinerInfo) {
  LOG_MARKER();
  SendMessageToRandomLookupNode(
      ComposeGetDSBlockMessage(lowBlockNum, highBlockNum, includeMinerInfo));
  return true;
}

bytes Lookup::ComposeGetDSBlockMessageForL2l(uint64_t blockNum) {
  bytes getdsblock = {MessageType::LOOKUP,
                      LookupInstructionType::GETDSBLOCKFROML2LDATAPROVIDER};
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetDSBlockMessageForL2l for block " << blockNum);

  if (!Messenger::SetLookupGetDSBlockFromL2l(getdsblock, MessageOffset::BODY,
                                             blockNum, m_mediator.m_selfPeer,
                                             m_extSeedKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetDSBlockFromL2l failed.");
    return {};
  }
  return getdsblock;
}

bytes Lookup::ComposeGetVCFinalBlockMessageForL2l(uint64_t blockNum) {
  LOG_MARKER();

  bytes getVcFinalBlockMessage = {
      MessageType::LOOKUP,
      LookupInstructionType::GETVCFINALBLOCKFROML2LDATAPROVIDER};

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetVCFinalBlockMessageForL2l for block " << blockNum);

  if (!Messenger::SetLookupGetVCFinalBlockFromL2l(
          getVcFinalBlockMessage, MessageOffset::BODY, blockNum,
          m_mediator.m_selfPeer, m_extSeedKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetVCFinalBlockFromL2l failed.");
    return {};
  }

  return getVcFinalBlockMessage;
}

bytes Lookup::ComposeGetMBnForwardTxnMessageForL2l(uint64_t blockNum,
                                                   uint32_t shardId) {
  bytes getmbntxn = {MessageType::LOOKUP,
                     LookupInstructionType::GETMBNFWDTXNFROML2LDATAPROVIDER};

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetMBnForwardTxnMessageForL2l for block "
                << blockNum << " and shard " << shardId);

  if (!Messenger::SetLookupGetMBnForwardTxnFromL2l(
          getmbntxn, MessageOffset::BODY, blockNum, shardId,
          m_mediator.m_selfPeer, m_extSeedKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetMBnForwardTxnFromL2l failed.");
    return {};
  }
  return getmbntxn;
}

bytes Lookup::ComposeGetPendingTxnMessageForL2l(uint64_t blockNum,
                                                uint32_t shardId) {
  bytes getpendingtxn = {
      MessageType::LOOKUP,
      LookupInstructionType::GETPENDINGTXNFROML2LDATAPROVIDER};
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetPendingTxnMessageForL2l for block "
                << blockNum << " and shard " << shardId);

  if (!Messenger::SetLookupGetPendingTxnFromL2l(
          getpendingtxn, MessageOffset::BODY, blockNum, shardId,
          m_mediator.m_selfPeer, m_extSeedKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetPendingTxnFromL2l failed.");
    return {};
  }
  return getpendingtxn;
}

bool Lookup::GetDSBlockFromL2lDataProvider(uint64_t blockNum) {
  LOG_MARKER();

  // loop until ds block is received
  while (!m_mediator.m_lookup->m_vcDsBlockProcessed &&
         (GetSyncType() == SyncType::NO_SYNC)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "GetDSBlockFromL2lDataProvider for block " << blockNum);
    SendMessageToRandomL2lDataProvider(
        ComposeGetDSBlockMessageForL2l(blockNum));

    unique_lock<mutex> lock(m_mediator.m_lookup->m_mutexVCDSBlockProcessed);
    if (m_mediator.m_lookup->cv_vcDsBlockProcessed.wait_for(
            lock, chrono::seconds(SEED_SYNC_SMALL_PULL_INTERVAL)) ==
            std::cv_status::timeout &&
        !m_exitPullThread) {
      LOG_GENERAL(WARNING,
                  "GetDSBlockFromL2lDataProvider Timeout... may be ds block "
                  "yet to be mined");
    } else {
      m_mediator.m_lookup->m_vcDsBlockProcessed = false;
      return true;
    }
  }
  return false;
}

bool Lookup::GetVCFinalBlockFromL2lDataProvider(uint64_t blockNum) {
  LOG_MARKER();

  // loop until vcfinal block is received
  auto getmessage = ComposeGetVCFinalBlockMessageForL2l(blockNum);
  while (!m_mediator.m_lookup->m_vcFinalBlockProcessed &&
         (GetSyncType() == SyncType::NO_SYNC)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "GetVCFinalBlockFromL2lDataProvider for block " << blockNum);
    SendMessageToRandomL2lDataProvider(getmessage);
    unique_lock<mutex> lock(m_mediator.m_lookup->m_mutexVCFinalBlockProcessed);
    if (m_mediator.m_lookup->cv_vcFinalBlockProcessed.wait_for(
            lock, chrono::seconds(SEED_SYNC_SMALL_PULL_INTERVAL)) ==
            std::cv_status::timeout &&
        !m_exitPullThread) {
      LOG_GENERAL(WARNING,
                  "GetVCFinalBlockFromL2lDataProvider Timeout... may be "
                  "vc/final block yet to be mined");
    } else {
      m_mediator.m_lookup->m_vcFinalBlockProcessed = false;
      return true;
    }
  }

  return false;
}

bool Lookup::GetMBnForwardTxnFromL2lDataProvider(uint64_t blockNum,
                                                 uint32_t shardId) {
  LOG_MARKER();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "GetMBnForwardTxnFromL2lDataProvider for block "
                << blockNum << " and shard " << shardId);
  SendMessageToRandomL2lDataProvider(
      ComposeGetMBnForwardTxnMessageForL2l(blockNum, shardId));
  return true;
}

bool Lookup::GetPendingTxnFromL2lDataProvider(uint64_t blockNum,
                                              uint32_t shardId) {
  LOG_MARKER();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "GetPendingTxnFromL2lDataProvider for block "
                << blockNum << " and shard " << shardId);
  SendMessageToRandomL2lDataProvider(
      ComposeGetPendingTxnMessageForL2l(blockNum, shardId));
  return true;
}

bool Lookup::GetDSBlockFromSeedNodes(uint64_t lowBlockNum,
                                     uint64_t highBlockNum,
                                     const bool includeMinerInfo) {
  LOG_MARKER();
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ComposeGetDSBlockMessage for blocks " << lowBlockNum << " to "
                                                   << highBlockNum);
  SendMessageToRandomSeedNode(
      ComposeGetDSBlockMessage(lowBlockNum, highBlockNum, includeMinerInfo));
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
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    SendMessageToRandomL2lDataProvider(
        ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
  } else {
    SendMessageToRandomSeedNode(
        ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
  }

  return true;
}

bool Lookup::GetStateDeltaFromSeedNodes(const uint64_t& blockNum)

{
  LOG_MARKER();
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    SendMessageToRandomL2lDataProvider(ComposeGetStateDeltaMessage(blockNum));
  } else {
    SendMessageToRandomSeedNode(ComposeGetStateDeltaMessage(blockNum));
  }
  return true;
}

bool Lookup::GetStateDeltasFromSeedNodes(uint64_t lowBlockNum,
                                         uint64_t highBlockNum)

{
  LOG_MARKER();

  if (m_syncType == SyncType::LOOKUP_SYNC) {
    SendMessageToRandomLookupNode(
        ComposeGetStateDeltasMessage(lowBlockNum, highBlockNum));
  } else if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    SendMessageToRandomL2lDataProvider(
        ComposeGetStateDeltasMessage(lowBlockNum, highBlockNum));
  } else {
    SendMessageToRandomSeedNode(
        ComposeGetStateDeltasMessage(lowBlockNum, highBlockNum));
  }

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

      struct in_addr ip_addr {};
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

void Lookup::SendMessageToRandomL2lDataProvider(const bytes& message) const {
  LOG_MARKER();

  if (message.empty()) {
    LOG_GENERAL(WARNING, "Ignoring sending empty message");
    return;
  }

  lock_guard<mutex> lock(m_mutexL2lDataProviders);
  if (0 == m_l2lDataProviders.size()) {
    LOG_GENERAL(WARNING, "l2l data providers are empty");
    return;
  }

  int index = RandomGenerator::GetRandomInt(m_l2lDataProviders.size());
  auto resolved_ip = TryGettingResolvedIP(m_l2lDataProviders[index].second);

  Blacklist::GetInstance().Whitelist(
      resolved_ip);  // exclude this l2lookup ip from blacklisting
  Peer tmpPeer(resolved_ip,
               m_l2lDataProviders[index].second.GetListenPortHost());
  LOG_GENERAL(INFO, "Sending message to l2l: " << tmpPeer);
  P2PComm::GetInstance().SendMessage(tmpPeer, message);
}

void Lookup::SendMessageToRandomSeedNode(const bytes& message) const {
  LOG_MARKER();

  VectorOfPeer notBlackListedSeedNodes;
  {
    lock_guard<mutex> lock(m_mutexSeedNodes);
    if (0 == m_seedNodes.size()) {
      LOG_GENERAL(WARNING, "Seed nodes are empty");
      return;
    }

    for (const auto& node : m_seedNodes) {
      auto seedNodeIpToSend = TryGettingResolvedIP(node.second);
      if (!Blacklist::GetInstance().Exist(seedNodeIpToSend) &&
          (m_mediator.m_selfPeer.GetIpAddress() != seedNodeIpToSend)) {
        notBlackListedSeedNodes.push_back(
            Peer(seedNodeIpToSend, node.second.GetListenPortHost()));
      }
    }
  }

  if (notBlackListedSeedNodes.empty()) {
    LOG_GENERAL(WARNING,
                "All the seed nodes are blacklisted, please check you network "
                "connection.");
    return;
  }

  auto index = RandomGenerator::GetRandomInt(notBlackListedSeedNodes.size());
  LOG_GENERAL(INFO, "Sending message to " << notBlackListedSeedNodes[index]);
  P2PComm::GetInstance().SendMessage(notBlackListedSeedNodes[index], message);
}

bool Lookup::IsWhitelistedExtSeed(const PubKey& pubKey) {
  lock_guard<mutex> g(m_mutexExtSeedWhitelisted);
  return m_extSeedWhitelisted.end() != m_extSeedWhitelisted.find(pubKey);
}

bool Lookup::ProcessGetDSBlockFromL2l(const bytes& message, unsigned int offset,
                                      const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetDSBlockFromL2l not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t blockNum = 0;
  Peer requestorPeer;
  PubKey senderPubKey;

  if (!Messenger::GetLookupGetDSBlockFromL2l(message, offset, blockNum,
                                             requestorPeer, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetDSBlockFromL2l failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetDSBlockFromL2l requested by " << from << " for block "
                                                     << blockNum);

  // some validation before processing this request
  if (from.GetIpAddress() != requestorPeer.GetIpAddress()) {
    LOG_GENERAL(WARNING,
                "Requestor's IP does not match the one in message. so ignoring "
                "request!");
    return false;
  }

  // check if requestor's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  // check the raw store if requested ds block message exist
  // if asking for older or current ds block and not found in local store,
  // try recreating latest ds block from disk. Issue is we can't recreate it
  // for older ds block because we don't store sharding structure of
  // older ds epoch but only for latest one.
  // Receiving seed should process the latest ds block and know that its
  // lagging too much and will initiate Rejoin.
  {
    uint64_t latestDSBlkNum =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    if (blockNum < latestDSBlkNum) {
      blockNum = latestDSBlkNum;
    }

    std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexVCDSBlockStore);

    if (m_mediator.m_node->m_vcDSBlockStore.find(blockNum) ==
        m_mediator.m_node->m_vcDSBlockStore.end()) {
      if (blockNum == latestDSBlkNum) {
        ComposeAndStoreVCDSBlockMessage(blockNum);
      } else {
        // Have not received DS Block yet.
        return true;
      }
    }

    auto it = m_mediator.m_node->m_vcDSBlockStore.find(blockNum);
    if (it != m_mediator.m_node->m_vcDSBlockStore.end()) {
      LOG_GENERAL(INFO, "Sending VCDSBlock msg to " << requestorPeer);
      P2PComm::GetInstance().SendMessage(requestorPeer, it->second);
    }
  }

  return true;
}

bool Lookup::ProcessGetVCFinalBlockFromL2l(const bytes& message,
                                           unsigned int offset,
                                           const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessGetVCFinalBlockFromL2l not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t blockNum = 0;
  Peer requestorPeer;
  PubKey senderPubKey;

  if (!Messenger::GetLookupGetVCFinalBlockFromL2l(
          message, offset, blockNum, requestorPeer, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetVCFinalBlockFromL2l failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetVCFinalBlockFromL2l requested by "
                << from << " for block " << blockNum);

  // some validation before processing this request
  if (from.GetIpAddress() != requestorPeer.GetIpAddress()) {
    LOG_GENERAL(WARNING,
                "Requestor's IP does not match the one in message. so ignoring "
                "request!");
    return false;
  }

  // check is requester's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  // check the raw store if requested vcfinalblock message exist
  // if asking for vcfinalblock message from older dsepoch, always send one
  // for latest txepoch. if asking for vcfinalblock message from current
  // dsepoch  and not found in local store, try recreating it from disk for
  // requested blocknum. Receiving seed should process the latest
  // vcfinalblock message and know that its lagging too much and will
  // initiate Rejoin.
  {
    uint64_t lowestLimitNum =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
    if (blockNum < lowestLimitNum) {  // requested from older ds epoch
      blockNum = m_mediator.m_currentEpochNum - 1;
    }

    std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexVCFinalBlock);
    if (m_mediator.m_node->m_vcFinalBlockStore.find(blockNum) ==
        m_mediator.m_node->m_vcFinalBlockStore.end()) {
      if (blockNum < m_mediator.m_currentEpochNum - 1) {
        ComposeAndStoreVCFinalBlockMessage(blockNum);
      } else {
        // Have not received FB yet.
        return true;
      }
    }

    auto it = m_mediator.m_node->m_vcFinalBlockStore.find(blockNum);
    if (it != m_mediator.m_node->m_vcFinalBlockStore.end()) {
      LOG_GENERAL(INFO, "Sending VCFinalBlock msg to " << requestorPeer);
      P2PComm::GetInstance().SendMessage(requestorPeer, it->second);
    }
  }

  return true;
}

bool Lookup::ProcessGetMBnForwardTxnFromL2l(const bytes& message,
                                            unsigned int offset,
                                            const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ProcessGetMBnForwardTxnFromL2l not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t blockNum = 0;
  uint32_t shardId = 0;
  Peer requestorPeer;
  PubKey senderPubKey;

  if (!Messenger::GetLookupGetMBnForwardTxnFromL2l(
          message, offset, blockNum, shardId, requestorPeer, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetMBnForwardTxnFromL2l failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetMBnForwardTxnFromL2l requested by "
                << from << " for block " << blockNum << " and shard "
                << shardId);

  // some validation before processing this request
  if (from.GetIpAddress() != requestorPeer.GetIpAddress()) {
    LOG_GENERAL(WARNING,
                "Requestor's IP does not match the one in message. so ignoring "
                "request!");
    return false;
  }

  // check if requestor's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  // check the raw store if requested mbtxns message exist
  int retryCount = MAX_FETCH_BLOCK_RETRIES;
  while (retryCount-- > 0) {
    {
      std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexMBnForwardedTxnStore);
      auto it = m_mediator.m_node->m_mbnForwardedTxnStore.find(blockNum);
      if (it != m_mediator.m_node->m_mbnForwardedTxnStore.end()) {
        auto it2 = it->second.find(shardId);
        if (it2 != it->second.end()) {
          LOG_GENERAL(INFO, "Sending MbnForrwardTxn msg to " << requestorPeer);
          P2PComm::GetInstance().SendMessage(requestorPeer, it2->second);
          return true;
        }
      } else {
        LOG_GENERAL(WARNING, "Failed to fetch mbtxns message, retry... ");
        // if first retry and asking for mbtxn of older tx blocks
        if ((retryCount == MAX_FETCH_BLOCK_RETRIES - 1) &&
            (blockNum < m_mediator.m_currentEpochNum - 1)) {
          ComposeAndStoreMBnForwardTxnMessage(blockNum);
        }
      }
    }
    this_thread::sleep_for(chrono::seconds(2));
  }

  return true;
}

bool Lookup::ComposeAndStoreMBnForwardTxnMessage(const uint64_t& blockNum) {
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP || !MULTIPLIER_SYNC_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ComposeAndStoreMBnForwardTxnMessage not expected to be called "
        "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  TxBlockSharedPtr finalBlkPtr;
  if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, finalBlkPtr)) {
    LOG_GENERAL(WARNING,
                "Failed to fetch txblock " << blockNum << " from disk");
    return false;
  }

  const auto& microBlockInfos = finalBlkPtr->GetMicroBlockInfos();
  for (const auto& info : microBlockInfos) {
    MicroBlockSharedPtr microBlockPtr;
    std::vector<TransactionWithReceipt> txns_to_send;

    if (BlockStorage::GetBlockStorage().GetMicroBlock(info.m_microBlockHash,
                                                      microBlockPtr)) {
      const vector<TxnHash>& tx_hashes = microBlockPtr->GetTranHashes();
      for (const auto& tx_hash : tx_hashes) {
        TxBodySharedPtr txBodyPtr;
        if (!BlockStorage::GetBlockStorage().GetTxBody(tx_hash, txBodyPtr)) {
          LOG_GENERAL(WARNING, "Could not find " << tx_hash);
          continue;
        }
        txns_to_send.emplace_back(*txBodyPtr);
      }

      // Transaction body sharing
      bytes mb_txns_message = {MessageType::NODE,
                               NodeInstructionType::MBNFORWARDTRANSACTION};

      if (!Messenger::SetNodeMBnForwardTransaction(
              mb_txns_message, MessageOffset::BODY, *microBlockPtr,
              txns_to_send)) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Messenger::SetNodeMBnForwardTransaction failed.");
        return false;
      }

      // Store to local map for MBNFORWARDTRANSACTION
      m_mediator.m_node
          ->m_mbnForwardedTxnStore[microBlockPtr->GetHeader().GetEpochNum()]
                                  [microBlockPtr->GetHeader().GetShardId()] =
          mb_txns_message;
    } else {
      LOG_GENERAL(WARNING,
                  "Failed to find mb in disk : " << info.m_microBlockHash);
    }
  }
  return true;
}

bool Lookup::ComposeAndStoreVCDSBlockMessage(const uint64_t& blockNum) {
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP || !MULTIPLIER_SYNC_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ComposeAndStoreVCDSBlockMessage not expected to be called "
        "from other than the LookUp node.");
    return false;
  }

  LOG_MARKER();

  // Hack to make sure sharding structure is received if this node had just
  // rejoined.
  DequeOfShard shardingStruct;
  {
    std::lock_guard<mutex> lock(m_mediator.m_ds->m_mutexShards);
    if (m_mediator.m_ds->m_shards.empty()) {
      LOG_GENERAL(INFO,
                  "Sharding structure for current ds epoch yet not received.");
      return false;
    }
    shardingStruct = m_mediator.m_ds->m_shards;
  }

  DSBlockSharedPtr vcdsBlkPtr;
  if (!BlockStorage::GetBlockStorage().GetDSBlock(blockNum, vcdsBlkPtr)) {
    LOG_GENERAL(WARNING,
                "Failed to fetch dsblock " << blockNum << " from disk");
    return false;
  }

  std::vector<VCBlock> vcBlocks;
  {
    std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexhistVCBlkForDSBlock);
    auto vcBlockPtrs = m_mediator.m_node->m_histVCBlocksForDSBlock[blockNum];
    for (const auto& it : vcBlockPtrs) {
      vcBlocks.emplace_back(*it);
    }
  }

  bytes vcdsblock_message = {MessageType::NODE, NodeInstructionType::DSBLOCK};

  if (!Messenger::SetNodeVCDSBlocksMessage(
          vcdsblock_message, MessageOffset::BODY, 0, *vcdsBlkPtr, vcBlocks,
          SHARDINGSTRUCTURE_VERSION, shardingStruct)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeVCDSBlocksMessage failed " << *vcdsBlkPtr);
    return false;
  } else {
    // Store to local map for VCDSBLOCK
    m_mediator.m_node->m_vcDSBlockStore[blockNum] = vcdsblock_message;
  }

  return true;
}

bool Lookup::ComposeAndStoreVCFinalBlockMessage(const uint64_t& blockNum) {
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP || !MULTIPLIER_SYNC_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::ComposeAndStoreVCFinalBlockMessage not expected to be called "
        "from other than the LookUp node.");
    return false;
  }

  LOG_MARKER();

  TxBlockSharedPtr finalBlkPtr;
  if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, finalBlkPtr)) {
    LOG_GENERAL(WARNING,
                "Failed to fetch txblock " << blockNum << " from disk");
    return false;
  }

  bytes stateDelta = {};
  if (!BlockStorage::GetBlockStorage().GetStateDelta(blockNum, stateDelta)) {
    LOG_GENERAL(WARNING, "Failed to fetch statedelta from disk for txblock "
                             << blockNum);
    return false;
  }

  std::vector<VCBlock> vcBlocks;
  {
    std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexhistVCBlkForTxBlock);
    auto vcBlockPtrs = m_mediator.m_node->m_histVCBlocksForTxBlock[blockNum];
    for (const auto& it : vcBlockPtrs) {
      vcBlocks.emplace_back(*it);
    }
  }

  bytes vc_fb_message = {MessageType::NODE, NodeInstructionType::VCFINALBLOCK};
  if (!Messenger::SetNodeVCFinalBlock(vc_fb_message, MessageOffset::BODY,
                                      finalBlkPtr->GetHeader().GetDSBlockNum(),
                                      0 /*dummy since unused*/, *finalBlkPtr,
                                      stateDelta, vcBlocks)) {
    LOG_GENERAL(WARNING, "Messenger::SetNodeVCFinalBlock failed");
  } else {
    // Store to local map for VCFINALBLOCK
    m_mediator.m_node->m_vcFinalBlockStore[blockNum] = vc_fb_message;
  }

  return true;
}

bool Lookup::ProcessGetPendingTxnFromL2l(const bytes& message,
                                         unsigned int offset,
                                         const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessGetPendingTxnFromL2l not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  uint64_t blockNum = 0;
  uint32_t shardId = 0;
  Peer requestorPeer;
  PubKey senderPubKey;

  if (!Messenger::GetLookupGetPendingTxnFromL2l(
          message, offset, blockNum, shardId, requestorPeer, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupGetPendingTxnFromL2l failed.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessGetPendingTxnFromL2l requested by "
                << from << " for block " << blockNum << " and shard "
                << shardId);

  // some validation before processing this request
  if (from.GetIpAddress() != requestorPeer.GetIpAddress()) {
    LOG_GENERAL(WARNING,
                "Requestor's IP does not match the one in message. so ignoring "
                "request!");
    return false;
  }

  // check if requestor's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  // check the raw store if requested pendingtxns message exist
  int retryCount = MAX_FETCH_BLOCK_RETRIES;
  while (retryCount-- > 0) {
    {
      std::lock_guard<mutex> g1(m_mediator.m_node->m_mutexPendingTxnStore);
      auto it = m_mediator.m_node->m_pendingTxnStore.find(blockNum);
      if (it != m_mediator.m_node->m_pendingTxnStore.end()) {
        auto it2 = it->second.find(shardId);
        if (it2 != it->second.end()) {
          LOG_GENERAL(INFO, "Sending pending txns to " << requestorPeer);
          P2PComm::GetInstance().SendMessage(requestorPeer, it2->second);
          return true;
        }
      }
    }
    this_thread::sleep_for(chrono::seconds(2));
  }
  LOG_GENERAL(INFO, "No pendingtxns!");

  return true;
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
  bool includeMinerInfo = false;

  if (!Messenger::GetLookupGetDSBlockFromSeed(message, offset, lowBlockNum,
                                              highBlockNum, portNo,
                                              includeMinerInfo)) {
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

  bytes returnMsg = {MessageType::LOOKUP,
                     LookupInstructionType::SETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupSetDSBlockFromSeed(returnMsg, MessageOffset::BODY,
                                              lowBlockNum, highBlockNum,
                                              m_mediator.m_selfKey, dsBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupSetDSBlockFromSeed failed.");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  LOG_GENERAL(INFO, requestingNode);
  P2PComm::GetInstance().SendMessage(requestingNode, returnMsg);

  // Send minerInfo as a separate message since it is not critical information
  if (includeMinerInfo) {
    LOG_GENERAL(INFO, "Miner info requested");
    map<uint64_t, pair<MinerInfoDSComm, MinerInfoShards>> minerInfoPerDS;
    for (const auto& dsBlock : dsBlocks) {
      const uint64_t dsBlockNum = dsBlock.GetHeader().GetBlockNum();
      MinerInfoDSComm minerInfoDSComm;
      MinerInfoShards minerInfoShards;
      if (!BlockStorage::GetBlockStorage().GetMinerInfoDSComm(
              dsBlockNum, minerInfoDSComm)) {
        LOG_GENERAL(WARNING,
                    "GetMinerInfoDSComm failed for block " << dsBlockNum);
        continue;
      }
      if (!BlockStorage::GetBlockStorage().GetMinerInfoShards(
              dsBlockNum, minerInfoShards)) {
        LOG_GENERAL(WARNING,
                    "GetMinerInfoShards failed for block " << dsBlockNum);
        continue;
      }
      minerInfoPerDS.emplace(dsBlockNum,
                             make_pair(minerInfoDSComm, minerInfoShards));
      LOG_GENERAL(INFO, "Added info for " << dsBlockNum);
    }

    if (minerInfoPerDS.size() > 0) {
      // Ok to reuse returnMsg at this point
      returnMsg = {MessageType::LOOKUP,
                   LookupInstructionType::SETMINERINFOFROMSEED};
      if (!Messenger::SetLookupSetMinerInfoFromSeed(
              returnMsg, MessageOffset::BODY, m_mediator.m_selfKey,
              minerInfoPerDS)) {
        LOG_GENERAL(WARNING,
                    "Messenger::SetLookupSetMinerInfoFromSeed failed.");
        return false;
      }

      P2PComm::GetInstance().SendMessage(requestingNode, returnMsg);
      LOG_GENERAL(INFO, "Sent miner info. Count=" << minerInfoPerDS.size());
    } else {
      LOG_GENERAL(INFO, "No miner info sent");
    }
  }

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
      continue;
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
  PubKey senderPubKey;
  uint32_t shardingStructureVersion = 0;
  if (!Messenger::GetLookupSetShardsFromSeed(
          message, offset, senderPubKey, shardingStructureVersion, shards)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetShardsFromSeed failed.");
    return false;
  }

  if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
    LOG_CHECK_FAIL("Sharding structure version", shardingStructureVersion,
                   SHARDINGSTRUCTURE_VERSION);
    return false;
  }

  if (!(VerifySenderNode(GetLookupNodesStatic(), senderPubKey) ||
        VerifySenderNode(GetSeedNodes(), senderPubKey))) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
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
  if (!BlockStorage::GetBlockStorage().PutMicroBlock(
          microblock.GetBlockHash(), microblock.GetHeader().GetEpochNum(),
          microblock.GetHeader().GetShardId(), body)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in body");
    return false;
  }

  return true;
}

bool Lookup::ProcessGetMicroBlockFromLookup(const bytes& message,
                                            unsigned int offset,
                                            const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  // verify if sender is from whitelisted list
  uint128_t ipAddr = from.m_ipAddress;
  if (!Blacklist::GetInstance().IsWhitelistedSeed(ipAddr)) {
    LOG_GENERAL(
        WARNING,
        "Requesting IP : "
            << from.GetPrintableIPAddress()
            << " is not in whitelisted seeds IP list. Ignore the request");
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

  LOG_GENERAL(INFO, "Request for " << microBlockHashes.size() << " blocks");
  if (microBlockHashes.size() > MAX_FETCHMISSINGMBS_NUM) {
    LOG_GENERAL(WARNING, "Requesting for more than max allowed : "
                             << MAX_FETCHMISSINGMBS_NUM
                             << ". Looks Suspicious so ignore request");
    return false;
  }

  Peer requestingNode(ipAddr, portNo);
  vector<MicroBlock> retMicroBlocks;

  for (const auto& mbhash : microBlockHashes) {
    LOG_GENERAL(INFO, "[SendMB]"
                          << "Request for microBlockHash " << mbhash);
    shared_ptr<MicroBlock> mbptr;
    int retryCount = MAX_FETCH_BLOCK_RETRIES;

    while (retryCount-- > 0) {
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbhash, mbptr)) {
        LOG_GENERAL(WARNING,
                    "Failed to fetch micro block Hash, retry... " << mbhash);
        this_thread::sleep_for(chrono::seconds(1));
        continue;
      } else {
        LOG_GENERAL(
            INFO, "Request for microBlockHash " << mbhash << " successfully.");
        retMicroBlocks.push_back(*mbptr);
        break;
      }
    }
  }

  bytes retMsg = {MessageType::LOOKUP,
                  LookupInstructionType::SETMICROBLOCKFROMLOOKUP};

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

bool Lookup::ProcessGetMicroBlockFromL2l(const bytes& message,
                                         unsigned int offset,
                                         const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  vector<BlockHash> microBlockHashes;
  uint32_t portNo = 0;
  PubKey senderPubKey;
  if (!Messenger::GetLookupGetMicroBlockFromL2l(
          message, offset, microBlockHashes, portNo, senderPubKey)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return false;
  }

  // check if requestor's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  if (microBlockHashes.size() == 0) {
    LOG_GENERAL(INFO, "No MicroBlock requested");
    return true;
  }

  LOG_GENERAL(INFO, "Request for " << microBlockHashes.size() << " blocks");
  if (microBlockHashes.size() > MAX_FETCHMISSINGMBS_NUM) {
    LOG_GENERAL(WARNING, "Requesting for more than max allowed : "
                             << MAX_FETCHMISSINGMBS_NUM
                             << ". Looks Suspicious so ignore request");
    return false;
  }

  Peer requestingNode(from.m_ipAddress, portNo);
  vector<MicroBlock> retMicroBlocks;

  for (const auto& mbhash : microBlockHashes) {
    LOG_GENERAL(INFO, "[SendMB]"
                          << "Request for microBlockHash " << mbhash);
    shared_ptr<MicroBlock> mbptr;
    int retryCount = MAX_FETCH_BLOCK_RETRIES;

    while (retryCount-- > 0) {
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbhash, mbptr)) {
        LOG_GENERAL(WARNING,
                    "Failed to fetch micro block Hash, retry... " << mbhash);
        this_thread::sleep_for(chrono::seconds(1));
        continue;
      } else {
        LOG_GENERAL(
            INFO, "Request for microBlockHash " << mbhash << " successfully.");
        retMicroBlocks.push_back(*mbptr);
        break;
      }
    }
  }

  bytes retMsg = {MessageType::LOOKUP,
                  LookupInstructionType::SETMICROBLOCKFROMLOOKUP};

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

bool Lookup::ProcessSetMicroBlockFromLookup(const bytes& message,
                                            unsigned int offset,
                                            [[gnu::unused]] const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  LOG_MARKER();

  vector<MicroBlock> mbs;
  PubKey senderPubKey;

  if (!Messenger::GetLookupSetMicroBlockFromLookup(message, offset,
                                                   senderPubKey, mbs)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return false;
  }

  if (!MULTIPLIER_SYNC_MODE &&
      !VerifySenderNode(m_l2lDataProviders, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my l2l data provider list.");
    return false;
  } else if (MULTIPLIER_SYNC_MODE &&
             !VerifySenderNode(GetLookupNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
    return false;
  }

  vector<TxnHash> txnhashes;

  for (const auto& mb : mbs) {
    LOG_GENERAL(INFO, "[SendMB]"
                          << " Recvd " << mb.GetHeader().GetEpochNum()
                          << " MBHash:" << mb.GetBlockHash());
    AddMicroBlockToStorage(mb);
    if (!MULTIPLIER_SYNC_MODE) {
      SendGetTxnsFromL2l(mb.GetBlockHash(), mb.GetTranHashes());
    } else {
      SendGetTxnsFromLookup(mb.GetBlockHash(), mb.GetTranHashes());
    }
  }

  return true;
}

void Lookup::SendGetMicroBlockFromLookup(const vector<BlockHash>& mbHashes) {
  LOG_MARKER();

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

void Lookup::SendGetMicroBlockFromL2l(const vector<BlockHash>& mbHashes) {
  LOG_MARKER();

  bytes msg = {MessageType::LOOKUP,
               LookupInstructionType::GETMICROBLOCKFROML2LDATAPROVIDER};

  if (mbHashes.size() == 0) {
    LOG_GENERAL(INFO, "No microBlock requested");
    return;
  }

  if (!Messenger::SetLookupGetMicroBlockFromL2l(
          msg, MessageOffset::BODY, mbHashes,
          m_mediator.m_selfPeer.m_listenPortHost, m_extSeedKey)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return;
  }

  SendMessageToRandomL2lDataProvider(msg);
}

bool Lookup::ProcessGetCosigsRewardsFromSeed(
    [[gnu::unused]] const bytes& message, [[gnu::unused]] unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  // verify if sender is from know DS Committee
  const uint128_t& ipSenderAddr = from.m_ipAddress;
  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (!VerifySenderNode(*m_mediator.m_DSCommittee, ipSenderAddr)) {
      LOG_GENERAL(
          WARNING,
          "Requesting IP : "
              << from.GetPrintableIPAddress()
              << " is not in Present DS Committee list. Ignore the request");
      return false;
    }
  }

  uint64_t blockNum;
  uint32_t portNo = 0;
  PubKey dsPubKey;
  if (!Messenger::GetLookupGetCosigsRewardsFromSeed(message, offset, dsPubKey,
                                                    blockNum, portNo)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return false;
  }

  const uint64_t& currDsEpoch =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
  if (blockNum < currDsEpoch) {
    LOG_GENERAL(WARNING,
                "Requested cosigs/rewards for txBlock that is beyond the "
                "current DS epoch "
                "(requested txblk  :"
                    << blockNum << ", curr DS Epoch : " << currDsEpoch << ")");
    return false;
  }

  {
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (!VerifySenderNode(*m_mediator.m_DSCommittee, dsPubKey)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "The message sender pubkey: "
                    << dsPubKey << " is not in current ds committee list.");
      return false;
    }
  }

  LOG_GENERAL(INFO, "Request for cosig/rewards for blockNum " << blockNum);

  Peer requestingNode(ipSenderAddr, portNo);
  vector<MicroBlock> retMicroBlocks;

  TxBlockSharedPtr txblkPtr;
  int retryCount = MAX_FETCH_BLOCK_RETRIES;
  while (retryCount > 0) {
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, txblkPtr)) {
      LOG_GENERAL(WARNING,
                  "Failed to fetch tx block " << blockNum << " , retry... ");
      this_thread::sleep_for(chrono::seconds(1));
    } else {
      break;
    }

    --retryCount;
  }

  if (retryCount == 0) {
    LOG_GENERAL(WARNING, "Failed to fetch tx block, giving up !");
    return false;
  }

  const auto& microblockInfos = txblkPtr->GetMicroBlockInfos();
  std::vector<MicroBlock> microblocks;
  for (const auto& mbInfo : microblockInfos) {
    if (mbInfo.m_shardId ==
        m_mediator.m_ds->GetNumShards()) {  // ignore ds microblock
      continue;
    }
    MicroBlockSharedPtr mbptr;
    retryCount = MAX_FETCH_BLOCK_RETRIES;
    while (retryCount > 0) {
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(
              mbInfo.m_microBlockHash, mbptr)) {
        LOG_GENERAL(WARNING, "Could not get MicroBlock "
                                 << mbInfo.m_microBlockHash << ", retry..");
        this_thread::sleep_for(chrono::seconds(1));
      } else {
        break;
      }

      --retryCount;
    }
    if (retryCount == 0) {
      LOG_GENERAL(WARNING, "Failed to fetch MicroBlock "
                               << mbInfo.m_microBlockHash << " , giving up !");
      return false;
    }
    microblocks.emplace_back(*mbptr);
  }

  bytes retMsg = {MessageType::DIRECTORY,
                  DSInstructionType::SETCOSIGSREWARDSFROMSEED};

  if (!Messenger::SetLookupSetCosigsRewardsFromSeed(
          retMsg, MessageOffset::BODY, m_mediator.m_selfKey, blockNum,
          microblocks, *txblkPtr, m_mediator.m_ds->GetNumShards())) {
    LOG_GENERAL(WARNING, "Failed to Process ");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, retMsg);
  return true;
}

bool Lookup::NoOp([[gnu::unused]] const bytes& message,
                  [[gnu::unused]] unsigned int offset,
                  [[gnu::unused]] const Peer& from) {
  LOG_MARKER();
  return true;
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

  // If first epoch and I'm a lookup and I am not syncing right now
  if ((m_mediator.m_currentEpochNum <= 1) && LOOKUP_NODE_MODE &&
      (GetSyncType() == SyncType::NO_SYNC)) {
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

  for (i = 0; i < m_mediator.m_blocklinkchain.GetBuiltDSComm().size(); i++) {
    if (!(dsNodes.at(i).first ==
          m_mediator.m_blocklinkchain.GetBuiltDSComm().at(i).first)) {
      LOG_GENERAL(WARNING, "Mis-match of ds comm at index " << i);
      lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
      m_mediator.m_DSCommittee->clear();
      return false;
    }
  }

  LOG_GENERAL(INFO, "[DSINFOVERIF] Success");

  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
  *m_mediator.m_DSCommittee = move(dsNodes);

  // Add ds guard to exclude list for lookup at bootstrap
  Guard::GetInstance().AddDSGuardToBlacklistExcludeList(
      *m_mediator.m_DSCommittee);

  //    Data::GetInstance().SetDSPeers(dsPeers);
  //#endif // IS_LOOKUP_NODE

  if (m_dsInfoWaitingNotifying &&
      (m_syncType != NO_SYNC ||
       m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)) {
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
  PubKey senderPubKey;
  std::vector<DSBlock> dsBlocks;
  if (!Messenger::GetLookupSetDSBlockFromSeed(
          message, offset, lowBlockNum, highBlockNum, senderPubKey, dsBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetDSBlockFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
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

    // only process DS block for lookup nodes, otherwise for normal node
    // it's purpose is just for indication if new DS block is mined or not
    if (LOOKUP_NODE_MODE) {
      vector<boost::variant<DSBlock, VCBlock>> dirBlocks;
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
              dirBlocks, m_mediator.m_blocklinkchain.GetBuiltDSComm(),
              index_num, newDScomm)) {
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
  }

  return true;
}

bool Lookup::ProcessSetMinerInfoFromSeed(const bytes& message,
                                         unsigned int offset,
                                         [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  map<uint64_t, pair<MinerInfoDSComm, MinerInfoShards>> minerInfoPerDS;
  PubKey senderPubKey;
  if (!Messenger::GetLookupSetMinerInfoFromSeed(message, offset, senderPubKey,
                                                minerInfoPerDS)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetMinerInfoFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
    return false;
  }

  for (const auto& dsBlockAndMinerInfo : minerInfoPerDS) {
    if (!BlockStorage::GetBlockStorage().PutMinerInfoDSComm(
            dsBlockAndMinerInfo.first, dsBlockAndMinerInfo.second.first)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutMinerInfoDSComm failed");
      continue;
    }
    if (!BlockStorage::GetBlockStorage().PutMinerInfoShards(
            dsBlockAndMinerInfo.first, dsBlockAndMinerInfo.second.second)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutMinerInfoShards failed");
      continue;
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
  PubKey senderPubKey;

  if (!Messenger::GetLookupSetTxBlockFromSeed(
          message, offset, lowBlockNum, highBlockNum, senderPubKey, txBlocks)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetTxBlockFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
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
        static_cast<unsigned int>(m_mediator.m_aveBlockTimeInSeconds) *
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
      case Validator::TxBlockValidationMsg::VALID:
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
      case Validator::TxBlockValidationMsg::INVALID:
        LOG_GENERAL(INFO, "[TxBlockVerif]"
                              << "Invalid blocks");
        break;
      case Validator::TxBlockValidationMsg::STALEDSINFO:
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

  if (!LOOKUP_NODE_MODE || ARCHIVAL_LOOKUP) {
    GetDSInfoFromSeedNodes();
  } else {
    GetDSInfoFromLookupNodes();
  }

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
            "At new DS epoch now, already have state. Getting DSInfo.");

  if (!GetDSInfo()) {
    LOG_GENERAL(WARNING, "DSInfo not received!");
    return;
  }

  LOG_GENERAL(INFO, "DSInfo received -> Starting PoW now");

  InitMining();
}

void Lookup::CommitTxBlocks(const vector<TxBlock>& txBlocks) {
  LOG_GENERAL(INFO, "[TxBlockVerif]"
                        << "Success");
  uint64_t lowBlockNum = txBlocks.front().GetHeader().GetBlockNum();
  uint64_t highBlockNum = txBlocks.back().GetHeader().GetBlockNum();
  bool placeholder = false;
  if (m_syncType != SyncType::RECOVERY_ALL_SYNC) {
    unsigned int retry = 1;
    while (retry <= RETRY_GETSTATEDELTAS_COUNT) {
      // Get the state-delta for all txBlocks from random lookup nodes
      GetStateDeltasFromSeedNodes(lowBlockNum, highBlockNum);
      std::unique_lock<std::mutex> cv_lk(m_mutexSetStateDeltasFromSeed);
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
    uint64_t blockNum = txBlock.GetHeader().GetBlockNum();

    if (!BlockStorage::GetBlockStorage().PutTxBlock(blockNum,
                                                    serializedTxBlock)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
      return;
    }

    // If txblk not from vacaous epoch and is rejoining as ds node
    if ((blockNum + 1) % NUM_FINAL_BLOCK_PER_POW != 0 &&
        (m_syncType == SyncType::DS_SYNC ||
         m_syncType == SyncType::GUARD_DS_SYNC)) {
      // Coinbase
      uint128_t rewards = txBlock.GetHeader().GetRewards();
      LOG_GENERAL(INFO, "Update coin base for finalblock with blockNum: "
                            << blockNum << ", reward: " << rewards);
      m_mediator.m_ds->SaveCoinbase(txBlock.GetB1(), txBlock.GetB2(),
                                    CoinbaseReward::FINALBLOCK_REWARD,
                                    blockNum + 1);
      // Need if it join immediately before vacaous. And will be used in
      // InitCoinbase in final blk consensus in vacaous epoch.
      m_mediator.m_ds->m_totalTxnFees += rewards;
    }

    if ((LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP &&
         m_syncType == SyncType::NEW_LOOKUP_SYNC) ||
        (LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP &&
         m_syncType == SyncType::LOOKUP_SYNC)) {
      m_mediator.m_node->LoadUnavailableMicroBlockHashes(
          txBlock, placeholder, true /*skip shardid check*/);
    }

    if (m_syncType == SyncType::DS_SYNC ||
        m_syncType == SyncType::GUARD_DS_SYNC) {
      // Compose And Send GetCosigRewards for this txBlk from seed
      ComposeAndSendGetCosigsRewardsFromSeed(txBlock.GetHeader().GetBlockNum());
    }

    if (LOOKUP_NODE_MODE) {
      m_mediator.m_node->ClearPendingAndDroppedTxn();
    }
  }

  m_mediator.m_currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  // To trigger m_isVacuousEpoch calculation
  m_mediator.IncreaseEpochNum();

  if ((LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP &&
       m_syncType == SyncType::NEW_LOOKUP_SYNC) ||
      (LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP &&
       m_syncType == SyncType::LOOKUP_SYNC)) {
    m_mediator.m_node->CommitMBnForwardedTransactionBuffer();
    m_mediator.m_node->CommitPendingTxnBuffer();
    // Additional safe-guard mechanism, if have not received the MBNdFWDTXNS at
    // all for last few txBlks.
    FindMissingMBsForLastNTxBlks(LAST_N_TXBLKS_TOCHECK_FOR_MISSINGMBS);
    CheckAndFetchUnavailableMBs(false);
  }

  m_mediator.m_consensusID =
      m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW;

  m_mediator.UpdateTxBlockRand();

  if (m_syncType == SyncType::NEW_SYNC || m_syncType == SyncType::NORMAL_SYNC) {
    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) {
      SetSyncType(SyncType::NO_SYNC);
      m_isFirstLoop = true;
      m_currDSExpired = false;
      m_mediator.m_node->m_confirmedNotInNetwork = false;
      PrepareForStartPow();
    } else {
      // check if already identified as not being part of any shard.
      // If yes, just keep sycing until vacaous epoch. Don't proceed further.
      if (!m_mediator.m_node->m_confirmedNotInNetwork) {
        // Ask for the sharding structure from lookup
        ComposeAndSendGetShardingStructureFromSeed();
        std::unique_lock<std::mutex> cv_lk(m_mutexShardStruct);
        if (cv_shardStruct.wait_for(
                cv_lk, std::chrono::seconds(GETSHARD_TIMEOUT_IN_SECONDS)) ==
            std::cv_status::timeout) {
          LOG_GENERAL(
              WARNING,
              "Didn't receive sharding structure! Try checking next epoch");
        } else {
          bool ipChanged = false;
          if (!m_mediator.m_node->RecalculateMyShardId(ipChanged)) {
            LOG_GENERAL(
                INFO, "I was not in any shard in current ds epoch previously");
            m_mediator.m_node->m_confirmedNotInNetwork = true;
          } else if (m_mediator.m_node->LoadShardingStructure()) {
            LOG_GENERAL(INFO,
                        "I was already part of shard in current ds epoch");
            if (!m_currDSExpired &&
                m_mediator.m_dsBlockChain.GetLastBlock()
                        .GetHeader()
                        .GetEpochNum() < m_mediator.m_currentEpochNum) {
              GetDSInfo();
              m_isFirstLoop = true;
              SetSyncType(SyncType::NO_SYNC);

              if (ipChanged) {
                // Send new network info to own shard, ds committee and lookups
                m_mediator.m_node->UpdateShardNodeIdentity();
              } else {
                // Send whitelist request to all peers.
                m_mediator.m_node->ComposeAndSendRemoveNodeFromBlacklist(
                    Node::PEER);
              }

              m_mediator.m_node->StartFirstTxEpoch(
                  true);  // Starts with WAITING_FINALBLOCK
            }
          }
          m_currDSExpired = false;
        }
      }
    }
  } else if ((m_syncType == SyncType::DS_SYNC ||
              m_syncType == SyncType::GUARD_DS_SYNC) &&
             (!m_mediator.m_ds->m_dsguardPodDelete ||
              /* Re-assigned DSGUARD-POD allowed to rejoin only in vacaous epoch
                 for now */
              m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)) {
    if (!m_currDSExpired &&
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum() <
            m_mediator.m_currentEpochNum) {
      m_isFirstLoop = true;
      SetSyncType(SyncType::NO_SYNC);

      m_mediator.m_ds->FinishRejoinAsDS(lowBlockNum % NUM_FINAL_BLOCK_PER_POW ==
                                        0);
    }
    m_currDSExpired = false;
  } else if (m_syncType == SyncType::LOOKUP_SYNC ||
             m_syncType == SyncType::NEW_LOOKUP_SYNC) {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "Lookup / New lookup node - Already should have latest state by now.");
    if (GetDSInfo()) {
      if (!m_currDSExpired) {
        if (ARCHIVAL_LOOKUP || (!ARCHIVAL_LOOKUP && FinishRejoinAsLookup())) {
          SetSyncType(SyncType::NO_SYNC);

          if (m_lookupServer) {
            if (m_lookupServer->StartListening()) {
              LOG_GENERAL(INFO, "API Server started to listen again");
            } else {
              LOG_GENERAL(WARNING, "API Server couldn't start");
            }
          }
          m_isFirstLoop = true;

          if (m_stakingServer) {
            if (m_stakingServer->StartListening()) {
              LOG_GENERAL(INFO, "Staking Server started to listen again");
            } else {
              LOG_GENERAL(WARNING, "Staking Server couldn't start");
            }
          }
        }
        m_currDSExpired = false;
        // If seed node, start Pull if this seed opted for this approach
        if (ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
          auto func = [this]() -> void {
            bool firstPull = true;
            bool dsBlockReceived = false;
            LOG_GENERAL(INFO,
                        "Starting the pull thread from l2l_data_providers");
            m_exitPullThread = false;
            while (GetSyncType() == SyncType::NO_SYNC) {
              if ((m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW ==
                   0) &&
                  !dsBlockReceived) {
                // This return only after receiving next ds block
                if (GetDSBlockFromL2lDataProvider(
                        m_mediator.m_dsBlockChain.GetBlockCount())) {
                  dsBlockReceived = true;
                  if (m_exitPullThread) {
                    m_exitPullThread = false;
                    break;
                  }
                } else {
                  continue;
                }
              } else {
                // This returns only after receiving next vc final block
                if (GetVCFinalBlockFromL2lDataProvider(
                        m_mediator.m_txBlockChain.GetBlockCount())) {
                  // reset the dsblockreceived flag
                  dsBlockReceived = false;
                  if (m_exitPullThread) {
                    m_exitPullThread = false;
                    break;
                  }
                  FetchMbTxPendingTxMessageFromL2l(
                      m_mediator.m_txBlockChain.GetLastBlock()
                          .GetHeader()
                          .GetBlockNum());  // last block
                } else {
                  continue;
                }
              }
              if (!firstPull && !dsBlockReceived) {
                // we take the liberty to have longer wait window because curr
                // blk is just received.
                this_thread::sleep_for(
                    chrono::seconds(SEED_SYNC_LARGE_PULL_INTERVAL));
              } else {  // check again after smaller wait window
                firstPull = false;
                this_thread::sleep_for(
                    chrono::seconds(SEED_SYNC_SMALL_PULL_INTERVAL));
              }
            }
            LOG_GENERAL(INFO, "Stopped the pull thread from l2l_data_provider");
          };

          DetachedFunction(1, func);  // main thread pulling data forever
        }
      }
    }
  }

  cv_setTxBlockFromSeed.notify_all();
  cv_waitJoined.notify_all();
}

void Lookup::FindMissingMBsForLastNTxBlks(const uint32_t& num) {
  LOG_MARKER();
  uint64_t upperLimit =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t lowerLimit = 1;

  if (upperLimit > num) {
    lowerLimit = upperLimit - num + 1;
  }

  for (auto& i = lowerLimit; i <= upperLimit; i++) {
    TxBlock b = m_mediator.m_txBlockChain.GetBlock(i);
    auto mbsinfo = b.GetMicroBlockInfos();
    for (const auto& info : mbsinfo) {
      MicroBlockSharedPtr mbptr;
      if (!BlockStorage::GetBlockStorage().CheckMicroBlock(
              info.m_microBlockHash) &&
          info.m_txnRootHash != TxnHash()) {
        lock_guard<mutex> g(m_mediator.m_node->m_mutexUnavailableMicroBlocks);
        auto& mbs = m_mediator.m_node->GetUnavailableMicroBlocks()[i];
        if (std::find_if(mbs.begin(), mbs.end(),
                         [info](const std::pair<BlockHash, TxnHash>& e) {
                           return e.first == info.m_microBlockHash;
                         }) == mbs.end()) {
          mbs.push_back({info.m_microBlockHash, info.m_txnRootHash});
          LOG_GENERAL(INFO,
                      "[TxBlk:" << i << "] Add unavailable block [MbBlockHash] "
                                << info.m_microBlockHash << " [TxnRootHash] "
                                << info.m_txnRootHash << " shardID "
                                << info.m_shardId);
        }
      }
    }
  }
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
  PubKey senderPubKey;

  if (!Messenger::GetLookupSetStateDeltaFromSeed(message, offset, blockNum,
                                                 senderPubKey, stateDelta)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetStateDeltaFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "ProcessSetStateDeltaFromSeed sent by " << from << " for block "
                                                    << blockNum);

  if (!m_skipAddStateDeltaToAccountStore &&
      !AccountStore::GetInstance().DeserializeDelta(stateDelta, 0)) {
    LOG_GENERAL(WARNING, "AccountStore::GetInstance().DeserializeDelta failed");
    return false;
  }

  BlockStorage::GetBlockStorage().PutStateDelta(blockNum, stateDelta);

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
  PubKey senderPubKey;

  if (!Messenger::GetLookupSetStateDeltasFromSeed(message, offset, lowBlockNum,
                                                  highBlockNum, senderPubKey,
                                                  stateDeltas)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetLookupSetStateDeltasFromSeed failed.");
    return false;
  }

  if (!VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
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
    // But not crucial right now since we do verify sender i.e lookup and
    // trust it.

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
      if (txBlkNum + NUM_FINAL_BLOCK_PER_POW > highBlockNum) {
        if (!AccountStore::GetInstance().MoveUpdatesToDisk()) {
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

bool Lookup::ProcessGetTxnsFromLookup([[gnu::unused]] const bytes& message,
                                      [[gnu::unused]] unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  // verify if sender is from whitelisted list
  uint128_t ipAddr = from.m_ipAddress;
  if (!Blacklist::GetInstance().IsWhitelistedSeed(ipAddr)) {
    LOG_GENERAL(
        WARNING,
        "Requesting IP : "
            << from.GetPrintableIPAddress()
            << " is not in whitelisted seeds IP list. Ignore the request");
    return false;
  }

  vector<TxnHash> txnhashes;
  BlockHash mbHash;
  uint32_t portNo = 0;
  if (!Messenger::GetLookupGetTxnsFromLookup(message, offset, mbHash, txnhashes,
                                             portNo)) {
    LOG_GENERAL(WARNING, "Failed to Process");
    return false;
  }

  auto requestedNum = txnhashes.size();
  if (requestedNum == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return true;
  }

  if (requestedNum > max(DS_MICROBLOCK_GAS_LIMIT, SHARD_MICROBLOCK_GAS_LIMIT)) {
    LOG_GENERAL(WARNING,
                "No microblock can have more than "
                    << max(DS_MICROBLOCK_GAS_LIMIT, SHARD_MICROBLOCK_GAS_LIMIT)
                    << " missing txns. Looks suspicious so will "
                       "ignore the message and blacklist sender");
    Blacklist::GetInstance().Add(from.GetIpAddress());
    return false;
  }

  MicroBlockSharedPtr mbptr;
  if (BlockStorage::GetBlockStorage().GetMicroBlock(mbHash, mbptr)) {
    if (mbptr->GetHeader().GetNumTxs() != requestedNum) {
      LOG_GENERAL(WARNING, "Num of requested txnhashes "
                               << requestedNum
                               << " does not match local storage count "
                               << mbptr->GetTranHashes().size());
      return false;
    }
  } else {
    LOG_GENERAL(WARNING,
                "Microblock (" << mbHash << ") does not exist locally");
    return false;
  }

  LOG_GENERAL(INFO, "Num of requested txnhashes = " << requestedNum);

  vector<TransactionWithReceipt> txns;
  for (const auto& txnhash : txnhashes) {
    shared_ptr<TransactionWithReceipt> txnptr;
    if (!BlockStorage::GetBlockStorage().GetTxBody(txnhash, txnptr)) {
      LOG_GENERAL(WARNING, "Could not find " << txnhash);
      // TBD - may be want to blacklist.
      continue;
    }
    txns.emplace_back(*txnptr);
  }

  LOG_GENERAL(INFO, "Num of txnhashes found locally = " << txns.size());

  Peer requestingNode(ipAddr, portNo);

  bytes setTxnMsg = {MessageType::LOOKUP,
                     LookupInstructionType::SETTXNFROMLOOKUP};

  if (!Messenger::SetLookupSetTxnsFromLookup(
          setTxnMsg, MessageOffset::BODY, m_mediator.m_selfKey, mbHash, txns)) {
    LOG_GENERAL(WARNING, "Unable to Process");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, setTxnMsg);
  return true;
}

bool Lookup::ProcessGetTxnsFromL2l(const bytes& message, unsigned int offset,
                                   const Peer& from) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Function not expected to be called from non-lookup node");
    return false;
  }

  vector<TxnHash> txnhashes;
  BlockHash mbHash;
  uint32_t portNo = 0;
  PubKey senderPubKey;
  if (!Messenger::GetLookupGetTxnsFromL2l(message, offset, mbHash, txnhashes,
                                          portNo, senderPubKey)) {
    LOG_GENERAL(WARNING, "Failed to Process");
    return false;
  }

  // check if requestor's pubkey is from whitelisted extseed pub keys.
  if (!IsWhitelistedExtSeed(senderPubKey)) {
    LOG_GENERAL(WARNING, "Requestor's extseed pubkey is not whitelisted!");
    return false;
  }

  auto requestedNum = txnhashes.size();
  if (requestedNum == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return true;
  }

  if (requestedNum > max(DS_MICROBLOCK_GAS_LIMIT, SHARD_MICROBLOCK_GAS_LIMIT)) {
    LOG_GENERAL(WARNING,
                "No microblock can have more than "
                    << max(DS_MICROBLOCK_GAS_LIMIT, SHARD_MICROBLOCK_GAS_LIMIT)
                    << " missing txns. Looks suspicious so will "
                       "ignore the message and blacklist sender");
    Blacklist::GetInstance().Add(from.GetIpAddress());
    return false;
  }

  MicroBlockSharedPtr mbptr;
  if (BlockStorage::GetBlockStorage().GetMicroBlock(mbHash, mbptr)) {
    if (mbptr->GetHeader().GetNumTxs() != requestedNum) {
      LOG_GENERAL(WARNING, "Num of requested txnhashes "
                               << requestedNum
                               << " does not match local storage count "
                               << mbptr->GetTranHashes().size());
      return false;
    }
  } else {
    LOG_GENERAL(WARNING,
                "Microblock (" << mbHash << ") does not exist locally");
    return false;
  }

  LOG_GENERAL(INFO, "Num of requested txnhashes = " << requestedNum);

  vector<TransactionWithReceipt> txns;
  for (const auto& txnhash : txnhashes) {
    shared_ptr<TransactionWithReceipt> txnptr;
    if (!BlockStorage::GetBlockStorage().GetTxBody(txnhash, txnptr)) {
      LOG_GENERAL(WARNING, "Could not find " << txnhash);
      // TBD - may be want to blacklist.
      continue;
    }
    txns.emplace_back(*txnptr);
  }

  LOG_GENERAL(INFO, "Num of txnhashes found locally = " << txns.size());

  Peer requestingNode(from.m_ipAddress, portNo);

  bytes setTxnMsg = {MessageType::LOOKUP,
                     LookupInstructionType::SETTXNFROMLOOKUP};

  if (!Messenger::SetLookupSetTxnsFromLookup(
          setTxnMsg, MessageOffset::BODY, m_mediator.m_selfKey, mbHash, txns)) {
    LOG_GENERAL(WARNING, "Unable to Process");
    return false;
  }

  P2PComm::GetInstance().SendMessage(requestingNode, setTxnMsg);
  return true;
}

// Ex archival code
bool Lookup::ProcessSetTxnsFromLookup(const bytes& message, unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  BlockHash mbHash;
  vector<TransactionWithReceipt> txns;
  PubKey senderPubKey;

  if (!Messenger::GetLookupSetTxnsFromLookup(message, offset, senderPubKey,
                                             mbHash, txns)) {
    LOG_GENERAL(WARNING, "Failed to Process");
    return false;
  }

  if (!MULTIPLIER_SYNC_MODE &&
      !VerifySenderNode(m_l2lDataProviders, senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my l2l data provider list.");
    return false;
  } else if (MULTIPLIER_SYNC_MODE &&
             !VerifySenderNode(GetLookupNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
    return false;
  }

  LOG_GENERAL(INFO,
              "Received " << txns.size() << " txns for microblock :" << mbHash);

  uint64_t epochNum = 0;
  {
    MicroBlockSharedPtr microBlockPtr;
    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbHash, microBlockPtr)) {
      LOG_GENERAL(WARNING, "Failed to get MB with hash " << mbHash);
      return false;
    }
    epochNum = microBlockPtr->GetHeader().GetEpochNum();
  }

  for (const auto& txn : txns) {
    bytes serializedTxBody;
    txn.Serialize(serializedTxBody, 0);

    if (!BlockStorage::GetBlockStorage().PutTxBody(
            epochNum, txn.GetTransaction().GetTranID(), serializedTxBody)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutTxBody failed "
                               << txn.GetTransaction().GetTranID());
      continue;  // Transaction already existed locally. Move on so as to delete
                 // the entry from unavailable list
    }
  }

  // Delete the mb from unavailable list here
  std::lock_guard<mutex> lock(m_mediator.m_node->m_mutexUnavailableMicroBlocks);
  auto& unavailableMBs = m_mediator.m_node->GetUnavailableMicroBlocks();
  for (auto it = unavailableMBs.begin(); it != unavailableMBs.end();) {
    auto& mbsVec = it->second;
    auto origSiz = mbsVec.size();
    mbsVec.erase(
        std::remove_if(mbsVec.begin(), mbsVec.end(),
                       [mbHash](const std::pair<BlockHash, TxnHash>& e) {
                         return e.first == mbHash;
                       }),
        mbsVec.end());
    if (mbsVec.size() < origSiz) {
      LOG_GENERAL(
          INFO, "[TxBlk - "
                    << it->first
                    << "] Removed entry of unavailable microblock: " << mbHash);
    }
    if (mbsVec.size() == 0) {
      // Finally delete the entry for this final block
      LOG_GENERAL(INFO,
                  "Removed entry of unavailable microblocks list for TxBlk: "
                      << it->first);
      it = unavailableMBs.erase(it);
    } else {
      ++it;
    }
  }

  return true;
}

void Lookup::SendGetTxnsFromLookup(const BlockHash& mbHash,
                                   const vector<TxnHash>& txnhashes) {
  LOG_MARKER();

  bytes msg = {MessageType::LOOKUP, LookupInstructionType::GETTXNFROMLOOKUP};

  if (txnhashes.size() == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return;
  }

  if (!Messenger::SetLookupGetTxnsFromLookup(
          msg, MessageOffset::BODY, mbHash, txnhashes,
          m_mediator.m_selfPeer.m_listenPortHost)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return;
  }
  SendMessageToRandomLookupNode(msg);
}

void Lookup::SendGetTxnsFromL2l(const BlockHash& mbHash,
                                const vector<TxnHash>& txnhashes) {
  LOG_MARKER();

  bytes msg = {MessageType::LOOKUP,
               LookupInstructionType::GETTXNSFROML2LDATAPROVIDER};

  if (txnhashes.size() == 0) {
    LOG_GENERAL(INFO, "No txn requested");
    return;
  }

  if (!Messenger::SetLookupGetTxnsFromL2l(
          msg, MessageOffset::BODY, mbHash, txnhashes,
          m_mediator.m_selfPeer.m_listenPortHost, m_extSeedKey)) {
    LOG_GENERAL(WARNING, "Failed to process");
    return;
  }
  SendMessageToRandomL2lDataProvider(msg);
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

bool Lookup::InitMining() {
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
  // set the node as synced
  SetSyncType(NO_SYNC);
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
      dsBlockRand, txBlockRand, 0);

  uint64_t lastTxBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  unique_lock<mutex> lk(m_mutexCVJoined);
  cv_waitJoined.wait(lk);

  m_startedPoW = false;

  // It is new DS epoch now, clear the seed node from black list
  RemoveSeedNodesFromBlackList();

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
    VectorOfPeer lookupNodesOffline;
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

void Lookup::StartSynchronization() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::StartSynchronization not expected to be called "
                "from other than the LookUp node.");
    return;
  }

  LOG_MARKER();

  auto func = [this]() -> void {
    if (!ARCHIVAL_LOOKUP) {
      GetMyLookupOffline();
    }
    while (GetSyncType() != SyncType::NO_SYNC) {
      ComposeAndSendGetDirectoryBlocksFromSeed(
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1, ARCHIVAL_LOOKUP,
          LOOKUP_NODE_MODE);
      if (ARCHIVAL_LOOKUP) {
        GetTxBlockFromSeedNodes(m_mediator.m_txBlockChain.GetBlockCount(), 0);
      } else {
        GetTxBlockFromLookupNodes(m_mediator.m_txBlockChain.GetBlockCount(), 0);
      }

      this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
    }
    // Ask for the sharding structure from lookup (may have got new ds block
    // with new sharding struct)
    ComposeAndSendGetShardingStructureFromSeed();
    std::unique_lock<std::mutex> cv_lk(m_mutexShardStruct);
    if (cv_shardStruct.wait_for(
            cv_lk, std::chrono::seconds(GETSHARD_TIMEOUT_IN_SECONDS)) ==
        std::cv_status::timeout) {
      LOG_GENERAL(WARNING, "Didn't receive sharding structure!");
    } else {
      ProcessEntireShardingStructure();
    }
  };
  DetachedFunction(1, func);
}

bool Lookup::GetDSInfoLoop() {
  unsigned int counter = 0;
  // Allow over-writing ds committee because of corner case where node rejoined
  // in first tx epoch of ds epoch. Node can get started rejoining from incr db
  // which holds older ds comm at this point. So time to try fetching and
  // over-wriiting ds comm from lookup up in this case.
  /*{
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    if (m_mediator.m_DSCommittee->size() > 0) {
      LOG_GENERAL(WARNING,
                  "DS comm already set, make sure you cleaned variables");
      return false;
    }
  }*/

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
    // If recovering a lookup, we don't expect it to be in the offline list,
    // so just set found to true here
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

void Lookup::RejoinAsNewLookup(bool fromLookup) {
  if (!LOOKUP_NODE_MODE || !ARCHIVAL_LOOKUP) {
    LOG_GENERAL(WARNING,
                "Lookup::RejoinAsNewLookup not expected to be called from "
                "other than the NewLookup node.");
    return;
  }

  LOG_MARKER();
  if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
    m_mediator.m_lookup->SetSyncType(SyncType::NEW_LOOKUP_SYNC);
    // Exit the existing pull thread.
    if (!MULTIPLIER_SYNC_MODE) {
      m_exitPullThread = true;
      this_thread::sleep_for(chrono::seconds(SEED_SYNC_SMALL_PULL_INTERVAL));
    }
    auto func1 = [this]() mutable -> void {
      if (m_lookupServer) {
        m_lookupServer->StopListening();
        LOG_GENERAL(INFO, "API Server stopped listen for syncing");
      }
      if (m_stakingServer) {
        m_stakingServer->StopListening();
        LOG_GENERAL(INFO, "Staking Server stopped listen for syncing");
      }
    };
    DetachedFunction(1, func1);

    if (fromLookup && MULTIPLIER_SYNC_MODE) {  // level2lookups and seed nodes
                                               // syncing via multiplier
      LOG_GENERAL(INFO, "Syncing from lookup ...");
      auto func2 = [this]() mutable -> void { StartSynchronization(); };
      DetachedFunction(1, func2);
    } else {
      LOG_GENERAL(INFO, "Syncing from S3 ...");
      auto func2 = [this]() mutable -> void {
        while (true) {
          this->CleanVariables();
          m_mediator.m_node->CleanUnavailableMicroBlocks();
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

        if (m_seedNodes.empty()) {
          SetAboveLayer(m_seedNodes,
                        "node.upper_seed");  // since may have called
                                             // CleanVariable earlier
        }

        if (!MULTIPLIER_SYNC_MODE && m_l2lDataProviders.empty()) {
          SetAboveLayer(m_l2lDataProviders, "node.l2l_data_providers");
        }

        // Check if next ds epoch was crossed -cornercase after syncing from S3
        if ((m_mediator.m_txBlockChain.GetBlockCount() %
                 NUM_FINAL_BLOCK_PER_POW ==
             0)                // Can fetch dsblock and txblks from new ds epoch
            || GetDSInfo()) {  // have same ds committee as upper seeds to
                               // confirm if no new ds epoch started
          InitSync();
        } else {
          // Sync from S3 again
          LOG_GENERAL(INFO,
                      "I am lagging behind by ds epoch! Will rejoin again!");
          m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);
          RejoinAsLookup(false);
        }
      };
      DetachedFunction(1, func2);
    }
  }
}

void Lookup::RejoinAsLookup(bool fromLookup) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::RejoinAsLookup not expected to be called from "
                "other than the Lookup node.");
    return;
  }

  LOG_MARKER();

  if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
    m_mediator.m_lookup->SetSyncType(SyncType::LOOKUP_SYNC);
    auto func1 = [this]() mutable -> void {
      if (m_lookupServer) {
        m_lookupServer->StopListening();
        LOG_GENERAL(INFO, "API Server stopped listen for syncing");
      }
      if (m_stakingServer) {
        m_stakingServer->StopListening();
        LOG_GENERAL(INFO, "Staking Server stopped listen for syncing");
      }
    };

    DetachedFunction(1, func1);

    if (fromLookup) {  // Lookup syncing from other lookups
      LOG_GENERAL(INFO, "Syncing from lookup ...");
      auto func2 = [this]() mutable -> void { StartSynchronization(); };
      DetachedFunction(1, func2);
    } else {
      LOG_GENERAL(INFO, "Syncing from S3 ...");
      auto func2 = [this]() mutable -> void {
        while (true) {
          this->CleanVariables();
          m_mediator.m_node->CleanUnavailableMicroBlocks();
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
          if (m_mediator.m_node->Install(SyncType::LOOKUP_SYNC, true)) {
            break;
          };
          this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
        }
        if (m_seedNodes.empty()) {
          SetAboveLayer(m_seedNodes,
                        "node.upper_seed");  // since may have called
                                             // CleanVariable earlier
        }
        // Check if next ds epoch was crossed - corner case after syncing from
        // S3
        if ((m_mediator.m_txBlockChain.GetBlockCount() %
                 NUM_FINAL_BLOCK_PER_POW ==
             0)                // Can fetch dsblock and txblks from new ds epoch
            || GetDSInfo()) {  // have same ds committee as other lookups to
                               // confirm if no new ds epoch started
          StartSynchronization();
        } else {
          // Sync from S3 again
          LOG_GENERAL(
              INFO,
              "I am lagging behind again by ds epoch! Will rejoin again!");
          m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);
          RejoinAsLookup(false);
          /* Note: We would like to try to sync the missing txblocks that comes
             before and after next ds epoch from other lookups. ( instead of
             complete sync from S3) However, we dont't store the statedeltas
             from previous ds epoch. So can't fetch the statedeltas for txblocks
             that comes before next ds epoch. So those txblks would fails the
             verification.
          */
        }
      };
      DetachedFunction(1, func2);
    }
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
  m_mediator.m_node->CleanWhitelistReqs();
  m_mediator.m_node->ClearAllPendingAndDroppedTxn();

  m_confirmedLatestDSBlock = false;

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
          ins_byte != LookupInstructionType::SETMICROBLOCKFROMLOOKUP &&
          ins_byte != LookupInstructionType::SETTXNFROMLOOKUP &&
          ins_byte != LookupInstructionType::SETSTATEDELTAFROMSEED &&
          ins_byte != LookupInstructionType::SETSTATEDELTASFROMSEED &&
          ins_byte != LookupInstructionType::SETDIRBLOCKSFROMSEED &&
          ins_byte != LookupInstructionType::SETMINERINFOFROMSEED);
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
  bool includeMinerInfo;
  if (!Messenger::GetLookupGetDirectoryBlocksFromSeed(
          message, offset, portNo, index_num, includeMinerInfo)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupGetDirectoryBlocksFromSeed failed");
    return false;
  }

  bytes msg = {MessageType::LOOKUP,
               LookupInstructionType::SETDIRBLOCKSFROMSEED};

  vector<boost::variant<DSBlock, VCBlock>> dirBlocks;

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

  // Send minerInfo as a separate message since it is not critical information
  if (includeMinerInfo) {
    LOG_GENERAL(INFO, "Miner info requested");
    map<uint64_t, pair<MinerInfoDSComm, MinerInfoShards>> minerInfoPerDS;
    for (uint64_t i = index_num;
         i <= m_mediator.m_blocklinkchain.GetLatestIndex(); i++) {
      BlockLink b = m_mediator.m_blocklinkchain.GetBlockLink(i);
      if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::DS) {
        MinerInfoDSComm minerInfoDSComm;
        MinerInfoShards minerInfoShards;
        uint64_t dsBlockNum =
            m_mediator.m_dsBlockChain.GetBlock(get<BlockLinkIndex::DSINDEX>(b))
                .GetHeader()
                .GetBlockNum();
        if (!BlockStorage::GetBlockStorage().GetMinerInfoDSComm(
                dsBlockNum, minerInfoDSComm)) {
          LOG_GENERAL(WARNING,
                      "GetMinerInfoDSComm failed for block " << dsBlockNum);
          continue;
        }
        if (!BlockStorage::GetBlockStorage().GetMinerInfoShards(
                dsBlockNum, minerInfoShards)) {
          LOG_GENERAL(WARNING,
                      "GetMinerInfoShards failed for block " << dsBlockNum);
          continue;
        }
        minerInfoPerDS.emplace(dsBlockNum,
                               make_pair(minerInfoDSComm, minerInfoShards));
        LOG_GENERAL(INFO, "Added info for " << dsBlockNum);
      }
    }

    if (minerInfoPerDS.size() > 0) {
      // Ok to reuse msg at this point
      msg = {MessageType::LOOKUP, LookupInstructionType::SETMINERINFOFROMSEED};
      if (!Messenger::SetLookupSetMinerInfoFromSeed(
              msg, MessageOffset::BODY, m_mediator.m_selfKey, minerInfoPerDS)) {
        LOG_GENERAL(WARNING,
                    "Messenger::SetLookupSetMinerInfoFromSeed failed.");
        return false;
      }

      P2PComm::GetInstance().SendMessage(peer, msg);
      LOG_GENERAL(INFO, "Sent miner info. Count=" << minerInfoPerDS.size());
    } else {
      LOG_GENERAL(INFO, "No miner info sent");
    }
  }

  return true;
}

bool Lookup::ProcessSetDirectoryBlocksFromSeed(
    const bytes& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  vector<boost::variant<DSBlock, VCBlock>> dirBlocks;
  uint64_t index_num;
  uint32_t shardingStructureVersion = 0;
  PubKey senderPubKey;

  lock(m_mutexCheckDirBlocks, m_mutexSetTxBlockFromSeed);

  lock_guard<mutex> g(m_mutexCheckDirBlocks, adopt_lock);
  lock_guard<mutex> lock(m_mutexSetTxBlockFromSeed, adopt_lock);
  if (!Messenger::GetLookupSetDirectoryBlocksFromSeed(
          message, offset, shardingStructureVersion, dirBlocks, index_num,
          senderPubKey)) {
    LOG_GENERAL(WARNING,
                "Messenger::GetLookupSetDirectoryBlocksFromSeed failed");
    return false;
  }

  if (!Lookup::VerifySenderNode(GetSeedNodes(), senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << senderPubKey << " is not in my lookup node list.");
    return false;
  }

  // make sure no VCDSBlock is processed at same time
  lock_guard<mutex> g1(m_mediator.m_node->m_mutexDSBlock);

  // Not all calls to GetLookupSetDirectoryBlocksFromSeed set
  // shardingStructureVersion

  // if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
  //   LOG_GENERAL(WARNING, "Sharding structure version check failed.
  //   Expected:
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
    Validator::TxBlockValidationMsg res = m_mediator.m_validator->CheckTxBlocks(
        m_txBlockBuffer, m_mediator.m_blocklinkchain.GetBuiltDSComm(),
        m_mediator.m_blocklinkchain.GetLatestBlockLink());

    switch (res) {
      case Validator::TxBlockValidationMsg::VALID:
        CommitTxBlocks(m_txBlockBuffer);
        m_txBlockBuffer.clear();
        break;
      case Validator::TxBlockValidationMsg::STALEDSINFO:
        LOG_GENERAL(WARNING,
                    "Even after the recving latest ds info, the information "
                    "is stale ");
        break;
      case Validator::TxBlockValidationMsg::INVALID:
        LOG_GENERAL(WARNING, "The blocks in buffer are invalid ");
        m_txBlockBuffer.clear();
        break;
      default:
        LOG_GENERAL(WARNING,
                    "The return value of Validator::CheckTxBlocks does not "
                    "match any type");
    }
  }
}

void Lookup::ComposeAndSendGetDirectoryBlocksFromSeed(
    const uint64_t& index_num, bool toSendSeed, const bool includeMinerInfo) {
  LOG_MARKER();
  bytes message = {MessageType::LOOKUP,
                   LookupInstructionType::GETDIRBLOCKSFROMSEED};

  if (!Messenger::SetLookupGetDirectoryBlocksFromSeed(
          message, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost,
          index_num, includeMinerInfo)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetDirectoryBlocksFromSeed");
    return;
  }
  LOG_GENERAL(INFO, "blocklink index = " << index_num);
  if (!toSendSeed) {
    SendMessageToRandomLookupNode(message);
  } else {
    if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
      SendMessageToRandomL2lDataProvider(message);
    } else {
      SendMessageToRandomSeedNode(message);
    }
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
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && !MULTIPLIER_SYNC_MODE) {
    SendMessageToRandomL2lDataProvider(message);
  } else {
    SendMessageToRandomSeedNode(message);
  }
}

void Lookup::ComposeAndSendGetCosigsRewardsFromSeed(const uint64_t& block_num) {
  LOG_MARKER();
  bytes message = {MessageType::LOOKUP,
                   LookupInstructionType::GETCOSIGSREWARDSFROMSEED};

  if (!Messenger::SetLookupGetCosigsRewardsFromSeed(
          message, MessageOffset::BODY, block_num,
          m_mediator.m_selfPeer.m_listenPortHost, m_mediator.m_selfKey)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetCosigsRewardsFromSeed");
    return;
  }
  LOG_GENERAL(INFO,
              "Sending req for cosigs/rewards of block num = " << block_num);
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
      &Lookup::NoOp,  // Previously for GETSTATEFROMSEED
      &Lookup::NoOp,  // Previously for SETSTATEFROMSEED
      &Lookup::ProcessSetLookupOffline,
      &Lookup::ProcessSetLookupOnline,
      &Lookup::ProcessGetOfflineLookups,
      &Lookup::ProcessSetOfflineLookups,
      &Lookup::NoOp,  // Previously for ProcessRaiseStartPoW
      &Lookup::NoOp,  // Previously for ProcessGetStartPoWFromSeed
      &Lookup::NoOp,  // Previously for ProcessSetStartPoWFromSeed
      &Lookup::ProcessGetShardFromSeed,  // UNUSED
      &Lookup::ProcessSetShardFromSeed,  // UNUSED
      &Lookup::ProcessGetMicroBlockFromLookup,
      &Lookup::ProcessSetMicroBlockFromLookup,
      &Lookup::ProcessGetTxnsFromLookup,
      &Lookup::ProcessSetTxnsFromLookup,
      &Lookup::ProcessGetDirectoryBlocksFromSeed,
      &Lookup::ProcessSetDirectoryBlocksFromSeed,
      &Lookup::ProcessGetStateDeltaFromSeed,
      &Lookup::ProcessGetStateDeltasFromSeed,
      &Lookup::ProcessSetStateDeltaFromSeed,
      &Lookup::ProcessSetStateDeltasFromSeed,
      &Lookup::ProcessVCGetLatestDSTxBlockFromSeed,
      &Lookup::ProcessForwardTxn,
      &Lookup::ProcessGetDSGuardNetworkInfo,
      &Lookup::NoOp,  // Previously for SETHISTORICALDB
      &Lookup::ProcessGetCosigsRewardsFromSeed,
      &Lookup::ProcessSetMinerInfoFromSeed,
      &Lookup::ProcessGetDSBlockFromL2l,
      &Lookup::ProcessGetVCFinalBlockFromL2l,
      &Lookup::ProcessGetMBnForwardTxnFromL2l,
      &Lookup::ProcessGetPendingTxnFromL2l,
      &Lookup::ProcessGetMicroBlockFromL2l,
      &Lookup::ProcessGetTxnsFromL2l};

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

void Lookup::RemoveSeedNodesFromBlackList() {
  LOG_MARKER();

  lock_guard<mutex> lock(m_mutexSeedNodes);

  for (const auto& node : m_seedNodes) {
    auto seedNodeIp = TryGettingResolvedIP(node.second);
    Blacklist::GetInstance().Remove(seedNodeIp);
  }
}

bool Lookup::AddToTxnShardMap(const Transaction& tx, uint32_t shardId,
                              TxnShardMap& txnShardMap,
                              mutex& txnShardMapMutex) {
  lock_guard<mutex> g(txnShardMapMutex);

  uint32_t size = 0;

  for (const auto& x : txnShardMap) {
    size += x.second.size();
  }

  if (size >= TXN_STORAGE_LIMIT) {
    LOG_GENERAL(INFO, "Number of txns exceeded limit");
    return false;
  }

  // case where txn already exist
  if (find_if(txnShardMap[shardId].begin(), txnShardMap[shardId].end(),
              [tx](const Transaction& txn) {
                return tx.GetTranID() == txn.GetTranID();
              }) != txnShardMap[shardId].end()) {
    LOG_GENERAL(WARNING, "Same hash present " << tx.GetTranID());
    return false;
  }

  txnShardMap[shardId].push_back(tx);
  LOG_GENERAL(INFO, "Added Txn " << tx.GetTranID().hex() << " to shard "
                                 << shardId << " of fromAddr "
                                 << tx.GetSenderAddr());
  if (REMOTESTORAGE_DB_ENABLE && !ARCHIVAL_LOOKUP) {
    RemoteStorageDB::GetInstance().InsertTxn(tx, TxnStatus::DISPATCHED,
                                             m_mediator.m_currentEpochNum);
  }

  return true;
}

bool Lookup::AddToTxnShardMap(const Transaction& tx, uint32_t shardId) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::AddToTxnShardMap not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

  return AddToTxnShardMap(tx, shardId, m_txnShardMap, m_txnShardMapMutex);
}

bool Lookup::DeleteTxnShardMap(uint32_t shardId) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::DeleteTxnShardMap not expected to be called from "
                "other than the LookUp node.");
    return true;
  }

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
          m_txnShardMap[oldNumShards].emplace_back(tx);
          continue;
        }
      }

      tempTxnShardMap[fromShard].emplace_back(tx);
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

  if (m_mediator.m_disableTxns) {
    LOG_GENERAL(INFO, "Txns disabled - skipping dispatch to shards");
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

      if (LOG_PARAMETERS) {
        LOG_STATE("[TXNPKT][" << m_mediator.m_currentEpochNum << "] Shard=" << i
                              << " NumTx="
                              << (GetTxnFromShardMap(i).size() + mp[i].size()));
      }

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
        uint32_t leader_id = m_mediator.m_node->CalculateShardLeader(
            lastBlockHash, m_mediator.m_ds->m_shards.at(i).size());
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

      lock_guard<mutex> g(m_txnShardMapMutex);
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

      lock_guard<mutex> g(m_txnShardMapMutex);
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

bool Lookup::VerifySenderNode(const VectorOfNode& vecNodes,
                              const PubKey& pubKeyToVerify) {
  auto iter = std::find_if(vecNodes.cbegin(), vecNodes.cend(),
                           [&pubKeyToVerify](const PairOfNode& node) {
                             return node.first == pubKeyToVerify;
                           });
  return vecNodes.cend() != iter;
}

bool Lookup::VerifySenderNode(const VectorOfNode& vecNodes,
                              const uint128_t& ipToVerify) {
  auto iter = std::find_if(vecNodes.cbegin(), vecNodes.cend(),
                           [&ipToVerify](const PairOfNode& node) {
                             return node.second.m_ipAddress == ipToVerify;
                           });
  return vecNodes.cend() != iter;
}

bool Lookup::VerifySenderNode(const DequeOfNode& deqNodes,
                              const PubKey& pubKeyToVerify) {
  auto iter = std::find_if(deqNodes.cbegin(), deqNodes.cend(),
                           [&pubKeyToVerify](const PairOfNode& node) {
                             return node.first == pubKeyToVerify;
                           });
  return deqNodes.cend() != iter;
}

bool Lookup::VerifySenderNode(const DequeOfNode& deqNodes,
                              const uint128_t& ipToVerify) {
  auto iter = std::find_if(deqNodes.cbegin(), deqNodes.cend(),
                           [&ipToVerify](const PairOfNode& node) {
                             return node.second.m_ipAddress == ipToVerify;
                           });
  return deqNodes.cend() != iter;
}

bool Lookup::VerifySenderNode(const Shard& shard,
                              const PubKey& pubKeyToVerify) {
  auto iter = std::find_if(
      shard.cbegin(), shard.cend(),
      [&pubKeyToVerify](const tuple<PubKey, Peer, uint16_t>& node) {
        return get<SHARD_NODE_PUBKEY>(node) == pubKeyToVerify;
      });
  return shard.cend() != iter;
}

bool Lookup::ProcessForwardTxn(const bytes& message, unsigned int offset,
                               const Peer& from) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Lookup::ProcessForwardTxn not expected to be called from "
                "non-lookup node");
  }

  if (m_mediator.m_disableTxns) {
    LOG_GENERAL(INFO, "Txns disabled - dropping txn packet");
    return false;
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

    if (!m_sendSCCallsToDS) {
      for (const auto& txn : txnsShard) {
        unsigned int shard = txn.GetShardIndex(shard_size);
        AddToTxnShardMap(txn, shard);
      }
    } else {
      LOG_GENERAL(INFO, "Sending all contract calls to DS committee");
      for (const auto& txn : txnsShard) {
        const Transaction::ContractType txnType =
            Transaction::GetTransactionType(txn);
        if (txnType == Transaction::ContractType::CONTRACT_CALL) {
          AddToTxnShardMap(txn, shard_size);
        } else {
          unsigned int shard = txn.GetShardIndex(shard_size);
          AddToTxnShardMap(txn, shard);
        }
      }
    }

    LOG_GENERAL(INFO, "Size of DS txns " << txnsDS.size());

    for (const auto& txn : txnsDS) {
      AddToTxnShardMap(txn, shard_size);
    }
    if (REMOTESTORAGE_DB_ENABLE) {
      RemoteStorageDB::GetInstance().ExecuteWrite();
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

void Lookup::FetchMbTxPendingTxMessageFromL2l(uint64_t blockNum) {
  if (!LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP) {
    LOG_GENERAL(WARNING,
                "Lookup::FetchMbTxPendingTxMessageFromL2l not expected to be "
                "called from "
                "other than the ARCHIVAL LOOKUP.");
    return;
  }
  LOG_MARKER();
  auto func = [this, blockNum]() mutable -> void {
    std::lock_guard<mutex> lock(
        m_mediator.m_node->m_mutexUnavailableMicroBlocks);
    auto& unavailableMBs = m_mediator.m_node->GetUnavailableMicroBlocks();
    for (auto& m : unavailableMBs) {
      // consider only those from latest final block
      if (m.first == blockNum) {
        LOG_GENERAL(INFO, "Unavailable microblock bodies in finalblock "
                              << m.first << ": " << m.second.size());

        // Delete missing mbs from unavailable list which has no txns
        auto& mbs = m.second;
        mbs.erase(std::remove_if(mbs.begin(), mbs.end(),
                                 [](const std::pair<BlockHash, TxnHash>& e) {
                                   return e.second == TxnHash();
                                 }),
                  mbs.end());

        LOG_GENERAL(INFO,
                    "After deleting microblock bodies with no transactions, "
                    "Unavailable count = "
                        << mbs.size());

        // for each nonempty mb, send the request to l2l data provider
        auto txBlock = m_mediator.m_txBlockChain.GetBlock(blockNum);
        const auto& microBlockInfos = txBlock.GetMicroBlockInfos();

        for (const auto& mb : mbs) {
          for (const auto& info : microBlockInfos) {
            if (info.m_microBlockHash == mb.first) {
              GetMBnForwardTxnFromL2lDataProvider(blockNum, info.m_shardId);
              break;
            }
          }
        }

        // for each shard, send request for pending txn message to l2l data
        // provider
        for (const auto& info : microBlockInfos) {
          GetPendingTxnFromL2lDataProvider(blockNum, info.m_shardId);
        }

        break;
      }
    }

    // Delete the entry for those fb with no pending mbs
    for (auto it = unavailableMBs.begin(); it != unavailableMBs.end();) {
      if (it->second.empty()) {
        it = unavailableMBs.erase(it);
      } else
        ++it;
    }
  };
  DetachedFunction(1, func);
}

void Lookup::CheckAndFetchUnavailableMBs(bool skipLatestTxBlk) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Lookup::CheckAndFetchUnavailableMBs not expected to be called from "
        "other than the LOOKUP.");
    return;
  }
  LOG_MARKER();

  if (m_startedFetchMissingMBsThread) {
    LOG_GENERAL(
        WARNING,
        "The last FetchMissingMBsThread hasn't finished, discard this time");
    return;
  }

  unsigned int maxMBSToBeFetched = MAX_FETCHMISSINGMBS_NUM;
  auto main_func = [this, maxMBSToBeFetched,
                    skipLatestTxBlk]() mutable -> void {
    m_startedFetchMissingMBsThread = true;
    std::lock_guard<mutex> lock(
        m_mediator.m_node->m_mutexUnavailableMicroBlocks);
    auto& unavailableMBs = m_mediator.m_node->GetUnavailableMicroBlocks();
    unsigned int count = 0;
    bool limitReached = false;
    for (auto& m : unavailableMBs) {
      // skip mbs from latest final block
      if (skipLatestTxBlk && (m.first == m_mediator.m_currentEpochNum - 1)) {
        continue;
      }
      LOG_GENERAL(INFO, "Unavailable microblock bodies in finalblock "
                            << m.first << ": " << m.second.size());

      // Delete missing mbs from unavailable list which has no txns
      auto& mbs = m.second;
      mbs.erase(std::remove_if(mbs.begin(), mbs.end(),
                               [](const std::pair<BlockHash, TxnHash>& e) {
                                 return e.second == TxnHash();
                               }),
                mbs.end());

      LOG_GENERAL(INFO,
                  "After deleting microblock bodies with no transactions, "
                  "Unavailable count = "
                      << mbs.size());

      if (mbs.empty()) {
        continue;
      }

      vector<BlockHash> mbHashes;
      for (const auto& mb : mbs) {
        count++;
        if (count > maxMBSToBeFetched) {
          LOG_GENERAL(INFO, "Max fetch missing mbs limit of "
                                << maxMBSToBeFetched
                                << " is reached. Remaining missing mbs will be "
                                   "handled in next epoch");
          limitReached = true;
          break;
        }
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "BlockHash = " << mb.first << ", TxnHash = " << mb.second);
        mbHashes.emplace_back(mb.first);
      }
      if (limitReached) {
        break;
      }
      if (!MULTIPLIER_SYNC_MODE) {
        SendGetMicroBlockFromL2l(mbHashes);
      } else {
        SendGetMicroBlockFromLookup(mbHashes);
      }
    }

    // Delete the entry for those fb with no pending mbs
    for (auto it = unavailableMBs.begin(); it != unavailableMBs.end();) {
      if (it->second.empty()) {
        it = unavailableMBs.erase(it);
      } else
        ++it;
    }

    m_startedFetchMissingMBsThread = false;
  };
  DetachedFunction(1, main_func);
}
