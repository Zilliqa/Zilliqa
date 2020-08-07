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

#include <vector>

#include "Validator.h"
#include "libData/AccountData/Account.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libUtils/BitVector.h"

using namespace std;
using namespace boost::multiprecision;

using ShardingHash = dev::h256;

Validator::Validator(Mediator& mediator) : m_mediator(mediator) {}

Validator::~Validator() {}

bool Validator::VerifyTransaction(const Transaction& tran) {
  bytes txnData;
  tran.SerializeCoreFields(txnData, 0);

  return Schnorr::Verify(txnData, tran.GetSignature(), tran.GetSenderPubKey());
}

bool Validator::CheckCreatedTransaction(const Transaction& tx,
                                        TransactionReceipt& receipt,
                                        ErrTxnStatus& error_code) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransaction not expected to be "
                "called from LookUp node.");
    return true;
  }
  error_code = ErrTxnStatus::NOT_PRESENT;
  // LOG_MARKER();

  // LOG_GENERAL(INFO, "Tran: " << tx.GetTranID());

  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    LOG_GENERAL(WARNING, "CHAIN_ID incorrect");
    error_code = ErrTxnStatus::VERIF_ERROR;
    return false;
  }

  if (DataConversion::UnpackB(tx.GetVersion()) != TRANSACTION_VERSION) {
    LOG_GENERAL(WARNING, "Transaction version incorrect "
                             << "Expected:" << TRANSACTION_VERSION << " Actual:"
                             << DataConversion::UnpackB(tx.GetVersion()));
    error_code = ErrTxnStatus::VERIF_ERROR;
    return false;
  }

  // Check if from account is sharded here
  const PubKey& senderPubKey = tx.GetSenderPubKey();
  Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);

  if (IsNullAddress(fromAddr)) {
    LOG_GENERAL(WARNING, "Invalid address for issuing transactions");
    error_code = ErrTxnStatus::INVALID_FROM_ACCOUNT;
    return false;
  }

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_GENERAL(WARNING, "fromAddr not found: " << fromAddr
                                                << ". Transaction rejected: "
                                                << tx.GetTranID());
    error_code = ErrTxnStatus::INVALID_FROM_ACCOUNT;
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    error_code = ErrTxnStatus::INSUFFICIENT_BALANCE;
    return false;
  }

  receipt.SetEpochNum(m_mediator.m_currentEpochNum);

  return AccountStore::GetInstance().UpdateAccountsTemp(
      m_mediator.m_currentEpochNum, m_mediator.m_node->getNumShards(),
      m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE, tx, receipt,
      error_code);
}

