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
#include "RumorManager.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "libUtils/ThreadPool.h"

using broadcast_list_func = std::function<std::vector<Peer>(
    unsigned char msg_type, unsigned char ins_type, const Peer&)>;

/// Provides network layer functionality.
class P2PComm
{
public:
    // TYPES
    using SocketCloser = std::unique_ptr<int, void (*)(int*)>;
    using Dispatcher
        = std::function<void(const std::vector<unsigned char>&, const Peer&)>;

private:
    // STATIC MEMBERS
    const static uint32_t MAXRETRYCONN = 3;
    const static uint32_t PUMPMESSAGE_MILLISECONDS = 1000;

    // MEMBERS
    std::set<std::vector<unsigned char>> m_broadcastHashes;
    std::mutex m_broadcastHashesMutex;
    std::mutex m_broadcastCoreMutex;
    std::mutex m_sendMessageMutex;
    Peer m_selfPeer;
    ThreadPool m_SendPool{MAXMESSAGE, "SendPool"};
    ThreadPool m_RecvPool{MAXMESSAGE, "RecvPool"};
    RumorManager m_rumorManager;

    // METHODS
    /// Delegate to `SendMessageSocketCore`. Retries a limited number of times.
    void SendMessageCore(const Peer& peer,
                         const std::vector<unsigned char>& message,
                         unsigned char start_byte,
                         const std::vector<unsigned char>& msg_hash);

    /**
      *
      * @param peer receiver
      * @param message
      * @param start_byte
      * @param msg_hash
      * @return true if message was sent successfully
      *
      * Send a message to a peer via a socket.
      *
      * Transmission format:
      * 0x01 ~ 0xFF - version, defined in constant file
      * 0x11 - start byte
      * 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
      * <message>
      *
      * 0x01 ~ 0xFF - version, defined in constant file
      * 0x22 - start byte (broadcast)
      * 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
      * <32-byte hash> <message>
      *
      * 0x01 ~ 0xFF - version, defined in constant file
      * 0x33 - start byte (report)
      * 0x00 0x00 0x00 0x01 - 4-byte length of message
      * 0x00
      *
      * 0x01 ~ 0xFF - version, defined in constant file
      * 0x33 - start byte (gossip)
      * 0x44 - start byte (gosisp)
      * 0xLL 0xLL 0xLL 0xLL - 4-byte length of type + round + message
      * <type> <round> <message>
      */
    bool SendMessageSocketCore(const Peer& peer,
                               const std::vector<unsigned char>& message,
                               unsigned char start_byte,
                               const std::vector<unsigned char>& msg_hash);

    template<typename Container>
    void
    SendBroadcastMessageCore(const Container& peers,
                             const std::vector<unsigned char>& message,
                             const std::vector<unsigned char>& message_hash);

    void
    ClearBroadcastHashAsync(const std::vector<unsigned char>& message_hash);

    template<typename Container>
    void SendBroadcastMessageHelper(const Container& peers,
                                    const std::vector<unsigned char>& message);

    template<unsigned char START_BYTE, typename Container>
    void SendMessagePoolHelper(const Container& peers,
                               const std::vector<unsigned char>& message,
                               const std::vector<unsigned char>& message_hash);

    static void HandleAcceptedConnectionNormal(int cli_sock, const Peer& from,
                                               const Dispatcher& dispatcher,
                                               uint32_t message_length,
                                               SocketCloser cli_sock_closer);

    static void HandleAcceptedConnectionBroadcast(
        int cli_sock, const Peer& from, const Dispatcher& dispatcher,
        broadcast_list_func broadcast_list_retriever, uint32_t message_length,
        SocketCloser cli_sock_closer);

    // CONSTRUCTORS
    P2PComm();
    ~P2PComm();

    // Singleton should not implement these
    P2PComm(P2PComm const&) = delete;
    void operator=(P2PComm const&) = delete;

    // FRIENDS
    friend class RumorManager;

public:
    // STATIC METHODS
    /// Returns the singleton P2PComm instance.
    static P2PComm& GetInstance();

    // METHODS
    /// Sets `m_selfPeer` to the specified `self`.
    void SetSelfPeer(const Peer& self);

    /**
     * @param cli_sock Socket descriptor
     * @param from Peer
     * @param dispatcher callback to dispatch message
     * @param broadcast_list_retriever  callback to retrieve broadcast peer list
     *
     * Receives incoming message and assigns to designated message dispatcher.
     *
     * Reception formats:
     * 0x01 ~ 0xFF - version, defined in constant file
     * 0x11 - start byte
     * 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
     * <message>
     *
     * 0x01 ~ 0xFF - version, defined in constant file
     * 0x22 - start byte (broadcast)
     * 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
     * <32-byte hash> <message>
     *
     * 0x01 ~ 0xFF - version, defined in constant file
     * 0x33 - start byte (report)
     * 0x00 0x00 0x00 0x01 - 4-byte length of message
     * 0x00
     *
     * 0x01 ~ 0xFF - version, defined in constant file
     * 0x44 - start byte (gosisp)
     * 0xLL 0xLL 0xLL 0xLL - 4-byte length of type + round + message
     * <type> <round> <message>
     */
    static bool
    HandleAcceptedConnection(int cli_sock, Peer from, Dispatcher dispatcher,
                             broadcast_list_func broadcast_list_retriever);

    /// Accept TCP connection for libevent usage
    static void ConnectionAccept(int serv_sock, short event, void* arg);

    /// Listens for incoming socket connections.
    void StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
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

    void SpreadRumor(const std::vector<Peer>& peers,
                     const std::vector<unsigned char>& message);
};

#endif // __P2PCOMM_H__
