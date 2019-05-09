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

#include "Synchronizer.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

DSBlock Synchronizer::ConstructGenesisDSBlock() {
  BlockHash prevHash;

  for (unsigned int i = 0; i < prevHash.asArray().size(); i++) {
    prevHash.asArray().at(i) = i + 1;
  }

  bytes tmppubkey;
  DataConversion::HexStrToUint8Vec(GENESIS_PUBKEY, tmppubkey);

  PubKey pubKey(tmppubkey, 0);
  uint64_t genesisBlockNumer = 0;
  uint64_t genesisEpochNumer = 0;
  std::map<PubKey, Peer> powDSWinners;

  DSBlock dsBlock(
      DSBlockHeader(DS_POW_DIFFICULTY, POW_DIFFICULTY, pubKey,
                    genesisBlockNumer, genesisEpochNumer, PRECISION_MIN_VALUE,
                    SWInfo(), powDSWinners, DSBlockHashSet(), DSBLOCK_VERSION,
                    CommitteeHash(), prevHash),
      CoSignatures());
  return dsBlock;
}

bool Synchronizer::AddGenesisDSBlockToBlockChain(DSBlockChain& dsBlockChain,
                                                 const DSBlock& dsBlock) {
  dsBlockChain.AddBlock(dsBlock);

  // Store DS Block to disk
  bytes serializedDSBlock;
  dsBlock.Serialize(serializedDSBlock, 0);
  if (!BlockStorage::GetBlockStorage().PutDSBlock(
          dsBlock.GetHeader().GetBlockNum(), serializedDSBlock)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutDSBlock failed " << dsBlock);
    return false;
  }

  return true;
}

bool Synchronizer::InitializeGenesisDSBlock(DSBlockChain& dsBlockChain) {
  DSBlock dsBlock = ConstructGenesisDSBlock();
  return AddGenesisDSBlockToBlockChain(dsBlockChain, dsBlock);
}

TxBlock Synchronizer::ConstructGenesisTxBlock() {
  bytes tmppubkey;
  DataConversion::HexStrToUint8Vec(GENESIS_PUBKEY, tmppubkey);

  PubKey pubKey(tmppubkey, 0);
  TxBlock txBlock(TxBlockHeader(1, 1, 1, 0, TxBlockHashSet(), 0, pubKey, 0,
                                TXBLOCK_VERSION, CommitteeHash(), BlockHash()),
                  vector<MicroBlockInfo>(), CoSignatures());
  return txBlock;
}

bool Synchronizer::AddGenesisTxBlockToBlockChain(TxBlockChain& txBlockChain,
                                                 const TxBlock& txBlock) {
  txBlockChain.AddBlock(txBlock);

  // Store Tx Block to disk
  bytes serializedTxBlock;
  txBlock.Serialize(serializedTxBlock, 0);
  if (!BlockStorage::GetBlockStorage().PutTxBlock(
          txBlock.GetHeader().GetBlockNum(), serializedTxBlock)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
    return false;
  }
  return true;
}

bool Synchronizer::InitializeGenesisTxBlock(TxBlockChain& txBlockChain) {
  TxBlock txBlock = ConstructGenesisTxBlock();
  return AddGenesisTxBlockToBlockChain(txBlockChain, txBlock);
}

bool Synchronizer::InitializeGenesisBlocks(DSBlockChain& dsBlockChain,
                                           TxBlockChain& txBlockChain) {
  LOG_MARKER();
  return InitializeGenesisDSBlock(dsBlockChain) &&
         InitializeGenesisTxBlock(txBlockChain);
}

bool Synchronizer::FetchDSInfo(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchDSInfo not expected to be called from "
                "LookUp node.");
    return true;
  }

  return lookup->GetDSInfoFromLookupNodes();
}

bool Synchronizer::FetchLatestDSBlocks(Lookup* lookup,
                                       uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestDSBlocks not expected to be "
                "called from LookUp node.");
    return true;
  }

  return lookup->GetDSBlockFromLookupNodes(currentBlockChainSize, 0);
}

bool Synchronizer::FetchLatestDSBlocksSeed(Lookup* lookup,
                                           uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestDSBlocksSeed not expected to be "
                "called from LookUp node.");
    return true;
  }

  return lookup->GetDSBlockFromSeedNodes(currentBlockChainSize, 0);
}

bool Synchronizer::FetchLatestTxBlocks(Lookup* lookup,
                                       uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestTxBlocks not expected to be "
                "called from LookUp node.");
    return true;
  }

  return lookup->GetTxBlockFromLookupNodes(currentBlockChainSize, 0);
}

bool Synchronizer::FetchLatestTxBlockSeed(Lookup* lookup,
                                          uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestTxBlocksSeed not expected to be "
                "called from LookUp node.");
    return true;
  }

  return lookup->GetTxBlockFromSeedNodes(currentBlockChainSize, 0);
}

bool Synchronizer::AttemptPoW(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::AttemptPoW not expected to be called from "
                "LookUp node.");
    return true;
  }

  if (lookup->InitMining(uint32_t() - 1)) {
    LOG_GENERAL(INFO, "new node attempted pow");
    return true;
  } else {
    LOG_GENERAL(INFO, "new node did not attempt pow")
    return false;
  }
}

bool Synchronizer::FetchOfflineLookups(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchOfflineLookups not expected to be "
                "called from LookUp node.");
    return true;
  }

  return lookup->GetOfflineLookupNodes();
}
