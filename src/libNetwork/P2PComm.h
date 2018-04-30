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

#ifndef __P2PCOMM_H__
#define __P2PCOMM_H__

#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <vector>

#include "Peer.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "libUtils/ThreadPool.h"

typedef std::function<std::vector<Peer>(unsigned char msg_type,
                                        unsigned char ins_type, const Peer&)>
    broadcast_list_func;

/// Provides network layer functionality.
class P2PComm
{
    std::set<std::vector<unsigned char>> m_broadcastHashes;
    std::mutex m_broadcastHashesMutex;
    std::mutex m_broadcastCoreMutex;
    std::mutex m_startMessagePumpMutex;
    std::mutex m_sendMessageMutex;

    const static uint32_t MAXRETRYCONN = 3;
    const static uint32_t MAXPUMPMESSAGE = 128;
    const static uint32_t PUMPMESSAGE_MILLISECONDS = 1000;

    void SendMessageCore(const Peer& peer,
                         const std::vector<unsigned char>& message,
                         unsigned char start_byte,
                         const std::vector<unsigned char>& msg_hash);
    bool SendMessageSocketCore(const Peer& peer,
                               const std::vector<unsigned char>& message,
                               unsigned char start_byte,
                               const std::vector<unsigned char>& msg_hash);

    template<typename Container>
    void
    SendBroadcastMessageCore(const Container& peers,
                             const std::vector<unsigned char>& message,
                             const std::vector<unsigned char>& message_hash);

    template<typename Container>
    void SendBroadcastMessageHelper(const Container& peers,
                                    const std::vector<unsigned char>& message);

    template<unsigned char START_BYTE, typename Container>
    void SendMessagePoolHelper(const Container& peers,
                               const std::vector<unsigned char>& message,
                               const std::vector<unsigned char>& message_hash);

    P2PComm();
    ~P2PComm();

#ifdef STAT_TEST
    Peer m_selfPeer;
#endif // STAT_TEST

    ThreadPool m_SendPool{MAXMESSAGE, "SendPool"};

#if 0 //clark
    ThreadPool m_RecvPool{MAXMESSAGE, "RecvPool"};
#endif
public:
    /// Returns the singleton P2PComm instance.
    static P2PComm& GetInstance();

    /// Receives incoming message and assigns to designated message dispatcher.
#if 1 //clark
    static void HandleAcceptedConnection(int cli_sock, short event, void* arg);
#else
    void HandleAcceptedConnection(
        int cli_sock, Peer from,
        std::function<void(const std::vector<unsigned char>&, const Peer&)>
            dispatcher,
        broadcast_list_func broadcast_list_retriever);
#endif

#if 1 //clark
    static void ConnectionAccept(int serv_sock, short event, void* arg);
#endif

    /// Listens for incoming socket connections.
    void StartMessagePump(
        uint32_t listen_port_host,
        std::function<void(const std::vector<unsigned char>&, const Peer&)>
            dispatcher,
        broadcast_list_func broadcast_list_retriever);

    /// Multicasts message to specified list of peers.
    void SendMessage(const std::vector<Peer>& peers,
                     const std::vector<unsigned char>& message);

    /// Multicasts message to specified list of peers.
    void SendMessage(const std::deque<Peer>& peers,
                     const std::vector<unsigned char>& message);

    /// Sends message to specified peer.
    void SendMessage(const Peer& peer,
                     const std::vector<unsigned char>& message);

    /// Multicasts message of type=broadcast to specified list of peers.
    void SendBroadcastMessage(const std::vector<Peer>& peers,
                              const std::vector<unsigned char>& message);

    /// Multicasts message of type=broadcast to specified list of peers.
    void SendBroadcastMessage(const std::deque<Peer>& peers,
                              const std::vector<unsigned char>& message);

#if 1 //clark
    std::set<std::vector<unsigned char>>& GetBroadcastHashes()
    {
        return m_broadcastHashes;
    }

    std::mutex& GetBroadcastHashesMutex() { return m_broadcastHashesMutex; }
#endif

#ifdef STAT_TEST
    void SetSelfPeer(const Peer& self);
#if 1 //clark
    const Peer& GetSelfPeer() { return m_selfPeer; }
#endif
#endif // STAT_TEST
};

#endif // __P2PCOMM_H__
