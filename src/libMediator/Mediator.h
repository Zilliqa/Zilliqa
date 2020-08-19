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

#ifndef ZILLIQA_SRC_LIBMEDIATOR_MEDIATOR_H_
#define ZILLIQA_SRC_LIBMEDIATOR_MEDIATOR_H_

#include <atomic>
#include <deque>

#include <Schnorr.h>
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
  PairOfKey m_selfKey;

  /// The Zilliqa instance's IP information.
  Peer m_selfPeer;

  /// The reference to the DirectoryService instance.
  DirectoryService* m_ds;

  /// The reference to the Node instance.
  Node* m_node;

  /// The reference to the Lookup instance.
  Lookup* m_lookup;

  /// Pointer to the Validator instance.
  Validator* m_validator;

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
  std::shared_ptr<DequeOfNode> m_DSCommittee;
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

  /// Prevent node from mining PoW at the next DS epoch
  std::atomic<bool> m_disablePoW;

  /// Prevent transactions from being created, forwarded, and dispatched
  static std::atomic<bool> m_disableTxns;

  /// ValidateDB state, used by StatusServer
  std::atomic<ValidateState> m_validateState;

  /// Constructor.
  Mediator(const PairOfKey& key, const Peer& peer);

  /// Destructor.
  ~Mediator();

  /// Sets the references to the subclass instances.
  void RegisterColleagues(DirectoryService* ds, Node* node, Lookup* lookup,
                          Validator* validator);

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

  void SetupLogLevel();

  bool ToProcessTransaction();
};

#endif  // ZILLIQA_SRC_LIBMEDIATOR_MEDIATOR_H_
