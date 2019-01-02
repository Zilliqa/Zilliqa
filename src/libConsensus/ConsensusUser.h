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

#ifndef __CONSENSUSUSER_H__
#define __CONSENSUSUSER_H__

#include <shared_mutex>
#include "Consensus.h"
#include "common/Broadcastable.h"
#include "common/Executable.h"

/// [TEST ONLY] Internal class for testing consensus.
class ConsensusUser : public Executable, public Broadcastable {
 private:
  bool ProcessSetLeader(const bytes& message, unsigned int offset,
                        const Peer& from);
  bool ProcessStartConsensus(const bytes& message, unsigned int offset,
                             const Peer& from);
  bool ProcessConsensusMessage(const bytes& message, unsigned int offset,
                               const Peer& from);

  std::pair<PrivKey, PubKey> m_selfKey;
  Peer m_selfPeer;
  bool m_leaderOrBackup;  // false = leader, true = backup
  std::shared_ptr<ConsensusCommon> m_consensus;

  std::mutex m_mutexProcessConsensusMessage;
  std::condition_variable cv_processConsensusMessage;

 public:
  enum InstructionType : unsigned char {
    SETLEADER = 0x00,
    STARTCONSENSUS = 0x01,
    CONSENSUS = 0x02  // These are messages that ConsensusLeader or
                      // ConsensusBackup will process (transparent to user)
  };

  ConsensusUser(const std::pair<PrivKey, PubKey>& key, const Peer& peer);
  ~ConsensusUser();

  bool Execute(const bytes& message, unsigned int offset, const Peer& from);

  bool MyMsgValidatorFunc(const bytes& message,
                          bytes& errorMsg);  // Needed by backup
};

#endif  // __CONSENSUSUSER_H__