bool Validator::CheckCreatedTransactionFromLookup(const Transaction& tx,
                                                  ErrTxnStatus& error_code) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransactionFromLookup not expected "
                "to be called from LookUp node.");
    return true;
  }

  error_code = ErrTxnStatus::NOT_PRESENT;

  // LOG_MARKER();

  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    LOG_GENERAL(WARNING, "CHAIN_ID incorrect");
    error_code = ErrTxnStatus::VERIF_ERROR;
    return false;
  }

  if (DataConversion::UnpackB(tx.GetVersion()) != TRANSACTION_VERSION) {
    LOG_GENERAL(WARNING, "Transaction version incorrect "
                             << "Expected:" << TRANSACTION_VERSION << " Actual:"
                             << DataConversion::UnpackB(tx.GetVersion()));
    error_code = ErrTxnStatus::VERIF_ERROR;
    return false;
  }

  // Check if from account is sharded here

  const Address fromAddr = tx.GetSenderAddr();
  unsigned int shardId = m_mediator.m_node->GetShardId();
  unsigned int numShards = m_mediator.m_node->getNumShards();

  if (tx.GetGasLimit() >
      (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE
           ? SHARD_MICROBLOCK_GAS_LIMIT
           : DS_MICROBLOCK_GAS_LIMIT)) {
    error_code = ErrTxnStatus::HIGH_GAS_LIMIT;
    // Already should be checked at lookup
    LOG_GENERAL(WARNING, "Txn gas limit too high");
    return false;
  }

  if (IsNullAddress(fromAddr)) {
    LOG_GENERAL(WARNING, "Invalid address for issuing transactions");
    error_code = ErrTxnStatus::INVALID_FROM_ACCOUNT;
    return false;
  }

  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
    unsigned int correct_shard_from =
        Transaction::GetShardIndex(fromAddr, numShards);
    if (correct_shard_from != shardId) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "This tx is not sharded to me!"
                    << " From Account  = 0x" << fromAddr
                    << " Correct shard = " << correct_shard_from
                    << " This shard    = " << m_mediator.m_node->GetShardId());
      error_code = ErrTxnStatus::INCORRECT_SHARD;
      return false;
      // // Transaction created from the GenTransactionBulk will be rejected
      // // by all shards but one. Next line is commented to avoid this
      // return false;
    }

    if (Transaction::GetTransactionType(tx) == Transaction::CONTRACT_CALL) {
      unsigned int correct_shard_to =
          Transaction::GetShardIndex(tx.GetToAddr(), numShards);
      if (correct_shard_to != correct_shard_from) {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "The fromShard " << correct_shard_from << " and toShard "
                                   << correct_shard_to
                                   << " is different for the call SC txn");
        // Already checked at lookup
        error_code = ErrTxnStatus::CONTRACT_CALL_WRONG_SHARD;
        return false;
      }
    }
  }

  if (tx.GetCode().size() > MAX_CODE_SIZE_IN_BYTES) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Code size " << tx.GetCode().size()
                           << " larger than maximum code size allowed "
                           << MAX_CODE_SIZE_IN_BYTES);
    // Already checked at lookup
    error_code = ErrTxnStatus::HIGH_BYTE_SIZE_CODE;
    return false;
  }

  if (tx.GetGasPrice() <
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice()) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "GasPrice " << tx.GetGasPrice()
                          << " lower than minimum allowable "
                          << m_mediator.m_dsBlockChain.GetLastBlock()
                                 .GetHeader()
                                 .GetGasPrice());
    // Should be checked at lookup also
    error_code = ErrTxnStatus::INSUFFICIENT_GAS;
    return false;
  }

  if (!VerifyTransaction(tx)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Signature incorrect: " << fromAddr << ". Transaction rejected: "
                                      << tx.GetTranID());
    error_code = ErrTxnStatus::VERIF_ERROR;
    return false;
  }

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "fromAddr not found: " << fromAddr << ". Transaction rejected: "
                                     << tx.GetTranID());
    error_code = ErrTxnStatus::INVALID_FROM_ACCOUNT;
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    error_code = ErrTxnStatus::INSUFFICIENT_BALANCE;
    return false;
  }

  return true;
}

template <class Container, class DirectoryBlock>
bool Validator::CheckBlockCosignature(const DirectoryBlock& block,
                                      const Container& commKeys,
                                      const bool showLogs) {
  if (showLogs) {
    LOG_MARKER();
  }

  unsigned int index = 0;
  unsigned int count = 0;

  const vector<bool>& B2 = block.GetB2();
  if (commKeys.size() != B2.size()) {
    LOG_GENERAL(WARNING, "Mismatch: committee size = "
                             << commKeys.size()
                             << ", co-sig bitmap size = " << B2.size());
    return false;
  }

  // Generate the aggregated key
  vector<PubKey> keys;
  for (auto const& kv : commKeys) {
    if (B2.at(index)) {
      keys.emplace_back(get<PubKey>(kv));
      count++;
    }
    index++;
  }

  if (count != ConsensusCommon::NumForConsensus(B2.size())) {
    LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
    return false;
  }

  shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
  if (aggregatedKey == nullptr) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    return false;
  }

  // Verify the collective signature
  bytes serializedHeader;
  block.GetHeader().Serialize(serializedHeader, 0);
  block.GetCS1().Serialize(serializedHeader, serializedHeader.size());
  BitVector::SetBitVector(serializedHeader, serializedHeader.size(),
                          block.GetB1());
  if (!MultiSig::MultiSigVerify(serializedHeader, 0, serializedHeader.size(),
                                block.GetCS2(), *aggregatedKey)) {
    LOG_GENERAL(WARNING, "Cosig verification failed");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return false;
  }

  return true;
}

