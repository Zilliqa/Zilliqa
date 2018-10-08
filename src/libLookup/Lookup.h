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
#include "libData/BlockData/Block/MicroBlock.h"
#include "libDirectoryService/ShardStruct.h"
#include "libNetwork/Peer.h"
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
  std::vector<Peer> m_lookupNodes;
  std::vector<Peer> m_lookupNodesOffline;
  std::vector<Peer> m_seedNodes;
  bool m_dsInfoWaitingNotifying = false;
  bool m_fetchedDSInfo = false;

  // To ensure that the confirm of DS node rejoin won't be later than
  // It receiving a new DS block
  bool m_currDSExpired = false;
  bool m_isFirstLoop = true;
  // tells if server is running or not
  bool m_isServer = false;

  // Sharding committee members

  std::mutex m_mutexNodesInNetwork;
  std::vector<Peer> m_nodesInNetwork;
  std::unordered_set<Peer> l_nodesInNetwork;
  std::map<uint32_t, std::vector<Transaction>> m_txnShardMap;
  std::mutex m_txnShardMapMutex;

  // Start PoW variables
  bool m_receivedRaiseStartPoW = false;
  std::mutex m_MutexCVStartPoWSubmission;
  std::condition_variable cv_startPoWSubmission;

  // Rsync the lost txBodies from remote lookup nodes if this lookup are doing
  // its recovery
  Peer GetLookupPeerToRsync();

  // Doing Rsync commands
  bool RsyncTxBodies();

  /// Post processing after the DS node successfully synchronized with the
  /// network
  bool FinishRejoinAsLookup();

  // Reset certain variables to the initial state
  bool CleanVariables();

  // To block certain types of incoming message for certain states
  bool ToBlockMessage(unsigned char ins_byte);

  std::mutex m_mutexSetDSBlockFromSeed;
  std::mutex m_mutexSetTxBlockFromSeed;
  std::mutex m_mutexSetTxBodyFromSeed;
  std::mutex m_mutexSetState;
  std::mutex m_mutexOfflineLookups;
  std::mutex m_mutexMicroBlocksBuffer;

  std::vector<unsigned char> ComposeGetDSInfoMessage();
  std::vector<unsigned char> ComposeGetStateMessage();

  std::unordered_map<uint64_t, std::vector<MicroBlock>> m_microBlocksBuffer;

  std::vector<unsigned char> ComposeGetDSBlockMessage(uint64_t lowBlockNum,
                                                      uint64_t highBlockNum);
  std::vector<unsigned char> ComposeGetTxBlockMessage(uint64_t lowBlockNum,
                                                      uint64_t highBlockNum);

  std::vector<unsigned char> ComposeGetLookupOfflineMessage();
  std::vector<unsigned char> ComposeGetLookupOnlineMessage();

  std::vector<unsigned char> ComposeGetOfflineLookupNodes();

  // Append time stamp to the message to avoid discarding due to same message
  // hash
  void AppendTimestamp(std::vector<unsigned char>& message,
                       unsigned int& offset);

 public:
  /// Constructor.
  Lookup(Mediator& mediator);

  /// Destructor.
  ~Lookup();

  // Setting the lookup nodes
  // Hardcoded for now -- to be called by constructor
  void SetLookupNodes();

  bool CheckStateRoot();

  // Getter for m_lookupNodes
  std::vector<Peer> GetLookupNodes();

  // Gen n valid txns
  bool GenTxnToSend(size_t num_txn,
                    std::map<uint32_t, std::vector<unsigned char>>& mp,
                    uint32_t numShards);

  // Calls P2PComm::SendBroadcastMessage to Lookup Nodes
  void SendMessageToLookupNodes(
      const std::vector<unsigned char>& message) const;

  // Calls P2PComm::SendMessage serially to every Lookup Nodes
  void SendMessageToLookupNodesSerial(
      const std::vector<unsigned char>& message) const;

  // Calls P2PComm::SendMessage to one of the last x Lookup Nodes randomly
  void SendMessageToRandomLookupNode(
      const std::vector<unsigned char>& message) const;

  // Calls P2PComm::SendMessage serially for every Seed peer
  void SendMessageToSeedNodes(const std::vector<unsigned char>& message) const;

  // TODO: move the Get and ProcessSet functions to Synchronizer
  bool GetSeedPeersFromLookup();
  bool GetDSInfoFromSeedNodes();
  bool GetDSBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBlockFromSeedNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetDSInfoFromLookupNodes();
  bool GetDSBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBlockFromLookupNodes(uint64_t lowBlockNum, uint64_t highBlockNum);
  bool GetTxBodyFromSeedNodes(std::string txHashStr);
  bool GetStateFromLookupNodes();

  bool ProcessGetShardFromSeed(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);

  bool ProcessSetShardFromSeed(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
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

  bool DeleteTxnShardMap(uint32_t shardId);

  void SetServerTrue();

  bool GetIsServer();

  void SenderTxnBatchThread();

  void SendTxnPacketToNodes(uint32_t);

  bool ProcessEntireShardingStructure();
  bool ProcessGetSeedPeersFromLookup(const std::vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from);
  bool ProcessGetDSInfoFromSeed(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessGetDSBlockFromSeed(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
  bool ProcessGetTxBlockFromSeed(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
  bool ProcessGetTxBodyFromSeed(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessGetStateFromSeed(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);

  bool ProcessGetNetworkId(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from);

  bool ProcessSetMicroBlockFromSeed(const std::vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from);

  bool ProcessGetTxnsFromLookup(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessSetTxnsFromLookup(const std::vector<unsigned char>& message,
                                unsigned int offset,
                                [[gnu::unused]] const Peer& from);
  void SendGetTxnFromLookup(const std::vector<TxnHash>& txnhashes);

  void CommitMicroBlockStorage();

  void SendGetMicroBlockFromLookup(
      const std::map<uint64_t, std::vector<uint32_t>>& mbInfos);

  bool ProcessGetMicroBlockFromLookup(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from);
  bool ProcessSetMicroBlockFromLookup(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from);
  bool AddMicroBlockToStorage(const uint64_t& blocknum,
                              const MicroBlock& microblock);

  bool ProcessGetOfflineLookups(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);

  bool ProcessSetSeedPeersFromLookup(const std::vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from);
  bool ProcessSetDSInfoFromSeed(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessSetDSBlockFromSeed(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
  bool ProcessSetTxBlockFromSeed(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);
  bool ProcessSetTxBodyFromSeed(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);
  bool ProcessSetStateFromSeed(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);

  bool ProcessSetLookupOffline(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
  bool ProcessSetLookupOnline(const std::vector<unsigned char>& message,
                              unsigned int offset, const Peer& from);

  bool ProcessSetOfflineLookups(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from);

  bool ProcessRaiseStartPoW(const std::vector<unsigned char>& message,
                            unsigned int offset, const Peer& from);
  bool ProcessGetStartPoWFromSeed(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);
  bool ProcessSetStartPoWFromSeed(const std::vector<unsigned char>& message,
                                  unsigned int offset, const Peer& from);

  bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
               const Peer& from);

  bool m_fetchedOfflineLookups = false;
  std::mutex m_mutexOfflineLookupsUpdation;
  std::condition_variable cv_offlineLookups;

  bool InitMining();

  /// To indicate which type of synchronization is using
  unsigned int m_syncType = SyncType::NO_SYNC;

  /// Helper variables used by new node synchronization
  bool m_startedPoW = false;

  bool AlreadyJoinedNetwork();

  std::mutex m_mutexDSInfoUpdation;
  std::condition_variable cv_dsInfoUpdate;
};

#endif  // __LOOKUP_H__
