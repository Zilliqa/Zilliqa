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
