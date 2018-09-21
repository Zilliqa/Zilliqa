#include "RumorManager.h"

#include <chrono>
#include <thread>

#include "P2PComm.h"
#include "common/Messages.h"

const unsigned char START_BYTE_GOSSIP = 0x33;

namespace
{
    RRS::Message::Type convertType(uint8_t type)
    {
        switch (type)
        {
        case 1:
            return RRS::Message::Type::PUSH;
        case 2:
            return RRS::Message::Type::PULL;
        case 3:
            return RRS::Message::Type::EMPTY_PUSH;
        case 4:
            return RRS::Message::Type::EMPTY_PULL;
        default:
            return RRS::Message::Type::UNDEFINED;
        }
    }

} // anonymous namespace

// CONSTRUCTORS
RumorManager::RumorManager()
    : m_peerIdPeerBimap()
    , m_peerIdSet()
    , m_rumorIdRumorBimap()
    , m_selfPeer()
    , m_rumorIdGenerator(0)
    , m_mutex()
    , m_continueRoundMutex()
    , m_continueRound(false)
    , m_condStopRound()
{
}

RumorManager::~RumorManager() {}

// PRIVATE METHODS

void RumorManager::startRounds()
{
    LOG_MARKER();

    std::thread([&]() {
        std::unique_lock<std::mutex> guard(m_continueRoundMutex);
        m_continueRound = true;
        while (true)
        {
            { // critical section
                std::lock_guard<std::mutex> guard(m_mutex);
                std::pair<std::vector<int>, std::vector<RRS::Message>> result
                    = m_rumorHolder->advanceRound();

                // Get the corresponding Peer to which to send Push Messages if any.
                for (const auto& i : result.first)
                {
                    auto l = m_peerIdPeerBimap.left.find(i);
                    if (l != m_peerIdPeerBimap.left.end())
                    {
                        LOG_GENERAL(DEBUG,
                                    "Sending " << result.second.size()
                                               << " push messages");
                        SendMessages(l->second, result.second);
                    }
                }
            } // end critical section
            if (m_condStopRound.wait_for(
                    guard, std::chrono::milliseconds(ROUND_TIME_IN_MS),
                    [&] { return !m_continueRound; }))
            {
                return;
            }
        }
    })
        .detach();
}

void RumorManager::stopRounds()
{
    LOG_MARKER();
    {
        std::lock_guard<std::mutex> guard(m_continueRoundMutex);
        m_continueRound = false;
    }
    m_condStopRound.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(ROUND_TIME_IN_MS));
}

// PUBLIC METHODS
bool RumorManager::Initialize(const std::vector<Peer>& peers,
                              const Peer& myself)
{
    LOG_MARKER();
    {
        std::lock_guard<std::mutex> guard(m_continueRoundMutex);
        if (m_continueRound)
        {
            // Seems logical error. Round should have been already Stopped.
            LOG_GENERAL(WARNING,
                        "Round is still running.. So won't re-initialize the "
                        "rumor mamager.");
            return false;
        }
    }

    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    m_rumorIdGenerator = 0;
    m_peerIdPeerBimap.clear();
    m_rumorIdRumorBimap.clear();
    m_peerIdSet.clear();
    m_selfPeer = myself;

    int peerIdGenerator = 0;
    for (const auto& p : peers)
    {
        if (p.m_listenPortHost != 0)
        {
            ++peerIdGenerator;
            m_peerIdPeerBimap.insert(
                PeerIdPeerBiMap::value_type(peerIdGenerator, p));
            m_peerIdSet.insert(peerIdGenerator);
        }
    }

    // Now create the one and only RumorHolder
    if (GOSSIP_CUSTOM_ROUNDS_SETTINGS)
    {
        m_rumorHolder.reset(new RRS::RumorHolder(
            m_peerIdSet, MAX_ROUNDS_IN_BSTATE, MAX_ROUNDS_IN_CSTATE,
            MAX_TOTAL_ROUNDS, MAX_NEIGHBORS_PER_ROUND, 0));
    }
    else
    {
        m_rumorHolder.reset(new RRS::RumorHolder(m_peerIdSet, 0));
    }

    return true;
}

