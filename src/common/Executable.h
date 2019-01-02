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

#ifndef __EXECUTABLE_H__
#define __EXECUTABLE_H__

#include <vector>
#include "libNetwork/Peer.h"

/// Specifies the interface required for classes that process messages.
class Executable {
 public:
  /// Message processing function.
  virtual bool Execute(const bytes& message, unsigned int offset,
                       const Peer& from) = 0;

  /// Virtual destructor.
  virtual ~Executable() {}
};

#endif  // __EXECUTABLE_H__
