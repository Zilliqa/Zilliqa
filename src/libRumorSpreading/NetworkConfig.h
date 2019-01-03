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

#ifndef __NETWORKSTATE_H__
#define __NETWORKSTATE_H__

#include <string>
#include <unordered_map>

namespace RRS {

class NetworkConfig {
 private:
  // MEMBERS
  /// Number of peers
  size_t m_networkSize;

  /**
   * Maximum number of rounds while in state B (NEW).
   * Specified in the paper as `O(ln(ln(n)))`.
   * Can be configured.
   */
  int m_maxRoundsInB;

  /**
   * Maximum number of rounds while in state C (KNOWN).
   * Specified in the paper as `O(ln(ln(n)))`.
   * Can be configured.
   */
  int m_maxRoundsInC;

  /**
   * The maximum number of rounds. This is termination condition for a given
   * rumor. Once a peer reaches this number of rounds it will advance to state D
   * (OLD). Specified in the paper as `O(ln(n))`. Can be configured.
   */
  int m_maxRoundsTotal;

 public:
  // CONSTRUCTORS
  /// Create a NetworkConfig instance with the default initialization based on
  /// theory.
  explicit NetworkConfig(size_t numOfPeers);

  /// Create a NetworkConfig with user specified configuration.
  NetworkConfig(size_t networkSize, int maxRoundsInB, int maxRoundsInC,
                int maxRoundsTotal);

  // CONST METHODS
  size_t networkSize() const;

  int maxRoundsInB() const;

  int maxRoundsInC() const;

  int maxRoundsTotal() const;
};

}  // namespace RRS

#endif  //__NETWORKSTATE_H__