bool Validator::CheckDirBlocks(
    const vector<boost::variant<DSBlock, VCBlock,
                                FallbackBlockWShardingStructure>>& dirBlocks,
    const DequeOfNode& initDsComm, const uint64_t& index_num,
    DequeOfNode& newDSComm) {
  DequeOfNode mutable_ds_comm = initDsComm;

  bool ret = true;

  uint64_t prevdsblocknum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t totalIndex = index_num;
  ShardingHash prevShardingHash =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetShardingHash();
  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());

  for (const auto& dirBlock : dirBlocks) {
    if (typeid(DSBlock) == dirBlock.type()) {
      const auto& dsblock = get<DSBlock>(dirBlock);
      if (dsblock.GetHeader().GetBlockNum() != prevdsblocknum + 1) {
        LOG_GENERAL(WARNING, "DSblocks not in sequence "
                                 << dsblock.GetHeader().GetBlockNum() << " "
                                 << prevdsblocknum);
        ret = false;
        break;
      }
      if (dsblock.GetHeader().GetMyHash() != dsblock.GetBlockHash()) {
        LOG_GENERAL(WARNING, "DSblock "
                                 << prevdsblocknum + 1
                                 << " has different blockhash than stored "
                                 << " Stored: " << dsblock.GetBlockHash());
        ret = false;
        break;
      }

      if (!CheckBlockCosignature(dsblock, mutable_ds_comm)) {
        LOG_GENERAL(WARNING, "Co-sig verification of ds block "
                                 << prevdsblocknum + 1 << " failed");
        ret = false;
        break;
      }
      if (prevHash != dsblock.GetHeader().GetPrevHash()) {
        LOG_GENERAL(WARNING, "prevHash incorrect "
                                 << prevHash << " "
                                 << dsblock.GetHeader().GetPrevHash()
                                 << " in DS block " << prevdsblocknum + 1);
        ret = false;
        break;
      }
      prevdsblocknum++;
      prevShardingHash = dsblock.GetHeader().GetShardingHash();
      m_mediator.m_blocklinkchain.AddBlockLink(
          totalIndex, prevdsblocknum, BlockType::DS, dsblock.GetBlockHash());
      m_mediator.m_dsBlockChain.AddBlock(dsblock);
      bytes serializedDSBlock;
      dsblock.Serialize(serializedDSBlock, 0);
      prevHash = dsblock.GetBlockHash();
      if (!BlockStorage::GetBlockStorage().PutDSBlock(
              dsblock.GetHeader().GetBlockNum(), serializedDSBlock)) {
        LOG_GENERAL(WARNING, "BlockStorage::PutDSBlock failed " << dsblock);
        return false;
      }
      m_mediator.m_node->UpdateDSCommitteeComposition(mutable_ds_comm, dsblock);
      totalIndex++;
      if (!BlockStorage::GetBlockStorage().ResetDB(BlockStorage::STATE_DELTA)) {
        LOG_GENERAL(WARNING, "BlockStorage::ResetDB failed");
        return false;
      }
    } else if (typeid(VCBlock) == dirBlock.type()) {
      const auto& vcblock = get<VCBlock>(dirBlock);

      if (vcblock.GetHeader().GetViewChangeDSEpochNo() != prevdsblocknum + 1) {
        LOG_GENERAL(WARNING,
                    "VC block ds epoch number does not match the number being "
                    "processed "
                        << prevdsblocknum << " "
                        << vcblock.GetHeader().GetViewChangeDSEpochNo());
        ret = false;
        break;
      }
      if (vcblock.GetHeader().GetMyHash() != vcblock.GetBlockHash()) {
        LOG_GENERAL(WARNING, "VCblock in "
                                 << prevdsblocknum
                                 << " has different blockhash than stored "
                                 << " Stored: " << vcblock.GetBlockHash());
        ret = false;
        break;
      }
      if (!CheckBlockCosignature(vcblock, mutable_ds_comm)) {
        LOG_GENERAL(WARNING, "Co-sig verification of vc block in "
                                 << prevdsblocknum << " failed"
                                 << totalIndex + 1);
        ret = false;
        break;
      }

      if (prevHash != vcblock.GetHeader().GetPrevHash()) {
        LOG_GENERAL(WARNING, "prevHash incorrect "
                                 << prevHash << " "
                                 << vcblock.GetHeader().GetPrevHash()
                                 << "in VC block " << prevdsblocknum + 1);
        ret = false;
        break;
      }

      m_mediator.m_node->UpdateRetrieveDSCommitteeCompositionAfterVC(
          vcblock, mutable_ds_comm);
      m_mediator.m_blocklinkchain.AddBlockLink(totalIndex, prevdsblocknum + 1,
                                               BlockType::VC,
                                               vcblock.GetBlockHash());
      bytes vcblockserialized;
      vcblock.Serialize(vcblockserialized, 0);
      if (!BlockStorage::GetBlockStorage().PutVCBlock(vcblock.GetBlockHash(),
                                                      vcblockserialized)) {
        LOG_GENERAL(WARNING, "BlockStorage::PutVCBlock failed " << vcblock);
        return false;
      }
      prevHash = vcblock.GetBlockHash();
      totalIndex++;
    } else if (typeid(FallbackBlockWShardingStructure) == dirBlock.type()) {
      const auto& fallbackwshardingstructure =
          get<FallbackBlockWShardingStructure>(dirBlock);

      const auto& fallbackblock = fallbackwshardingstructure.m_fallbackblock;
      const DequeOfShard& shards = fallbackwshardingstructure.m_shards;

      if (fallbackblock.GetHeader().GetFallbackDSEpochNo() !=
          prevdsblocknum + 1) {
        LOG_GENERAL(WARNING,
                    "Fallback block ds epoch number does not match the number "
                    "being processed "
                        << prevdsblocknum << " "
                        << fallbackblock.GetHeader().GetFallbackDSEpochNo());
        ret = false;
        break;
      }

      if (fallbackblock.GetHeader().GetMyHash() !=
          fallbackblock.GetBlockHash()) {
        LOG_GENERAL(WARNING,
                    "Fallbackblock in "
                        << prevdsblocknum
                        << " has different blockhash than stored "
                        << " Stored: " << fallbackblock.GetBlockHash());
        ret = false;
        break;
      }

      if (prevHash != fallbackblock.GetHeader().GetPrevHash()) {
        LOG_GENERAL(WARNING, "prevHash incorrect "
                                 << prevHash << " "
                                 << fallbackblock.GetHeader().GetPrevHash()
                                 << "in FB block " << prevdsblocknum + 1);
        ret = false;
        break;
      }

      ShardingHash shardinghash;
      if (!Messenger::GetShardingStructureHash(SHARDINGSTRUCTURE_VERSION,
                                               shards, shardinghash)) {
        LOG_GENERAL(WARNING, "GetShardingStructureHash failed");
        ret = false;
        break;
      }

      if (shardinghash != prevShardingHash) {
        LOG_GENERAL(WARNING, "ShardingHash does not match ");
        ret = false;
        break;
      }

      uint32_t shard_id = fallbackblock.GetHeader().GetShardId();

      if (!CheckBlockCosignature(fallbackblock, shards.at(shard_id))) {
        LOG_GENERAL(WARNING, "Co-sig verification of fallbackblock in "
                                 << prevdsblocknum << " failed"
                                 << totalIndex + 1);
        ret = false;
        break;
      }
      const PubKey& leaderPubKey = fallbackblock.GetHeader().GetLeaderPubKey();
      const Peer& leaderNetworkInfo =
          fallbackblock.GetHeader().GetLeaderNetworkInfo();
      m_mediator.m_node->UpdateDSCommitteeAfterFallback(
          shard_id, leaderPubKey, leaderNetworkInfo, mutable_ds_comm, shards);
      m_mediator.m_blocklinkchain.AddBlockLink(totalIndex, prevdsblocknum + 1,
                                               BlockType::FB,
                                               fallbackblock.GetBlockHash());
      bytes fallbackblockser;
      fallbackwshardingstructure.Serialize(fallbackblockser, 0);
      if (!BlockStorage::GetBlockStorage().PutFallbackBlock(
              fallbackblock.GetBlockHash(), fallbackblockser)) {
        LOG_GENERAL(WARNING,
                    "BlockStorage::PutFallbackBlock failed " << fallbackblock);
        return false;
      }
      prevHash = fallbackblock.GetBlockHash();
      totalIndex++;
    } else {
      LOG_GENERAL(WARNING, "dirBlock type unexpected ");
    }
  }

  newDSComm = move(mutable_ds_comm);
  return ret;
}

