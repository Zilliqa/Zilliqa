#ifndef RANDOMIZEDRUMORSPREADING_RumorHolder_H
#define RANDOMIZEDRUMORSPREADING_RumorHolder_H

#include <functional>
#include <map>
#include <mutex>
#include <unordered_set>

#include "MemberID.h"
#include "NetworkConfig.h"
#include "RumorSpreadingInterface.h"
#include "RumorStateMachine.h"

namespace RRS
{

    // This is a thread-safe implementation of the 'RumorSpreadingInterface'.
    class RumorHolder : public RumorSpreadingInterface
    {
    public:
        // TYPES
        typedef std::function<int()> NextMemberCb;

        // ENUMS
        enum class StatisticKey
        {
            NumPeers,
            NumMessagesReceived,
            Rounds,
            NumPushMessages,
            NumEmptyPushMessages,
            NumPullMessages,
            NumEmptyPullMessages,
        };

        static std::map<StatisticKey, std::string> s_enumKeyToString;

    private:
        // MEMBERS
        const int m_id;
        NetworkConfig m_networkConfig;
        std::vector<int> m_peers;
        std::unordered_set<int> m_peersInCurrentRound;
        std::unordered_map<int, RumorStateMachine> m_rumors;
        mutable std::mutex m_mutex;
        NextMemberCb m_nextMemberCb;
        std::unordered_set<int> m_nonPriorPeers;
        std::map<StatisticKey, double> m_statistics;

        // METHODS
        // Copy the member ids into a vector
        void toVector(const std::unordered_set<int>& peers);

        // Return a randomly selected member id
        int chooseRandomMember();

        // Add the specified 'value' to the previous statistic value
        void increaseStatValue(StatisticKey key, double value);

    public:
        // CONSTRUCTORS
        /// Create an instance which automatically figures out the network parameters.
        RumorHolder(const std::unordered_set<int>& peers,
                    int id = MemberID::next());
        RumorHolder(const std::unordered_set<int>& peers,
                    const NextMemberCb& cb, int id = MemberID::next());

        /// Used for manually passed network parameters.
        RumorHolder(const std::unordered_set<int>& peers,
                    const NetworkConfig& networkConfig,
                    int id = MemberID::next());
        RumorHolder(const std::unordered_set<int>& peers,
                    const NetworkConfig& networkConfig, const NextMemberCb& cb,
                    int id = MemberID::next());

        RumorHolder(const RumorHolder& other);

        RumorHolder(RumorHolder&& other) noexcept;

        // METHODS
        bool addRumor(int rumorId) override;

        std::pair<int, std::vector<Message>>
        receivedMessage(const Message& message, int fromPeer) override;

        std::pair<int, std::vector<Message>> advanceRound() override;

        // CONST METHODS
        int id() const;

        const NetworkConfig& networkConfig() const;

        const std::unordered_map<int, RumorStateMachine>& rumorsMap() const;

        bool rumorExists(int rumorId) const;

        bool isOld(int rumorId) const;

        const std::map<StatisticKey, double>& statistics() const;

        std::ostream& printStatistics(std::ostream& outStream) const;

        bool operator==(const RumorHolder& other) const;
    };

    // Required by std::unordered_set
    struct MemberHash
    {
        int operator()(const RumorHolder& obj) const;
    };

} // project namespace

#endif //RANDOMIZEDRUMORSPREADING_RumorHolder_H
