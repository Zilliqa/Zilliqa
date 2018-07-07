#include "RumorMember.h"

#include <random>
#include <cassert>

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<RumorMember::StatisticKey, std::string> RumorMember::s_enumKeyToString = {
    {NumPeers,             LITERAL(NumPeers)},
    {NumMessagesReceived,  LITERAL(NumMessagesReceived)},
    {Rounds,               LITERAL(Rounds)},
    {NumPushMessages,      LITERAL(NumPushMessages)},
    {NumEmptyPushMessages, LITERAL(NumEmptyPushMessages)},
    {NumPullMessages,      LITERAL(NumPullMessages)},
    {NumEmptyPullMessages, LITERAL(NumEmptyPullMessages)},
};

// PRIVATE METHODS
void RumorMember::toVector(const std::unordered_set<int>& peers)
{
    for (const int p : peers) {
        if (p != m_id) {
            m_peers.push_back(p);
        }
    }
    increaseStatValue(StatisticKey::NumPeers, peers.size() - 1);
}

int RumorMember::chooseRandomMember()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, static_cast<int>(m_peers.size() - 1));
    return m_peers[dis(gen)];
}

void RumorMember::increaseStatValue(StatisticKey key, double value)
{
    if (m_statistics.count(key) <= 0) {
        m_statistics[key] = value;
    }
    else {
        m_statistics[key] += value;
    }
}

// CONSTRUCTORS
RumorMember::RumorMember(const std::unordered_set<int>& peers, int id)
: m_id(id)
, m_networkConfig(peers.size())
, m_peers()
, m_rumors()
, m_mutex()
, m_nextMemberCb()
{
    toVector(peers);
}

RumorMember::RumorMember(const std::unordered_set<int>& peers, const NextMemberCb& cb, int id)
: m_id(id)
  , m_networkConfig(peers.size())
  , m_peers()
  , m_rumors()
  , m_mutex()
  , m_nextMemberCb(cb)
{
    toVector(peers);
}


RumorMember::RumorMember(const std::unordered_set<int>& peers,
                         const NetworkConfig& networkConfig,
                         int id)
: m_id(id)
, m_networkConfig(networkConfig)
, m_peers()
, m_rumors()
, m_mutex()
, m_nextMemberCb()
, m_statistics()
{
    assert(networkConfig.networkSize() == peers.size());
    toVector(peers);
}

RumorMember::RumorMember(const std::unordered_set<int>& peers,
                         const NetworkConfig& networkConfig,
                         const NextMemberCb& cb,
                         int id)
: m_id(id)
, m_networkConfig(networkConfig)
, m_peers()
, m_rumors()
, m_mutex()
, m_nextMemberCb(cb)
, m_statistics()
{
    assert(networkConfig.networkSize() == peers.size());
    toVector(peers);
}

// COPY CONSTRUCTOR
RumorMember::RumorMember(const RumorMember& other)
: m_id(other.m_id)
, m_networkConfig(other.m_networkConfig)
, m_peers(other.m_peers)
, m_rumors(other.m_rumors)
, m_mutex()
, m_nextMemberCb(other.m_nextMemberCb)
, m_statistics(other.m_statistics)
{
}

// MOVE CONSTRUCTOR
RumorMember::RumorMember(RumorMember&& other) noexcept
: m_id(other.m_id)
, m_networkConfig(other.m_networkConfig)
, m_peers(std::move(other.m_peers))
, m_rumors(std::move(other.m_rumors))
, m_mutex()
, m_nextMemberCb(std::move(other.m_nextMemberCb))
, m_statistics(std::move(other.m_statistics))
{
}

// PUBLIC METHODS
bool RumorMember::addRumor(int rumorId)
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section
    return m_rumors.insert(std::make_pair(rumorId, &m_networkConfig)).second;
}

std::pair<int, std::vector<Message>>
RumorMember::receivedMessage(const Message& message, int fromPeer)
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    bool isNewPeer = m_peersInCurrentRound.insert(fromPeer).second;
    increaseStatValue(NumMessagesReceived, 1);

    std::vector<Message> pullMessages;
    if (isNewPeer && message.type() == Message::PUSH) {
        for (auto& kv : m_rumors) {
            RumorStateMachine& stateMach = kv.second;
            if (stateMach.age() >= 0) {
                pullMessages.emplace_back(Message(Message::PULL, kv.first, kv.second.age()));
            }
        }

        // No PULL messages to sent i.e. no rumors received yet
        if (pullMessages.empty()) {
            pullMessages.emplace_back(Message(Message::PULL, -1, 0));
            increaseStatValue(NumEmptyPullMessages, 1);
        }
        else {
            increaseStatValue(NumPullMessages, pullMessages.size());
        }
    }

    // An empty response from a peer that was sent a PULL
    const int receivedRumorId = message.rumorId();
    const int theirRound = message.age();
    if (receivedRumorId >= 0) {
        if (m_rumors.count(receivedRumorId) > 0) {
            m_rumors[receivedRumorId].rumorReceived(fromPeer, message.age());
        }
        else {
            m_rumors[receivedRumorId] = RumorStateMachine(&m_networkConfig, fromPeer, theirRound);
        }
    }

    return std::make_pair(fromPeer, pullMessages);
}

std::pair<int, std::vector<Message>> RumorMember::advanceRound()
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    if(m_rumors.empty()) {
        return {-1, std::vector<Message>()};
    }

    increaseStatValue(Rounds, 1);

    int toMember = m_nextMemberCb ? m_nextMemberCb() : chooseRandomMember();

    // Construct the push messages
    std::vector<Message> pushMessages;
    for (auto& kv : m_rumors) {
        RumorStateMachine& stateMach = kv.second;
        stateMach.advanceRound(m_peersInCurrentRound);
        pushMessages.emplace_back(Message(Message::PUSH, kv.first, kv.second.age()));
    }
    increaseStatValue(NumPushMessages, pushMessages.size());

    // No PUSH messages but still want to sent a response to peer.
    if (pushMessages.empty()) {
        pushMessages.emplace_back(Message(Message::PUSH, -1, 0));
        increaseStatValue(NumEmptyPushMessages, 1);
    }

    // Clear round state
    m_peersInCurrentRound.clear();

    return std::make_pair(toMember, pushMessages);
}

// PUBLIC CONST METHODS
int RumorMember::id() const
{
    return m_id;
}

const std::unordered_map<int, RumorStateMachine>& RumorMember::rumorsMap() const
{
    return m_rumors;
}

const std::map<RumorMember::StatisticKey, double>& RumorMember::statistics() const
{
    return m_statistics;
}

bool RumorMember::rumorExists(int rumorId) const
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section
    return m_rumors.count(rumorId) > 0;
}

bool RumorMember::isOld(int rumorId) const
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    const auto& iter = m_rumors.find(rumorId);
    if (iter != m_rumors.end()) {
        return iter->second.isOld();
    }

    return false;
}

std::ostream& RumorMember::printStatistics(std::ostream& outStream) const
{
    outStream << m_id << ": {" << "\n";
    for (const auto& stat : m_statistics) {
        outStream << "  " << s_enumKeyToString.at(stat.first) << ": " << stat.second << "\n";
    }
    outStream << "}";
    return outStream;
}

// OPERATORS
bool RumorMember::operator==(const RumorMember& other) const
{
    return m_id == other.m_id;
}

// FREE OPERATORS
int MemberHash::operator()(const RRS::RumorMember& obj) const
{
    return obj.id();
}

} // project namespace