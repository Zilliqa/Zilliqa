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
#ifndef ZILLIQA_SRC_LIBMESSAGE_MESSENGER_H_
#define ZILLIQA_SRC_LIBMESSAGE_MESSENGER_H_

#include <boost/variant.hpp>
#include <map>
#include "MessengerCommon.h"
#include "common/BaseType.h"
#include "common/TxnStatus.h"
#include "libBlockchain/Block.h"
#include "libData/AccountData/MBnForwardedTxnEntry.h"
#include "libData/CoinbaseData/CoinbaseStruct.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libData/MiningData/MinerInfo.h"
#include "libDirectoryService/DirectoryService.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"

class AccountBase;
class Account;
class AccountStore;
class AccountStoreTemp;

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
  static bool GetShardHash(const DequeOfShardMembers& shard,
                           CommitteeHash& dst);

  static bool GetShardingStructureHash(const uint32_t& version,
                                       const DequeOfShardMembers& members,
                                       ShardingHash& dst);

  static bool SetAccountBase(zbytes& dst, const unsigned int offset,
                             const AccountBase& accountbase);
  static bool GetAccountBase(const zbytes& src, const unsigned int offset,
                             AccountBase& accountbase);
  static bool GetAccountBase(const std::string& src, const unsigned int offset,
                             AccountBase& accountbase);

  static bool SetAccount(zbytes& dst, const unsigned int offset,
                         const Account& account);
  static bool GetAccount(const zbytes& src, const unsigned int offset,
                         Account& account);

  static bool SetAccountDelta(zbytes& dst, const unsigned int offset,
                              Account* oldAccount, const Account& newAccount);

  // These are called by AccountStoreBase template class
  template <class MAP>
  static bool SetAccountStore(zbytes& dst, const unsigned int offset,
                              const MAP& addressToAccount);
  template <class MAP>
  static bool GetAccountStore(const zbytes& src, const unsigned int offset,
                              MAP& addressToAccount);
  static bool GetAccountStore(const zbytes& src, const unsigned int offset,
                              AccountStore& accountStore);
  static bool GetAccountStore(const std::string& src, const unsigned int offset,
                              AccountStore& accountStore);

  // These are called by AccountStore class
  static bool SetAccountStoreDelta(zbytes& dst, const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp,
                                   AccountStore& accountStore);
  static bool GetAccountStoreDelta(const zbytes& src, const unsigned int offset,
                                   AccountStore& accountStore,
                                   const bool revertible, bool temp);
  static bool GetAccountStoreDelta(const zbytes& src, const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp,
                                   bool temp);

  static bool GetMbInfoHash(const std::vector<MicroBlockInfo>& mbInfos,
                            MBInfoHash& dst);

  static bool SetTransactionCoreInfo(zbytes& dst, const unsigned int offset,
                                     const TransactionCoreInfo& transaction);
  static bool GetTransactionCoreInfo(const zbytes& src,
                                     const unsigned int offset,
                                     TransactionCoreInfo& transaction);
  static bool SetTransaction(zbytes& dst, const unsigned int offset,
                             const Transaction& transaction);
  static bool GetTransaction(const zbytes& src, const unsigned int offset,
                             Transaction& transaction);
  static bool GetTransaction(const std::string& src, const unsigned int offset,
                             Transaction& transaction);
  static bool SetTransactionFileOffset(zbytes& dst, const unsigned int offset,
                                       const std::vector<uint32_t>& txnOffsets);
  static bool GetTransactionFileOffset(const zbytes& src,
                                       const unsigned int offset,
                                       std::vector<uint32_t>& txnOffsets);
  static bool SetTransactionArray(zbytes& dst, const unsigned int offset,
                                  const std::vector<Transaction>& txns);
  static bool GetTransactionArray(const zbytes& src, const unsigned int offset,
                                  std::vector<Transaction>& txns);
  static bool SetTransactionReceipt(
      zbytes& dst, const unsigned int offset,
      const TransactionReceipt& transactionReceipt);
  static bool GetTransactionReceipt(const zbytes& src,
                                    const unsigned int offset,
                                    TransactionReceipt& transactionReceipt);
  static bool GetTransactionReceipt(const std::string& src,
                                    const unsigned int offset,
                                    TransactionReceipt& transactionReceipt);

  static bool SetTransactionWithReceipt(
      zbytes& dst, const unsigned int offset,
      const TransactionWithReceipt& transactionWithReceipt);
  static bool GetTransactionWithReceipt(
      const zbytes& src, const unsigned int offset,
      TransactionWithReceipt& transactionWithReceipt);
  static bool GetTransactionWithReceipt(
      const std::string& src, const unsigned int offset,
      TransactionWithReceipt& transactionWithReceipt);

  static bool SetPeer(zbytes& dst, const unsigned int offset, const Peer& peer);
  static bool GetPeer(const zbytes& src, const unsigned int offset, Peer& peer);

  static bool StateDeltaToAddressMap(
      const zbytes& src, const unsigned int offset,
      std::unordered_map<Address, boost::multiprecision::int256_t>& accountMap);

  static bool SetBlockLink(zbytes& dst, const unsigned int offset,
                           const std::tuple<uint32_t, uint64_t, uint64_t,
                                            BlockType, BlockHash>& blocklink);
  static bool GetBlockLink(const zbytes& src, const unsigned int offset,
                           std::tuple<uint32_t, uint64_t, uint64_t, BlockType,
                                      BlockHash>& blocklink);

  static bool SetDiagnosticDataNodes(zbytes& dst, const unsigned int offset,
                                     const uint32_t& shardingStructureVersion,
                                     const DequeOfShardMembers& members,
                                     const uint32_t& dsCommitteeVersion,
                                     const DequeOfNode& dsCommittee);
  static bool GetDiagnosticDataNodes(const zbytes& src,
                                     const unsigned int offset,
                                     uint32_t& shardingStructureVersion,
                                     DequeOfShardMembers& members,
                                     uint32_t& dsCommitteeVersion,
                                     DequeOfNode& dsCommittee);

  static bool SetDiagnosticDataCoinbase(zbytes& dst, const unsigned int offset,
                                        const DiagnosticDataCoinbase& entry);
  static bool GetDiagnosticDataCoinbase(const zbytes& src,
                                        const unsigned int offset,
                                        DiagnosticDataCoinbase& entry);

  // ============================================================================
  // Peer Manager messages
  // ============================================================================

  static bool SetPMHello(zbytes& dst, const unsigned int offset,
                         const PairOfKey& key, const uint32_t listenPort);
  static bool GetPMHello(const zbytes& src, const unsigned int offset,
                         PubKey& pubKey, uint32_t& listenPort);

  // ============================================================================
  // Directory Service messages
  // ============================================================================

  static bool SetDSPoWSubmission(
      zbytes& dst, const unsigned int offset, const uint64_t blockNumber,
      const uint8_t difficultyLevel, const Peer& submitterPeer,
      const PairOfKey& submitterKey, const uint64_t nonce,
      const std::string& resultingHash, const std::string& mixHash, const zbytes& extraData,
      const uint32_t& lookupId, const uint128_t& gasPrice,
      const GovProposalIdVotePair& govProposal, const std::string& version);

  static bool GetDSPoWSubmission(
      const zbytes& src, const unsigned int offset, uint64_t& blockNumber,
      uint8_t& difficultyLevel, Peer& submitterPeer, PubKey& submitterPubKey,
      uint64_t& nonce, std::string& resultingHash, std::string& mixHash, zbytes& extraData,
      Signature& signature, uint32_t& lookupId, uint128_t& gasPrice,
      uint32_t& proposalId, uint32_t& voteValue, std::string& version);

  static bool SetDSPoWPacketSubmission(
      zbytes& dst, const unsigned int offset,
      const std::vector<DSPowSolution>& dsPowSolutions, const PairOfKey& keys);

  static bool GetDSPowPacketSubmission(
      const zbytes& src, const unsigned int offset,
      std::vector<DSPowSolution>& dsPowSolutions, PubKey& pubKey);

  static bool SetDSMicroBlockSubmission(
      zbytes& dst, const unsigned int offset,
      const unsigned char microBlockType, const uint64_t epochNumber,
      const std::vector<MicroBlock>& microBlocks,
      const std::vector<zbytes>& stateDeltas, const PairOfKey& keys);
  static bool GetDSMicroBlockSubmission(const zbytes& src,
                                        const unsigned int offset,
                                        unsigned char& microBlockType,
                                        uint64_t& epochNumber,
                                        std::vector<MicroBlock>& microBlocks,
                                        std::vector<zbytes>& stateDeltas,
                                        PubKey& pubKey);

  static bool SetDSDSBlockAnnouncement(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
      const DSBlock& dsBlock, const DequeOfShardMembers& members,
      const MapOfPubKeyPoW& allPoWs, const MapOfPubKeyPoW& dsWinnerPoWs,
      zbytes& messageToCosign);

  static bool GetDSDSBlockAnnouncement(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, DSBlock& dsBlock,
      DequeOfShardMembers& members, MapOfPubKeyPoW& allPoWs,
      MapOfPubKeyPoW& dsWinnerPoWs, zbytes& messageToCosign);

  static bool SetDSFinalBlockAnnouncement(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
      const TxBlock& txBlock, const std::shared_ptr<MicroBlock>& microBlock,
      zbytes& messageToCosign);

  static bool GetDSFinalBlockAnnouncement(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, TxBlock& txBlock,
      std::shared_ptr<MicroBlock>& microBlock, zbytes& messageToCosign);

  static bool SetDSVCBlockAnnouncement(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
      const VCBlock& vcBlock, zbytes& messageToCosign);

  static bool GetDSVCBlockAnnouncement(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, VCBlock& vcBlock,
      zbytes& messageToCosign);

  static bool SetDSMissingMicroBlocksErrorMsg(
      zbytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& missingMicroBlockHashes,
      const uint64_t epochNum, const uint32_t listenPort);
  static bool GetDSMissingMicroBlocksErrorMsg(
      const zbytes& src, const unsigned int offset,
      std::vector<BlockHash>& missingMicroBlockHashes, uint64_t& epochNum,
      uint32_t& listenPort);

  // ============================================================================
  // Node messages
  // ============================================================================

  static bool SetNodeVCDSBlocksMessage(zbytes& dst, const unsigned int offset,
                                       const uint32_t shardId,
                                       const DSBlock& dsBlock,
                                       const std::vector<VCBlock>& vcBlocks,
                                       const uint32_t& shardingStructureVersion,
                                       const DequeOfShardMembers& members);

  static bool GetNodeVCDSBlocksMessage(const zbytes& src,
                                       const unsigned int offset,
                                       uint32_t& shardId, DSBlock& dsBlock,
                                       std::vector<VCBlock>& vcBlocks,
                                       uint32_t& shardingStructureVersion,
                                       DequeOfShardMembers& members);

  static bool SetNodeVCFinalBlock(zbytes& dst, const unsigned int offset,
                                  const uint64_t dsBlockNumber,
                                  const uint32_t consensusID,
                                  const TxBlock& txBlock,
                                  const zbytes& stateDelta,
                                  const std::vector<VCBlock>& vcBlocks);

  static bool GetNodeVCFinalBlock(const zbytes& src, const unsigned int offset,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  zbytes& stateDelta,
                                  std::vector<VCBlock>& vcBlocks);

  static bool SetNodeFinalBlock(zbytes& dst, const unsigned int offset,
                                const uint64_t dsBlockNumber,
                                const uint32_t consensusID,
                                const TxBlock& txBlock,
                                const zbytes& stateDelta);

  static bool GetNodeFinalBlock(const zbytes& src, const unsigned int offset,
                                uint64_t& dsBlockNumber, uint32_t& consensusID,
                                TxBlock& txBlock, zbytes& stateDelta);

  static bool SetNodeVCBlock(zbytes& dst, const unsigned int offset,
                             const VCBlock& vcBlock);
  static bool GetNodeVCBlock(const zbytes& src, const unsigned int offset,
                             VCBlock& vcBlock);

  static bool SetNodeMBnForwardTransaction(
      zbytes& dst, const unsigned int offset, const MicroBlock& microBlock,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetNodeMBnForwardTransaction(const zbytes& src,
                                           const unsigned int offset,
                                           MBnForwardedTxnEntry& entry);
  static bool GetNodePendingTxn(
      const zbytes& src, const unsigned offset, uint64_t& epochnum,
      std::unordered_map<TxnHash, TxnStatus>& hashCodeMap, uint32_t& shardId,
      PubKey& pubKey, zbytes& txnListHash);

  static bool SetNodePendingTxn(
      zbytes& dst, const unsigned offset, const uint64_t& epochnum,
      const std::unordered_map<TxnHash, TxnStatus>& hashCodeMap,
      const uint32_t shardId, const PairOfKey& key);

  static bool SetNodeForwardTxnBlock(zbytes& dst, const unsigned int offset,
                                     const uint64_t& epochNumber,
                                     const uint64_t& dsBlockNum,
                                     const uint32_t shardId,
                                     const PairOfKey& lookupKey,
                                     std::vector<Transaction>& transactions);
  static bool SetNodeForwardTxnBlock(zbytes& dst, const unsigned int offset,
                                     const uint64_t& epochNumber,
                                     const uint64_t& dsBlockNum,
                                     const uint32_t& shardId,
                                     const PubKey& lookupKey,
                                     std::vector<Transaction>& txns,
                                     const Signature& signature);
  static bool GetNodeForwardTxnBlock(
      const zbytes& src, const unsigned int offset, uint64_t& epochNumber,
      uint64_t& dsBlockNum, uint32_t& shardId, PubKey& lookupPubKey,
      std::vector<Transaction>& txns, Signature& signature);

  static bool SetNodeMicroBlockAnnouncement(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey,
      const MicroBlock& microBlock, zbytes& messageToCosign);

  static bool GetNodeMicroBlockAnnouncement(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PubKey& leaderKey, MicroBlock& microBlock,
      zbytes& messageToCosign);

  static bool ShardStructureToArray(zbytes& dst, const unsigned int offset,
                                    const uint32_t& version,
                                    const DequeOfShardMembers& members);
  static bool ArrayToShardStructure(const zbytes& src,
                                    const unsigned int offset,
                                    uint32_t& version,
                                    DequeOfShardMembers& members);

  static bool SetNodeMissingTxnsErrorMsg(
      zbytes& dst, const unsigned int offset,
      const std::vector<TxnHash>& missingTxnHashes, const uint64_t epochNum,
      const uint32_t listenPort);
  static bool GetNodeMissingTxnsErrorMsg(const zbytes& src,
                                         const unsigned int offset,
                                         std::vector<TxnHash>& missingTxnHashes,
                                         uint64_t& epochNum,
                                         uint32_t& listenPort);

  static bool SetNodeGetVersion(zbytes& dst, const unsigned int offset,
                                const uint32_t listenPort);
  static bool GetNodeGetVersion(const zbytes& src, const unsigned int offset,
                                uint32_t& listenPort);
  static bool SetNodeSetVersion(zbytes& dst, const unsigned int offset,
                                const std::string& version);
  static bool GetNodeSetVersion(const zbytes& src, const unsigned int offset,
                                std::string& version);

  // ============================================================================
  // Lookup messages
  // ============================================================================

  static bool SetLookupGetSeedPeers(zbytes& dst, const unsigned int offset,
                                    const uint32_t listenPort);
  static bool GetLookupGetSeedPeers(const zbytes& src,
                                    const unsigned int offset,
                                    uint32_t& listenPort);
  static bool SetLookupSetSeedPeers(zbytes& dst, const unsigned int offset,
                                    const PairOfKey& lookupKey,
                                    const VectorOfPeer& candidateSeeds);
  static bool GetLookupSetSeedPeers(const zbytes& src,
                                    const unsigned int offset,
                                    PubKey& lookupPubKey,
                                    VectorOfPeer& candidateSeeds);
  static bool SetLookupGetDSInfoFromSeed(zbytes& dst, const unsigned int offset,
                                         const uint32_t listenPort,
                                         const bool initialDS);
  static bool GetLookupGetDSInfoFromSeed(const zbytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort, bool& initialDS);
  static bool SetLookupSetDSInfoFromSeed(zbytes& dst, const unsigned int offset,
                                         const PairOfKey& senderKey,
                                         const uint32_t& dsCommitteeVersion,
                                         const DequeOfNode& dsNodes,
                                         const bool initialDS);
  static bool GetLookupSetDSInfoFromSeed(const zbytes& src,
                                         const unsigned int offset,
                                         PubKey& senderPubKey,
                                         uint32_t& dsCommitteeVersion,
                                         DequeOfNode& dsNodes, bool& initialDS);
  static bool SetLookupGetDSBlockFromSeed(zbytes& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort,
                                          const bool includeMinerInfo);
  static bool GetLookupGetDSBlockFromSeed(
      const zbytes& src, const unsigned int offset, uint64_t& lowBlockNum,
      uint64_t& highBlockNum, uint32_t& listenPort, bool& includeMinerInfo);
  static bool SetLookupSetDSBlockFromSeed(zbytes& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const PairOfKey& lookupKey,
                                          const std::vector<DSBlock>& dsBlocks);
  static bool GetLookupSetDSBlockFromSeed(const zbytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
                                          std::vector<DSBlock>& dsBlocks);
  static bool SetLookupSetMinerInfoFromSeed(
      zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const std::map<uint64_t, std::pair<MinerInfoDSComm, MinerInfoShards>>&
          minerInfoPerDS);
  static bool GetLookupSetMinerInfoFromSeed(
      const zbytes& src, const unsigned int offset, PubKey& lookupPubKey,
      std::map<uint64_t, std::pair<MinerInfoDSComm, MinerInfoShards>>&
          minerInfoPerDS);
  static bool SetLookupGetTxBlockFromSeed(zbytes& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const uint32_t listenPort);
  static bool GetLookupGetTxBlockFromSeed(const zbytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          uint32_t& listenPort);
  static bool SetLookupGetVCFinalBlockFromL2l(zbytes& dst,
                                              const unsigned int offset,
                                              const uint64_t& blockNum,
                                              const Peer& sender,
                                              const PairOfKey& seedKey);
  static bool GetLookupGetVCFinalBlockFromL2l(const zbytes& src,
                                              const unsigned int offset,
                                              uint64_t& blockNum, Peer& from,
                                              PubKey& senderPubKey);
  static bool SetLookupGetDSBlockFromL2l(zbytes& dst, const unsigned int offset,
                                         const uint64_t& blockNum,
                                         const Peer& sender,
                                         const PairOfKey& seedKey);
  static bool GetLookupGetDSBlockFromL2l(const zbytes& src,
                                         const unsigned int offset,
                                         uint64_t& blockNum, Peer& from,
                                         PubKey& senderPubKey);
  static bool SetLookupGetMBnForwardTxnFromL2l(
      zbytes& dst, const unsigned int offset, const uint64_t& blockNum,
      const uint32_t& shardId, const Peer& sender, const PairOfKey& seedKey);
  static bool GetLookupGetMBnForwardTxnFromL2l(const zbytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               uint32_t& shardId, Peer& from,
                                               PubKey& senderPubKey);
  static bool SetLookupGetPendingTxnFromL2l(
      zbytes& dst, const unsigned int offset, const uint64_t& blockNum,
      const uint32_t& shardId, const Peer& sender, const PairOfKey& seedKey);
  static bool GetLookupGetPendingTxnFromL2l(const zbytes& src,
                                            const unsigned int offset,
                                            uint64_t& blockNum,
                                            uint32_t& shardId, Peer& from,
                                            PubKey& senderPubKey);
  static bool SetLookupSetTxBlockFromSeed(zbytes& dst,
                                          const unsigned int offset,
                                          const uint64_t lowBlockNum,
                                          const uint64_t highBlockNum,
                                          const PairOfKey& lookupKey,
                                          const std::vector<TxBlock>& txBlocks);
  static bool GetLookupSetTxBlockFromSeed(const zbytes& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
                                          std::vector<TxBlock>& txBlocks);
  static bool SetLookupGetStateDeltaFromSeed(zbytes& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNum,
                                             const uint32_t listenPort);
  static bool SetLookupGetStateDeltasFromSeed(zbytes& dst,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              const uint32_t listenPort);
  static bool GetLookupGetStateDeltaFromSeed(const zbytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             uint32_t& listenPort);
  static bool GetLookupGetStateDeltasFromSeed(const zbytes& src,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              uint32_t& listenPort);
  static bool SetLookupSetStateDeltaFromSeed(zbytes& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNum,
                                             const PairOfKey& lookupKey,
                                             const zbytes& stateDelta);
  static bool SetLookupSetStateDeltasFromSeed(
      zbytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
      const uint64_t highBlockNum, const PairOfKey& lookupKey,
      const std::vector<zbytes>& stateDeltas);
  static bool GetLookupSetStateDeltaFromSeed(const zbytes& src,
                                             const unsigned int offset,
                                             uint64_t& blockNum,
                                             PubKey& lookupPubKey,
                                             zbytes& stateDelta);
  static bool GetLookupSetStateDeltasFromSeed(const zbytes& src,
                                              const unsigned int offset,
                                              uint64_t& lowBlockNum,
                                              uint64_t& highBlockNum,
                                              PubKey& lookupPubKey,
                                              std::vector<zbytes>& stateDeltas);
  static bool SetLookupSetLookupOffline(zbytes& dst, const unsigned int offset,
                                        const uint8_t msgType,
                                        const uint32_t listenPort,
                                        const PairOfKey& lookupKey);
  static bool GetLookupSetLookupOffline(const zbytes& src,
                                        const unsigned int offset,
                                        uint8_t& msgType, uint32_t& listenPort,
                                        PubKey& lookupPubkey);
  static bool SetLookupSetLookupOnline(zbytes& dst, const unsigned int offset,
                                       const uint8_t msgType,
                                       const uint32_t listenPort,
                                       const PairOfKey& lookupKey);
  static bool GetLookupSetLookupOnline(const zbytes& src,
                                       const unsigned int offset,
                                       uint8_t& msgType, uint32_t& listenPort,
                                       PubKey& pubKey);
  static bool SetLookupGetOfflineLookups(zbytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetOfflineLookups(const zbytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetOfflineLookups(zbytes& dst, const unsigned int offset,
                                         const PairOfKey& lookupKey,
                                         const VectorOfPeer& nodes);
  static bool GetLookupSetOfflineLookups(const zbytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         VectorOfPeer& nodes);
  // UNUSED
  static bool SetLookupGetShardsFromSeed(zbytes& dst, const unsigned int offset,
                                         const uint32_t listenPort);

  // UNUSED
  static bool GetLookupGetShardsFromSeed(const zbytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  // UNUSED
  static bool SetLookupSetShardsFromSeed(
      zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const uint32_t& shardingStructureVersion,
      const DequeOfShardMembers& members);

  static bool GetLookupSetShardsFromSeed(const zbytes& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         uint32_t& shardingStructureVersion,
                                         DequeOfShardMembers& members);

  static bool SetForwardTxnBlockFromSeed(
      zbytes& dst, const unsigned int offset,
      const std::vector<Transaction>& transactions);

  static bool GetForwardTxnBlockFromSeed(
      const zbytes& src, const unsigned int offset,
      std::vector<Transaction>& transactions);

  static bool SetLookupGetMicroBlockFromLookup(
      zbytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& microBlockHashes, const uint32_t portNo);

  static bool GetLookupGetMicroBlockFromLookup(
      const zbytes& src, const unsigned int offset,
      std::vector<BlockHash>& microBlockHashes, uint32_t& portNo);

  static bool SetLookupGetMicroBlockFromL2l(
      zbytes& dst, const unsigned int offset,
      const std::vector<BlockHash>& microBlockHashes, const uint32_t portNo,
      const PairOfKey& seedKey);

  static bool GetLookupGetMicroBlockFromL2l(
      const zbytes& src, const unsigned int offset,
      std::vector<BlockHash>& microBlockHashes, uint32_t& portNo,
      PubKey& senderPubKey);

  static bool SetLookupSetMicroBlockFromLookup(
      zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const std::vector<MicroBlock>& mbs);

  static bool GetLookupSetMicroBlockFromLookup(const zbytes& src,
                                               const unsigned int offset,
                                               PubKey& lookupPubKey,
                                               std::vector<MicroBlock>& mbs);

  static bool SetLookupGetTxnsFromLookup(zbytes& dst, const unsigned int offset,
                                         const BlockHash& mbHash,
                                         const std::vector<TxnHash>& txnhashes,
                                         const uint32_t portNo);
  static bool GetLookupGetTxnsFromLookup(const zbytes& src,
                                         const unsigned int offset,
                                         BlockHash& mbHash,
                                         std::vector<TxnHash>& txnhashes,
                                         uint32_t& portNo);
  static bool SetLookupGetTxnsFromL2l(zbytes& dst, const unsigned int offset,
                                      const BlockHash& mbHash,
                                      const std::vector<TxnHash>& txnhashes,
                                      const uint32_t portNo,
                                      const PairOfKey& seedKey);
  static bool GetLookupGetTxnsFromL2l(const zbytes& src,
                                      const unsigned int offset,
                                      BlockHash& mbHash,
                                      std::vector<TxnHash>& txnhashes,
                                      uint32_t& portNo, PubKey& senderPubKey);
  // UNUSED
  static bool SetLookupSetTxnsFromLookup(
      zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const BlockHash& mbHash, const std::vector<TransactionWithReceipt>& txns);

  // USUSED
  static bool GetLookupSetTxnsFromLookup(
      const zbytes& src, const unsigned int offset, PubKey& lookupPubKey,
      BlockHash& mbHash, std::vector<TransactionWithReceipt>& txns);

  static bool SetLookupGetDirectoryBlocksFromSeed(zbytes& dst,
                                                  const unsigned int offset,
                                                  const uint32_t portNo,
                                                  const uint64_t& indexNum,
                                                  const bool includeMinerInfo);
  static bool GetLookupGetDirectoryBlocksFromSeed(const zbytes& src,
                                                  const unsigned int offset,
                                                  uint32_t& portNo,
                                                  uint64_t& indexNum,
                                                  bool& includeMinerInfo);

  static bool SetLookupSetDirectoryBlocksFromSeed(
      zbytes& dst, const unsigned int offset,
      const uint32_t& shardingStructureVersion,
      const std::vector<boost::variant<DSBlock, VCBlock>>& directoryBlocks,
      const uint64_t& indexNum, const PairOfKey& lookupKey);
  static bool GetLookupSetDirectoryBlocksFromSeed(
      const zbytes& src, const unsigned int offset,
      uint32_t& shardingStructureVersion,
      std::vector<boost::variant<DSBlock, VCBlock>>& directoryBlocks,
      uint64_t& indexNum, PubKey& pubKey);

  // ============================================================================
  // Consensus messages
  // ============================================================================

  template <class T>
  static bool PreProcessMessage(const zbytes& src, const unsigned int offset,
                                uint32_t& consensusID, PubKey& senderPubKey,
                                zbytes& reserializedSrc) {
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

    zbytes tmp(consensus_message.consensusinfo().ByteSizeLong());
    consensus_message.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

    ProtobufByteArrayToSerializable(consensus_message.pubkey(), senderPubKey);

    Signature signature;

    ProtobufByteArrayToSerializable(consensus_message.signature(), signature);

    if (!Schnorr::Verify(tmp, signature, senderPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in ConsensusConsensusFailure.");
      return false;
    }

    consensusID = consensus_message.consensusinfo().consensusid();

    // Copy src into reserializedSrc, trimming away any excess bytes beyond the
    // definition of protobuf message T
    reserializedSrc.resize(offset + consensus_message.ByteSizeLong());
    copy(src.begin(), src.begin() + offset, reserializedSrc.begin());
    consensus_message.SerializeToArray(reserializedSrc.data() + offset,
                                       consensus_message.ByteSizeLong());

    return true;
  }

  static bool SetConsensusCommit(zbytes& dst, const unsigned int offset,
                                 const uint32_t consensusID,
                                 const uint64_t blockNumber,
                                 const zbytes& blockHash,
                                 const uint16_t backupID,
                                 const std::vector<CommitInfo>& commitInfo,
                                 const PairOfKey& backupKey);
  static bool GetConsensusCommit(const zbytes& src, const unsigned int offset,
                                 const uint32_t consensusID,
                                 const uint64_t blockNumber,
                                 const zbytes& blockHash, uint16_t& backupID,
                                 std::vector<CommitInfo>& commitInfo,
                                 const DequeOfNode& committeeKeys);

  static bool SetConsensusChallenge(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID,
      const std::vector<ChallengeSubsetInfo>& subsetInfo,
      const PairOfKey& leaderKey);
  static bool GetConsensusChallenge(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, std::vector<ChallengeSubsetInfo>& subsetInfo,
      const PubKey& leaderKey);

  static bool SetConsensusResponse(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t backupID,
      const std::vector<ResponseSubsetInfo>& subsetInfo,
      const PairOfKey& backupKey);
  static bool GetConsensusResponse(const zbytes& src, const unsigned int offset,
                                   const uint32_t consensusID,
                                   const uint64_t blockNumber,
                                   const zbytes& blockHash, uint16_t& backupID,
                                   std::vector<ResponseSubsetInfo>& subsetInfo,
                                   const DequeOfNode& committeeKeys);

  static bool SetConsensusCollectiveSig(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const Signature& collectiveSig,
      const std::vector<bool>& bitmap, const PairOfKey& leaderKey,
      const zbytes& newAnnouncementMessage);
  static bool GetConsensusCollectiveSig(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, std::vector<bool>& bitmap,
      Signature& collectiveSig, const PubKey& leaderKey,
      zbytes& newAnnouncementMessage);

  static bool SetConsensusCommitFailure(zbytes& dst, const unsigned int offset,
                                        const uint32_t consensusID,
                                        const uint64_t blockNumber,
                                        const zbytes& blockHash,
                                        const uint16_t backupID,
                                        const zbytes& errorMsg,
                                        const PairOfKey& backupKey);
  static bool GetConsensusCommitFailure(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash, uint16_t& backupID,
      zbytes& errorMsg, const DequeOfNode& committeeKeys);

  static bool SetConsensusConsensusFailure(
      zbytes& dst, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash,
      const uint16_t leaderID, const PairOfKey& leaderKey);
  static bool GetConsensusConsensusFailure(
      const zbytes& src, const unsigned int offset, const uint32_t consensusID,
      const uint64_t blockNumber, const zbytes& blockHash, uint16_t& leaderID,
      const PubKey& leaderKey);

  // ============================================================================
  // View change pre check messages
  // ============================================================================

  static bool SetLookupGetDSTxBlockFromSeed(
      zbytes& dst, const unsigned int offset, const uint64_t dsLowBlockNum,
      const uint64_t dsHighBlockNum, const uint64_t txLowBlockNum,
      const uint64_t txHighBlockNum, const uint32_t listenPort);

  static bool GetLookupGetDSTxBlockFromSeed(
      const zbytes& src, const unsigned int offset, uint64_t& dsLowBlockNum,
      uint64_t& dsHighBlockNum, uint64_t& txLowBlockNum,
      uint64_t& txHighBlockNum, uint32_t& listenPort);
  static bool SetVCNodeSetDSTxBlockFromSeed(
      zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
      const std::vector<DSBlock>& DSBlocks,
      const std::vector<TxBlock>& txBlocks);
  static bool GetVCNodeSetDSTxBlockFromSeed(const zbytes& src,
                                            const unsigned int offset,
                                            std::vector<DSBlock>& dsBlocks,
                                            std::vector<TxBlock>& txBlocks,
                                            PubKey& lookupPubKey);

  // ============================================================================
  // Shard Guard network information update
  // ============================================================================

  static bool SetNodeNewShardNodeNetworkInfo(
      zbytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
      const Peer& shardNodeNewNetworkInfo, const uint64_t timestamp,
      const PairOfKey& shardNodeKey);

  static bool GetNodeNewShardNodeNetworkInfo(const zbytes& src,
                                             const unsigned int offset,
                                             uint64_t& dsEpochNumber,
                                             Peer& shardNodeNewNetworkInfo,
                                             uint64_t& timestamp,
                                             PubKey& shardNodePubkey);

  // ============================================================================
  // DS Guard network information update
  // ============================================================================

  static bool SetDSLookupNewDSGuardNetworkInfo(
      zbytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
      const Peer& dsGuardNewNetworkInfo, const uint64_t timestamp,
      const PairOfKey& dsguardkey);

  static bool GetDSLookupNewDSGuardNetworkInfo(
      const zbytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
      Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey);

  static bool SetLookupGetNewDSGuardNetworkInfoFromLookup(
      zbytes& dst, const unsigned int offset, const uint32_t portNo,
      const uint64_t dsEpochNumber, const PairOfKey& lookupKey);

  static bool GetLookupGetNewDSGuardNetworkInfoFromLookup(
      const zbytes& src, const unsigned int offset, uint32_t& portNo,
      uint64_t& dsEpochNumber);

  static bool SetNodeSetNewDSGuardNetworkInfo(
      zbytes& dst, unsigned int offset,
      const std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      const PairOfKey& lookupKey);

  static bool SetNodeGetNewDSGuardNetworkInfo(
      const zbytes& src, const unsigned int offset,
      std::vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
      PubKey& lookupPubKey);

  static bool SetNodeRemoveFromBlacklist(zbytes& dst, const unsigned int offset,
                                         const PairOfKey& myKey,
                                         const uint128_t& ipAddress,
                                         const uint64_t& dsEpochNumber);
  static bool GetNodeRemoveFromBlacklist(const zbytes& src,
                                         const unsigned int offset,
                                         PubKey& senderPubKey,
                                         uint128_t& ipAddress,
                                         uint64_t& dsEpochNumber);

  static bool SetLookupGetCosigsRewardsFromSeed(zbytes& dst,
                                                const unsigned int offset,
                                                const uint64_t txBlkNum,
                                                const uint32_t listenPort,
                                                const PairOfKey& keys);

  static bool GetLookupGetCosigsRewardsFromSeed(const zbytes& src,
                                                const unsigned int offset,
                                                PubKey& senderPubKey,
                                                uint64_t& txBlockNumber,
                                                uint32_t& port);

  static bool SetLookupGetDSLeaderTxnPool(zbytes& dst, unsigned int offset,
                                          const PairOfKey& keys,
                                          uint32_t listenPort);

  static bool GetLookupGetDSLeaderTxnPool(const zbytes& src,
                                          unsigned int offset,
                                          PubKey& senderPubkey,
                                          uint32_t& listenPort);

  static bool SetLookupSetDSLeaderTxnPool(
      zbytes& dst, unsigned int offset,
      const std::vector<Transaction>& transactions);

  static bool GetLookupSetDSLeaderTxnPool(
      const zbytes& src, unsigned int offset,
      std::vector<Transaction>& transactions);

  static bool SetLookupSetCosigsRewardsFromSeed(
      zbytes& dst, const unsigned int offset, const PairOfKey& myKey,
      const uint64_t& txBlkNumber, const std::vector<MicroBlock>& microblocks,
      const TxBlock& txBlock, const uint32_t& numberOfShards);

  static bool GetLookupSetCosigsRewardsFromSeed(
      const zbytes& src, const unsigned int offset,
      std::vector<CoinbaseStruct>& cosigrewards, PubKey& senderPubkey);

  static bool SetMinerInfoDSComm(zbytes& dst, const unsigned int offset,
                                 const MinerInfoDSComm& minerInfo);
  static bool GetMinerInfoDSComm(const zbytes& src, const unsigned int offset,
                                 MinerInfoDSComm& minerInfo);

  static bool SetMinerInfoShards(zbytes& dst, const unsigned int offset,
                                 const MinerInfoShards& minerInfo);
  static bool GetMinerInfoShards(const zbytes& src, const unsigned int offset,
                                 MinerInfoShards& minerInfo);

  static bool SetMicroBlockKey(zbytes& dst, const unsigned int offset,
                               const uint64_t& epochNum,
                               const uint32_t& shardID);
  static bool GetMicroBlockKey(const zbytes& src, const unsigned int offset,
                               uint64_t& epochNum, uint32_t& shardID);

  static bool SetTxEpoch(zbytes& dst, const unsigned int offset,
                         const uint64_t& epochNum);
  static bool GetTxEpoch(const zbytes& src, const unsigned int offset,
                         uint64_t& epochNum);
};
#endif  // ZILLIQA_SRC_LIBMESSAGE_MESSENGER_H_
