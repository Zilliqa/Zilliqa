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
#include "libNetwork/P2PComm.h"
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
using TxnShardMap =
    std::map<uint32_t, std::deque<std::pair<Transaction, uint32_t>>>;

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
  std::vector<Address> m_myDSGenesisAccounts1;
  std::vector<Address> m_myDSGenesisAccounts2;

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
  std::map<Address, uint64_t> m_gentxnAddrLatestNonceSent;

  // Get StateDeltas from seed
  std::mutex m_mutexSetStateDeltasFromSeed;
  std::condition_variable cv_setStateDeltasFromSeed;

  // TxBlockBuffer
  std::vector<TxBlock> m_txBlockBuffer;

  std::shared_ptr<LookupServer> m_lookupServer;
  std::shared_ptr<StakingServer> m_stakingServer;

  bytes ComposeGetDSInfoMessage(bool initialDS = false);

  bytes ComposeGetDSBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum,
                                 const bool includeMinerInfo = false);
  bytes ComposeGetDSBlockMessageForL2l(uint64_t blockNum);
  bytes ComposeGetVCFinalBlockMessageForL2l(uint64_t blockNum);
  bytes ComposeGetMBnForwardTxnMessageForL2l(uint64_t blockNum,
                                             uint32_t shardId);
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
  void GetInitialBlocksAndShardingStructure();

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

  std::deque<std::pair<Transaction, uint32_t>>& GetTxnFromShardMap(
      uint32_t index);  // Use m_txnShardMapMutex with this function

  std::mutex m_mutexShardStruct;
  std::condition_variable cv_shardStruct;

  void ComposeAndSendGetShardingStructureFromSeed();

  bool IsLookupNode(const PubKey& pubKey) const;

  bool IsLookupNode(const Peer& peerInfo) const;

  // Gen n valid txns
  bool GenTxnToSend(
      size_t num_txn,
      std::map<uint32_t, std::deque<std::pair<Transaction, uint32_t>>>& mp,
      uint32_t numShards, const bool updateRemoteStorageDBForGenTxns);
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

  // UNUSED
  bool ProcessGetShardFromSeed([[gnu::unused]] const bytes& message,
                               [[gnu::unused]] unsigned int offset,
                               const Peer& from,
                               const unsigned char& startByte);
  // UNUSED
  bool ProcessSetShardFromSeed([[gnu::unused]] const bytes& message,
                               [[gnu::unused]] unsigned int offset,
                               [[gnu::unused]] const Peer& from,
                               [[gnu::unused]] const unsigned char& startByte);
  bool GetDSBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highblocknum,
                               const bool includeMinerInfo = false);

  // USED when MULTIPLIER_SYNC_MODE == false
  bool GetDSBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetVCFinalBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetMBnForwardTxnFromL2lDataProvider(uint64_t blockNum, uint32_t shardId);

  bool ProcessGetDSBlockFromL2l(const bytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);
  bool ProcessGetVCFinalBlockFromL2l(const bytes& message, unsigned int offset,
                                     const Peer& from,
                                     const unsigned char& startByte);
  bool ProcessGetMBnForwardTxnFromL2l(const bytes& message, unsigned int offset,
                                      const Peer& from,
                                      const unsigned char& startByte);

  // Get the offline lookup nodes from lookup nodes
  bool GetOfflineLookupNodes();

  bool SetDSCommitteInfo(bool replaceMyPeerWithDefault = false);

  DequeOfNode GetDSComm();
  DequeOfShard GetShardPeers();
  VectorOfPeer GetNodePeers();

  // Start synchronization with other lookup nodes as a lookup node
  void StartSynchronization();

  // Set my lookup ip offline in other lookup nodes
  bool GetMyLookupOffline();

  // Set my lookup ip online in other lookup nodes
  bool GetMyLookupOnline(bool fromRecovery = false);

  // Rejoin the network as a lookup node in case of failure happens in protocol
  void RejoinAsLookup(bool fromLookup = true);

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
                            const uint32_t newNumShards,
                            const bool updateRemoteStorageDBForGenTxns = true);
  void SendTxnPacketToNodes(const uint32_t oldNumShards,
                            const uint32_t newNumShards);
  void SendTxnPacketToDS(const uint32_t oldNumShards,
                         const uint32_t newNumShards);
  void SendTxnPacketToShard(const uint32_t shardId, bool toDS,
                            bool afterSoftConfirmation = false);

  bool ProcessEntireShardingStructure();
  bool ProcessGetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);
  bool ProcessGetDSBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from,
                                 const unsigned char& startByte);
  bool ProcessGetTxBlockFromSeed(const bytes& message, unsigned int offset,
                                 const Peer& from,
                                 const unsigned char& startByte);
  bool ProcessGetStateDeltaFromSeed(const bytes& message, unsigned int offset,
                                    const Peer& from,
                                    const unsigned char& startByte);
  bool ProcessGetStateDeltasFromSeed(const bytes& message, unsigned int offset,
                                     const Peer& from,
                                     const unsigned char& startByte);

  bool ProcessGetTxnsFromLookup(const bytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);

  bool ProcessGetTxnsFromL2l(const bytes& message, unsigned int offset,
                             const Peer& from, const unsigned char& startByte);

  bool ProcessSetTxnsFromLookup(const bytes& message, unsigned int offset,
                                [[gnu::unused]] const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);

  void SendGetTxnsFromLookup(const BlockHash& mbHash,
                             const std::vector<TxnHash>& txnhashes);

  void SendGetTxnsFromL2l(const BlockHash& mbHash,
                          const std::vector<TxnHash>& txnhashes);

  void SendGetMicroBlockFromLookup(const std::vector<BlockHash>& mbHashes);

  void SendGetMicroBlockFromL2l(const std::vector<BlockHash>& mbHashes);

  bool ProcessGetMicroBlockFromLookup(const bytes& message, unsigned int offset,
                                      const Peer& from,
                                      const unsigned char& startByte);

  bool ProcessGetMicroBlockFromL2l(const bytes& message, unsigned int offset,
                                   const Peer& from,
                                   const unsigned char& startByte);

  bool ProcessSetMicroBlockFromLookup(
      const bytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool AddMicroBlockToStorage(const MicroBlock& microblock);

  bool ProcessGetOfflineLookups(const bytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);

  bool ProcessSetDSInfoFromSeed(const bytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetDSBlockFromSeed(
      const bytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetMinerInfoFromSeed(
      const bytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetTxBlockFromSeed(
      const bytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool CommitTxBlocks(const std::vector<TxBlock>& txBlocks);
  void PrepareForStartPow();
  bool GetDSInfo();
  bool ProcessSetStateDeltaFromSeed(
      const bytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetStateDeltasFromSeed(
      const bytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessSetLookupOffline(const bytes& message, unsigned int offset,
                               const Peer& from,
                               [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetLookupOnline(const bytes& message, unsigned int offset,
                              const Peer& from,
                              [[gnu::unused]] const unsigned char& startByte);

  bool ProcessSetOfflineLookups(const bytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetDirectoryBlocksFromSeed(const bytes& message,
                                         unsigned int offset, const Peer& from,
                                         const unsigned char& startByte);

  bool ProcessSetDirectoryBlocksFromSeed(
      const bytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessVCGetLatestDSTxBlockFromSeed(const bytes& message,
                                           unsigned int offset,
                                           const Peer& from,
                                           const unsigned char& startByte);
  bool ProcessForwardTxn(const bytes& message, unsigned int offset,
                         const Peer& from,
                         [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetDSGuardNetworkInfo(
      const bytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetCosigsRewardsFromSeed(const bytes& message,
                                       unsigned int offset, const Peer& from,
                                       const unsigned char& startByte);

  bool NoOp([[gnu::unused]] const bytes& message,
            [[gnu::unused]] unsigned int offset,
            [[gnu::unused]] const Peer& from,
            [[gnu::unused]] const unsigned char& startByte);

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
  void FetchMBnForwardTxMessageFromL2l(uint64_t blockNum);

  /// Find any unavailable mbs from last N txblks and add to
  /// m_unavailableMicroBlocks
  void FindMissingMBsForLastNTxBlks(const uint32_t& num);

  bool Execute(const bytes& message, unsigned int offset, const Peer& from,
               const unsigned char& startByte);

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

  void RejoinNetwork();

  uint16_t m_rejoinNetworkAttempts{0};

  bool m_fetchedOfflineLookups = false;
  std::mutex m_mutexOfflineLookupsUpdation;
  std::condition_variable cv_offlineLookups;

  bool m_fetchedLatestDSBlock = false;
  std::mutex m_mutexLatestDSBlockUpdation;
  std::condition_variable cv_latestDSBlock;
  bool m_confirmedLatestDSBlock = false;

  std::mutex m_MutexCVSetTxBlockFromSeed;
  std::condition_variable cv_setTxBlockFromSeed;
  std::condition_variable cv_setStateDeltaFromSeed;
  std::mutex m_mutexSetStateDeltaFromSeed;
  bool m_skipAddStateDeltaToAccountStore = false;
  std::atomic<bool> m_fetchNextTxBlock{false};

  std::mutex m_mutexCVJoined;
  std::condition_variable cv_waitJoined;

  // Get cosigrewards from seed
  std::mutex m_mutexSetCosigRewardsFromSeed;
  std::condition_variable cv_setCosigRewardsFromSeed;

  // Seed rejoin recovery
  std::mutex m_mutexCvSetRejoinRecovery;
  std::condition_variable cv_setRejoinRecovery;

  std::atomic<bool> m_rejoinInProgress{false};

  bool InitMining();

  /// Helper variables used by new node synchronization
  bool m_startedPoW = false;

  bool AlreadyJoinedNetwork();

  void RemoveSeedNodesFromBlackList();

  std::mutex m_mutexDSInfoUpdation;
  std::condition_variable cv_dsInfoUpdate;

  // For use by lookup for dispatching transactions
  std::atomic<bool> m_sendSCCallsToDS{};

  // For use by lookup for sending all transactions
  std::atomic<bool> m_sendAllToDS{};

  // extseed key
  PairOfKey m_extSeedKey;

  std::mutex m_mutexExtSeedWhitelisted;
  std::unordered_set<PubKey> m_extSeedWhitelisted;
  bool AddToWhitelistExtSeed(const PubKey& pubKey);
  bool RemoveFromWhitelistExtSeed(const PubKey& pubKey);
  bool IsWhitelistedExtSeed(const PubKey& pubKey, const Peer& from,
                            const unsigned char& startByte);

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
