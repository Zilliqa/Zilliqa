#include "RumorManager.h"

#include <chrono>
#include <thread>

#include "P2PComm.h"
#include "common/Messages.h"

const unsigned char START_BYTE_GOSSIP = 0x33;

namespace
{
    const std::chrono::milliseconds ROUND_TIME(500);

    Message::Type convertType(uint8_t type)
    {
        switch (type)
        {
        case 1:
            return Message::Type::PUSH;
        case 2:
            return Message::Type::PULL;
        case 3:
            return Message::Type::EMPTY_PUSH;
        case 4:
            return Message::Type::EMPTY_PULL;
        default:
            return Message::Type::UNDEFINED;
        }
    }

} // anonymous namespace

// PRIVATE METHODS

void RumorManager::startRounds()
{
    // critical section
    std::thread([&]() {
        std::unique_lock<std::mutex> guard(m_continueRoundMutex);
        m_continueRound = true;
        while (m_continueRound)
        {
            { // critical section
                std::lock_guard<std::mutex> guard(m_mutex);

                std::pair<int, std::vector<Message>> result
                    = m_member->advanceRound();

                // Get the corresponding Peer to which to send Push Messages if any.
                auto l = m_peerIdPeerBimap.left.find(result.first);
                if (l != m_peerIdPeerBimap.left.end())
                {
                    SendMessages(l->second, result.second);
                }
            } // end critical section
            std::this_thread::sleep_for(ROUND_TIME - std::chrono::seconds(15));
            m_condStopRound.wait_for(guard, std::chrono::seconds(15));
        }
    })
        .detach();
}

// CONSTRUCTORS
RumorManager::RumorManager() {}

RumorManager::~RumorManager() {}

void RumorManager::stopRounds()
{
    {
        std::lock_guard<std::mutex> guard(m_continueRoundMutex);
        m_continueRound = false;
    }
    m_condStopRound.notify_one();
}

// PUBLIC METHODS
void RumorManager::Initialize(const std::vector<Peer>& peers)
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    m_rumorIdGenerator = 0;
    m_peerIdPeerBimap.clear();
    m_rumorIdRumorBimap.clear();
    m_peerIdSet.clear();

    int peerIdGenerator = 0;
    for (const auto& p : peers)
    {
        ++peerIdGenerator;
        m_peerIdPeerBimap.insert(
            PeerIdPeerBiMap::value_type(peerIdGenerator, p));
        m_peerIdSet.insert(peerIdGenerator);
    }

    // Now create the one and only RumorMember
    m_member.reset(new RumorMember(m_peerIdSet, 0));
}

bool RumorManager::addRumor(const RumorManager::RawBytes& message)
{
    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    if (m_peerIdSet.size() > 0)
    {
        return false;
    }

    m_rumorIdRumorBimap.insert(
        RumorIdRumorBimap::value_type(++m_rumorIdGenerator, message));

    m_member->addRumor(m_rumorIdGenerator);

    return true;
}

bool RumorManager::rumorReceived(uint8_t type, int32_t round,
                                 const RawBytes& message, const Peer& from)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    auto p = m_peerIdPeerBimap.right.find(from);
    if (p == m_peerIdPeerBimap.right.end())
    {
        // I dont know this peer, missing in my peerlist.
        return false;
    }

    int64_t recvdRumorId = -1;
    Message::Type t = convertType(type);
    bool toBeDispatched = false;
    if (t == Message::Type::EMPTY_PUSH || t == Message::Type::EMPTY_PULL)
    {
        /* Don't add it to local RumorMap because it's not the rumor itself */
    }
    else
    {
        auto it = m_rumorIdRumorBimap.right.find(message);
        if (it == m_rumorIdRumorBimap.right.end())
        {
            recvdRumorId = ++m_rumorIdGenerator;
            m_rumorIdRumorBimap.insert(
                RumorIdRumorBimap::value_type(recvdRumorId, message));
            toBeDispatched = true;
        }
        else // already received , pass it on to member for state calculations
        {
            recvdRumorId = it->second;
        }
    }

    Message recvMsg(t, recvdRumorId, round);
    int peerId = p->second;
    std::pair<int, std::vector<Message>> pullMsgs
        = m_member->receivedMessage(recvMsg, peerId);

    // Get the corresponding Peer to which to send Pull Messages if any.
    auto l = m_peerIdPeerBimap.left.find(pullMsgs.first);
    if (l != m_peerIdPeerBimap.left.end())
    {
        SendMessages(l->second, pullMsgs.second);
    }

    return toBeDispatched;
}

void RumorManager::SendMessages(const Peer& toPeer,
                                const std::vector<Message>& messages)
{
    // Get the real messages based on rumor ids.
    for (auto& k : messages)
    {
        // Add round and type
        RawBytes cmd = {(unsigned char)k.type()};
        unsigned int cur_offset = RRSMessageOffset::R_AGE;

        Serializable::SetNumber<uint32_t>(cmd, cur_offset, k.age(),
                                          sizeof(uint32_t));

        //Incase of empty pull/push there won't be message body
        auto m = m_rumorIdRumorBimap.left.find(k.rumorId());
        if (m != m_rumorIdRumorBimap.left.end())
        {
            // Add raw message
            cmd.insert(cmd.end(), m->second.begin(), m->second.end());
        }

        // Send the message to peer .
        P2PComm::GetInstance().SendMessageNoQueue(toPeer, cmd,
                                                  START_BYTE_GOSSIP);
    }
}

// PUBLIC CONST METHODS
const RumorManager::RumorIdRumorBimap& RumorManager::rumors() const
{
    return m_rumorIdRumorBimap;
}
