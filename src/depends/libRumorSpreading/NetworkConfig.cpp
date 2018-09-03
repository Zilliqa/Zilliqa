#include <cmath>
#include "NetworkConfig.h"

namespace RRS {

// CONSTRUCTORS
NetworkConfig::NetworkConfig(size_t numOfPeers)
: m_networkSize(numOfPeers)
, m_maxRoundsInB()
, m_maxRoundsInC()
, m_maxRoundsTotal()
{
    // Refer to "Randomized Rumor Spreading" paper
    int magicNumber = static_cast<int>(std::ceil(std::log(std::log(m_networkSize))));
    m_maxRoundsInB = std::max(1, magicNumber);
    m_maxRoundsInC = m_maxRoundsInB;
    m_maxRoundsTotal = static_cast<int>(std::ceil(std::log(m_networkSize)));
}

NetworkConfig::NetworkConfig(size_t networkSize,
                             int maxRoundsInB,
                             int maxRoundsInC,
                             int maxRoundsTotal)
: m_networkSize(networkSize)
, m_maxRoundsInB(maxRoundsInB)
, m_maxRoundsInC(maxRoundsInC)
, m_maxRoundsTotal(maxRoundsTotal)
{}

// PUBLIC CONST METHODS
size_t NetworkConfig::networkSize() const
{
    return m_networkSize;
}

int NetworkConfig::maxRoundsInB() const
{
    return m_maxRoundsInB;
}

int NetworkConfig::maxRoundsInC() const
{
    return m_maxRoundsInC;
}

int NetworkConfig::maxRoundsTotal() const
{
    return m_maxRoundsTotal;
}

bool NetworkConfig::operator==(const NetworkConfig& other) const
{
    return  m_networkSize == other.m_networkSize &&
            m_maxRoundsInB == other.m_maxRoundsInB &&
            m_maxRoundsInC == other.m_maxRoundsInC &&
            m_maxRoundsTotal == other.m_maxRoundsTotal;
}

} // project namespace