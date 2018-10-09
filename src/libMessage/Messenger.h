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

#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/ForwardedTxnEntry.h"
#include "libData/BlockData/Block.h"
#include "libDirectoryService/ShardStruct.h"
#include "libNetwork/Peer.h"

#ifndef __MESSENGER_H__
#define __MESSENGER_H__

class Messenger {
 public:
  // ============================================================================
  // Directory Service messages
  // ============================================================================

  static bool SetDSPoWSubmission(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t blockNumber, const uint8_t difficultyLevel,
      const Peer& submitterPeer, const std::pair<PrivKey, PubKey>& submitterKey,
      const uint64_t nonce, const std::string& resultingHash,
      const std::string& mixHash);

  static bool GetDSPoWSubmission(const std::vector<unsigned char>& src,
                                 const unsigned int offset,
                                 uint64_t& blockNumber,
                                 uint8_t& difficultyLevel, Peer& submitterPeer,
                                 PubKey& submitterPubKey, uint64_t& nonce,
                                 std::string& resultingHash,
                                 std::string& mixHash, Signature& signature);

  static bool SetDSMicroBlockSubmission(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const unsigned char microBlockType, const uint64_t blockNumber,
      const std::vector<MicroBlock>& microBlocks,
      const std::vector<unsigned char>& stateDelta);
  static bool GetDSMicroBlockSubmission(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        unsigned char& microBlockType,
                                        uint64_t& blockNumber,
                                        std::vector<MicroBlock>& microBlocks,
                                        std::vector<unsigned char>& stateDelta);

