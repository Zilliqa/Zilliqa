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

#ifndef __LOOKUP_H__
#define __LOOKUP_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "common/Broadcastable.h"
#include "common/Executable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block/DSBlock.h"
#include "libData/BlockData/Block/MicroBlock.h"
#include "libData/BlockData/Block/TxBlock.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"
#include "libUtils/Logger.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>

class Mediator;
class Synchronizer;

/// Processes requests pertaining to network, transaction, or block information
class Lookup : public Executable, public Broadcastable {
  Mediator& m_mediator;

  // Info about lookup node
  VectorOfLookupNode m_lookupNodes;
  VectorOfLookupNode m_lookupNodesOffline;
  VectorOfLookupNode m_seedNodes;
  std::mutex mutable m_mutexSeedNodes;
  bool m_dsInfoWaitingNotifying = false;
  bool m_fetchedDSInfo = false;

  // To ensure that the confirm of DS node rejoin won't be later than
  // It receiving a new DS block
  bool m_currDSExpired = false;
  bool m_isFirstLoop = true;
  // tells if server is running or not
  bool m_isServer = false;
  uint8_t m_level = (uint8_t)-1;

  // Sharding committee members

  std::mutex m_mutexNodesInNetwork;
  std::vector<Peer> m_nodesInNetwork;
  std::unordered_set<Peer> l_nodesInNetwork;

  std::atomic<bool> m_startedTxnBatchThread;

  // Start PoW variables
  bool m_receivedRaiseStartPoW = false;
  std::mutex m_MutexCVStartPoWSubmission;
  std::condition_variable cv_startPoWSubmission;

  /// To indicate which type of synchronization is using
  SyncType m_syncType = SyncType::NO_SYNC;

  void SetAboveLayer();

  /// Post processing after the DS node successfully synchronized with the
  /// network
  bool FinishRejoinAsLookup();

  /// Post processing after the new Lookup node successfully synchronized with
  /// the network
  bool FinishNewJoinAsLookup();

  // Reset certain variables to the initial state
  bool CleanVariables();

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  /// Initialize all blockchains and blocklinkchain
  void InitAsNewJoiner();

  std::mutex m_mutexSetDSBlockFromSeed;
  std::mutex m_mutexSetTxBlockFromSeed;
  std::mutex m_mutexSetStateDeltaFromSeed;
  std::mutex m_mutexSetTxBodyFromSeed;
  std::mutex m_mutexSetState;
  std::mutex mutable m_mutexLookupNodes;
  std::mutex m_mutexMicroBlocksBuffer;

  // TxBlockBuffer
  std::vector<TxBlock> m_txBlockBuffer;

  bytes ComposeGetDSInfoMessage(bool initialDS = false);
  bytes ComposeGetStateMessage();