bool Validator::CheckDirBlocksNoUpdate(
    const vector<boost::variant<DSBlock, VCBlock,
                                FallbackBlockWShardingStructure>>& dirBlocks,
    const DequeOfNode& initDsComm, const uint64_t& index_num,
    DequeOfNode& newDSComm) {
  DequeOfNode mutable_ds_comm = initDsComm;

  bool ret = true;

  uint64_t prevdsblocknum = 0;
  uint64_t totalIndex = index_num;
  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      BlockLinkChain::GetFromPersistentStorage(0));

  for (const auto& dirBlock : dirBlocks) {
    if (typeid(DSBlock) == dirBlock.type()) {
      const auto& dsblock = get<DSBlock>(dirBlock);
      if (dsblock.GetHeader().GetBlockNum() != prevdsblocknum + 1) {
        LOG_GENERAL(WARNING, "DSblocks not in sequence "
                                 << dsblock.GetHeader().GetBlockNum() << " "
                                 << prevdsblocknum);
        ret = false;
        break;
      }
      if (dsblock.GetHeader().GetMyHash() != dsblock.GetBlockHash()) {
        LOG_GENERAL(WARNING, "DSblock "
                                 << prevdsblocknum + 1
                                 << " has different blockhash than stored "
                                 << " Stored: " << dsblock.GetBlockHash());
        ret = false;
        break;
      }
      if (!CheckBlockCosignature(dsblock, mutable_ds_comm, false)) {
        LOG_GENERAL(WARNING, "Co-sig verification of ds block "
                                 << prevdsblocknum + 1 << " failed");
        ret = false;
        break;
      }
      if (prevHash != dsblock.GetHeader().GetPrevHash()) {
        LOG_GENERAL(WARNING, "prevHash incorrect "
                                 << prevHash << " "
                                 << dsblock.GetHeader().GetPrevHash()
                                 << " in DS block " << prevdsblocknum + 1);
        ret = false;
        break;
      }
      prevdsblocknum++;
      prevHash = dsblock.GetBlockHash();
      m_mediator.m_node->UpdateDSCommitteeComposition(mutable_ds_comm, dsblock,
                                                      false);
      totalIndex++;
    } else if (typeid(VCBlock) == dirBlock.type()) {
      const auto& vcblock = get<VCBlock>(dirBlock);

      if (vcblock.GetHeader().GetViewChangeDSEpochNo() != prevdsblocknum + 1) {
        LOG_GENERAL(WARNING,
                    "VC block ds epoch number does not match the number being "
                    "processed "
                        << prevdsblocknum << " "
                        << vcblock.GetHeader().GetViewChangeDSEpochNo());
        ret = false;
        break;
      }
      if (vcblock.GetHeader().GetMyHash() != vcblock.GetBlockHash()) {
        LOG_GENERAL(WARNING, "VCblock in "
                                 << prevdsblocknum
                                 << " has different blockhash than stored "
                                 << " Stored: " << vcblock.GetBlockHash());
        ret = false;
        break;
      }
      if (!CheckBlockCosignature(vcblock, mutable_ds_comm, false)) {
        LOG_GENERAL(WARNING, "Co-sig verification of vc block in "
                                 << prevdsblocknum << " failed"
                                 << totalIndex + 1);
        ret = false;
        break;
      }

      if (prevHash != vcblock.GetHeader().GetPrevHash()) {
        LOG_GENERAL(WARNING, "prevHash incorrect "
                                 << prevHash << " "
                                 << vcblock.GetHeader().GetPrevHash()
                                 << "in VC block " << prevdsblocknum + 1);
        ret = false;
        break;
      }

      m_mediator.m_node->UpdateRetrieveDSCommitteeCompositionAfterVC(
          vcblock, mutable_ds_comm, false);
      prevHash = vcblock.GetBlockHash();
      totalIndex++;
    } else if (typeid(FallbackBlockWShardingStructure) == dirBlock.type()) {
      // TODO
    } else {
      LOG_GENERAL(WARNING, "dirBlock type unexpected ");
    }
  }

  newDSComm = move(mutable_ds_comm);
  return ret;
}

