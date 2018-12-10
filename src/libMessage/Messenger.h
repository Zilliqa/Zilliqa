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
  // ============================================================================
  // Primitives
  // ============================================================================

  static bool GetDSCommitteeHash(
      const std::deque<std::pair<PubKey, Peer>>& dsCommittee,
      CommitteeHash& dst);
  static bool GetShardHash(const Shard& shard, CommitteeHash& dst);

  static bool GetShardingStructureHash(const DequeOfShard& shards,
                                       ShardingHash& dst);

  static bool SetAccount(std::vector<unsigned char>& dst,
                         const unsigned int offset, const Account& account);
  [[gnu::unused]] static bool GetAccount(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         Account& account);

  static bool SetAccountDelta(std::vector<unsigned char>& dst,
                              const unsigned int offset, Account* oldAccount,
                              const Account& newAccount);
  static bool GetAccountDelta(const std::vector<unsigned char>& src,
                              const unsigned int offset, Account& account,
                              const bool fullCopy);

  // These are called by AccountStoreBase template class
  template <class MAP>
  static bool SetAccountStore(std::vector<unsigned char>& dst,
                              const unsigned int offset,
                              const MAP& addressToAccount);
  template <class MAP>
  static bool GetAccountStore(const std::vector<unsigned char>& src,
                              const unsigned int offset, MAP& addressToAccount);
  static bool GetAccountStore(const std::vector<unsigned char>& src,
                              const unsigned int offset,
                              AccountStore& accountStore);

  // These are called by AccountStore class
  static bool SetAccountStoreDelta(std::vector<unsigned char>& dst,
                                   const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp,
                                   AccountStore& accountStore);
  static bool GetAccountStoreDelta(const std::vector<unsigned char>& src,
                                   const unsigned int offset,
                                   AccountStore& accountStore,
                                   const bool reversible);
  static bool GetAccountStoreDelta(const std::vector<unsigned char>& src,
                                   const unsigned int offset,
                                   AccountStoreTemp& accountStoreTemp);

  static bool GetMbInfoHash(const std::vector<MicroBlockInfo>& mbInfos,
                            MBInfoHash& dst);

  static bool SetDSBlockHeader(std::vector<unsigned char>& dst,
                               const unsigned int offset,
                               const DSBlockHeader& dsBlockHeader,
                               bool concreteVarsOnly = false);
  static bool GetDSBlockHeader(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               DSBlockHeader& dsBlockHeader);
  static bool SetDSBlock(std::vector<unsigned char>& dst,
                         const unsigned int offset, const DSBlock& dsBlock);
  static bool GetDSBlock(const std::vector<unsigned char>& src,
                         const unsigned int offset, DSBlock& dsBlock);

  static bool SetMicroBlockHeader(std::vector<unsigned char>& dst,
                                  const unsigned int offset,
                                  const MicroBlockHeader& microBlockHeader);
  static bool GetMicroBlockHeader(const std::vector<unsigned char>& src,
                                  const unsigned int offset,
                                  MicroBlockHeader& microBlockHeader);
  static bool SetMicroBlock(std::vector<unsigned char>& dst,
                            const unsigned int offset,
                            const MicroBlock& microBlock);
  static bool GetMicroBlock(const std::vector<unsigned char>& src,
                            const unsigned int offset, MicroBlock& microBlock);

  static bool SetTxBlockHeader(std::vector<unsigned char>& dst,
                               const unsigned int offset,
                               const TxBlockHeader& txBlockHeader);
  static bool GetTxBlockHeader(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               TxBlockHeader& txBlockHeader);
  static bool SetTxBlock(std::vector<unsigned char>& dst,
                         const unsigned int offset, const TxBlock& txBlock);
  static bool GetTxBlock(const std::vector<unsigned char>& src,
                         const unsigned int offset, TxBlock& txBlock);

  static bool SetVCBlockHeader(std::vector<unsigned char>& dst,
                               const unsigned int offset,
                               const VCBlockHeader& vcBlockHeader);
  static bool GetVCBlockHeader(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               VCBlockHeader& vcBlockHeader);
  static bool SetVCBlock(std::vector<unsigned char>& dst,
                         const unsigned int offset, const VCBlock& vcBlock);
  static bool GetVCBlock(const std::vector<unsigned char>& src,
                         const unsigned int offset, VCBlock& vcBlock);

  static bool SetFallbackBlockHeader(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const FallbackBlockHeader& fallbackBlockHeader);
  static bool GetFallbackBlockHeader(const std::vector<unsigned char>& src,
                                     const unsigned int offset,
                                     FallbackBlockHeader& fallbackBlockHeader);
  static bool SetFallbackBlock(std::vector<unsigned char>& dst,
                               const unsigned int offset,
                               const FallbackBlock& fallbackBlock);
  static bool GetFallbackBlock(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               FallbackBlock& fallbackBlock);
  static bool SetTransactionCoreInfo(std::vector<unsigned char>& dst,
                                     const unsigned int offset,
                                     const TransactionCoreInfo& transaction);
  static bool GetTransactionCoreInfo(const std::vector<unsigned char>& src,
                                     const unsigned int offset,
                                     TransactionCoreInfo& transaction);
  static bool SetTransaction(std::vector<unsigned char>& dst,
                             const unsigned int offset,
                             const Transaction& transaction);
  static bool GetTransaction(const std::vector<unsigned char>& src,
                             const unsigned int offset,
                             Transaction& transaction);
  static bool SetTransactionFileOffset(std::vector<unsigned char>& dst,
                                       const unsigned int offset,
                                       const std::vector<uint32_t>& txnOffsets);
  static bool GetTransactionFileOffset(const std::vector<unsigned char>& src,
                                       const unsigned int offset,
                                       std::vector<uint32_t>& txnOffsets);
  static bool SetTransactionArray(std::vector<unsigned char>& dst,
                                  const unsigned int offset,
                                  const std::vector<Transaction>& txns);
  static bool GetTransactionArray(const std::vector<unsigned char>& src,
                                  const unsigned int offset,
                                  std::vector<Transaction>& txns);
  static bool SetTransactionReceipt(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const TransactionReceipt& transactionReceipt);
  static bool GetTransactionReceipt(const std::vector<unsigned char>& src,
                                    const unsigned int offset,
                                    TransactionReceipt& transactionReceipt);
  static bool SetTransactionWithReceipt(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const TransactionWithReceipt& transactionWithReceipt);
  static bool GetTransactionWithReceipt(
      const std::vector<unsigned char>& src, const unsigned int offset,
      TransactionWithReceipt& transactionWithReceipt);
  static bool SetPeer(std::vector<unsigned char>& dst,
                      const unsigned int offset, const Peer& peer);
  static bool GetPeer(const std::vector<unsigned char>& src,
                      const unsigned int offset, Peer& peer);

  static bool StateDeltaToAddressMap(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::unordered_map<Address, int>& accountMap);

  // ============================================================================
  // Directory Service messages
  // ============================================================================

  static bool SetDSPoWSubmission(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t blockNumber, const uint8_t difficultyLevel,
      const Peer& submitterPeer, const std::pair<PrivKey, PubKey>& submitterKey,
      const uint64_t nonce, const std::string& resultingHash,
      const std::string& mixHash, const uint32_t& lookupId,
      const boost::multiprecision::uint128_t& gasPrice);

  static bool GetDSPoWSubmission(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint64_t& blockNumber, uint8_t& difficultyLevel, Peer& submitterPeer,
      PubKey& submitterPubKey, uint64_t& nonce, std::string& resultingHash,
      std::string& mixHash, Signature& signature, uint32_t& lookupId,
      boost::multiprecision::uint128_t& gasPrice);

  static bool SetDSPoWPacketSubmission(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::vector<DSPowSolution>& dsPowSolutions);

  static bool GetDSPowPacketSubmission(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<DSPowSolution>& dsPowSolutions);

  static bool SetDSMicroBlockSubmission(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const unsigned char microBlockType, const uint64_t epochNumber,
      const std::vector<MicroBlock>& microBlocks,
      const std::vector<std::vector<unsigned char>>& stateDeltas);
  static bool GetDSMicroBlockSubmission(
      const std::vector<unsigned char>& src, const unsigned int offset,
      unsigned char& microBlockType, uint64_t& epochNumber,
      std::vector<MicroBlock>& microBlocks,
      std::vector<std::vector<unsigned char>>& stateDeltas);

  static bool SetDSDSBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const DSBlock& dsBlock,
      const DequeOfShard& shards, const MapOfPubKeyPoW& allPoWs,
      const MapOfPubKeyPoW& dsWinnerPoWs,
      std::vector<unsigned char>& messageToCosign);

  static bool GetDSDSBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, DSBlock& dsBlock, DequeOfShard& shards,
      MapOfPubKeyPoW& allPoWs, MapOfPubKeyPoW& dsWinnerPoWs,
      std::vector<unsigned char>& messageToCosign);

  static bool SetDSFinalBlockAnnouncement(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const std::pair<PrivKey, PubKey>& leaderKey, const TxBlock& txBlock,
      const std::shared_ptr<MicroBlock>& microBlock,
      std::vector<unsigned char>& messageToCosign);

  static bool GetDSFinalBlockAnnouncement(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, const uint16_t leaderID,
      const PubKey& leaderKey, TxBlock& txBlock,
      std::shared_ptr<MicroBlock>& microBlock,
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

  static bool SetNodeVCDSBlocksMessage(std::vector<unsigned char>& dst,
                                       const unsigned int offset,
                                       const uint32_t shardId,
                                       const DSBlock& dsBlock,
                                       const std::vector<VCBlock>& vcBlocks,
                                       const DequeOfShard& shards);

  static bool GetNodeVCDSBlocksMessage(const std::vector<unsigned char>& src,
                                       const unsigned int offset,
                                       uint32_t& shardId, DSBlock& dsBlock,
                                       std::vector<VCBlock>& vcBlocks,
                                       DequeOfShard& shards);

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

  static bool SetNodeMBnForwardTransaction(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const MicroBlock& microBlock,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetNodeMBnForwardTransaction(
      const std::vector<unsigned char>& src, const unsigned int offset,
      MBnForwardedTxnEntry& entry);

  static bool SetNodeForwardTxnBlock(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t& epochNumber, const uint64_t& dsBlockNum,
      const uint32_t& shardId, const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<Transaction>& txnsCurrent,
      const std::vector<Transaction>& txnsGenerated);
  static bool GetNodeForwardTxnBlock(const std::vector<unsigned char>& src,
                                     const unsigned int offset,
                                     uint64_t& epochNumber,
                                     uint64_t& dsBlockNum, uint32_t& shardId,
                                     PubKey& lookupPubKey,
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

  static bool ShardStructureToArray(std::vector<unsigned char>& dst,
                                    const unsigned int offset,
                                    const DequeOfShard& shards);
  static bool ArrayToShardStructure(const std::vector<unsigned char>& src,
                                    const unsigned int offset,
                                    DequeOfShard& shards);

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
                                    const std::pair<PrivKey, PubKey>& lookupKey,
                                    const std::vector<Peer>& candidateSeeds);
  static bool GetLookupSetSeedPeers(const std::vector<unsigned char>& src,
                                    const unsigned int offset,
                                    PubKey& lookupPubKey,
                                    std::vector<Peer>& candidateSeeds);
  static bool SetLookupGetDSInfoFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort,
                                         const bool initialDS);
  static bool GetLookupGetDSInfoFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort, bool& initialDS);
  static bool SetLookupSetDSInfoFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& senderKey,
      const std::deque<std::pair<PubKey, Peer>>& dsNodes, const bool initialDS);
  static bool GetLookupSetDSInfoFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      PubKey& senderPubKey, std::deque<std::pair<PubKey, Peer>>& dsNodes,
      bool& initialDS);
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
  static bool SetLookupSetDSBlockFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t lowBlockNum, const uint64_t highBlockNum,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<DSBlock>& dsBlocks);
  static bool GetLookupSetDSBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
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
  static bool SetLookupSetTxBlockFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t lowBlockNum, const uint64_t highBlockNum,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<TxBlock>& txBlocks);
  static bool GetLookupSetTxBlockFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint64_t& lowBlockNum,
                                          uint64_t& highBlockNum,
                                          PubKey& lookupPubKey,
                                          std::vector<TxBlock>& txBlocks);
  static bool SetLookupGetStateDeltaFromSeed(std::vector<unsigned char>& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNum,
                                             const uint32_t listenPort);
  static bool GetLookupGetStateDeltaFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint64_t& blockNum, uint32_t& listenPort);
  static bool SetLookupSetStateDeltaFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t blockNum, const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<unsigned char>& stateDelta);
  static bool GetLookupSetStateDeltaFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint64_t& blockNum, PubKey& lookupPubKey,
      std::vector<unsigned char>& stateDelta);
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
  static bool SetLookupSetStateFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const AccountStore& accountStore);
  static bool GetLookupSetStateFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      PubKey& lookupPubKey, std::vector<unsigned char>& accountStoreBytes);
  static bool SetLookupSetLookupOffline(std::vector<unsigned char>& dst,
                                        const unsigned int offset,
                                        const uint32_t listenPort);
  static bool GetLookupSetLookupOffline(const std::vector<unsigned char>& src,
                                        const unsigned int offset,
                                        uint32_t& listenPort);
  static bool SetLookupSetLookupOnline(std::vector<unsigned char>& dst,
                                       const unsigned int offset,
                                       const uint32_t listenPort,
                                       const PubKey& pubKey);
  static bool GetLookupSetLookupOnline(const std::vector<unsigned char>& src,
                                       const unsigned int offset,
                                       uint32_t& listenPort, PubKey& pubKey);
  static bool SetLookupGetOfflineLookups(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);
  static bool GetLookupGetOfflineLookups(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
  static bool SetLookupSetOfflineLookups(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<Peer>& nodes);
  static bool GetLookupSetOfflineLookups(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         std::vector<Peer>& nodes);
  static bool SetLookupGetStartPoWFromSeed(std::vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort);
  static bool GetLookupGetStartPoWFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint32_t& listenPort);
  static bool SetLookupSetStartPoWFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint64_t blockNumber, const std::pair<PrivKey, PubKey>& lookupKey);
  static bool GetLookupSetStartPoWFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      PubKey& lookupPubKey);

  static bool SetLookupGetShardsFromSeed(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);

  static bool GetLookupGetShardsFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);

  static bool SetLookupSetShardsFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey, const DequeOfShard& shards);

  static bool GetLookupSetShardsFromSeed(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         PubKey& lookupPubKey,
                                         DequeOfShard& shards);

  static bool SetLookupGetMicroBlockFromLookup(
      std::vector<unsigned char>& dest, const unsigned int offset,
      const std::vector<BlockHash>& microBlockHashes, uint32_t portNo);

  static bool GetLookupGetMicroBlockFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<BlockHash>& microBlockHashes, uint32_t& portNo);

  static bool SetLookupSetMicroBlockFromLookup(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<MicroBlock>& mbs);

  static bool GetLookupSetMicroBlockFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      PubKey& lookupPubKey, std::vector<MicroBlock>& mbs);

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
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<TransactionWithReceipt>& txns);
  static bool GetLookupSetTxnsFromLookup(
      const std::vector<unsigned char>& src, const unsigned int offset,
      PubKey& lookupPubKey, std::vector<TransactionWithReceipt>& txns);

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
      const uint16_t subsetID, const std::vector<unsigned char>& blockHash,
      const uint16_t leaderID, const CommitPoint& aggregatedCommit,
      const PubKey& aggregatedKey, const Challenge& challenge,
      const std::pair<PrivKey, PubKey>& leaderKey);
  static bool GetConsensusChallenge(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      uint16_t& subsetID, const std::vector<unsigned char>& blockHash,
      const uint16_t leaderID, CommitPoint& aggregatedCommit,
      PubKey& aggregatedKey, Challenge& challenge, const PubKey& leaderKey);

  static bool SetConsensusResponse(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const uint16_t subsetID, const std::vector<unsigned char>& blockHash,
      const uint16_t backupID, const Response& response,
      const std::pair<PrivKey, PubKey>& backupKey);
  static bool GetConsensusResponse(
      const std::vector<unsigned char>& src, const unsigned int offset,
      const uint32_t consensusID, const uint64_t blockNumber,
      const std::vector<unsigned char>& blockHash, uint16_t& backupID,
      uint16_t& subsetID, Response& response,
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
  static bool SetBlockLink(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink);
  static bool GetBlockLink(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink);
  static bool SetFallbackBlockWShardingStructure(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const FallbackBlock& fallbackblock, const DequeOfShard& shards);
  static bool GetFallbackBlockWShardingStructure(
      const std::vector<unsigned char>& src, const unsigned int offset,
      FallbackBlock& fallbackblock, DequeOfShard& shards);
  static bool GetLookupGetDirectoryBlocksFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint32_t& portno, uint64_t& index_num);
  static bool SetLookupGetDirectoryBlocksFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const uint32_t portno, const uint64_t& index_num);
  static bool SetLookupSetDirectoryBlocksFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      const uint64_t& index_num);
  static bool GetLookupSetDirectoryBlocksFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<
          boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
          directoryBlocks,
      uint64_t& index_num);

  // ============================================================================
  // View change pre check messages
  // ============================================================================

  static bool SetLookupGetDSTxBlockFromSeed(std::vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t dsLowBlockNum,
                                            const uint64_t dsHighBlockNum,
                                            const uint64_t txLowBlockNum,
                                            const uint64_t txHighBlockNum,
                                            const uint32_t listenPort);

  static bool GetLookupGetDSTxBlockFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      uint64_t& dsLowBlockNum, uint64_t& dsHighBlockNum,
      uint64_t& txLowBlockNum, uint64_t& txHighBlockNum, uint32_t& listenPort);
  static bool SetVCNodeSetDSTxBlockFromSeed(
      std::vector<unsigned char>& dst, const unsigned int offset,
      const std::pair<PrivKey, PubKey>& lookupKey,
      const std::vector<DSBlock>& DSBlocks,
      const std::vector<TxBlock>& txBlocks);
  static bool GetVCNodeSetDSTxBlockFromSeed(
      const std::vector<unsigned char>& src, const unsigned int offset,
      std::vector<DSBlock>& dsBlocks, std::vector<TxBlock>& txBlocks,
      PubKey& lookupPubKey);
};
#endif  // __MESSENGER_H__