  bytes ComposeGetDSBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum);
  bytes ComposeGetTxBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum);
  bytes ComposeGetStateDeltaMessage(uint64_t blockNum);

  bytes ComposeGetLookupOfflineMessage();
  bytes ComposeGetLookupOnlineMessage();

  bytes ComposeGetOfflineLookupNodes();

  void RetrieveDSBlocks(std::vector<DSBlock>& dsBlocks, uint64_t& lowBlockNum,
                        uint64_t& highBlockNum, bool partialRetrieve = false);
  void RetrieveTxBlocks(std::vector<TxBlock>& txBlocks, uint64_t& lowBlockNum,
                        uint64_t& highBlockNum);

 public:
  /// Constructor.
  Lookup(Mediator& mediator);

  /// Destructor.
  ~Lookup();

  /// Sync new lookup node.
  void InitSync();

  // Setting the lookup nodes
  // Hardcoded for now -- to be called by constructor
  void SetLookupNodes();

  bool CheckStateRoot();

  // Getter for m_lookupNodes
  VectorOfLookupNode GetLookupNodes() const;

  // Getter for m_seedNodes
  VectorOfLookupNode GetSeedNodes() const;

  std::mutex m_txnShardMapMutex;
  std::map<uint32_t, std::vector<Transaction>> m_txnShardMap;

  bool IsLookupNode(const PubKey& pubKey) const;

  bool IsLookupNode(const Peer& peerInfo) const;

  // Gen n valid txns
  bool GenTxnToSend(size_t num_txn,
                    std::map<uint32_t, std::vector<Transaction>>& mp,
                    uint32_t numShards);
  bool GenTxnToSend(size_t num_txn, std::vector<Transaction>& txn);

  // Calls P2PComm::SendBroadcastMessage to Lookup Nodes
  void SendMessageToLookupNodes(const bytes& message) const;

  // Calls P2PComm::SendMessage serially to every Lookup Nodes
  void SendMessageToLookupNodesSerial(const bytes& message) const;

  // Calls P2PComm::SendMessage to one of the last x Lookup Nodes randomly
  void SendMessageToRandomLookupNode(const bytes& message) const;

  // Calls P2PComm::SendMessage serially for every Seed peer
  void SendMessageToSeedNodes(const bytes& message) const;

  void SendMessageToRandomSeedNode(const bytes& message) const;

  // TODO: move the Get and ProcessSet functions to Synchronizer
  std::vector<Peer> GetAboveLayer();
  bool GetDSInfoFromSeedNodes();
  bool GetDSInfoLoop();
  bool GetDSInfoFromLookupNodes(bool initialDS = false);
  bool GetDSBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetStateDeltaFromLookupNodes(const uint64_t& blockNum);
  bool GetStateDeltaFromSeedNodes(const uint64_t& blockNum);
  bool GetTxBodyFromSeedNodes(std::string txHashStr);
  bool GetStateFromLookupNodes();
  bool GetStateFromSeedNodes();
  bool ProcessGetShardFromSeed(const bytes& message, unsigned int offset,
                               const Peer& from);
  bool GetDSBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highblocknum);
  bool ProcessSetShardFromSeed(const bytes& message, unsigned int offset,
                               const Peer& from);
  bool GetShardFromLookup();
  // Get the offline lookup nodes from lookup nodes
  bool GetOfflineLookupNodes();

  bool SetDSCommitteInfo();

  DequeOfShard GetShardPeers();
  std::vector<Peer> GetNodePeers();

  // Start synchronization with other lookup nodes as a lookup node
  void StartSynchronization();

  // Set my lookup ip offline in other lookup nodes
  bool GetMyLookupOffline();

  // Set my lookup ip online in other lookup nodes
  bool GetMyLookupOnline();

  // Rejoin the network as a lookup node in case of failure happens in protocol
  void RejoinAsLookup();

  bool AddToTxnShardMap(const Transaction& tx, uint32_t shardId);

  void CheckBufferTxBlocks();

  bool DeleteTxnShardMap(uint32_t shardId);

  void SetServerTrue();

  bool GetIsServer();

  void SenderTxnBatchThread();

  void SendTxnPacketToNodes(uint32_t);

  bool ProcessEntireShardingStructure();
  bool ProcessGetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessGetDSBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessGetTxBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessGetStateDeltaFromSeed(const bytes& message, unsigned int offset,
                                    const Peer& from);
  bool ProcessGetTxBodyFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessGetStateFromSeed(const bytes& message, unsigned int offset,
                               const Peer& from);

  bool ProcessGetNetworkId(const bytes& message, unsigned int offset,
                           const Peer& from);

  bool ProcessGetTxnsFromLookup(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessSetTxnsFromLookup(const bytes& message, unsigned int offset,
                                [[gnu::unused]] const Peer& from);
  void SendGetTxnFromLookup(const std::vector<TxnHash>& txnhashes);

  void SendGetMicroBlockFromLookup(const std::vector<BlockHash>& mbHashes);

  bool ProcessGetMicroBlockFromLookup(const bytes& message, unsigned int offset,
                                      const Peer& from);
  bool ProcessSetMicroBlockFromLookup(const bytes& message, unsigned int offset,
                                      const Peer& from);
  bool AddMicroBlockToStorage(const MicroBlock& microblock);

  bool ProcessGetOfflineLookups(const bytes& message, unsigned int offset,
                                const Peer& from);

  bool ProcessSetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessSetDSBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessSetTxBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  void CommitTxBlocks(const std::vector<TxBlock>& txBlocks);
  bool ProcessSetStateDeltaFromSeed(const bytes& message, unsigned int offset,
                                    const Peer& from);
  bool ProcessSetTxBodyFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessSetStateFromSeed(const bytes& message, unsigned int offset,
                               const Peer& from);

  bool ProcessSetLookupOffline(const bytes& message, unsigned int offset,
                               const Peer& from);
  bool ProcessSetLookupOnline(const bytes& message, unsigned int offset,
                              const Peer& from);

  bool ProcessSetOfflineLookups(const bytes& message, unsigned int offset,
                                const Peer& from);

  bool ProcessRaiseStartPoW(const bytes& message, unsigned int offset,
                            const Peer& from);
  bool ProcessGetStartPoWFromSeed(const bytes& message, unsigned int offset,
                                  const Peer& from);
  bool ProcessSetStartPoWFromSeed(const bytes& message, unsigned int offset,
                                  const Peer& from);

  bool ProcessGetDirectoryBlocksFromSeed(const bytes& message,
                                         unsigned int offset, const Peer& from);

  bool ProcessSetDirectoryBlocksFromSeed(const bytes& message,
                                         unsigned int offset, const Peer& from);

  bool ProcessVCGetLatestDSTxBlockFromSeed(const bytes& message,
                                           unsigned int offset,
                                           const Peer& from);
  bool ProcessForwardTxn(const bytes& message, unsigned int offset,
                         const Peer& from);

  bool ProcessGetDSGuardNetworkInfo(const bytes& message, unsigned int offset,
                                    const Peer& from);

  bool ProcessSetHistoricalDB(const bytes& message, unsigned int offset,
                              const Peer& from);
  void ComposeAndSendGetDirectoryBlocksFromSeed(const uint64_t& index_num,
                                                bool toSendSeed = true);

  static bool VerifySenderNode(const VectorOfLookupNode& vecLookupNodes,
                               const PubKey& pubKeyToVerify);

  bool Execute(const bytes& message, unsigned int offset, const Peer& from);

  inline SyncType GetSyncType() const { return m_syncType; }
  void SetSyncType(SyncType syncType);

  bool m_fetchedOfflineLookups = false;
  std::mutex m_mutexOfflineLookupsUpdation;
  std::condition_variable cv_offlineLookups;

  bool m_historicalDB = false;

  bool m_fetchedLatestDSBlock = false;
  std::mutex m_mutexLatestDSBlockUpdation;
  std::condition_variable cv_latestDSBlock;

  std::mutex m_MutexCVSetTxBlockFromSeed;
  std::condition_variable cv_setTxBlockFromSeed;
  std::mutex m_MutexCVSetStateDeltaFromSeed;
  std::condition_variable cv_setStateDeltaFromSeed;

  std::mutex m_mutexCVJoined;
  std::condition_variable cv_waitJoined;

  bool InitMining(uint32_t lookupIndex);

  /// Helper variables used by new node synchronization
  bool m_startedPoW = false;

  bool AlreadyJoinedNetwork();

  std::mutex m_mutexDSInfoUpdation;
  std::condition_variable cv_dsInfoUpdate;
};

#endif  // __LOOKUP_H__
