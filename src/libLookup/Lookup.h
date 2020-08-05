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

#ifndef ZILLIQA_SRC_LIBLOOKUP_LOOKUP_H_
#define ZILLIQA_SRC_LIBLOOKUP_LOOKUP_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Schnorr.h>
#include "common/Executable.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block/DSBlock.h"
#include "libData/BlockData/Block/MicroBlock.h"
#include "libData/BlockData/Block/TxBlock.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <vector>

class Mediator;
class Synchronizer;
class LookupServer;
class StakingServer;

// The "first" element in the pair is a map of shard to its transactions
// The "second" element in the pair counts the total number of transactions in
// the whole map
using TxnShardMap = std::map<uint32_t, std::vector<Transaction>>;

// Enum used to tell send type to seed node
enum SEND_TYPE { ARCHIVAL_SEND_SHARD = 0, ARCHIVAL_SEND_DS };

/// Processes requests pertaining to network, transaction, or block information
class Lookup : public Executable {
  Mediator& m_mediator;

  // Info about lookup node
  VectorOfNode m_lookupNodes;
  VectorOfNode m_lookupNodesOffline;
  VectorOfNode m_seedNodes;
  VectorOfNode m_multipliers;
  std::mutex mutable m_mutexSeedNodes;
  VectorOfNode m_l2lDataProviders;
  std::mutex mutable m_mutexL2lDataProviders;
  bool m_dsInfoWaitingNotifying = false;
  bool m_fetchedDSInfo = false;

  // m_lookupNodes can change during operation if some lookups go offline.
  // m_lookupNodesStatic is the fixed copy of m_lookupNodes after loading from
  // constants.xml.
  VectorOfNode m_lookupNodesStatic;

  // This is used only for testing with gentxn
  std::vector<Address> m_myGenesisAccounts1;
  std::vector<Address> m_myGenesisAccounts2;

  // To ensure that the confirm of DS node rejoin won't be later than
  // It receiving a new DS block
  bool m_currDSExpired = false;
  bool m_isFirstLoop = true;
  // tells if server is running or not
  bool m_isServer = false;
  uint8_t m_level = (uint8_t)-1;

  // Sharding committee members

  std::mutex m_mutexNodesInNetwork;
  VectorOfPeer m_nodesInNetwork;
  std::unordered_set<Peer> l_nodesInNetwork;

  std::atomic<bool> m_startedTxnBatchThread{};

  std::atomic<bool> m_startedFetchMissingMBsThread{};

  // Start PoW variables
  std::atomic<bool> m_receivedRaiseStartPoW{};

  // Store the StateRootHash of latest txBlock before States are repopulated.
  StateHash m_prevStateRootHashTemp;

  /// To indicate which type of synchronization is using
  std::atomic<SyncType> m_syncType{};  // = SyncType::NO_SYNC;

  void SetAboveLayer(VectorOfNode& aboveLayer, const std::string& xml_node);

  /// Post processing after the lookup node successfully synchronized with the
  /// network
  bool FinishRejoinAsLookup();

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  std::mutex m_mutexSetDSBlockFromSeed;
  std::mutex m_mutexSetTxBlockFromSeed;
  std::mutex m_mutexSetTxBodyFromSeed;
  std::mutex m_mutexSetState;
  std::mutex mutable m_mutexLookupNodes;
  std::mutex m_mutexCheckDirBlocks;
  std::mutex m_mutexMicroBlocksBuffer;

  TxnShardMap m_txnShardMap;
  TxnShardMap m_txnShardMapGenerated;

  // Get StateDeltas from seed
  std::mutex m_mutexSetStateDeltasFromSeed;
  std::condition_variable cv_setStateDeltasFromSeed;

  // TxBlockBuffer
  std::vector<TxBlock> m_txBlockBuffer;

  std::shared_ptr<LookupServer> m_lookupServer;
  std::shared_ptr<StakingServer> m_stakingServer;

  bytes ComposeGetDSInfoMessage(bool initialDS = false);
  bytes ComposeGetStateMessage();

