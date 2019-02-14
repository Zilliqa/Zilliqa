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

#ifndef __SYNCHRONIZER_H__
#define __SYNCHRONIZER_H__

#include "libData/BlockChainData/BlockChain.h"
#include "libLookup/Lookup.h"
#include "libNetwork/Peer.h"
#include "libUtils/Logger.h"

class Synchronizer {
  DSBlock ConstructGenesisDSBlock();
  bool AddGenesisDSBlockToBlockChain(DSBlockChain& dsBlockChain,
                                     const DSBlock& dsBlock);
  bool InitializeGenesisDSBlock(DSBlockChain& dsBlockChain);

  TxBlock ConstructGenesisTxBlock();
  bool AddGenesisTxBlockToBlockChain(TxBlockChain& txBlockChain,
                                     const TxBlock& txBlock);
  bool InitializeGenesisTxBlock(TxBlockChain& txBlockChain);

 public:
  bool InitializeGenesisBlocks(DSBlockChain& dsBlockChain,
                               TxBlockChain& txBlockChain);

  bool FetchDSInfo(Lookup* lookup);
  bool FetchLatestDSBlocks(Lookup* lookup, uint64_t currentBlockChainSize);
  bool FetchLatestDSBlocksSeed(Lookup* lookup, uint64_t currentBlockChainSize);
  bool FetchLatestTxBlocks(Lookup* lookup, uint64_t currentBlockChainSize);
  bool AttemptPoW(Lookup* lookup);
  bool FetchOfflineLookups(Lookup* lookup);

  bool FetchLatestTxBlockSeed(Lookup* lookup, uint64_t currentBlockChainSize);
};

#endif  // __SYNCHRONIZER_H__
