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

#define PROTOBUFBYTEARRAYTOSERIALIZABLE(ba, s)                       \
  if (!ProtobufByteArrayToSerializable(ba, s)) {                     \
    LOG_GENERAL(WARNING, "ProtobufByteArrayToSerializable failed."); \
    return false;                                                    \
  }

namespace ZilliqaMessage {
class ByteArray;
}

bool ProtobufByteArrayToSerializable(const ZilliqaMessage::ByteArray& byteArray,
                                     Serializable& serializable);

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

  static bool GetDSCommitteeHash(const DequeOfNode& dsCommittee,
                                 CommitteeHash& dst);
  static bool GetShardHash(const Shard& shard, CommitteeHash& dst);

  static bool GetShardingStructureHash(const uint32_t& version,
                                       const DequeOfShard& shards,
                                       ShardingHash& dst);

  static bool SetAccountBase(bytes& dst, const unsigned int offset,
                             const AccountBase& accountbase);
  static bool GetAccountBase(const bytes& src, const unsigned int offset,
                             AccountBase& accountbase);

  static bool SetAccount(bytes& dst, const unsigned int offset,
                         const Account& account);
  static bool GetAccount(const bytes& src, const unsigned int offset,
                         Account& account);

  static bool SetAccountDelta(bytes& dst, const unsigned int offset,
                              Account* oldAccount, const Account& newAccount);

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
                                   const bool revertible, bool temp);
  static bool GetAccountStoreDelta(const bytes& src, const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp,
                                   bool temp);

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

  static bool SetStateIndex(bytes& dst, const unsigned int offset,
                            const std::vector<Contract::Index>& indexes);
  static bool GetStateIndex(const bytes& src, const unsigned int offset,
                            std::vector<Contract::Index>& indexes);
  static bool SetStateData(bytes& dst, const unsigned int offset,
                           const Contract::StateEntry& entry);
  static bool GetStateData(const bytes& src, const unsigned int offset,
                           Contract::StateEntry& entry, uint32_t& version);

  static bool SetPeer(bytes& dst, const unsigned int offset, const Peer& peer);
  static bool GetPeer(const bytes& src, const unsigned int offset, Peer& peer);

  static bool StateDeltaToAddressMap(
      const bytes& src, const unsigned int offset,
      std::unordered_map<Address, boost::multiprecision::int256_t>& accountMap);

  static bool SetBlockLink(bytes& dst, const unsigned int offset,
                           const std::tuple<uint32_t, uint64_t, uint64_t,
                                            BlockType, BlockHash>& blocklink);
  static bool GetBlockLink(const bytes& src, const unsigned int offset,
                           std::tuple<uint32_t, uint64_t, uint64_t, BlockType,
                                      BlockHash>& blocklink);

  static bool SetFallbackBlockWShardingStructure(
      bytes& dst, const unsigned int offset, const FallbackBlock& fallbackblock,
      const uint32_t& shardingStructureVersion, const DequeOfShard& shards);
  static bool GetFallbackBlockWShardingStructure(
      const bytes& src, const unsigned int offset, FallbackBlock& fallbackblock,
      uint32_t& shardingStructureVersion, DequeOfShard& shards);

  static bool SetDiagnosticDataNodes(bytes& dst, const unsigned int offset,
                                     const uint32_t& shardingStructureVersion,
                                     const DequeOfShard& shards,
                                     const uint32_t& dsCommitteeVersion,
                                     const DequeOfNode& dsCommittee);
  static bool GetDiagnosticDataNodes(const bytes& src,
                                     const unsigned int offset,
                                     uint32_t& shardingStructureVersion,
                                     DequeOfShard& shards,
                                     uint32_t& dsCommitteeVersion,
                                     DequeOfNode& dsCommittee);

  static bool SetDiagnosticDataCoinbase(bytes& dst, const unsigned int offset,
                                        const DiagnosticDataCoinbase& entry);
  static bool GetDiagnosticDataCoinbase(const bytes& src,
                                        const unsigned int offset,
                                        DiagnosticDataCoinbase& entry);

  // ============================================================================
  // Peer Manager messages
  // ============================================================================

  static bool SetPMHello(bytes& dst, const unsigned int offset,
                         const PairOfKey& key, const uint32_t listenPort);
  static bool GetPMHello(const bytes& src, const unsigned int offset,
                         PubKey& pubKey, uint32_t& listenPort);

  // ============================================================================
  // Directory Service messages
  // ============================================================================

  static bool SetDSPoWSubmission(
      bytes& dst, const unsigned int offset, const uint64_t blockNumber,
      const uint8_t difficultyLevel, const Peer& submitterPeer,
      const PairOfKey& submitterKey, const uint64_t nonce,
      const std::string& resultingHash, const std::string& mixHash,
      const uint32_t& lookupId, const uint128_t& gasPrice);

  static bool GetDSPoWSubmission(const bytes& src, const unsigned int offset,
                                 uint64_t& blockNumber,
                                 uint8_t& difficultyLevel, Peer& submitterPeer,
                                 PubKey& submitterPubKey, uint64_t& nonce,
                                 std::string& resultingHash,
                                 std::string& mixHash, Signature& signature,
                                 uint32_t& lookupId, uint128_t& gasPrice);

  static bool SetDSPoWPacketSubmission(
      bytes& dst, const unsigned int offset,
      const std::vector<DSPowSolution>& dsPowSolutions, const PairOfKey& keys);

  static bool GetDSPowPacketSubmission(
      const bytes& src, const unsigned int offset,
      std::vector<DSPowSolution>& dsPowSolutions, PubKey& pubKey);

  static bool SetDSMicroBlockSubmission(
      bytes& dst, const unsigned int offset, const unsigned char microBlockType,
      const uint64_t epochNumber, const std::vector<MicroBlock>& microBlocks,
      const std::vector<bytes>& stateDeltas, const PairOfKey& keys);
  static bool GetDSMicroBlockSubmission(const bytes& src,
                                        const unsigned int offset,
                                        unsigned char& microBlockType,
                                        uint64_t& epochNumber,
                                        std::vector<MicroBlock>& microBlocks,
                                        std::vector<bytes>& stateDeltas,
                                        PubKey& pubKey);

  static bool SetDSDSBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
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
      const uint16_t leaderID, const PairOfKey& leaderKey,
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
      const uint16_t leaderID, const PairOfKey& leaderKey,
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
                                       const uint32_t& shardingStructureVersion,
                                       const DequeOfShard& shards);

  static bool GetNodeVCDSBlocksMessage(const bytes& src,
                                       const unsigned int offset,
                                       uint32_t& shardId, DSBlock& dsBlock,
                                       std::vector<VCBlock>& vcBlocks,
                                       uint32_t& shardingStructureVersion,
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
      const PairOfKey& lookupKey, const std::vector<Transaction>& txnsCurrent,
      const std::vector<Transaction>& txnsGenerated);
  static bool SetNodeForwardTxnBlock(bytes& dst, const unsigned int offset,
                                     const uint64_t& epochNumber,
                                     const uint64_t& dsBlockNum,
                                     const uint32_t& shardId,
                                     const PubKey& lookupKey,
                                     std::vector<Transaction>& txns,
                                     const Signature& signature);
  static bool GetNodeForwardTxnBlock(
      const bytes& src, const unsigned int offset, uint64_t& epochNumber,
      uint64_t& dsBlockNum, uint32_t& shardId, PubKey& lookupPubKey,
      std::vector<Transaction>& txns, Signature& signature);

  static bool SetNodeMicroBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
      const MicroBlock& microBlock, bytes& messageToCosign);

  static bool GetNodeMicroBlockAnnouncement(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, MicroBlock& microBlock,
      bytes& messageToCosign);

  static bool SetNodeFallbackBlockAnnouncement(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
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
                                    const uint32_t& version,
                                    const DequeOfShard& shards);
  static bool ArrayToShardStructure(const bytes& src, const unsigned int offset,
                                    uint32_t& version, DequeOfShard& shards);

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
                                    const PairOfKey& lookupKey,
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
  static bool SetLookupSetDSInfoFromSeed(bytes& dst, const unsigned int offset,
                                         const PairOfKey& senderKey,
                                         const uint32_t& dsCommitteeVersion,
                                         const DequeOfNode& dsNodes,
                                         const bool initialDS);
  static bool GetLookupSetDSInfoFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         PubKey& senderPubKey,
                                         uint32_t& dsCommitteeVersion,
                                         DequeOfNode& dsNodes, bool& initialDS);
  static bool SetLookupGetDSBlockFromSeed(bytes& dst, const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetDSBlockFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupSetDSBlockFromSeed(bytes& dst, const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const PairOfKey& lookupKey,
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
  static bool SetLookupSetTxBlockFromSeed(bytes& dst, const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const PairOfKey& lookupKey,
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
  static bool SetLookupGetStateDeltasFromSeed(bytes& dst,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              const uint32_t listenPort);
  static bool GetLookupGetStateDeltaFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             uint32_t& listenPort);
  static bool GetLookupGetStateDeltasFromSeed(const bytes& src,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              uint32_t& listenPort);
  static bool SetLookupSetStateDeltaFromSeed(bytes& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNum,
                                             const PairOfKey& lookupKey,
                                             const bytes& stateDelta);
  static bool SetLookupSetStateDeltasFromSeed(
      bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
      const uint64_t highBlockNum, const PairOfKey& lookupKey,
      const std::vector<bytes>& stateDeltas);
  static bool GetLookupSetStateDeltaFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             PubKey& lookupPubKey,
                                             bytes& stateDelta);
  static bool GetLookupSetStateDeltasFromSeed(const bytes& src,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              PubKey& lookupPubKey,
                                              std::vector<bytes>& stateDeltas);
  static bool SetLookupGetStateFromSeed(bytes& dst, const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupGetStateFromSeed(const bytes& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetStateFromSeed(bytes& dst, const unsigned int offset,
                                        const PairOfKey& lookupKey,
                                        const AccountStore& accountStore);
  static bool GetLookupSetStateFromSeed(const bytes& src,
                                        const unsigned int offset,
                                        PubKey& lookupPubKey,
                                        bytes& accountStoreBytes);
  static bool SetLookupSetLookupOffline(bytes& dst, const unsigned int offset,
                                        const uint8_t msgType,
                                        const uint32_t listenPort,
                                        const PairOfKey& lookupKey);
  static bool GetLookupSetLookupOffline(const bytes& src,
                                        const unsigned int offset,
                                        uint8_t& msgType, uint32_t& listenPort,
                                        PubKey& lookupPubkey);
  static bool SetLookupSetLookupOnline(bytes& dst, const unsigned int offset,
                                       const uint8_t msgType,
                                       const uint32_t listenPort,
                                       const PairOfKey& lookupKey);
  static bool GetLookupSetLookupOnline(const bytes& src,
                                       const unsigned int offset,
                                       uint8_t& msgType, uint32_t& listenPort,
                                       PubKey& pubKey);
  static bool SetLookupGetOfflineLookups(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetOfflineLookups(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetOfflineLookups(bytes& dst, const unsigned int offset,
                                         const PairOfKey& lookupKey,
                                         const std::vector<Peer>& nodes);
  static bool GetLookupSetOfflineLookups(const bytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         std::vector<Peer>& nodes);
  static bool GetLookupSetRaiseStartPoW(const bytes& src,
                                        const unsigned int offset,
                                        uint8_t& msgType, uint64_t& blockNumber,
                                        PubKey& dsPubKey);
  static bool SetLookupSetRaiseStartPoW(bytes& dst, const unsigned int offset,
                                        const uint8_t msgType,
                                        const uint64_t blockNumber,
                                        const PairOfKey& dsKey);
  static bool SetLookupGetStartPoWFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort,
                                           const uint64_t blockNumber,
                                           const PairOfKey& keys);
  static bool GetLookupGetStartPoWFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort,
                                           uint64_t& blockNumber);
  static bool SetLookupSetStartPoWFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint64_t blockNumber,
                                           const PairOfKey& lookupKey);
  static bool GetLookupSetStartPoWFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey);
  // UNUSED
  static bool SetLookupGetShardsFromSeed(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);

  // UNUSED
  static bool GetLookupGetShardsFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  // UNUSED
  static bool SetLookupSetShardsFromSeed(
      bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const uint32_t& shardingStructureVersion, const DequeOfShard& shards);

  static bool GetLookupSetShardsFromSeed(const bytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         uint32_t& shardingStructureVersion,
                                         DequeOfShard& shards);

  static bool SetForwardTxnBlockFromSeed(
      bytes& dst, const unsigned int offset,
      const std::vector<Transaction>& shardTransactions,
      const std::vector<Transaction>& dsTransactions);

  static bool GetForwardTxnBlockFromSeed(
      const bytes& src, const unsigned int offset,
      std::vector<Transaction>& shardTransactions,
      std::vector<Transaction>& dsTransactions);

  static bool SetLookupGetMicroBlockFromLookup(
      bytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& microBlockHashes, uint32_t portNo);

  // UNUSED
  static bool GetLookupGetMicroBlockFromLookup(
      const bytes& src, const unsigned int offset,
      std::vector<BlockHash>& microBlockHashes, uint32_t& portNo);

  static bool SetLookupSetMicroBlockFromLookup(
      bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const std::vector<MicroBlock>& mbs);

  static bool GetLookupSetMicroBlockFromLookup(const bytes& src,
                                               const unsigned int offset,
                                               PubKey& lookupPubKey,
                                               std::vector<MicroBlock>& mbs);
  // UNUSED
  static bool SetLookupGetTxnsFromLookup(bytes& dst, const unsigned int offset,
                                         const std::vector<TxnHash>& txnhashes,
                                         uint32_t portNo);
  static bool GetLookupGetTxnsFromLookup(const bytes& src,
                                         const unsigned int offset,
                                         std::vector<TxnHash>& txnhashes,
                                         uint32_t& portNo);
  // UNUSED
  static bool SetLookupSetTxnsFromLookup(
      bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const std::vector<TransactionWithReceipt>& txns);

  // USUSED
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
      const uint32_t& shardingStructureVersion,
      const std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      const uint64_t& indexNum, const PairOfKey& lookupKey);
  static bool GetLookupSetDirectoryBlocksFromSeed(
      const bytes& src, const unsigned int offset,
      uint32_t& shardingStructureVersion,
      std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      uint64_t& indexNum, PubKey& pubKey);

  // ============================================================================
  // Consensus messages
  // ============================================================================

  template <class T>
  static bool PreProcessMessage(const bytes& src, const unsigned int offset,
                                uint32_t& consensusID, PubKey& senderPubKey,
                                bytes& reserializedSrc) {
    T consensus_message;

    consensus_message.ParseFromArray(src.data() + offset, src.size() - offset);

    if (!consensus_message.IsInitialized()) {
      LOG_GENERAL(WARNING, "Consensus message initialization failed.");
      return false;
    }

    if (!consensus_message.consensusinfo().IsInitialized()) {
      LOG_GENERAL(WARNING,
                  "Consensus message consensusinfo initialization failed.");
      return false;
    }

    bytes tmp(consensus_message.consensusinfo().ByteSize());
    consensus_message.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

    ProtobufByteArrayToSerializable(consensus_message.pubkey(), senderPubKey);

    Signature signature;

    ProtobufByteArrayToSerializable(consensus_message.signature(), signature);

    if (!Schnorr::GetInstance().Verify(tmp, signature, senderPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in ConsensusConsensusFailure.");
      return false;
    }

    consensusID = consensus_message.consensusinfo().consensusid();

    // Copy src into reserializedSrc, trimming away any excess bytes beyond the
    // definition of protobuf message T
    reserializedSrc.resize(offset + consensus_message.ByteSize());
    copy(src.begin(), src.begin() + offset, reserializedSrc.begin());
    consensus_message.SerializeToArray(reserializedSrc.data() + offset,
                                       consensus_message.ByteSize());

    return true;
  }

  static bool SetConsensusCommit(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t backupID, const CommitPoint& commitPoint,
      const CommitPointHash& commitPointHash, const PairOfKey& backupKey);
  static bool GetConsensusCommit(const bytes& src, const unsigned int offset,
                                 const uint32_t consensusID,
                                 const uint64_t blockNumber,
                                 const bytes& blockHash, uint16_t& backupID,
                                 CommitPoint& commitPoint,
                                 CommitPointHash& commitPointHash,
                                 const DequeOfNode& committeeKeys);

  static bool SetConsensusChallenge(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID,
      const std::vector<ChallengeSubsetInfo>& subsetInfo,
      const PairOfKey& leaderKey);
  static bool GetConsensusChallenge(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, std::vector<ChallengeSubsetInfo>& subsetInfo,
      const PubKey& leaderKey);

  static bool SetConsensusResponse(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t backupID,
      const std::vector<ResponseSubsetInfo>& subsetInfo,
      const PairOfKey& backupKey);
  static bool GetConsensusResponse(const bytes& src, const unsigned int offset,
                                   const uint32_t consensusID,
                                   const uint64_t blockNumber,
                                   const bytes& blockHash, uint16_t& backupID,
                                   std::vector<ResponseSubsetInfo>& subsetInfo,
                                   const DequeOfNode& committeeKeys);

  static bool SetConsensusCollectiveSig(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const Signature& collectiveSig,
      const std::vector<bool>& bitmap, const PairOfKey& leaderKey);
  static bool GetConsensusCollectiveSig(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, std::vector<bool>& bitmap,
      Signature& collectiveSig, const PubKey& leaderKey);

  static bool SetConsensusCommitFailure(bytes& dst, const unsigned int offset,
                                        const uint32_t consensusID,
                                        const uint64_t blockNumber,
                                        const bytes& blockHash,
                                        const uint16_t backupID,
                                        const bytes& errorMsg,
                                        const PairOfKey& backupKey);
  static bool GetConsensusCommitFailure(
      const bytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
      bytes& errorMsg, const DequeOfNode& committeeKeys);

  static bool SetConsensusConsensusFailure(
      bytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const bytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey);
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
      bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
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
      const PairOfKey& dsguardkey);

  static bool GetDSLookupNewDSGuardNetworkInfo(
      const bytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
      Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey);

  static bool SetLookupGetNewDSGuardNetworkInfoFromLookup(
      bytes& dst, const unsigned int offset, const uint32_t portNo,
      const uint64_t dsEpochNumber, const PairOfKey& lookupKey);

  static bool GetLookupGetNewDSGuardNetworkInfoFromLookup(
      const bytes& src, const unsigned int offset, uint32_t& portNo,
      uint64_t& dsEpochNumber);

  static bool SetNodeSetNewDSGuardNetworkInfo(
      bytes& dst, unsigned int offset,
      const std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      const PairOfKey& lookupKey);

  static bool SetNodeGetNewDSGuardNetworkInfo(
      const bytes& src, const unsigned int offset,
      std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      PubKey& lookupPubKey);

  static bool SetSeedNodeHistoricalDB(bytes& dst, const unsigned int offset,
                                      const PairOfKey& archivalKeys,
                                      const uint32_t code,
                                      const std::string& path);
  static bool GetSeedNodeHistoricalDB(const bytes& src,
                                      const unsigned int offset,
                                      PubKey& archivalPubKey, uint32_t& code,
                                      std::string& path);
};
#endif  // __MESSENGER_H__