  static bool SetDSDSBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const DSBlock& dsBlock,
      const DequeOfShard& shards, const std::vector<Peer>& dsReceivers,
      const std::vector<std::vector<Peer>>& shardReceivers,
      const std::vector<std::vector<Peer>>& shardSenders,
      std::vector<unsigned char>& messageToCosign);

  static bool GetDSDSBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, DSBlock& dsBlock, DequeOfShard& shards,
      std::vector<Peer>& dsReceivers,
      std::vector<std::vector<Peer>>& shardReceivers,
      std::vector<std::vector<Peer>>& shardSenders,
      std::vector<unsigned char>& messageToCosign);

  static bool SetDSFinalBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const TxBlock& txBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool GetDSFinalBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, TxBlock& txBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool SetDSVCBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const VCBlock& vcBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool GetDSVCBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, VCBlock& vcBlock,
      std::vector<unsigned char>& messageToCosign);

  // ============================================================================
  // Node messages
  // ============================================================================

  static bool SetNodeDSBlock(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t shardId, const DSBlock& dsBlock,
      const DequeOfShard& shards, const std::vector<Peer>& dsReceivers,
      const std::vector<std::vector<Peer>>& shardReceivers,
      const std::vector<std::vector<Peer>>& shardSenders);

  static bool GetNodeDSBlock(const std::vector<unsigned char>& src,
                             const unsigned int offset, uint32_t& shardId,
                             DSBlock& dsBlock, DequeOfShard& shards,
                             std::vector<Peer>& dsReceivers,
                             std::vector<std::vector<Peer>>& shardReceivers,
                             std::vector<std::vector<Peer>>& shardSenders);

  static bool SetNodeFinalBlock(std::vector<unsigned char>& dst,
                                const unsigned int offset,
                                const uint32_t shardId,
                                const uint64_t dsBlockNumber,
                                const uint32_t consensusID,
                                const TxBlock& txBlock,
                                const std::vector<unsigned char>& stateDelta);

  static bool GetNodeFinalBlock(const std::vector<unsigned char>& src,
                                const unsigned int offset, uint32_t& shardId,
                                uint64_t& dsBlockNumber, uint32_t& consensusID,
                                TxBlock& txBlock,
                                std::vector<unsigned char>& stateDelta);

  static bool SetNodeVCBlock(std::vector<unsigned char>& dst,
                             const unsigned int offset, const VCBlock& vcBlock);
  static bool GetNodeVCBlock(const std::vector<unsigned char>& src,
                             const unsigned int offset, VCBlock& vcBlock);

  static bool SetNodeForwardTransaction(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t blockNum, const MicroBlockHashSet& hashes,
      const uint32_t& shardId, const std::vector<TransactionWithReceipt>& txns);
  static bool GetNodeForwardTransaction(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        ForwardedTxnEntry& entry);

  static bool SetNodeForwardTxnBlock(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t epochNumber, const uint32_t shardId,
      const std::vector<Transaction>& txnsCurrent,
      const std::vector<unsigned char>& txnsGenerated);
  static bool GetNodeForwardTxnBlock(const std::vector<unsigned char>& src,
                                     const unsigned int offset,
                                     uint64_t& epochNumber, uint32_t& shardId,
                                     std::vector<Transaction>& txns);

  static bool SetNodeMicroBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const MicroBlock& microBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool GetNodeMicroBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, MicroBlock& microBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool SetNodeFallbackBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey,
      const FallbackBlock& fallbackBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool GetNodeFallbackBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, FallbackBlock& fallbackBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool SetNodeFallbackBlock(std::vector<unsigned char>& dst,
                                   const unsigned int offset,
                                   const FallbackBlock& fallbackBlock);
  static bool GetNodeFallbackBlock(const std::vector<unsigned char>& src,
                                   const unsigned int offset,
                                   FallbackBlock& fallbackBlock);

  // ============================================================================
  // Lookup messages
  // ============================================================================

  static bool SetLookupGetSeedPeers(std::vector<unsigned char>& dst,
                                    const unsigned int offset,
                                    const uint32_t listenPort);
  static bool GetLookupGetSeedPeers(const std::vector<unsigned char>& src,
                                    const unsigned int offset,
                                    uint32_t& listenPort);
  static bool SetLookupSetSeedPeers(std::vector<unsigned char>& dst,
                                    const unsigned int offset,
                                    const std::vector<Peer>& candidateSeeds);
  static bool GetLookupSetSeedPeers(const std::vector<unsigned char>& src,
                                    const unsigned int offset,
                                    std::vector<Peer>& candidateSeeds);
  static bool SetLookupGetDSInfoFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetDSInfoFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetDSInfoFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::deque<std::pair<PubKey, Peer>>& dsNodes);
  static bool GetLookupSetDSInfoFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::deque<std::pair<PubKey, Peer>>& dsNodes);
  static bool SetLookupGetDSBlockFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetDSBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupSetDSBlockFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const std::vector<DSBlock>& dsBlocks);
  static bool GetLookupSetDSBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          std::vector<DSBlock>& dsBlocks);
  static bool SetLookupGetTxBlockFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetTxBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupSetTxBlockFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const std::vector<TxBlock>& txBlocks);
  static bool GetLookupSetTxBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          std::vector<TxBlock>& txBlocks);
  static bool SetLookupGetTxBodyFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::vector<unsigned char>& txHash, const uint32_t listenPort);
  static bool GetLookupGetTxBodyFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         TxnHash& txHash, uint32_t& listenPort);
  static bool SetLookupSetTxBodyFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const TxnHash& txHash,
                                         const TransactionWithReceipt& txBody);
  static bool GetLookupSetTxBodyFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         TxnHash& txHash,
                                         TransactionWithReceipt& txBody);
  static bool SetLookupSetNetworkIDFromSeed(std::vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const std::string& networkID);
  static bool GetLookupSetNetworkIDFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::string& networkID);
  static bool SetLookupGetStateFromSeed(std::vector<unsigned char>& dst,
                                        const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupGetStateFromSeed(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetStateFromSeed(std::vector<unsigned char>& dst,
                                        const unsigned int offset,
                                        const AccountStore& accountStore);
  static bool GetLookupSetStateFromSeed(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        AccountStore& accountStore);
  static bool SetLookupSetLookupOffline(std::vector<unsigned char>& dst,
                                        const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupSetLookupOffline(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetLookupOnline(std::vector<unsigned char>& dst,
                                       const unsigned int offset,
                                       const uint32_t listenPort);
  static bool GetLookupSetLookupOnline(const std::vector<unsigned char>& src,
                                       const unsigned int offset,
                                       uint32_t& listenPort);
  static bool SetLookupGetOfflineLookups(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetOfflineLookups(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetOfflineLookups(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const std::vector<Peer>& nodes);
  static bool GetLookupSetOfflineLookups(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         std::vector<Peer>& nodes);
  static bool SetLookupGetStartPoWFromSeed(std::vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort);
  static bool GetLookupGetStartPoWFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint32_t& listenPort);

  static bool SetLookupGetShardsFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);

  static bool GetLookupGetShardsFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);

  static bool SetLookupSetShardsFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const DequeOfShard& shards);

  static bool GetLookupSetShardsFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         DequeOfShard& shards);

  static bool SetLookupGetMicroBlockFromLookup(
      std::vector<unsigned char>& dest, const unsigned int offset,
      const std::map<uint64_t, std::vector<uint32_t>>& microBlockInfo,
      uint32_t portNo);

  static bool GetLookupGetMicroBlockFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::map<uint64_t, std::vector<uint32_t>>& microBlockInfo,
      uint32_t& portNo);

  static bool SetLookupSetMicroBlockFromLookup(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::vector<MicroBlock>& mbs);

  static bool GetLookupSetMicroBlockFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<MicroBlock>& mbs);

  static bool SetLookupGetTxnsFromLookup(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const std::vector<TxnHash>& txnhashes,
                                         uint32_t portNo);
  static bool GetLookupGetTxnsFromLookup(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         std::vector<TxnHash>& txnhashes,
                                         uint32_t& portNo);
  static bool SetLookupSetTxnsFromLookup(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetLookupSetTxnsFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<TransactionWithReceipt>& txns);

  // ============================================================================
  // Consensus messages
  // ============================================================================

  template <class T>
  static bool GetConsensusID(const std::vector<unsigned char>& src,
                             const unsigned int offset, uint32_t& consensusID) {
    LOG_MARKER();

    T consensus_message;

    consensus_message.ParseFromArray(src.data() + offset, src.size() - offset);

    if (!consensus_message.IsInitialized()) {
      LOG_GENERAL(WARNING, "Consensus message initialization failed.");
      return false;
    }

    consensusID = consensus_message.consensusinfo().consensusid();

    return true;
  }

  static bool SetConsensusCommit(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t backupID,
      const CommitPoint& commit, const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusCommit(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, uint16_t& backupID,
      CommitPoint& commit,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);

  static bool SetConsensusChallenge(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const CommitPoint& aggregatedCommit, const PubKey& aggregatedKey,
      const Challenge& challenge, const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusChallenge(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      CommitPoint& aggregatedCommit, PubKey& aggregatedKey,
      Challenge& challenge, const PubKey& leaderKey);

  static bool SetConsensusResponse(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t backupID,
      const Response& response, const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusResponse(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, uint16_t& backupID,
      Response& response,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);

  static bool SetConsensusCollectiveSig(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const Signature& collectiveSig, const std::vector<bool>& bitmap,
      const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusCollectiveSig(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      std::vector<bool>& bitmap, Signature& collectiveSig,
      const PubKey& leaderKey);

  static bool SetConsensusCommitFailure(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t backupID,
      const std::vector<unsigned char>& errorMsg,
      const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusCommitFailure(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, uint16_t& backupID,
      std::vector<unsigned char>& errorMsg,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);
};
#endif  // __MESSENGER_H__
