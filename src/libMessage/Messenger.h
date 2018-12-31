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
#ifndef __MESSENGER_H__
#define __MESSENGER_H__

#include <boost/variant.hpp>
#include "common/BaseType.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/MBnForwardedTxnEntry.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libDirectoryService/DirectoryService.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"

class Messenger {
 public:
  template <class K, class V>
  static bool CopyWithSizeCheck(const K& arr, V& result) {
    // Fixed length copying.
    if (arr.size() != result.size()) {
      LOG_GENERAL(WARNING, "Size check while copying failed. Size expected = "
                               << result.size() << ", actual = " << arr.size());
      return false;
    }

    std::copy(arr.begin(), arr.end(), result.begin());
    return true;
  }

  // ============================================================================
  // Primitives
  // ============================================================================

  static bool GetDSCommitteeHash(const DequeOfDSNode& dsCommittee,
                                 CommitteeHash& dst);
  static bool GetShardHash(const Shard& shard, CommitteeHash& dst);

  static bool GetShardingStructureHash(const DequeOfShard& shards,
                                       ShardingHash& dst);

  static bool SetAccount(bytes& dst, const unsigned int offset,
                         const Account& account);
  [[gnu::unused]] static bool GetAccount(const bytes& src,
                                         const unsigned int offset,
                                         Account& account);

  static bool SetAccountDelta(bytes& dst, const unsigned int offset,
                              Account* oldAccount, const Account& newAccount);
  static bool GetAccountDelta(const bytes& src, const unsigned int offset,
                              Account& account, const bool fullCopy);

  // These are called by AccountStoreBase template class
  template <class MAP>
  static bool SetAccountStore(bytes& dst, const unsigned int offset,
                              const MAP& addressToAccount);
  template <class MAP>
  static bool GetAccountStore(const bytes& src, const unsigned int offset,
                              MAP& addressToAccount);
  static bool GetAccountStore(const bytes& src, const unsigned int offset,
                              AccountStore& accountStore);

  // These are called by AccountStore class
  static bool SetAccountStoreDelta(bytes& dst, const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp,
                                   AccountStore& accountStore);
  static bool GetAccountStoreDelta(const bytes& src, const unsigned int offset,
                                   AccountStore& accountStore,
                                   const bool reversible);
  static bool GetAccountStoreDelta(const bytes& src, const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp);

  static bool GetMbInfoHash(const std::vector<MicroBlockInfo>& mbInfos,
                            MBInfoHash& dst);

  static bool SetDSBlockHeader(bytes& dst, const unsigned int offset,
                               const DSBlockHeader& dsBlockHeader,
                               bool concreteVarsOnly = false);
  static bool GetDSBlockHeader(const bytes& src, const unsigned int offset,
                               DSBlockHeader& dsBlockHeader);
  static bool SetDSBlock(bytes& dst, const unsigned int offset,
                         const DSBlock& dsBlock);
  static bool GetDSBlock(const bytes& src, const unsigned int offset,
                         DSBlock& dsBlock);

  static bool SetMicroBlockHeader(bytes& dst, const unsigned int offset,
                                  const MicroBlockHeader& microBlockHeader);
  static bool GetMicroBlockHeader(const bytes& src, const unsigned int offset,
                                  MicroBlockHeader& microBlockHeader);
  static bool SetMicroBlock(bytes& dst, const unsigned int offset,
                            const MicroBlock& microBlock);
  static bool GetMicroBlock(const bytes& src, const unsigned int offset,
                            MicroBlock& microBlock);

  static bool SetTxBlockHeader(bytes& dst, const unsigned int offset,
                               const TxBlockHeader& txBlockHeader);
  static bool GetTxBlockHeader(const bytes& src, const unsigned int offset,
                               TxBlockHeader& txBlockHeader);
  static bool SetTxBlock(bytes& dst, const unsigned int offset,
                         const TxBlock& txBlock);
  static bool GetTxBlock(const bytes& src, const unsigned int offset,
                         TxBlock& txBlock);

  static bool SetVCBlockHeader(bytes& dst, const unsigned int offset,
                               const VCBlockHeader& vcBlockHeader);
  static bool GetVCBlockHeader(const bytes& src, const unsigned int offset,
                               VCBlockHeader& vcBlockHeader);
  static bool SetVCBlock(bytes& dst, const unsigned int offset,
                         const VCBlock& vcBlock);
  static bool GetVCBlock(const bytes& src, const unsigned int offset,
                         VCBlock& vcBlock);

