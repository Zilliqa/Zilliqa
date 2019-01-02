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
