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

#include "NetworkConfig.h"
#include <cmath>

namespace RRS {

// CONSTRUCTORS
NetworkConfig::NetworkConfig(size_t numOfPeers)
    : m_networkSize(numOfPeers),
      m_maxRoundsInB(),
      m_maxRoundsInC(),
      m_maxRoundsTotal() {
  // Refer to "Randomized Rumor Spreading" paper
  int magicNumber =
      static_cast<int>(std::ceil(std::log(std::log(m_networkSize))));
  m_maxRoundsInB = std::max(1, magicNumber);
  m_maxRoundsInC = m_maxRoundsInB;
  m_maxRoundsTotal = static_cast<int>(std::ceil(std::log(m_networkSize)));
}

NetworkConfig::NetworkConfig(size_t networkSize, int maxRoundsInB,
                             int maxRoundsInC, int maxRoundsTotal)
    : m_networkSize(networkSize),
      m_maxRoundsInB(maxRoundsInB),
      m_maxRoundsInC(maxRoundsInC),
      m_maxRoundsTotal(maxRoundsTotal) {}

// PUBLIC CONST METHODS
size_t NetworkConfig::networkSize() const { return m_networkSize; }

int NetworkConfig::maxRoundsInB() const { return m_maxRoundsInB; }

int NetworkConfig::maxRoundsInC() const { return m_maxRoundsInC; }

int NetworkConfig::maxRoundsTotal() const { return m_maxRoundsTotal; }

}  // namespace RRS