#ifndef ZILLIQA_RUMORMANAGER_H
#define ZILLIQA_RUMORMANAGER_H

#include <boost/bimap.hpp>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>

#include "libRumorSpreading/RumorHolder.h"

#include "Peer.h"

enum RRSMessageOffset : unsigned int
{
    R_TYPE = 0,
    R_AGE = 1,
};

class RumorManager
{
public:
    // TYPES
    typedef std::vector<unsigned char> RawBytes;

private:
    // TYPES
    typedef boost::bimap<int, Peer> PeerIdPeerBiMap;
    typedef boost::bimap<int, RawBytes> RumorIdRumorBimap;

    // MEMBERS
    std::shared_ptr<RRS::RumorHolder> m_rumorHolder;
    PeerIdPeerBiMap m_peerIdPeerBimap;
    std::unordered_set<int> m_peerIdSet;
    RumorIdRumorBimap m_rumorIdRumorBimap;
    Peer m_selfPeer;

    int64_t m_rumorIdGenerator;
    std::mutex m_mutex;
    std::mutex m_continueRoundMutex;
    bool m_continueRound;
    std::condition_variable m_condStopRound;

    void SendMessages(const Peer& peer,
                      const std::vector<RRS::Message>& messages);

public:
    // CREATORS
    RumorManager();
    ~RumorManager();

    // METHODS
    bool Initialize(const std::vector<Peer>& peers, const Peer& myself);

    bool addRumor(const RawBytes& message);

    bool rumorReceived(uint8_t type, int32_t round, const RawBytes& message,
                       const Peer& from);

    void startRounds();
    void stopRounds();

    // CONST METHODS
    const RumorIdRumorBimap& rumors() const;
};

#endif //ZILLIQA_RUMORMANAGER_H
