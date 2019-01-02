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

#ifndef __MEDIATOR_H__
#define __MEDIATOR_H__

#include <deque>

#include "libArchival/Archival.h"
#include "libArchival/BaseDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockChainData/BlockChain.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libDirectoryService/DirectoryService.h"
#include "libLookup/Lookup.h"
#include "libNetwork/Peer.h"
#include "libNode/Node.h"
#include "libValidator/Validator.h"

/// A mediator class for providing access to global members.
class Mediator {
 public:
  /// The Zilliqa instance's key pair.
  std::pair<PrivKey, PubKey> m_selfKey;

  /// The Zilliqa instance's IP information.
  Peer m_selfPeer;

  /// The reference to the DirectoryService instance.
  DirectoryService* m_ds;

  /// The reference to the Node instance.
  Node* m_node;

  /// The reference to the Lookup instance.
  Lookup* m_lookup;

  /// Pointer to the Validator instance.
  ValidatorBase* m_validator;

  // Archive DB pointer
  BaseDB* m_archDB;

  // Archival Node pointer
  Archival* m_archival;
  /// The transient DS blockchain.
  DSBlockChain m_dsBlockChain;

  /// The transient Tx blockchain.
  TxBlockChain m_txBlockChain;

  /// IndexBlockChain for linking ds/vc/fb blocks
  BlockLinkChain m_blocklinkchain;

  /// The current epoch.
  uint64_t m_currentEpochNum = 0;

  /// The consensus ID
  uint32_t m_consensusID;

  // DS committee members
  // Fixed-sized double-ended queue depending on size of DS committee at
  // bootstrap Leader is at head of queue PoW winner will be pushed in at head
  // of queue (new leader) Oldest member will be pushed out from tail of queue

  /// The public keys and current members of the DS committee.
  std::shared_ptr<DequeOfDSNode> m_DSCommittee;
  std::mutex m_mutexDSCommittee;

  std::shared_ptr<std::vector<PubKey>> m_initialDSCommittee;
  std::mutex m_mutexInitialDSCommittee;

  /// The current epoch randomness from the DS blockchain.
  std::array<unsigned char, POW_SIZE> m_dsBlockRand;

  /// The current epoch randomness from the Tx blockchain.
  std::array<unsigned char, POW_SIZE> m_txBlockRand;

  /// To determine if the node successfully recovered from persistence
  bool m_isRetrievedHistory;

  /// Flag for indicating whether it's vacuous epoch now
  bool m_isVacuousEpoch;
  std::mutex m_mutexVacuousEpoch;

  /// Record current software information which already downloaded to this node
  SWInfo m_curSWInfo;
  std::mutex m_mutexCurSWInfo;

  /// Constructor.
  Mediator(const std::pair<PrivKey, PubKey>& key, const Peer& peer);

  /// Destructor.
  ~Mediator();

  /// Sets the references to the subclass instances.
  void RegisterColleagues(DirectoryService* ds, Node* node, Lookup* lookup,
                          ValidatorBase* validator, BaseDB* archDB = nullptr,
                          Archival* arch = nullptr);

  /// Updates the DS blockchain random for PoW.
  void UpdateDSBlockRand(bool isGenesis = false);

  /// Updates the Tx blockchain random for PoW.
  void UpdateTxBlockRand(bool isGenesis = false);

  std::string GetNodeMode(const Peer& peer);

  void IncreaseEpochNum();

  bool GetIsVacuousEpoch();

  bool GetIsVacuousEpoch(const uint64_t& epochNum);

  uint32_t GetShardSize(const bool& useShardStructure) const;

  bool CheckWhetherBlockIsLatest(const uint64_t& dsblockNum,
                                 const uint64_t& epochNum);
};

#endif  // __MEDIATOR_H__
