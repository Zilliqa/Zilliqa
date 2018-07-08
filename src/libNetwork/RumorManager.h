#ifndef ZILLIQA_RUMORS_H
#define ZILLIQA_RUMORS_H

#include <boost/bimap.hpp>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>

#include "depends/libRumorSpreading/RumorMember.h"

#include "Peer.h"

using namespace RRS;

class RumorManager
{
public:
    // TYPES
    typedef std::vector<unsigned char> RawBytes;

private:
    // TYPES
    typedef boost::bimap<int, Peer> PeerBimap;
    typedef boost::bimap<int, RawBytes> RumorBimap;

    // STRUCTS
    struct RumorState
    {
        RumorMember m_member;
        PeerBimap m_peerIdBimap;
    };

    // TYPES
    typedef std::unordered_map<int, RumorState> RumorStateMap;

    // MEMBERS
    int m_rumorIdGenerator;
    RumorBimap m_rumorIdBimap;
    RumorStateMap m_rumorState;
    std::mutex m_mutex;

    // METHODS
    template<typename Container>
    void addRumorImp(const Container& peers, const RawBytes& message);

    void scheduleRounds(int rumorId);

public:
    // CREATORS
    RumorManager();
    ~RumorManager();

    // METHODS
    void addRumor(const std::vector<Peer>& peers, const RawBytes& message);
    void addRumor(const std::deque<Peer>& peers, const RawBytes& message);

    // CONST METHODS
    const RumorBimap& rumors() const;
};

#endif //ZILLIQA_RUMORS_H
