/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/

#include "Peer.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <functional>
#include <mutex>
#include <unordered_map>

class ReputationManager
{

    // Custom hasher
    // Ref: https://stackoverflow.com/questions/32082786/why-i-cannot-use-neither-stdunordered-map-nor-boostunordered-map-with-boost
    template<typename T> struct hash_str
    {
        size_t operator()(const T& t) const
        {
            return std::hash<std::string>()(t.str());
        }
    };

    ReputationManager();
    ~ReputationManager();

    // Singleton should not implement these
    ReputationManager(ReputationManager const&) = delete;
    void operator=(ReputationManager const&) = delete;

public:
    /// Returns the singleton P2PComm instance.
    static ReputationManager& GetInstance();
    void AddNodeIfNotKnown(boost::multiprecision::uint128_t IPAddress);
    bool IsNodeBanned(boost::multiprecision::uint128_t IPAddress);
    void PunishNode(boost::multiprecision::uint128_t IPAddress);
    void AwardNode(boost::multiprecision::uint128_t IPAddress);
    int32_t GetReputation(boost::multiprecision::uint128_t IPAddress);

    enum PenaltyType : int32_t
    {
        PENALTY_CONN_REFUSE = -5,
        PENALTY_INVALID_MESSAGE = -50
    };

    // Score
    static const int32_t UPPERREPTHRASHHOLD = 500;
    static const int32_t REPTHRASHHOLD = -500;
    static const int32_t GOOD = 0;
    static const int32_t BAN_MULTIPLIER = 24;

    // AWARD
    static const int32_t AWARD_FOR_GOOD_NODES = 50;

    std::mutex m_mutexReputations;

private:
    std::unordered_map<boost::multiprecision::uint128_t, int32_t,
                       hash_str<boost::multiprecision::uint128_t>>
        m_Reputations;
    void SetReputation(boost::multiprecision::uint128_t IPAddress,
                       int32_t ReputationScore);
    void UpdateReputation(boost::multiprecision::uint128_t IPAddress,
                          int32_t ReputationScoreDelta);
};