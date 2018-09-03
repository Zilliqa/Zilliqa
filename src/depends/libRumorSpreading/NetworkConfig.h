#ifndef RANDOMIZEDRUMORSPREADING_NETWORKSTATE_H
#define RANDOMIZEDRUMORSPREADING_NETWORKSTATE_H

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
     * The maximum number of rounds. This is termination condition for a given rumor.
     * Once a peer reaches this number of rounds it will advance to state D (OLD).
     * Specified in the paper as `O(ln(n))`.
     * Can be configured.
     */
    int m_maxRoundsTotal;

  public:
    // CONSTRUCTORS
    /// Create a NetworkConfig instance with the default initialization based on theory.
    explicit NetworkConfig(size_t numOfPeers);

    /// Create a NetworkConfig with user specified configuration.
    NetworkConfig(size_t networkSize, int maxRoundsInB, int maxRoundsInC, int maxRoundsTotal);

    // CONST METHODS
    size_t networkSize() const;

    int maxRoundsInB() const;

    int maxRoundsInC() const;

    int maxRoundsTotal() const;

    // OPERATORS
    bool operator==(const NetworkConfig& other) const;
};

} // project namespace

#endif //RANDOMIZEDRUMORSPREADING_NETWORKSTATE_H