  bytes ComposeGetDSBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum,
                                 const bool includeMinerInfo = false);
  bytes ComposeGetDSBlockMessageForL2l(uint64_t blockNum);
  bytes ComposeGetVCFinalBlockMessageForL2l(uint64_t blockNum);
  bytes ComposeGetMBnForwardTxnMessageForL2l(uint64_t blockNum,
                                             uint32_t shardId);
  bytes ComposeGetPendingTxnMessageForL2l(uint64_t blockNum, uint32_t shardId);
  bytes ComposeGetTxBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum);
  bytes ComposeGetStateDeltaMessage(uint64_t blockNum);
  bytes ComposeGetStateDeltasMessage(uint64_t lowBlockNum,
                                     uint64_t highBlockNum);

  bytes ComposeGetLookupOfflineMessage();
  bytes ComposeGetLookupOnlineMessage();

  bytes ComposeGetOfflineLookupNodes();

  void RetrieveDSBlocks(std::vector<DSBlock>& dsBlocks, uint64_t& lowBlockNum,
                        uint64_t& highBlockNum, bool partialRetrieve = false);
  void RetrieveTxBlocks(std::vector<TxBlock>& txBlocks, uint64_t& lowBlockNum,
                        uint64_t& highBlockNum);

 public:
  /// Constructor.
  Lookup(Mediator& mediator, SyncType syncType, bool multiplierSyncMode = true,
         PairOfKey extSeedKey = PairOfKey());

  /// Destructor.
  ~Lookup();

  /// Sync new lookup node.
  void InitSync();

  // Setting the lookup nodes
  // Hardcoded for now -- to be called by constructor
  void SetLookupNodes();

  void SetLookupNodes(const VectorOfNode&);

  bool CheckStateRoot();

  // Getter for m_lookupNodes
  VectorOfNode GetLookupNodes() const;

  // Getter for m_lookupNodesStatic
  VectorOfNode GetLookupNodesStatic() const;

  // Getter for m_seedNodes
  VectorOfNode GetSeedNodes() const;

  std::mutex m_txnShardMapMutex;
  std::mutex m_txnShardMapGeneratedMutex;

  const std::vector<Transaction>& GetTxnFromShardMap(
      uint32_t index);  // Use m_txnShardMapMutex with this function

  std::mutex m_mutexShardStruct;
  std::condition_variable cv_shardStruct;

  void ComposeAndSendGetShardingStructureFromSeed();

  bool IsLookupNode(const PubKey& pubKey) const;

  bool IsLookupNode(const Peer& peerInfo) const;

  // Gen n valid txns
  bool GenTxnToSend(size_t num_txn,
                    std::map<uint32_t, std::vector<Transaction>>& mp,
                    uint32_t numShards);
  bool GenTxnToSend(size_t num_txn, std::vector<Transaction>& shardTxn,
                    std::vector<Transaction>& DSTxn);

  // Try resolving ip from the given peer's DNS
  uint128_t TryGettingResolvedIP(const Peer& peer) const;

  // Calls P2PComm::SendBroadcastMessage to Lookup Nodes
  void SendMessageToLookupNodes(const bytes& message) const;

  // Calls P2PComm::SendMessage serially to every Lookup Nodes
  void SendMessageToLookupNodesSerial(const bytes& message) const;

  // Calls P2PComm::SendMessage to one of the last x Lookup Nodes randomly
  void SendMessageToRandomLookupNode(const bytes& message) const;

  // Calls P2PComm::SendMessage serially for every Seed peer
  void SendMessageToSeedNodes(const bytes& message) const;

  void SendMessageToRandomSeedNode(const bytes& message) const;

  void SendMessageToRandomL2lDataProvider(const bytes& message) const;

  void RectifyTxnShardMap(const uint32_t, const uint32_t);

  // TODO: move the Get and ProcessSet functions to Synchronizer
  bool GetDSInfoFromSeedNodes();
  bool GetDSInfoLoop();
  bool GetDSInfoFromLookupNodes(bool initialDS = false);
  bool GetDSBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum,
                                 const bool includeMinerInfo = false);
  bool GetTxBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetStateDeltaFromSeedNodes(const uint64_t& blockNum);
  bool GetStateDeltasFromSeedNodes(uint64_t lowBlockNum, uint64_t highBlockNum);

  bool GetStateFromSeedNodes();
  // UNUSED
  bool ProcessGetShardFromSeed([[gnu::unused]] const bytes& message,
                               [[gnu::unused]] unsigned int offset,
                               [[gnu::unused]] const Peer& from);
  // UNUSED
  bool ProcessSetShardFromSeed([[gnu::unused]] const bytes& message,
                               [[gnu::unused]] unsigned int offset,
                               [[gnu::unused]] const Peer& from);
  bool GetDSBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highblocknum,
                               const bool includeMinerInfo = false);

  // USED when MULTIPLIER_SYNC_MODE == false
  bool GetDSBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetVCFinalBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetMBnForwardTxnFromL2lDataProvider(uint64_t blockNum, uint32_t shardId);
  bool GetPendingTxnFromL2lDataProvider(uint64_t blockNum, uint32_t shardId);

  bool ProcessGetDSBlockFromL2l(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessGetVCFinalBlockFromL2l(const bytes& message, unsigned int offset,
                                     const Peer& from);
  bool ProcessGetMBnForwardTxnFromL2l(const bytes& message, unsigned int offset,
                                      const Peer& from);
  bool ProcessGetPendingTxnFromL2l(const bytes& message, unsigned int offset,
                                   const Peer& from);

  // Get the offline lookup nodes from lookup nodes
  bool GetOfflineLookupNodes();

  bool SetDSCommitteInfo(bool replaceMyPeerWithDefault = false);

  DequeOfShard GetShardPeers();
  VectorOfPeer GetNodePeers();

  // Start synchronization with other lookup nodes as a lookup node
  void StartSynchronization();

  // Set my lookup ip offline in other lookup nodes
  bool GetMyLookupOffline();

  // Set my lookup ip online in other lookup nodes
  bool GetMyLookupOnline(bool fromRecovery = false);

  // Rejoin the network as a lookup node in case of failure happens in protocol
  void RejoinAsLookup();

  // Rejoin the network as a newlookup node in case of failure happens in
  // protocol
  void RejoinAsNewLookup(bool fromLookup = true);

  bool AddToTxnShardMap(const Transaction& tx, uint32_t shardId);
  bool AddToTxnShardMap(const Transaction& tx, uint32_t shardId,
                        TxnShardMap& txnShardMap, std::mutex& txnShardMapMutex);

  void CheckBufferTxBlocks();

  bool DeleteTxnShardMap(uint32_t shardId);

  void SetServerTrue();

  bool GetIsServer();

  void SenderTxnBatchThread(const uint32_t, bool newDSEpoch = false);

  void SendTxnPacketPrepare(const uint32_t oldNumShards,
                            const uint32_t newNumShards);
  void SendTxnPacketToNodes(const uint32_t oldNumShards,
                            const uint32_t newNumShards);
  void SendTxnPacketToDS(const uint32_t oldNumShards,
                         const uint32_t newNumShards);
  void SendTxnPacketToShard(const uint32_t shardId, bool toDS);

  bool ProcessEntireShardingStructure();
  bool ProcessGetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from);
  bool ProcessGetDSBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessGetTxBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  bool ProcessGetStateDeltaFromSeed(const bytes& message, unsigned int offset,
                                    const Peer& from);
  bool ProcessGetStateDeltasFromSeed(const bytes& message, unsigned int offset,
                                     const Peer& from);
  bool ProcessGetStateFromSeed(const bytes& message, unsigned int offset,
                               const Peer& from);

  bool ProcessGetTxnsFromLookup(const bytes& message, unsigned int offset,
                                const Peer& from);

  bool ProcessGetTxnsFromL2l(const bytes& message, unsigned int offset,
                             const Peer& from);

  bool ProcessSetTxnsFromLookup(const bytes& message, unsigned int offset,
                                const Peer& from);

  void SendGetTxnsFromLookup(const BlockHash& mbHash,
                             const std::vector<TxnHash>& txnhashes);

  void SendGetTxnsFromL2l(const BlockHash& mbHash,
                          const std::vector<TxnHash>& txnhashes);

  void SendGetMicroBlockFromLookup(const std::vector<BlockHash>& mbHashes);

  void SendGetMicroBlockFromL2l(const std::vector<BlockHash>& mbHashes);

  bool ProcessGetMicroBlockFromLookup(const bytes& message, unsigned int offset,
                                      const Peer& from);

  bool ProcessGetMicroBlockFromL2l(const bytes& message, unsigned int offset,
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
  bool ProcessSetMinerInfoFromSeed(const bytes& message, unsigned int offset,
                                   const Peer& from);
  bool ProcessSetTxBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from);
  void CommitTxBlocks(const std::vector<TxBlock>& txBlocks);
  void PrepareForStartPow();
  bool GetDSInfo();
  bool ProcessSetStateDeltaFromSeed(const bytes& message, unsigned int offset,
                                    const Peer& from);
  bool ProcessSetStateDeltasFromSeed(const bytes& message, unsigned int offset,
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

  bool ProcessGetCosigsRewardsFromSeed(const bytes& message,
                                       unsigned int offset, const Peer& from);

  void ComposeAndSendGetDirectoryBlocksFromSeed(
      const uint64_t& index_num, bool toSendSeed = true,
      const bool includeMinerInfo = false);

  void ComposeAndSendGetCosigsRewardsFromSeed(const uint64_t& block_num);

  static bool VerifySenderNode(const VectorOfNode& vecNodes,
                               const PubKey& pubKeyToVerify);

  static bool VerifySenderNode(const VectorOfNode& vecNodes,
                               const uint128_t& ipToVerify);

  static bool VerifySenderNode(const DequeOfNode& deqNodes,
                               const PubKey& pubKeyToVerify);

  static bool VerifySenderNode(const DequeOfNode& deqNodes,
                               const uint128_t& ipToVerify);

  static bool VerifySenderNode(const Shard& shard,
                               const PubKey& pubKeyToVerify);

  /// Check and fetch unavailable microblocks
  void CheckAndFetchUnavailableMBs(bool skipLatestTxBlk = true);

  /// used by seed node using pull option
  void FetchMbTxPendingTxMessageFromL2l(uint64_t blockNum);

  /// Find any unavailable mbs from last N txblks and add to
  /// m_unavailableMicroBlocks
  void FindMissingMBsForLastNTxBlks(const uint32_t& num);

  bool Execute(const bytes& message, unsigned int offset, const Peer& from);

  inline SyncType GetSyncType() const { return m_syncType.load(); }
  void SetSyncType(SyncType syncType);

  // Create MBnForwardTxn raw message for older txblk if not available in store.
  bool ComposeAndStoreMBnForwardTxnMessage(const uint64_t& blockNum);

  // Create VCFinal raw message for older txblk if not available in store.
  bool ComposeAndStoreVCFinalBlockMessage(const uint64_t& blockNum);

  // Create VCDS Block raw message for older txblk if not available in store.
  bool ComposeAndStoreVCDSBlockMessage(const uint64_t& blockNum);

  // Reset certain variables to the initial state
  bool CleanVariables();

  void SetLookupServer(std::shared_ptr<LookupServer> lookupServer) {
    m_lookupServer = std::move(lookupServer);
  }

  void SetStakingServer(std::shared_ptr<StakingServer> stakingServer) {
    m_stakingServer = std::move(stakingServer);
  }

  bool m_fetchedOfflineLookups = false;
  std::mutex m_mutexOfflineLookupsUpdation;
  std::condition_variable cv_offlineLookups;

  bool m_historicalDB = false;

  bool m_fetchedLatestDSBlock = false;
  std::mutex m_mutexLatestDSBlockUpdation;
  std::condition_variable cv_latestDSBlock;

  std::mutex m_MutexCVSetTxBlockFromSeed;
  std::condition_variable cv_setTxBlockFromSeed;
  std::condition_variable cv_setStateDeltaFromSeed;
  std::mutex m_mutexSetStateDeltaFromSeed;
  bool m_skipAddStateDeltaToAccountStore = false;

  std::mutex m_mutexCVJoined;
  std::condition_variable cv_waitJoined;

  bool InitMining(uint32_t lookupIndex);

  /// Helper variables used by new node synchronization
  bool m_startedPoW = false;

  bool AlreadyJoinedNetwork();

  void RemoveSeedNodesFromBlackList();

  std::mutex m_mutexDSInfoUpdation;
  std::condition_variable cv_dsInfoUpdate;

  // Start PoW variables
  std::set<Peer> m_getStartPoWPeerSet;
  std::mutex m_mutexGetStartPoWPeerSet;

  // For use by lookup for dispatching transactions
  std::atomic<bool> m_sendSCCallsToDS{};

  // extseed key
  PairOfKey m_extSeedKey;

  std::mutex m_mutexExtSeedWhitelisted;
  std::unordered_set<PubKey> m_extSeedWhitelisted;
  bool AddToWhitelistExtSeed(const PubKey& pubKey);
  bool RemoveFromWhitelistExtSeed(const PubKey& pubKey);
  bool IsWhitelistedExtSeed(const PubKey& pubKey);

  // VCDSblock processed variables - used by seed nodes using PULL P1 option
  std::mutex m_mutexVCDSBlockProcessed;
  std::condition_variable cv_vcDsBlockProcessed;
  bool m_vcDsBlockProcessed = false;

  // VCFinalblock processed variables - used by seed nodes using PULL P1 option
  std::mutex m_mutexVCFinalBlockProcessed;
  std::condition_variable cv_vcFinalBlockProcessed;
  bool m_vcFinalBlockProcessed = false;

  // exit trigger
  std::atomic<bool> m_exitPullThread{};
};

#endif  // ZILLIQA_SRC_LIBLOOKUP_LOOKUP_H_
