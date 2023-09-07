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
#include <condition_variable>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Schnorr.h>
#include "libBlockchain/DSBlock.h"
#include "libBlockchain/MicroBlock.h"
#include "libBlockchain/TxBlock.h"
#include "libData/AccountData/Transaction.h"
#include "libNetwork/Executable.h"
#include "libNetwork/ShardStruct.h"
#include "libUtils/IPConverter.h"

class Mediator;
class Synchronizer;
class LookupServer;
class StakingServer;

using TxnMemPool = std::vector<Transaction>;

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

  TxnMemPool m_txnMemPool;
  TxnMemPool m_txnMemPoolGenerated;
  std::map<Address, uint64_t> m_gentxnAddrLatestNonceSent;

  // Get StateDeltas from seed
  std::mutex m_mutexSetStateDeltasFromSeed;
  std::condition_variable cv_setStateDeltasFromSeed;
  bool m_setStateDeltasFromSeedSignal;

  // TxBlockBuffer
  std::vector<TxBlock> m_txBlockBuffer;

  std::shared_ptr<LookupServer> m_lookupServer;
  std::shared_ptr<StakingServer> m_stakingServer;

  zbytes ComposeGetDSInfoMessage(bool initialDS = false);

  zbytes ComposeGetDSBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum,
                                  const bool includeMinerInfo = false);
  zbytes ComposeGetDSBlockMessageForL2l(uint64_t blockNum);
  zbytes ComposeGetVCFinalBlockMessageForL2l(uint64_t blockNum);
  zbytes ComposeGetMBnForwardTxnMessageForL2l(uint64_t blockNum,
                                              uint32_t shardId);
  zbytes ComposeGetTxBlockMessage(uint64_t lowBlockNum, uint64_t highBlockNum);
  zbytes ComposeGetStateDeltaMessage(uint64_t blockNum);
  zbytes ComposeGetStateDeltasMessage(uint64_t lowBlockNum,
                                      uint64_t highBlockNum);

  zbytes ComposeGetLookupOfflineMessage();
  zbytes ComposeGetLookupOnlineMessage();

  zbytes ComposeGetOfflineLookupNodes();

  void RetrieveDSBlocks(std::vector<DSBlock>& dsBlocks, uint64_t& lowBlockNum,
                        uint64_t& highBlockNum);
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

  auto GetTransactionsFromMemPool() const { return m_txnMemPool; }

  std::mutex m_txnMemPoolMutex;
  std::mutex m_txnMemPoolGeneratedMutex;

  std::mutex m_mutexShardStruct;
  std::condition_variable cv_shardStruct;
  bool m_shardStructSignal;

  void ComposeAndSendGetShardingStructureFromSeed();

  bool IsLookupNode(const PubKey& pubKey) const;

  bool IsLookupNode(const Peer& peerInfo) const;

  // Gen n valid txns
  bool GenTxnToSend(size_t num_txn, std::vector<Transaction>& txnContainer,
                    const bool updateRemoteStorageDBForGenTxns);
  bool GenTxnToSend(size_t num_txn, std::vector<Transaction>& txns);

  // Try resolving ip from the given peer's DNS
  uint128_t TryGettingResolvedIP(const Peer& peer) const;

  // Calls P2PComm::SendBroadcastMessage to Lookup Nodes
  void SendMessageToLookupNodes(const zbytes& message) const;

  // Calls P2PComm::SendMessage serially to every Lookup Nodes
  void SendMessageToLookupNodesSerial(const zbytes& message) const;

  // Calls P2PComm::SendMessage to one of the last x Lookup Nodes randomly
  void SendMessageToRandomLookupNode(const zbytes& message) const;

  // Calls P2PComm::SendMessage serially for every Seed peer
  void SendMessageToSeedNodes(const zbytes& message) const;

  void SendMessageToRandomSeedNode(const zbytes& message) const;

  void SendMessageToRandomL2lDataProvider(const zbytes& message) const;

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
  bool ProcessGetShardFromSeed([[gnu::unused]] const zbytes& message,
                               [[gnu::unused]] unsigned int offset,
                               const Peer& from,
                               const unsigned char& startByte);
  // UNUSED
  bool ProcessSetShardFromSeed([[gnu::unused]] const zbytes& message,
                               [[gnu::unused]] unsigned int offset,
                               [[gnu::unused]] const Peer& from,
                               [[gnu::unused]] const unsigned char& startByte);
  bool GetDSBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highblocknum,
                               const bool includeMinerInfo = false);

  // USED when MULTIPLIER_SYNC_MODE == false
  bool GetDSBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetVCFinalBlockFromL2lDataProvider(uint64_t blockNum);
  bool GetMBnForwardTxnFromL2lDataProvider(uint64_t blockNum, uint32_t shardId);

  bool ProcessGetDSBlockFromL2l(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);
  bool ProcessGetVCFinalBlockFromL2l(const zbytes& message, unsigned int offset,
                                     const Peer& from,
                                     const unsigned char& startByte);
  bool ProcessGetMBnForwardTxnFromL2l(const zbytes& message,
                                      unsigned int offset, const Peer& from,
                                      const unsigned char& startByte);

  std::optional<std::vector<Transaction>> GetDSLeaderTxnPool();

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

  bool AddTxnToMemPool(const Transaction& tx);
  bool AddTxnToMemPool(const Transaction& tx, TxnMemPool& txnMemPool,
                       std::mutex& txnMemPoolMutex);

  void CheckBufferTxBlocks();

  bool ClearTxnMemPool();

  void SetServerTrue();

  bool GetIsServer();

  void SenderTxnBatchThread(std::vector<Transaction> transactions);

  void SendTxnPacketPrepare(std::vector<Transaction>& transactionsToSend);
  void SendTxnsToDSShard(std::vector<Transaction> transactions);
  void SendTxnPacketToShard(std::vector<Transaction> transactions);

  bool ProcessEntireShardingStructure();
  bool ProcessGetDSInfoFromSeed(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);
  bool ProcessGetDSBlockFromSeed(const zbytes& message, unsigned int offset,
                                 const Peer& from,
                                 const unsigned char& startByte);
  bool ProcessGetTxBlockFromSeed(const zbytes& message, unsigned int offset,
                                 const Peer& from,
                                 const unsigned char& startByte);
  bool ProcessGetStateDeltaFromSeed(const zbytes& message, unsigned int offset,
                                    const Peer& from,
                                    const unsigned char& startByte);
  bool ProcessGetStateDeltasFromSeed(const zbytes& message, unsigned int offset,
                                     const Peer& from,
                                     const unsigned char& startByte);

  bool ProcessGetTxnsFromLookup(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);

  bool ProcessGetTxnsFromL2l(const zbytes& message, unsigned int offset,
                             const Peer& from, const unsigned char& startByte);

  bool ProcessSetDSLeaderTxnPoolFromSeed(const zbytes& message,
                                         unsigned int offset, const Peer& from,
                                         const unsigned char& startByte);

  bool ProcessSetTxnsFromLookup(const zbytes& message, unsigned int offset,
                                [[gnu::unused]] const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);

  void SendGetTxnsFromLookup(const BlockHash& mbHash,
                             const std::vector<TxnHash>& txnhashes);

  void SendGetTxnsFromL2l(const BlockHash& mbHash,
                          const std::vector<TxnHash>& txnhashes);

  void SendGetMicroBlockFromLookup(const std::vector<BlockHash>& mbHashes);

  void SendGetMicroBlockFromL2l(const std::vector<BlockHash>& mbHashes);

  bool ProcessGetMicroBlockFromLookup(const zbytes& message,
                                      unsigned int offset, const Peer& from,
                                      const unsigned char& startByte);

  bool ProcessGetMicroBlockFromL2l(const zbytes& message, unsigned int offset,
                                   const Peer& from,
                                   const unsigned char& startByte);

  bool ProcessSetMicroBlockFromLookup(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool AddMicroBlockToStorage(const MicroBlock& microblock);

  bool ProcessGetOfflineLookups(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                const unsigned char& startByte);

  bool ProcessSetDSInfoFromSeed(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetDSBlockFromSeed(
      const zbytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetMinerInfoFromSeed(
      const zbytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetTxBlockFromSeed(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool CommitTxBlocks(const std::vector<TxBlock>& txBlocks);
  void PrepareForStartPow();
  bool GetDSInfo();
  bool ProcessSetStateDeltaFromSeed(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetStateDeltasFromSeed(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessSetLookupOffline(const zbytes& message, unsigned int offset,
                               const Peer& from,
                               [[gnu::unused]] const unsigned char& startByte);
  bool ProcessSetLookupOnline(const zbytes& message, unsigned int offset,
                              const Peer& from,
                              [[gnu::unused]] const unsigned char& startByte);

  bool ProcessSetOfflineLookups(const zbytes& message, unsigned int offset,
                                const Peer& from,
                                [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetDirectoryBlocksFromSeed(const zbytes& message,
                                         unsigned int offset, const Peer& from,
                                         const unsigned char& startByte);

  bool ProcessSetDirectoryBlocksFromSeed(
      const zbytes& message, unsigned int offset,
      [[gnu::unused]] const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessVCGetLatestDSTxBlockFromSeed(const zbytes& message,
                                           unsigned int offset,
                                           const Peer& from,
                                           const unsigned char& startByte);
  bool ProcessForwardTxn(const zbytes& message, unsigned int offset,
                         const Peer& from,
                         [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetDSGuardNetworkInfo(
      const zbytes& message, unsigned int offset, const Peer& from,
      [[gnu::unused]] const unsigned char& startByte);

  bool ProcessGetCosigsRewardsFromSeed(const zbytes& message,
                                       unsigned int offset, const Peer& from,
                                       const unsigned char& startByte);

  bool NoOp([[gnu::unused]] const zbytes& message,
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

  bool Execute(const zbytes& message, unsigned int offset, const Peer& from,
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

  bool StartJsonRpcPort();
  bool StopJsonRpcPort();

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
  bool m_setCosigRewardsFromSeedSignal;

  // Seed rejoin recovery
  std::mutex m_mutexCvSetRejoinRecovery;
  std::condition_variable cv_setRejoinRecovery;
  std::atomic<bool> m_rejoinRecoverySignal{false};

  // Enable/Disable jsonrpc port
  std::mutex m_mutexJsonRpc;

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

  std::mutex m_mutexDSLeaderTxnPool;
  std::condition_variable cv_dsLeaderTxnPool;
  std::vector<Transaction> m_dsLeaderTxnPool;
  bool m_dsLeaderTxnPoolSignal = false;

  // exit trigger
  std::atomic<bool> m_exitPullThread{};
};

#endif  // ZILLIQA_SRC_LIBLOOKUP_LOOKUP_H_
