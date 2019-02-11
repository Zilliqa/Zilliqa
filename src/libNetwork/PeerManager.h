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

#ifndef __PEERMANAGER_H__
#define __PEERMANAGER_H__

#include <array>
#include <vector>

#include "common/Executable.h"

/// Processes messages related to PeerStore management.
class PeerManager : public Executable {
  PairOfKey m_selfKey;
  Peer m_selfPeer;

  bool ProcessAddPeer(const bytes& message, unsigned int offset,
                      const Peer& from);
  void SetupLogLevel();

 public:
  enum InstructionType : unsigned char {
    HELLO = 0x00,
    ADDPEER = 0x01,
  };

  /// Constructor.
  PeerManager(const PairOfKey& key, const Peer& peer, bool loadConfig);

  /// Destructor.
  ~PeerManager();

  /// Implements the Execute function inherited from Executable.
  bool Execute(const bytes& message, unsigned int offset, const Peer& from);
};

#endif  // __PEERMANAGER_H__
