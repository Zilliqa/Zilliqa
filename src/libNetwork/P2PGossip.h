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
#ifndef __P2PGOSSIP_H__
#define __P2PGOSSIP_H__

#include <chrono>
#include <functional>
#include <map>
#include <vector>

class Gossiper
{
public:
    struct Action
    {
        enum Type
        {
            Noop,
            SendToPeer,
            DropDuplicate,
            Reset,
            Dispatch
        };
        Type what = Noop;
        int peerId = -1;
        bool success = false;
    };

    enum State
    {
        VOID,
        I_SENT,
        RECEIVED_SOME_WITNESSES,
        DONE
    };

    using Actions = std::vector<Action>;

    Gossiper(unsigned broadcastExpirySeconds)
        : m_broadcastExpirySeconds(broadcastExpirySeconds)
    {
    }

    using Time = std::chrono::steady_clock::time_point;

    Actions broadcast(size_t peers, Time now)
    {
        Actions rv;
        if (m_state == VOID)
        {
            for (int i = 0; i < (int)peers; ++i)
            {
                rv.push_back({Action::SendToPeer, i});
            }
            m_state = I_SENT;
            m_t0 = now;
        }
        else
        {
            rv.push_back({Action::DropDuplicate});
        }
        return rv;
    }

    Actions onRumorReceived(int peerId, size_t peers, Time now)
    {
        Actions rv;

        if (m_state == VOID)
        {
            m_t0 = now;
        }

        if (peerId == -1)
        {
            return rv;
        }

        if (m_state == VOID || m_state == I_SENT
            || m_state == RECEIVED_SOME_WITNESSES)
        {
            m_gotFromPeers[peerId] = true;

            size_t cnt = 0;
            for (size_t peerId = 0; peerId < peers; ++peerId)
            {
                if (m_gotFromPeers[peerId])
                {
                    ++cnt;
                }
            }

            if (m_state == VOID)
            {
                for (int i = 0; i < (int)peers; ++i)
                {
                    rv.push_back({Action::SendToPeer, i});
                }
            }

            if (cnt >= ((2 * peers) / 3))
            {
                rv.push_back({Action::Dispatch});
                m_state = DONE;
            }
            else
            {
                m_state = RECEIVED_SOME_WITNESSES;
            }
        }

        return rv;
    }

    Actions tick(Time now)
    {
        Actions rv;
        if (now > (m_t0
                   + std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::duration<unsigned>(
                             m_broadcastExpirySeconds))))
        {
            rv.push_back({Action::Reset, -1, m_state == DONE});
        }
        return rv;
    }

private:
    const unsigned m_broadcastExpirySeconds;

    State m_state = VOID;
    std::map<int, bool> m_gotFromPeers;
    Time m_t0;
};

#endif // __P2PGOSSIP_H__