  static bool SetFallbackBlockHeader(
      bytes& dst, const unsigned int offset,
      const FallbackBlockHeader& fallbackBlockHeader);
  static bool GetFallbackBlockHeader(const bytes& src,
                                     const unsigned int offset,
                                     FallbackBlockHeader& fallbackBlockHeader);
  static bool SetFallbackBlock(bytes& dst, const unsigned int offset,
                               const FallbackBlock& fallbackBlock);
  static bool GetFallbackBlock(const bytes& src, const unsigned int offset,
                               FallbackBlock& fallbackBlock);
  static bool SetTransactionCoreInfo(bytes& dst, const unsigned int offset,
                                     const TransactionCoreInfo& transaction);
  static bool GetTransactionCoreInfo(const bytes& src,
                                     const unsigned int offset,
                                     TransactionCoreInfo& transaction);
  static bool SetTransaction(bytes& dst, const unsigned int offset,
                             const Transaction& transaction);
  static bool GetTransaction(const bytes& src, const unsigned int offset,
                             Transaction& transaction);
  static bool SetTransactionFileOffset(bytes& dst, const unsigned int offset,
                                       const std::vector<uint32_t>& txnOffsets);
  static bool GetTransactionFileOffset(const bytes& src,
                                       const unsigned int offset,
                                       std::vector<uint32_t>& txnOffsets);
  static bool SetTransactionArray(bytes& dst, const unsigned int offset,
                                  const std::vector<Transaction>& txns);
  static bool GetTransactionArray(const bytes& src, const unsigned int offset,
                                  std::vector<Transaction>& txns);
  static bool SetTransactionReceipt(
      bytes& dst, const unsigned int offset,
      const TransactionReceipt& transactionReceipt);
  static bool GetTransactionReceipt(const bytes& src, const unsigned int offset,
                                    TransactionReceipt& transactionReceipt);

  static bool SetTransactionWithReceipt(
      bytes& dst, const unsigned int offset,
      const TransactionWithReceipt& transactionWithReceipt);
  static bool GetTransactionWithReceipt(
      const bytes& src, const unsigned int offset,
      TransactionWithReceipt& transactionWithReceipt);

  static bool SetPeer(bytes& dst, const unsigned int offset, const Peer& peer);
  static bool GetPeer(const bytes& src, const unsigned int offset, Peer& peer);

  static bool StateDeltaToAddressMap(
      const bytes& src, const unsigned int offset,
      std::unordered_map<Address, boost::multiprecision::int256_t>& accountMap);

  static bool SetBlockLink(
      bytes& dst, const unsigned int offset,
      const std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink);
  static bool GetBlockLink(
      const bytes& src, const unsigned int offset,
      std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink);

  static bool SetFallbackBlockWShardingStructure(
      bytes& dst, const unsigned int offset, const FallbackBlock& fallbackblock,
      const DequeOfShard& shards);
  static bool GetFallbackBlockWShardingStructure(const bytes& src,
                                                 const unsigned int offset,
                                                 FallbackBlock& fallbackblock,
                                                 DequeOfShard& shards);

  static bool SetDiagnosticData(bytes& dst, const unsigned int offset,
                                const DequeOfShard& shards,
                                const DequeOfDSNode& dsCommittee);
  static bool GetDiagnosticData(const bytes& src, const unsigned int offset,
                                DequeOfShard& shards,
                                DequeOfDSNode& dsCommittee);

  // ============================================================================
  // Peer Manager messages
  // ============================================================================

  static bool SetPMHello(bytes& dst, const unsigned int offset,
                         const std::pair<PrivKey, PubKey>& key,
                         const uint32_t listenPort);
  static bool GetPMHello(const bytes& src, const unsigned int offset,
                         PubKey& pubKey, uint32_t& listenPort);

  // ============================================================================
  // Directory Service messages
  // ============================================================================

  static bool SetDSPoWSubmission(
      bytes& dst, const unsigned int offset, const uint64_t blockNumber,
      const uint8_t difficultyLevel, const Peer& submitterPeer,
      const std::pair<PrivKey, PubKey>& submitterKey, const uint64_t nonce,
      const std::string& resultingHash, const std::string& mixHash,
      const uint32_t& lookupId,
      const boost::multiprecision::uint128_t& gasPrice);

