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

#include <vector>

#include "Validator.h"
#include "libData/AccountData/Account.h"
#include "libMediator/Mediator.h"
#include "libUtils/BitVector.h"

using namespace std;
using namespace boost::multiprecision;

using ShardingHash = dev::h256;

Validator::Validator(Mediator& mediator) : m_mediator(mediator) {}

Validator::~Validator() {}

bool Validator::VerifyTransaction(const Transaction& tran) const {
  bytes txnData;
  tran.SerializeCoreFields(txnData, 0);

  return Schnorr::GetInstance().Verify(txnData, tran.GetSignature(),
                                       tran.GetSenderPubKey());
}

bool Validator::CheckCreatedTransaction(const Transaction& tx,
                                        TransactionReceipt& receipt) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransaction not expected to be "
                "called from LookUp node.");
    return true;
  }
  // LOG_MARKER();

  // LOG_GENERAL(INFO, "Tran: " << tx.GetTranID());

  // Check if from account is sharded here
  const PubKey& senderPubKey = tx.GetSenderPubKey();
  Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_GENERAL(INFO, "fromAddr not found: " << fromAddr
                                             << ". Transaction rejected: "
                                             << tx.GetTranID());
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    return false;
  }

  return AccountStore::GetInstance().UpdateAccountsTemp(
      m_mediator.m_currentEpochNum, m_mediator.m_node->getNumShards(),
      m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE, tx, receipt);
}

bool Validator::CheckCreatedTransactionFromLookup(const Transaction& tx) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransactionFromLookup not expected "
                "to be called from LookUp node.");
    return true;
  }

  // LOG_MARKER();

  // Check if from account is sharded here
  const PubKey& senderPubKey = tx.GetSenderPubKey();
  Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  unsigned int shardId = m_mediator.m_node->GetShardId();
  unsigned int numShards = m_mediator.m_node->getNumShards();

  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
    unsigned int correct_shard_from =
        Transaction::GetShardIndex(fromAddr, numShards);
    if (correct_shard_from != shardId) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "This tx is not sharded to me!"
                    << " From Account  = 0x" << fromAddr
                    << " Correct shard = " << correct_shard_from
                    << " This shard    = " << m_mediator.m_node->GetShardId());
      return false;
      // // Transaction created from the GenTransactionBulk will be rejected
      // // by all shards but one. Next line is commented to avoid this
      // return false;
    }

    if (tx.GetData().size() > 0 && tx.GetToAddr() != NullAddress) {
      unsigned int correct_shard_to =
          Transaction::GetShardIndex(tx.GetToAddr(), numShards);
      if (correct_shard_to != correct_shard_from) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "The fromShard " << correct_shard_from << " and toShard "
                                   << correct_shard_to
                                   << " is different for the call SC txn");
        return false;
      }
    }
  }

  if (tx.GetGasPrice() <
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "GasPrice " << tx.GetGasPrice()
                          << " lower than minimum allowable "
                          << m_mediator.m_dsBlockChain.GetLastBlock()
                                 .GetHeader()
                                 .GetGasPrice());
    return false;
  }

  if (!VerifyTransaction(tx)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Signature incorrect: " << fromAddr << ". Transaction rejected: "
                                      << tx.GetTranID());
    return false;
  }

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "fromAddr not found: " << fromAddr << ". Transaction rejected: "
                                     << tx.GetTranID());
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    return false;
  }

  return true;
}

template <class Container, class DirectoryBlock>
bool Validator::CheckBlockCosignature(const DirectoryBlock& block,
                                      const Container& commKeys) {
  LOG_MARKER();

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
  if (!Schnorr::GetInstance().Verify(serializedHeader, 0,
                                     serializedHeader.size(), block.GetCS2(),
                                     *aggregatedKey)) {
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
    const deque<pair<PubKey, Peer>>& initDsComm, const uint64_t& index_num,
    deque<pair<PubKey, Peer>>& newDSComm) {
  deque<pair<PubKey, Peer>> mutable_ds_comm = initDsComm;

  bool ret = true;

  uint64_t prevdsblocknum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t totalIndex = index_num;
  ShardingHash prevShardingHash =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetShardingHash();

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

      if (!CheckBlockCosignature(dsblock, mutable_ds_comm)) {
        LOG_GENERAL(WARNING, "Co-sig verification of ds block "
                                 << prevdsblocknum + 1 << " failed");
        ret = false;
        break;
      }
      prevdsblocknum++;
      prevShardingHash = dsblock.GetHeader().GetShardingHash();
      m_mediator.m_blocklinkchain.AddBlockLink(
          totalIndex, prevdsblocknum, BlockType::DS, dsblock.GetBlockHash());
      m_mediator.m_dsBlockChain.AddBlock(dsblock);
      // Store DS Block to disk
      if (!ARCHIVAL_NODE) {
        bytes serializedDSBlock;
        dsblock.Serialize(serializedDSBlock, 0);
        BlockStorage::GetBlockStorage().PutDSBlock(
            dsblock.GetHeader().GetBlockNum(), serializedDSBlock);
      } else {
        m_mediator.m_archDB->InsertDSBlock(dsblock);
      }
      m_mediator.m_node->UpdateDSCommiteeComposition(mutable_ds_comm, dsblock);
      totalIndex++;

    } else if (typeid(VCBlock) == dirBlock.type()) {
      const auto& vcblock = get<VCBlock>(dirBlock);

      if (vcblock.GetHeader().GetVieWChangeDSEpochNo() != prevdsblocknum + 1) {
        LOG_GENERAL(WARNING,
                    "VC block ds epoch number does not match the number being "
                    "processed "
                        << prevdsblocknum << " "
                        << vcblock.GetHeader().GetVieWChangeDSEpochNo());
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

      m_mediator.m_node->UpdateRetrieveDSCommiteeCompositionAfterVC(
          vcblock, mutable_ds_comm);
      m_mediator.m_blocklinkchain.AddBlockLink(totalIndex, prevdsblocknum + 1,
                                               BlockType::VC,
                                               vcblock.GetBlockHash());
      bytes vcblockserialized;
      vcblock.Serialize(vcblockserialized, 0);
      BlockStorage::GetBlockStorage().PutVCBlock(vcblock.GetBlockHash(),
                                                 vcblockserialized);
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

      ShardingHash shardinghash;
      if (!Messenger::GetShardingStructureHash(shards, shardinghash)) {
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
      BlockStorage::GetBlockStorage().PutFallbackBlock(
          fallbackblock.GetBlockHash(), fallbackblockser);
      totalIndex++;
    } else {
      LOG_GENERAL(WARNING, "dirBlock type unexpected ");
    }
  }

  newDSComm = move(mutable_ds_comm);
  return ret;
}

ValidatorBase::TxBlockValidationMsg Validator::CheckTxBlocks(
    const vector<TxBlock>& txBlocks, const deque<pair<PubKey, Peer>>& dsComm,
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
      LOG_GENERAL(WARNING, "Latest Tx Block fetched is stale ");
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