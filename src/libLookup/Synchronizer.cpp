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

  bytes tmpprivkey = DataConversion::HexStrToUint8Vec(
      "BCCDF94ACEC5B6F1A2D96BDDC6CBE22F3C6DFD89FD791F18B722080A908253CD");
  bytes tmppubkey = DataConversion::HexStrToUint8Vec(
      "02AAE728127EB5A30B07D798D5236251808AD2C8BA3F18B230449D0C938969B552");
  // FIXME: Handle exceptions.
  PrivKey privKey(tmpprivkey, 0);
  PubKey pubKey(tmppubkey, 0);
  std::pair<PrivKey, PubKey> keypair = make_pair(privKey, pubKey);
  uint64_t genesisBlockNumer = 0;
  uint64_t genesisEpochNumer = 0;
  std::map<PubKey, Peer> powDSWinners;

  // FIXME: Handle exceptions.
  DSBlock dsBlock(
      DSBlockHeader(DS_POW_DIFFICULTY, POW_DIFFICULTY, prevHash, keypair.second,
                    genesisBlockNumer, genesisEpochNumer, PRECISION_MIN_VALUE,
                    SWInfo(), powDSWinners, DSBlockHashSet(), CommitteeHash()),
      CoSignatures());
  return dsBlock;
}

bool Synchronizer::AddGenesisDSBlockToBlockChain(DSBlockChain& dsBlockChain,
                                                 const DSBlock& dsBlock) {
  dsBlockChain.AddBlock(dsBlock);

  // Store DS Block to disk
  bytes serializedDSBlock;
  dsBlock.Serialize(serializedDSBlock, 0);
  BlockStorage::GetBlockStorage().PutDSBlock(dsBlock.GetHeader().GetBlockNum(),
                                             serializedDSBlock);

  return true;
}

bool Synchronizer::InitializeGenesisDSBlock(DSBlockChain& dsBlockChain) {
  DSBlock dsBlock = ConstructGenesisDSBlock();
  AddGenesisDSBlockToBlockChain(dsBlockChain, dsBlock);
  return true;
}

TxBlock Synchronizer::ConstructGenesisTxBlock() {
  bytes tmpprivkey = DataConversion::HexStrToUint8Vec(
      "BCCDF94ACEC5B6F1A2D96BDDC6CBE22F3C6DFD89FD791F18B722080A908253CD");
  bytes tmppubkey = DataConversion::HexStrToUint8Vec(
      "02AAE728127EB5A30B07D798D5236251808AD2C8BA3F18B230449D0C938969B552");
  // FIXME: Handle exceptions.
  PrivKey privKey(tmpprivkey, 0);
  PubKey pubKey(tmppubkey, 0);

  std::pair<PrivKey, PubKey> keypair = make_pair(privKey, pubKey);

  TxBlock txBlock(TxBlockHeader(TXBLOCKTYPE::FINAL, BLOCKVERSION::VERSION1, 1,
                                1, 1, BlockHash(), 0, TxBlockHashSet(), 0,
                                keypair.second, 0, CommitteeHash()),
                  vector<MicroBlockInfo>(), CoSignatures());
  return txBlock;
}

bool Synchronizer::AddGenesisTxBlockToBlockChain(TxBlockChain& txBlockChain,
                                                 const TxBlock& txBlock) {
  txBlockChain.AddBlock(txBlock);

  // Store Tx Block to disk
  bytes serializedTxBlock;
  txBlock.Serialize(serializedTxBlock, 0);
  BlockStorage::GetBlockStorage().PutTxBlock(txBlock.GetHeader().GetBlockNum(),
                                             serializedTxBlock);

  return true;
}

bool Synchronizer::InitializeGenesisTxBlock(TxBlockChain& txBlockChain) {
  TxBlock txBlock = ConstructGenesisTxBlock();
  AddGenesisTxBlockToBlockChain(txBlockChain, txBlock);

  return true;
}

bool Synchronizer::InitializeGenesisBlocks(DSBlockChain& dsBlockChain,
                                           TxBlockChain& txBlockChain) {
  LOG_MARKER();
  InitializeGenesisDSBlock(dsBlockChain);
  InitializeGenesisTxBlock(txBlockChain);

  return true;
}

bool Synchronizer::FetchDSInfo(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchDSInfo not expected to be called from "
                "LookUp node.");
    return true;
  }

  lookup->GetDSInfoFromLookupNodes();
  // lookup->GetDSInfoFromSeedNodes();
  return true;
}

bool Synchronizer::FetchLatestDSBlocks(Lookup* lookup,
                                       uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestDSBlocks not expected to be "
                "called from LookUp node.");
    return true;
  }

  lookup->GetDSBlockFromLookupNodes(currentBlockChainSize, 0);
  return true;
}

bool Synchronizer::FetchLatestDSBlocksSeed(Lookup* lookup,
                                           uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestDSBlocksSeed not expected to be "
                "called from LookUp node.");
    return true;
  }

  lookup->GetDSBlockFromSeedNodes(currentBlockChainSize, 0);
  return true;
}

bool Synchronizer::FetchLatestTxBlocks(Lookup* lookup,
                                       uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestTxBlocks not expected to be "
                "called from LookUp node.");
    return true;
  }

  lookup->GetTxBlockFromLookupNodes(currentBlockChainSize, 0);
  return true;
}

bool Synchronizer::FetchLatestTxBlockSeed(Lookup* lookup,
                                          uint64_t currentBlockChainSize) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestTxBlocksSeed not expected to be "
                "called from LookUp node.");
    return true;
  }

  lookup->GetTxBlockFromSeedNodes(currentBlockChainSize, 0);
  return true;
}

bool Synchronizer::FetchLatestState(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestState not expected to be called "
                "from LookUp node.");
    return true;
  }

  lookup->GetStateFromLookupNodes();
  return true;
}

bool Synchronizer::FetchLatestStateSeed(Lookup* lookup) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Synchronizer::FetchLatestStateSeed not expected to be called "
                "from LookUp node.");
    return true;
  }

  lookup->GetStateFromSeedNodes();
  return true;
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

  lookup->GetOfflineLookupNodes();
  return true;
}