Validator::TxBlockValidationMsg Validator::CheckTxBlocks(
    const std::vector<TxBlock>& txBlocks, const DequeOfNode& dsComm,
    const BlockLink& latestBlockLink) {
  // Verify the last Tx Block
  uint64_t latestDSIndex = get<BlockLinkIndex::DSINDEX>(latestBlockLink);

  if (get<BlockLinkIndex::BLOCKTYPE>(latestBlockLink) != BlockType::DS) {
    if (latestDSIndex == 0) {
      LOG_GENERAL(WARNING, "The latestDSIndex is 0 and blocktype not DS");
      return TxBlockValidationMsg::INVALID;
    }
    latestDSIndex--;
  }

  const TxBlock& latestTxBlock = txBlocks.back();

  if (latestTxBlock.GetHeader().GetDSBlockNum() != latestDSIndex) {
    if (latestDSIndex > latestTxBlock.GetHeader().GetDSBlockNum()) {
      LOG_GENERAL(WARNING, "Latest Tx Block fetched is stale "
                               << latestDSIndex << " "
                               << latestTxBlock.GetHeader().GetDSBlockNum());
      return TxBlockValidationMsg::INVALID;
    }

    LOG_GENERAL(WARNING,
                "The latest DS index does not match that of the latest tx "
                "block ds num, try fetching Tx and Dir Blocks again "
                    << latestTxBlock.GetHeader().GetDSBlockNum() << " "
                    << latestDSIndex);
    return TxBlockValidationMsg::STALEDSINFO;
  }

  if (!CheckBlockCosignature(latestTxBlock, dsComm)) {
    return TxBlockValidationMsg::INVALID;
  }

  if (txBlocks.size() < 2) {
    return TxBlockValidationMsg::VALID;
  }

  BlockHash prevBlockHash = latestTxBlock.GetHeader().GetPrevHash();
  unsigned int sIndex = txBlocks.size() - 2;

  for (unsigned int i = 0; i < txBlocks.size() - 1; i++) {
    if (prevBlockHash != txBlocks.at(sIndex).GetHeader().GetMyHash()) {
      LOG_GENERAL(WARNING,
                  "Prev hash "
                      << prevBlockHash << " and hash of blocknum "
                      << txBlocks.at(sIndex).GetHeader().GetBlockNum());
      return TxBlockValidationMsg::INVALID;
    }
    prevBlockHash = txBlocks.at(sIndex).GetHeader().GetPrevHash();
    sIndex--;
  }

  return TxBlockValidationMsg::VALID;
}