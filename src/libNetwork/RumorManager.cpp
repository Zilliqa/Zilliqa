#include "RumorManager.h"

#include <chrono>
#include <thread>

// TODO: would be nice to decouple from this header
#include "P2PComm.h"
#include "common/Messages.h"

namespace
{
    // TODO: allow them to passed in as parameters
    const std::chrono::milliseconds ROUND_TIME(500);

    Message::Type convertType(uint32_t type)
    {
        switch (type)
        {
        case 1:
            return Message::PUSH;
        case 2:
            return Message::PUSH;
        default:
            return Message::UNDEFINED;
        }
    }

} // anonymous namespace

// PRIVATE METHODS
template<typename Container>
void RumorManager::addRumorImp(const Container& peers,
                               const RumorManager::RawBytes& message)
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    int rumorId = m_rumorIdGenerator++;
    m_rumorIdBimap.insert(RumorBimap::value_type(rumorId, message));

    int peerIdGenerator = 0;
    std::unordered_set<int> peerSet;
    PeerBimap peerBimap;
    for (const auto& p : peers)
    {
        peerBimap.insert(PeerBimap::value_type(peerIdGenerator++, p));
        peerSet.insert(peerIdGenerator++);
    }

    m_rumorState.insert({rumorId, {RumorMember(peerSet), peerBimap}});

    scheduleRounds(rumorId);
}

void RumorManager::scheduleRounds(int rumorId)
{
    // critical section
    auto it = m_rumorState.find(rumorId);
    if (it == m_rumorState.end())
    {
        return;
    }
    const RumorMember& member = it->second.m_member;
    int numOfRounds = member.networkConfig().maxRoundsTotal();

    std::thread([&]() {
        while (numOfRounds--)
        {
            { // critical section
                std::lock_guard<std::mutex> guard(m_mutex);

                if (m_rumorState.count(rumorId) <= 0)
                {
                    return;
                }

                RumorState& state = m_rumorState[rumorId];
                RumorMember& member = state.m_member;

                std::pair<int, std::vector<Message>> result
                    = member.advanceRound();

                const PeerBimap peerBimap = state.m_peerIdBimap;
                const Peer& peer = peerBimap.left.at(result.first);

                const RawBytes& rumorMessage = m_rumorIdBimap.left.at(rumorId);
                // TODO: add type and round
                P2PComm::GetInstance().SendMessageCore(
                    peer, rumorMessage, HeaderStartByte::GOSSIP, RawBytes());
            } // end critical section
            std::this_thread::sleep_for(ROUND_TIME);
        }
    })
        .detach();

    std::thread([&]() {
        std::this_thread::sleep_for(numOfRounds * ROUND_TIME);
        std::lock_guard<std::mutex> guard(m_mutex); // critical section
        m_rumorState.erase(rumorId);
    })
        .detach();
}

// CONSTRUCTORS
RumorManager::RumorManager()
    : m_rumorIdGenerator(0)
    , m_rumorIdBimap()
    , m_rumorState()
{
}

RumorManager::~RumorManager() {}

// PUBLIC METHODS
void RumorManager::addRumor(const std::vector<Peer>& peers,
                            const RawBytes& message)
{
    addRumorImp(peers, message);
}

void RumorManager::addRumor(const std::deque<Peer>& peers,
                            const RawBytes& message)
{
    addRumorImp(peers, message);
}

void RumorManager::rumorReceived(uint8_t type, int32_t round,
                                 const RawBytes& message, const Peer& from)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    int recvRumorId = m_rumorIdBimap.right.at(message);

    auto it = m_rumorState.find(recvRumorId);
    if (it == m_rumorState.end())
    {
        return;
    }

    PeerBimap& peerBimap = it->second.m_peerIdBimap;
    int peerId = peerBimap.right.at(from);

    RumorMember& member = it->second.m_member;

    Message recvMsg(convertType(type), recvRumorId, round);
    member.receivedMessage(recvMsg, peerId);
    // TODO handle PULL messages
}

// PUBLIC CONST METHODS
const RumorManager::RumorBimap& RumorManager::rumors() const
{
    return m_rumorIdBimap;
}
