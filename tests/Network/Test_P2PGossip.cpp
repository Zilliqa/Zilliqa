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

#include "libNetwork/P2PGossip.h"

#define BOOST_TEST_MODULE p2pgossip
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <functional>
#include <iostream>

using namespace std;
using namespace std::chrono;

BOOST_AUTO_TEST_SUITE(p2pgossip)

namespace
{ // test namespace

    using Time = Gossiper::Time;

    const unsigned BROADCAST_EXPIRY_SECONDS = 600;
    const unsigned START_TIME = 10000;
    const seconds SEC(1);

    Time t0 = steady_clock::time_point(duration<unsigned>(START_TIME));

    class Peer;
    class System
    {
    public:
        using Action = function<void(Time)>;

    private:
        using Actions = vector<pair<Time, Action>>;

        static bool timeAsc(const Actions::value_type& lhs,
                            const Actions::value_type& rhs)
        {
            return lhs.first < rhs.first;
        }

    public:
        System(unsigned peers)
            : m_peers()
            , m_todo()
        {
            for (unsigned peerId = 0; peerId < peers; ++peerId)
            {
                m_peers.push_back(make_shared<Peer>(*this, peerId));
            }
        }

        Peer& operator[](unsigned index) { return *m_peers[index]; }

        void at(Time time, const Action& what)
        {
            m_todo.push_back(make_pair(time, what));
            std::sort(m_todo.begin(), m_todo.end(), &System::timeAsc);
        }

        void timer(Time start, seconds period, const Action& what)
        {
            at(start + period, [=](Time now) {
                what(now);
                timer(now, period, what);
            });
        }

        void runTo(Time end)
        {
            while (true)
            {
                if (m_todo.empty())
                {
                    return;
                }
                auto timeAndAction = m_todo[0];
                m_todo.erase(m_todo.begin());
                Time now = timeAndAction.first;
                if (now >= end)
                {
                    return;
                }
                timeAndAction.second(now);
            }
        }

        size_t peerCount() const { return m_peers.size(); }

        void tick(Time now);

    private:
        vector<shared_ptr<Peer>> m_peers;
        Actions m_todo;
    };

    class Peer
    {
    public:
        Peer(System& system, unsigned self)
            : m_system(system)
            , m_self(self)
            , m_gossiper(BROADCAST_EXPIRY_SECONDS)
        {
        }

        void broadcast(Time now)
        {
            perform(m_gossiper.broadcast(m_system.peerCount(), now), now);
        }

        void onRumorReceived(int peerId, size_t peers, Time now)
        {
            ++received;
            perform(m_gossiper.onRumorReceived(peerId, peers, now), now);
        }

        void tick(Time now) { perform(m_gossiper.tick(now), now); }

    private:
        void perform(const Gossiper::Actions& actions, Time now)
        {
            for (auto& action : actions)
            {
                switch (action.what)
                {
                case Gossiper::Action::SendToPeer:
                {
                    if (byzantine)
                    {
                        // Malicious user doesn't send.
                        return;
                    }

                    ++send;
                    const int id = action.peerId;
                    assert(id >= 0);
                    assert(id < m_system.peerCount());

                    auto latency = (rand() % 5) * SEC;

                    m_system.at(now + latency, [&, id](Time now) {
                        m_system[id].onRumorReceived(m_self,
                                                     m_system.peerCount(), now);
                    });
                }
                break;
                case Gossiper::Action::DropDuplicate:
                {
                }
                break;
                case Gossiper::Action::Reset:
                {
                }
                break;
                case Gossiper::Action::Dispatch:
                {
                    dispatchedAt.push_back(now);
                }
                break;
                case Gossiper::Action::Noop:
                {
                }
                break;
                default:
                {
                    assert(false); // Unknown action type
                }
                }
            }
        }

        System& m_system;
        const unsigned m_self;
        Gossiper m_gossiper;

    public:
        bool byzantine = false;

        unsigned send = 0;
        unsigned received = 0;
        vector<Time> dispatchedAt;
    };

    void System::tick(Time now)
    {
        for (auto& peer : m_peers)
        {
            peer->tick(now);
        }
    }

} // test namespace

struct BasicTestFixture
{
    const unsigned PEERS;

    System system;

    BasicTestFixture()
        : PEERS(3)
        , system(PEERS)
    {
        system.at(t0 + 1 * SEC, [&](Time now) { system[0].broadcast(now); });

        system.timer(t0, 5 * SEC, [&](Time now) { system.tick(now); });
    }
};

BOOST_FIXTURE_TEST_CASE(HappyPath, BasicTestFixture)
{
    system.runTo(t0 + 1000 * SEC);

    for (unsigned i = 0; i < PEERS; ++i)
    {
        BOOST_REQUIRE_EQUAL(system[i].send, 3);
        BOOST_REQUIRE_EQUAL(system[i].received, 3);

        BOOST_REQUIRE_EQUAL(system[i].dispatchedAt.size(), 1); // safety
        BOOST_REQUIRE(system[i].dispatchedAt[0] < t0 + 100 * SEC); // liveness
    }
}

BOOST_FIXTURE_TEST_CASE(SingleByzantine, BasicTestFixture)
{
    system[1].byzantine = true;

    system.runTo(t0 + 1000 * SEC);

    BOOST_REQUIRE_EQUAL(system[0].send, 3);
    BOOST_REQUIRE_EQUAL(system[0].received, 2);
    BOOST_REQUIRE_EQUAL(system[0].dispatchedAt.size(), 1); // safety
    BOOST_REQUIRE(system[0].dispatchedAt[0] < t0 + 100 * SEC); // liveness

    BOOST_REQUIRE_EQUAL(system[1].send, 0);
    BOOST_REQUIRE_EQUAL(system[1].received, 2);
    BOOST_REQUIRE_EQUAL(system[1].dispatchedAt.size(), 1); // safety
    BOOST_REQUIRE(system[1].dispatchedAt[0] < t0 + 100 * SEC); // liveness

    BOOST_REQUIRE_EQUAL(system[2].send, 3);
    BOOST_REQUIRE_EQUAL(system[2].received, 2);
    BOOST_REQUIRE_EQUAL(system[2].dispatchedAt.size(), 1); // safety
    BOOST_REQUIRE(system[2].dispatchedAt[0] < t0 + 100 * SEC); // liveness
}

BOOST_FIXTURE_TEST_CASE(TwoFailed, BasicTestFixture)
{
    // This is byzantine, indeed. Since malicious>honest, there's loss
    system[1].byzantine = true;
    system[2].byzantine = true;

    system.runTo(t0 + 1000 * SEC);

    BOOST_REQUIRE_EQUAL(system[0].send, 3);
    BOOST_REQUIRE_EQUAL(system[0].received, 1);
    BOOST_REQUIRE(system[0].dispatchedAt.empty());

    BOOST_REQUIRE_EQUAL(system[1].send, 0);
    BOOST_REQUIRE_EQUAL(system[1].received, 1);
    BOOST_REQUIRE(system[1].dispatchedAt.empty());

    BOOST_REQUIRE_EQUAL(system[2].send, 0);
    BOOST_REQUIRE_EQUAL(system[2].received, 1);
    BOOST_REQUIRE(system[2].dispatchedAt.empty());
}

BOOST_AUTO_TEST_SUITE_END()