  static bool GetDSPoWSubmission(const bytes& src, const unsigned int offset,
                                 uint64_t& blockNumber,
                                 uint8_t& difficultyLevel, Peer& submitterPeer,
                                 PubKey& submitterPubKey, uint64_t& nonce,
                                 std::string& resultingHash,
                                 std::string& mixHash, Signature& signature,
                                 uint32_t& lookupId,
                                 boost::multiprecision::uint128_t& gasPrice);

  static bool SetDSPoWPacketSubmission(
      bytes& dst, const unsigned int offset,
      const std::vector<DSPowSolution>& dsPowSolutions);

  static bool GetDSPowPacketSubmission(
      const bytes& src, const unsigned int offset,
      std::vector<DSPowSolution>& dsPowSolutions);

  static bool SetDSMicroBlockSubmission(
      bytes& dst, const unsigned int offset, const unsigned char microBlockType,
      const uint64_t epochNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<bytes>& stateDeltas);
  static bool GetDSMicroBlockSubmission(const bytes& src,
                                        const unsigned int offset,
                                        unsigned char& microBlockType,
                                        uint64_t& epochNumber,
                                        std::vector<MicroBlock>& microBlocks,
                                        std::vector<bytes>& stateDeltas);

  static bool SetDSDSBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey,
      const DSBlock& dsBlock, const DequeOfShard& shards,
      const MapOfPubKeyPoW& allPoWs, const MapOfPubKeyPoW& dsWinnerPoWs,
      bytes& messageToCosign);

  static bool GetDSDSBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, DSBlock& dsBlock,
      DequeOfShard& shards, MapOfPubKeyPoW& allPoWs,
      MapOfPubKeyPoW& dsWinnerPoWs, bytes& messageToCosign);

  static bool SetDSFinalBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey,
      const TxBlock& txBlock, const std::shared_ptr<MicroBlock>& microBlock,
      bytes& messageToCosign);

  static bool GetDSFinalBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, TxBlock& txBlock,
      std::shared_ptr<MicroBlock>& microBlock, bytes& messageToCosign);

  static bool SetDSVCBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey,
      const VCBlock& vcBlock, bytes& messageToCosign);

  static bool GetDSVCBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, VCBlock& vcBlock,
      bytes& messageToCosign);

  static bool SetDSMissingMicroBlocksErrorMsg(
      bytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& missingMicroBlockHashes,
      const uint64_t epochNum, const uint32_t listenPort);
  static bool GetDSMissingMicroBlocksErrorMsg(
      const bytes& src, const unsigned int offset,
      std::vector<BlockHash>& missingMicroBlockHashes, uint64_t& epochNum,
      uint32_t& listenPort);

  // ============================================================================
  // Node messages
  // ============================================================================

  static bool SetNodeVCDSBlocksMessage(bytes& dst, const unsigned int offset,
                                       const uint32_t shardId,
                                       const DSBlock& dsBlock,
                                       const std::vector<VCBlock>& vcBlocks,
                                       const DequeOfShard& shards);

  static bool GetNodeVCDSBlocksMessage(const bytes& src,
                                       const unsigned int offset,
                                       uint32_t& shardId, DSBlock& dsBlock,
                                       std::vector<VCBlock>& vcBlocks,
                                       DequeOfShard& shards);

  static bool SetNodeFinalBlock(bytes& dst, const unsigned int offset,
                                const uint64_t dsBlockNumber,
                                const uint32_t consensusID,
                                const TxBlock& txBlock,
                                const bytes& stateDelta);

  static bool GetNodeFinalBlock(const bytes& src, const unsigned int offset,
                                uint64_t& dsBlockNumber, uint32_t& consensusID,
                                TxBlock& txBlock, bytes& stateDelta);

  static bool SetNodeVCBlock(bytes& dst, const unsigned int offset,
                             const VCBlock& vcBlock);
  static bool GetNodeVCBlock(const bytes& src, const unsigned int offset,
                             VCBlock& vcBlock);

  static bool SetNodeMBnForwardTransaction(
      bytes& dst, const unsigned int offset, const MicroBlock& microBlock,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetNodeMBnForwardTransaction(const bytes& src,
                                           const unsigned int offset,
                                           MBnForwardedTxnEntry& entry);

  static bool SetNodeForwardTxnBlock(
      bytes& dst, const unsigned int offset, const uint64_t& epochNumber,
      const uint64_t& dsBlockNum, const uint32_t& shardId,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<Transaction>& txnsCurrent,
      const std::vector<Transaction>& txnsGenerated);
  static bool GetNodeForwardTxnBlock(const bytes& src,
                                     const unsigned int offset,
                                     uint64_t& epochNumber,
                                     uint64_t& dsBlockNum, uint32_t& shardId,
                                     PubKey& lookupPubKey,
                                     std::vector<Transaction>& txns);

  static bool SetNodeMicroBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey,
      const MicroBlock& microBlock, bytes& messageToCosign);

  static bool GetNodeMicroBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, MicroBlock& microBlock,
      bytes& messageToCosign);

  static bool SetNodeFallbackBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey,
      const FallbackBlock& fallbackBlock, bytes& messageToCosign);

  static bool GetNodeFallbackBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey,
      FallbackBlock& fallbackBlock, bytes& messageToCosign);

  static bool SetNodeFallbackBlock(bytes& dst, const unsigned int offset,
                                   const FallbackBlock& fallbackBlock);
  static bool GetNodeFallbackBlock(const bytes& src, const unsigned int offset,
                                   FallbackBlock& fallbackBlock);

  static bool ShardStructureToArray(bytes& dst, const unsigned int offset,
                                    const DequeOfShard& shards);
  static bool ArrayToShardStructure(const bytes& src, const unsigned int offset,
                                    DequeOfShard& shards);

  static bool SetNodeMissingTxnsErrorMsg(
      bytes& dst, const unsigned int offset,
      const std::vector<TxnHash>& missingTxnHashes, const uint64_t epochNum,
      const uint32_t listenPort);
  static bool GetNodeMissingTxnsErrorMsg(const bytes& src,
                                         const unsigned int offset,
                                         std::vector<TxnHash>& missingTxnHashes,
                                         uint64_t& epochNum,
                                         uint32_t& listenPort);

  // ============================================================================
  // Lookup messages
  // ============================================================================

  static bool SetLookupGetSeedPeers(bytes& dst, const unsigned int offset,
                                    const uint32_t listenPort);
  static bool GetLookupGetSeedPeers(const bytes& src, const unsigned int offset,
                                    uint32_t& listenPort);
  static bool SetLookupSetSeedPeers(bytes& dst, const unsigned int offset,
                                    const std::pair<PrivKey, PubKey>& lookupKey,
                                    const std::vector<Peer>& candidateSeeds);
  static bool GetLookupSetSeedPeers(const bytes& src, const unsigned int offset,
                                    PubKey& lookupPubKey,
                                    std::vector<Peer>& candidateSeeds);
  static bool SetLookupGetDSInfoFromSeed(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort,
                                         const bool initialDS);
  static bool GetLookupGetDSInfoFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort, bool& initialDS);
  static bool SetLookupSetDSInfoFromSeed(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& senderKey,
      const std::deque<std::pair<PubKey, Peer>>& dsNodes, const bool initialDS);
  static bool GetLookupSetDSInfoFromSeed(
      const bytes& src, const unsigned int offset, PubKey& senderPubKey,
      std::deque<std::pair<PubKey, Peer>>& dsNodes, bool& initialDS);
  static bool SetLookupGetDSBlockFromSeed(bytes& dst, const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetDSBlockFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupSetDSBlockFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
      const uint64_t highBlockNum, const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<DSBlock>& dsBlocks);
  static bool GetLookupSetDSBlockFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
                                          std::vector<DSBlock>& dsBlocks);
  static bool SetLookupGetTxBlockFromSeed(bytes& dst, const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetTxBlockFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupSetTxBlockFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
      const uint64_t highBlockNum, const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<TxBlock>& txBlocks);
  static bool GetLookupSetTxBlockFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
                                          std::vector<TxBlock>& txBlocks);
  static bool SetLookupGetStateDeltaFromSeed(bytes& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNum,
                                             const uint32_t listenPort);
  static bool GetLookupGetStateDeltaFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             uint32_t& listenPort);
  static bool SetLookupSetStateDeltaFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t blockNum,
      const std::pair<PrivKey, PubKey>& lookupKey, const bytes& stateDelta);
  static bool GetLookupSetStateDeltaFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             PubKey& lookupPubKey,
                                             bytes& stateDelta);
  static bool SetLookupGetTxBodyFromSeed(bytes& dst, const unsigned int offset,
                                         const bytes& txHash,
                                         const uint32_t listenPort);
  static bool GetLookupGetTxBodyFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         TxnHash& txHash, uint32_t& listenPort);
  static bool SetLookupSetTxBodyFromSeed(bytes& dst, const unsigned int offset,
                                         const TxnHash& txHash,
                                         const TransactionWithReceipt& txBody);
  static bool GetLookupSetTxBodyFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         TxnHash& txHash,
                                         TransactionWithReceipt& txBody);
  static bool SetLookupSetNetworkIDFromSeed(bytes& dst,
                                            const unsigned int offset,
                                            const std::string& networkID);
  static bool GetLookupSetNetworkIDFromSeed(const bytes& src,
                                            const unsigned int offset,
                                            std::string& networkID);
  static bool SetLookupGetStateFromSeed(bytes& dst, const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupGetStateFromSeed(const bytes& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetStateFromSeed(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const AccountStore& accountStore);
  static bool GetLookupSetStateFromSeed(const bytes& src,
                                        const unsigned int offset,
                                        PubKey& lookupPubKey,
                                        bytes& accountStoreBytes);
  static bool SetLookupSetLookupOffline(bytes& dst, const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupSetLookupOffline(const bytes& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetLookupOnline(bytes& dst, const unsigned int offset,
                                       const uint32_t listenPort,
                                       const PubKey& pubKey);
  static bool GetLookupSetLookupOnline(const bytes& src,
                                       const unsigned int offset,
                                       uint32_t& listenPort, PubKey& pubKey);
  static bool SetLookupGetOfflineLookups(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetOfflineLookups(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetOfflineLookups(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<Peer>& nodes);
  static bool GetLookupSetOfflineLookups(const bytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         std::vector<Peer>& nodes);
  static bool SetLookupGetStartPoWFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort);
  static bool GetLookupGetStartPoWFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort);
  static bool SetLookupSetStartPoWFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t blockNumber,
      const std::pair<PrivKey, PubKey>& lookupKey);
  static bool GetLookupSetStartPoWFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey);

  static bool SetLookupGetShardsFromSeed(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);

  static bool GetLookupGetShardsFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);

  static bool SetLookupSetShardsFromSeed(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey, const DequeOfShard& shards);

  static bool GetLookupSetShardsFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         DequeOfShard& shards);

  static bool SetLookupGetMicroBlockFromLookup(
      bytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& microBlockHashes, uint32_t portNo);

  static bool GetLookupGetMicroBlockFromLookup(
      const bytes& src, const unsigned int offset,
      std::vector<BlockHash>& microBlockHashes, uint32_t& portNo);

  static bool SetLookupSetMicroBlockFromLookup(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<MicroBlock>& mbs);

  static bool GetLookupSetMicroBlockFromLookup(const bytes& src,
                                               const unsigned int offset,
                                               PubKey& lookupPubKey,
                                               std::vector<MicroBlock>& mbs);

  static bool SetLookupGetTxnsFromLookup(bytes& dst, const unsigned int offset,
                                         const std::vector<TxnHash>& txnhashes,
                                         uint32_t portNo);
  static bool GetLookupGetTxnsFromLookup(const bytes& src,
                                         const unsigned int offset,
                                         std::vector<TxnHash>& txnhashes,
                                         uint32_t& portNo);
  static bool SetLookupSetTxnsFromLookup(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetLookupSetTxnsFromLookup(
      const bytes& src, const unsigned int offset, PubKey& lookupPubKey,
      std::vector<TransactionWithReceipt>& txns);

  static bool SetLookupGetDirectoryBlocksFromSeed(bytes& dst,
                                                  const unsigned int offset,
                                                  const uint32_t portNo,
                                                  const uint64_t& indexNum);
  static bool GetLookupGetDirectoryBlocksFromSeed(const bytes& src,
                                                  const unsigned int offset,
                                                  uint32_t& portNo,
                                                  uint64_t& indexNum);

  static bool SetLookupSetDirectoryBlocksFromSeed(
      bytes& dst, const unsigned int offset,
      const std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      const uint64_t& indexNum);
  static bool GetLookupSetDirectoryBlocksFromSeed(
      const bytes& src, const unsigned int offset,
      std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      uint64_t& indexNum);

  // ============================================================================
  // Consensus messages
  // ============================================================================

  template <class T>
  static bool GetConsensusID(const bytes& src, const unsigned int offset,
                             uint32_t& consensusID) {
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

  static bool SetConsensusCommit(bytes& dst, const unsigned int offset,
                                 const uint32_t consensusID,
                                 const uint64_t blockNumber,
                                 const bytes& blockHash,
                                 const uint16_t backupID,
                                 const CommitPoint& commitPoint,
                                 const CommitPointHash& commitPointHash,
                                 const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusCommit(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
      CommitPoint& commitPoint, CommitPointHash& commitPointHash,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);

  static bool SetConsensusChallenge(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const uint16_t subsetID,
      const bytes& blockHash, const uint16_t leaderID,
      const CommitPoint& aggregatedCommit, const PubKey& aggregatedKey,
      const Challenge& challenge, const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusChallenge(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, uint16_t& subsetID, const bytes& blockHash,
      const uint16_t leaderID, CommitPoint& aggregatedCommit,
      PubKey& aggregatedKey, Challenge& challenge, const PubKey& leaderKey);

  static bool SetConsensusResponse(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const uint16_t subsetID,
      const bytes& blockHash, const uint16_t backupID, const Response& response,
      const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusResponse(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
      uint16_t& subsetID, Response& response,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);

  static bool SetConsensusCollectiveSig(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const Signature& collectiveSig,
      const std::vector<bool>& bitmap,
      const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusCollectiveSig(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, std::vector<bool>& bitmap,
      Signature& collectiveSig, const PubKey& leaderKey);

  static bool SetConsensusCommitFailure(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t backupID, const bytes& errorMsg,
      const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusCommitFailure(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
      bytes& errorMsg,
      const std::deque<std::pair<PubKey, Peer>>& committeeKeys);

  static bool SetConsensusConsensusFailure(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusConsensusFailure(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash, uint16_t& leaderID,
      const PubKey& leaderKey);

  // ============================================================================
  // View change pre check messages
  // ============================================================================

  static bool SetLookupGetDSTxBlockFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t dsLowBlockNum,
      const uint64_t dsHighBlockNum, const uint64_t txLowBlockNum,
      const uint64_t txHighBlockNum, const uint32_t listenPort);

  static bool GetLookupGetDSTxBlockFromSeed(
      const bytes& src, const unsigned int offset, uint64_t& dsLowBlockNum,
      uint64_t& dsHighBlockNum, uint64_t& txLowBlockNum,
      uint64_t& txHighBlockNum, uint32_t& listenPort);
  static bool SetVCNodeSetDSTxBlockFromSeed(
      bytes& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<DSBlock>& DSBlocks,
      const std::vector<TxBlock>& txBlocks);
  static bool GetVCNodeSetDSTxBlockFromSeed(const bytes& src,
                                            const unsigned int offset,
                                            std::vector<DSBlock>& dsBlocks,
                                            std::vector<TxBlock>& txBlocks,
                                            PubKey& lookupPubKey);

  // ============================================================================
  // DS Guard network information update
  // ============================================================================

  static bool SetDSLookupNewDSGuardNetworkInfo(
      bytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
      const Peer& dsGuardNewNetworkInfo, const uint64_t timestamp,
      const std::pair<PrivKey, PubKey>& dsguardkey);

  static bool GetDSLookupNewDSGuardNetworkInfo(
      const bytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
      Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey);

  static bool SetLookupGetNewDSGuardNetworkInfoFromLookup(
      bytes& dst, const unsigned int offset, const uint32_t portNo,
      const uint64_t dsEpochNumber);

  static bool GetLookupGetNewDSGuardNetworkInfoFromLookup(
      const bytes& src, const unsigned int offset, uint32_t& portNo,
      uint64_t& dsEpochNumber);

  static bool SetNodeSetNewDSGuardNetworkInfo(
      bytes& dst, unsigned int offset,
      const std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      const std::pair<PrivKey, PubKey>& lookupKey);

  static bool SetNodeGetNewDSGuardNetworkInfo(
      const bytes& src, const unsigned int offset,
      std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      PubKey& lookupPubKey);
};
#endif  // __MESSENGER_H__