bool RumorManager::addRumor(const RumorManager::RawBytes& message)
{
    LOG_MARKER();
    {
        std::lock_guard<std::mutex> guard(m_continueRoundMutex);
        if (!m_continueRound)
        {
            // Seems logical error. Round should have started.
            LOG_GENERAL(WARNING,
                        "Round is not running.. So won't add the rumor.");
            return false;
        }
    }

    std::lock_guard<std::mutex> guard(m_mutex); // critical section

    if (m_peerIdSet.size() == 0)
    {
        return false;
    }

    m_rumorIdRumorBimap.insert(
        RumorIdRumorBimap::value_type(++m_rumorIdGenerator, message));

    m_rumorHolder->addRumor(m_rumorIdGenerator);

    return true;
}

bool RumorManager::rumorReceived(uint8_t type, int32_t round,
                                 const RawBytes& message, const Peer& from)
{
    {
        std::lock_guard<std::mutex> guard(m_continueRoundMutex);
        if (!m_continueRound)
        {
            LOG_GENERAL(WARNING,
                        "Round is not running.. So won't accept the rumor "
                        "received. Will ignore..");
            return false;
        }
    }

    std::lock_guard<std::mutex> guard(m_mutex);

    //LOG_GENERAL(INFO, "Received message from " << from);

    auto p = m_peerIdPeerBimap.right.find(from);
    if (p == m_peerIdPeerBimap.right.end())
    {
        // I dont know this peer, missing in my peerlist.
        LOG_GENERAL(
            INFO,
            "Received Rumor from peer which does not exist in my peerlist "
                << from);
        return false;
    }

    int64_t recvdRumorId = -1;
    RRS::Message::Type t = convertType(type);
    bool toBeDispatched = false;
    if (t == RRS::Message::Type::EMPTY_PUSH
        || t == RRS::Message::Type::EMPTY_PULL)
    {
        /* Don't add it to local RumorMap because it's not the rumor itself */
        LOG_GENERAL(DEBUG,
                    "Received empty message of type: "
                        << RRS::Message::s_enumKeyToString[t]);
    }
    else
    {
        auto it = m_rumorIdRumorBimap.right.find(message);
        if (it == m_rumorIdRumorBimap.right.end())
        {
            recvdRumorId = ++m_rumorIdGenerator;
            LOG_GENERAL(INFO,
                        "We have received a new rumor. And new RumorId is "
                            << recvdRumorId);
            m_rumorIdRumorBimap.insert(
                RumorIdRumorBimap::value_type(recvdRumorId, message));
            toBeDispatched = true;
        }
        else // already received , pass it on to member for state calculations
        {
            recvdRumorId = it->second;
            LOG_GENERAL(INFO,
                        "We have received old rumor. And old RumorId is "
                            << recvdRumorId);
        }
    }

    RRS::Message recvMsg(t, recvdRumorId, round);

    int peerId = p->second;
    std::pair<int, std::vector<RRS::Message>> pullMsgs
        = m_rumorHolder->receivedMessage(recvMsg, peerId);

    // Get the corresponding Peer to which to send Pull Messages if any.
    auto l = m_peerIdPeerBimap.left.find(pullMsgs.first);
    if (l != m_peerIdPeerBimap.left.end())
    {
        LOG_GENERAL(DEBUG,
                    "Sending " << pullMsgs.second.size() << " PULL Messages");
        SendMessages(l->second, pullMsgs.second);
    }

    return toBeDispatched;
}

void RumorManager::SendMessages(const Peer& toPeer,
                                const std::vector<RRS::Message>& messages)
{
    for (auto& k : messages)
    {
        // Add round and type to outgoing message
        RawBytes cmd = {(unsigned char)k.type()};
        unsigned int cur_offset = RRSMessageOffset::R_AGE;

        Serializable::SetNumber<uint32_t>(cmd, cur_offset, k.age(),
                                          sizeof(uint32_t));

        cur_offset += sizeof(uint32_t);

        LOG_GENERAL(DEBUG, "My port is : " << m_selfPeer.m_listenPortHost);

        Serializable::SetNumber<uint32_t>(
            cmd, cur_offset, m_selfPeer.m_listenPortHost, sizeof(uint32_t));

        // Get the raw messages based on rumor ids.
        auto m = m_rumorIdRumorBimap.left.find(k.rumorId());
        if (m != m_rumorIdRumorBimap.left.end())
        {
            // Add raw message to outgoing message
            cmd.insert(cmd.end(), m->second.begin(), m->second.end());
            LOG_GENERAL(DEBUG,
                        "Sending Non Empty - Gossip Message - "
                            << k << " To Peer : " << toPeer);
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
