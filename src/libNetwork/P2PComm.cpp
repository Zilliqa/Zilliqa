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

#include <cstring>
#include <errno.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <memory>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "P2PComm.h"
#include "PeerStore.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

const unsigned char START_BYTE_NORMAL = 0x11;
const unsigned char START_BYTE_BROADCAST = 0x22;
const unsigned int HDR_LEN = 6;
const unsigned int HASH_LEN = 32;

P2PComm::Dispatcher P2PComm::m_dispatcher;
P2PComm::BroadcastListFunc P2PComm::m_broadcast_list_retriever;

/// Comparison operator for ordering the list of message hashes.
struct hash_compare
{
    bool operator()(const vector<unsigned char>& l,
                    const vector<unsigned char>& r)
    {
        return equal(l.begin(), l.end(), r.begin());
    }
};

static void close_socket(int* cli_sock)
{
    if (cli_sock != NULL)
    {
        shutdown(*cli_sock, SHUT_RDWR);
        close(*cli_sock);
    }
}

static bool comparePairSecond(
    const pair<vector<unsigned char>, chrono::time_point<chrono::system_clock>>&
        a,
    const pair<vector<unsigned char>, chrono::time_point<chrono::system_clock>>&
        b)
{
    return a.second < b.second;
}

P2PComm::P2PComm()
{
    auto func = [this]() -> void {
        std::vector<unsigned char> emptyHash;

        while (true)
        {
            this_thread::sleep_for(chrono::seconds(BROADCAST_INTERVAL));
            lock(m_broadcastToRemoveMutex, m_broadcastHashesMutex);
            lock_guard<mutex> g(m_broadcastToRemoveMutex, adopt_lock);
            lock_guard<mutex> g2(m_broadcastHashesMutex, adopt_lock);

            if (m_broadcastToRemove.empty()
                || m_broadcastToRemove.front().second
                    > chrono::system_clock::now()
                        - chrono::seconds(BROADCAST_EXPIRY))
            {
                continue;
            }

            auto up = upper_bound(
                m_broadcastToRemove.begin(), m_broadcastToRemove.end(),
                make_pair(emptyHash,
                          chrono::system_clock::now()
                              - chrono::seconds(BROADCAST_EXPIRY)),
                comparePairSecond);

            for (auto it = m_broadcastToRemove.begin(); it != up; ++it)
            {
                m_broadcastHashes.erase(it->first);
            }

            m_broadcastToRemove.erase(m_broadcastToRemove.begin(), up);
        }
    };

    DetachedFunction(1, func);
}

P2PComm::~P2PComm() {}

P2PComm& P2PComm::GetInstance()
{
    static P2PComm comm;
    return comm;
}

void P2PComm::SendMessageCore(const Peer& peer,
                              const std::vector<unsigned char>& message,
                              unsigned char start_byte,
                              const vector<unsigned char>& msg_hash)
{
    uint32_t retry_counter = 0;
    while (!SendMessageSocketCore(peer, message, start_byte, msg_hash))
    {
        retry_counter++;
        LOG_GENERAL(WARNING,
                    "Socket connect failed " << retry_counter << "/"
                                             << MAXRETRYCONN
                                             << ". IP address: " << peer);

        if (retry_counter > MAXRETRYCONN)
        {
            LOG_GENERAL(WARNING,
                        "Socket connect failed over " << MAXRETRYCONN
                                                      << " times.");
            return;
        }
        this_thread::sleep_for(
            chrono::milliseconds(rand() % PUMPMESSAGE_MILLISECONDS));
    }
}

namespace
{
    bool readMsg(vector<unsigned char>* buf, int cli_sock, const Peer& from,
                 const uint32_t message_length)
    {
        // Read the rest of the message
        assert(buf);
        buf->resize(message_length);
        uint32_t read_length = 0;
        uint32_t retryDuration = 1;

        while (read_length < message_length)
        {
            ssize_t n = read(cli_sock, &buf->at(read_length),
                             message_length - read_length);

            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);

                if (EAGAIN == errno)
                {
                    this_thread::sleep_for(chrono::milliseconds(retryDuration));
                    retryDuration <<= 1;
                    continue;
                }

                return false;
            }

            read_length += n;
        }

        if (HDR_LEN != message_length && HASH_LEN != message_length)
        {
            LOG_PAYLOAD(INFO, "Message received", *buf,
                        Logger::MAX_BYTES_TO_DISPLAY);
        }

        if (read_length != message_length)
        {
            LOG_GENERAL(WARNING, "Incorrect message length.");
            return false;
        }

        return true;
    }

    uint32_t writeMsg(const void* buf, int cli_sock, const Peer& from,
                      const uint32_t message_length)
    {
        uint32_t written_length = 0;

        while (written_length < message_length)
        {
            ssize_t n = write(cli_sock, (unsigned char*)buf + written_length,
                              message_length - written_length);

            if (errno == EPIPE)
            {
                LOG_GENERAL(WARNING,
                            " SIGPIPE detected. Error No: "
                                << errno << " Desc: " << std::strerror(errno));
                return written_length;
                // No retry as it is likely the other end terminate the conn due to duplicated msg.
            }

            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket write failed in message header. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address:" << from);
                return written_length;
            }

            written_length += n;
        }

        if (written_length > 1000000)
        {
            LOG_GENERAL(
                INFO, "DEBUG: Sent a total of " << written_length << " bytes");
        }

        return written_length;
    }

    uint32_t messageLength(const vector<unsigned char>& buf)
    {
        return (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8) + buf[5];
    }

} // anonymous namespace

bool P2PComm::SendMessageSocketCore(const Peer& peer,
                                    const std::vector<unsigned char>& message,
                                    unsigned char start_byte,
                                    const vector<unsigned char>& msg_hash)
{
    // LOG_MARKER();
    LOG_PAYLOAD(INFO, "Sending message to " << peer, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    if (peer.m_ipAddress == 0 && peer.m_listenPortHost == 0)
    {
        LOG_GENERAL(INFO,
                    "I am sending to 0.0.0.0 at port 0. Don't send anything.");
        return true;
    }
    else if (peer.m_listenPortHost == 0)
    {
        LOG_GENERAL(INFO,
                    "I am sending to " << peer.GetPrintableIPAddress()
                                       << " at port 0. Investigate why!");
        return true;
    }

    try
    {
        int cli_sock = socket(AF_INET, SOCK_STREAM, 0);
        unique_ptr<int, void (*)(int*)> cli_sock_closer(&cli_sock,
                                                        close_socket);

        // LINUX HAS NO SO_NOSIGPIPE
        //int set = 1;
        //setsockopt(cli_sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
        signal(SIGPIPE, SIG_IGN);
        if (cli_sock < 0)
        {
            LOG_GENERAL(WARNING,
                        "Socket creation failed. Code = "
                            << errno << " Desc: " << std::strerror(errno)
                            << ". IP address: " << peer);
            return false;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr
            = peer.m_ipAddress.convert_to<unsigned long>();
        serv_addr.sin_port = htons(peer.m_listenPortHost);

        if (connect(cli_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))
            < 0)
        {
            LOG_GENERAL(WARNING,
                        "Socket connect failed. Code = "
                            << errno << " Desc: " << std::strerror(errno)
                            << ". IP address: " << peer);
            return false;
        }

        // Transmission format:
        // 0x01 ~ 0xFF - version, defined in constant file
        // 0x11 - start byte
        // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
        // <message>

        // 0x01 ~ 0xFF - version, defined in constant file
        // 0x22 - start byte (broadcast)
        // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
        // <32-byte hash> <message>

        // 0x01 ~ 0xFF - version, defined in constant file
        // 0x33 - start byte (report)
        // 0x00 0x00 0x00 0x01 - 4-byte length of message
        // 0x00
        uint32_t length = message.size();

        if (start_byte == START_BYTE_BROADCAST)
        {
            length += HASH_LEN;
        }

        unsigned char buf[HDR_LEN] = {(unsigned char)(MSG_VERSION & 0xFF),
                                      start_byte,
                                      (unsigned char)((length >> 24) & 0xFF),
                                      (unsigned char)((length >> 16) & 0xFF),
                                      (unsigned char)((length >> 8) & 0xFF),
                                      (unsigned char)(length & 0xFF)};

        if (HDR_LEN != writeMsg(buf, cli_sock, peer, HDR_LEN))
        {
            LOG_GENERAL(INFO, "DEBUG: not written_length == " << HDR_LEN);
        }

        if (start_byte != START_BYTE_BROADCAST)
        {
            writeMsg(&message.at(0), cli_sock, peer, length);
            return true;
        }

        if (HASH_LEN != writeMsg(&msg_hash.at(0), cli_sock, peer, HASH_LEN))
        {
            LOG_GENERAL(WARNING, "Wrong message hash length.");
            return false;
        }

        length -= HASH_LEN;
        writeMsg(&message.at(0), cli_sock, peer, length);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING, "Error with write socket." << ' ' << e.what());
        return false;
    }
    return true;
}

template<typename Container>
void P2PComm::SendBroadcastMessageCore(
    const Container& peers, const vector<unsigned char>& message,
    const vector<unsigned char>& message_hash)
{
    // LOG_MARKER();
    lock_guard<mutex> guard(m_broadcastCoreMutex);

    SendMessagePoolHelper<START_BYTE_BROADCAST>(peers, message, message_hash);
}

void P2PComm::ClearBroadcastHashAsync(const vector<unsigned char>& message_hash)
{
    LOG_MARKER();
    lock_guard<mutex> guard(m_broadcastToRemoveMutex);
    m_broadcastToRemove.emplace_back(message_hash, chrono::system_clock::now());
}

void P2PComm::HandleAcceptedConnection(int cli_sock, Peer from)
{
    //LOG_MARKER();

    LOG_GENERAL(INFO, "Incoming message from " << from);

    SocketCloser cli_sock_closer(&cli_sock, close_socket);

    // Reception format:
    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x11 - start byte
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
    // <message>

    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x22 - start byte (broadcast)
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
    // <32-byte hash> <message>

    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x33 - start byte (report)
    // 0x00 0x00 0x00 0x01 - 4-byte length of message
    // 0x00

    vector<unsigned char> header = {0};

    if (!readMsg(&header, cli_sock, from, HDR_LEN))
    {
        return;
    }

    const unsigned char version = header[0];
    const unsigned char startByte = header[1];

    // If received version doesn't match expected version (defined in constant file), drop this message
    if (version != (unsigned char)(MSG_VERSION & 0xFF))
    {
        LOG_GENERAL(WARNING,
                    "Header version wrong, received ["
                        << version - 0x00 << "] while expected [" << MSG_VERSION
                        << "].");
        return;
    }

    if (startByte == START_BYTE_BROADCAST)
    {
        HandleAcceptedConnectionBroadcast(cli_sock, from, messageLength(header),
                                          move(cli_sock_closer));
    }
    else if (startByte == START_BYTE_NORMAL)
    {
        HandleAcceptedConnectionNormal(cli_sock, from, messageLength(header),
                                       move(cli_sock_closer));
    }
    else
    {
        // Unexpected start byte. Drop this message
        LOG_GENERAL(WARNING, "Header length or type wrong.");
    }
}

void P2PComm::HandleAcceptedConnectionNormal(int cli_sock, Peer from,
                                             uint32_t message_length,
                                             SocketCloser cli_sock_closer)
{
    vector<unsigned char> message;

    if (!readMsg(&message, cli_sock, from, message_length))
    {
        return;
    }

    cli_sock_closer.reset(); // close socket now so it can be reused

    m_dispatcher(message, from);
}

void P2PComm::HandleAcceptedConnectionBroadcast(int cli_sock, Peer from,
                                                uint32_t message_length,
                                                SocketCloser cli_sock_closer)
{
    vector<unsigned char> msg_hash;

    if (!readMsg(&msg_hash, cli_sock, from, HASH_LEN))
    {
        return;
    }

    // Check if this message has been received before
    vector<unsigned char> message;
    bool found = false;
    {
        lock_guard<mutex> guard(P2PComm::GetInstance().m_broadcastHashesMutex);

        found = (P2PComm::GetInstance().m_broadcastHashes.find(msg_hash)
                 != P2PComm::GetInstance().m_broadcastHashes.end());
        // While we have the lock, we should quickly add the hash
        if (!found)
        {
            if (!readMsg(&message, cli_sock, from, message_length - HASH_LEN))
            {
                return;
            }

            SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
            sha256.Update(message);
            vector<unsigned char> this_msg_hash = sha256.Finalize();

            if (this_msg_hash == msg_hash)
            {
                P2PComm::GetInstance().m_broadcastHashes.insert(this_msg_hash);
            }
            else
            {
                LOG_GENERAL(WARNING, "Incorrect message hash.");
                return;
            }
        }
    }

    cli_sock_closer.reset(); // close socket now so it can be reused

    if (found)
    {
        // We already sent and/or received this message before -> discard
        LOG_GENERAL(INFO, "Discarding duplicate broadcast message");
        return;
    }

    unsigned char msg_type = 0xFF;
    unsigned char ins_type = 0xFF;
    if (message.size() > MessageOffset::INST)
    {
        msg_type = message.at(MessageOffset::TYPE);
        ins_type = message.at(MessageOffset::INST);
    }

    vector<Peer> broadcast_list
        = m_broadcast_list_retriever(msg_type, ins_type, from);

    if (broadcast_list.size() > 0)
    {
        P2PComm::GetInstance().SendBroadcastMessageCore(broadcast_list, message,
                                                        msg_hash);
    }

    // Used to be done in SendBroadcastMessageCore, but it would never be called by lookup nodes
    P2PComm::GetInstance().ClearBroadcastHashAsync(msg_hash);

    LOG_STATE(
        "[BROAD][" << std::setw(15) << std::left
                   << P2PComm::GetInstance().m_selfPeer << "]["
                   << DataConversion::Uint8VecToHexStr(msg_hash).substr(0, 6)
                   << "] RECV");

    // Dispatch message normally
    m_dispatcher(message, from);
}

void P2PComm::ConnectionAccept([[gnu::unused]] evconnlistener* listener,
                               evutil_socket_t cli_sock,
                               struct sockaddr* cli_addr,
                               [[gnu::unused]] int socklen,
                               [[gnu::unused]] void* arg)
{
    Peer from(uint128_t(((struct sockaddr_in*)cli_addr)->sin_addr.s_addr),
              ((struct sockaddr_in*)cli_addr)->sin_port);

    auto func = [cli_sock, from]() -> void {
        HandleAcceptedConnection(cli_sock, from);
    };

    GetInstance().m_RecvPool.AddJob(func);
}

void P2PComm::StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
                               BroadcastListFunc broadcast_list_retriever)
{
    LOG_MARKER();

    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock < 0)
    {
        LOG_GENERAL(WARNING,
                    "Socket creation failed. Code = " << errno << " Desc: "
                                                      << std::strerror(errno));
        return;
    }

    int enable = 1;
    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))
        < 0)
    {
        LOG_GENERAL(WARNING,
                    "Socket set option SO_REUSEADDR failed. Code = "
                        << errno << " Desc: " << std::strerror(errno));
        return;
    }

    m_dispatcher = dispatcher;
    m_broadcast_list_retriever = broadcast_list_retriever;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(listen_port_host);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    struct event_base* base = event_base_new();
    struct evconnlistener* listener = evconnlistener_new_bind(
        base, ConnectionAccept, nullptr,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));
    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);
}

/// Send message to the peers using the threads from the pool
template<unsigned char START_BYTE, typename Container>
void P2PComm::SendMessagePoolHelper(const Container& peers,
                                    const vector<unsigned char>& message,
                                    const vector<unsigned char>& message_hash)
{
    vector<unsigned int> indexes(peers.size());

    for (unsigned int i = 0; i < indexes.size(); i++)
    {
        indexes.at(i) = i;
    }
    random_shuffle(indexes.begin(), indexes.end());

    auto sharedMessage = make_shared<vector<unsigned char>>(message);
    auto sharedMessageHash = make_shared<vector<unsigned char>>(message_hash);

    for (vector<unsigned int>::const_iterator curr = indexes.begin();
         curr < indexes.end(); curr++)
    {
        Peer peer = peers.at(*curr);
        auto func1
            = [this, peer, sharedMessage, sharedMessageHash]() mutable -> void {
            SendMessageCore(peer, *sharedMessage.get(), START_BYTE,
                            *sharedMessageHash.get());
        };
        m_SendPool.AddJob(func1);
    }
}

void P2PComm::SendMessage(const vector<Peer>& peers,
                          const vector<unsigned char>& message)
{
    LOG_MARKER();
    lock_guard<mutex> guard(m_sendMessageMutex);

    SendMessagePoolHelper<START_BYTE_NORMAL>(peers, message, {});
}

void P2PComm::SendMessage(const deque<Peer>& peers,
                          const vector<unsigned char>& message)
{
    LOG_MARKER();
    lock_guard<mutex> guard(m_sendMessageMutex);

    SendMessagePoolHelper<START_BYTE_NORMAL>(peers, message, {});
}

void P2PComm::SendMessage(const Peer& peer,
                          const vector<unsigned char>& message)
{
    LOG_MARKER();
    lock_guard<mutex> guard(m_sendMessageMutex);
    SendMessageCore(peer, message, START_BYTE_NORMAL, vector<unsigned char>());
}

template<typename Container>
void P2PComm::SendBroadcastMessageHelper(
    const Container& peers, const std::vector<unsigned char>& message)
{
    if (peers.empty())
    {
        return;
    }

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(message);
    vector<unsigned char> this_msg_hash = sha256.Finalize();

    {
        lock_guard<mutex> guard(m_broadcastHashesMutex);
        m_broadcastHashes.insert(this_msg_hash);
    }

    LOG_STATE("[BROAD]["
              << std::setw(15) << std::left
              << m_selfPeer.GetPrintableIPAddress() << "]["
              << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
              << "] BEGN");

    SendBroadcastMessageCore(peers, message, this_msg_hash);

    LOG_STATE("[BROAD]["
              << std::setw(15) << std::left
              << m_selfPeer.GetPrintableIPAddress() << "]["
              << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
              << "] DONE");
}

void P2PComm::SendBroadcastMessage(const vector<Peer>& peers,
                                   const vector<unsigned char>& message)
{
    LOG_MARKER();
    SendBroadcastMessageHelper(peers, message);
}

void P2PComm::SendBroadcastMessage(const deque<Peer>& peers,
                                   const vector<unsigned char>& message)
{
    LOG_MARKER();
    SendBroadcastMessageHelper(peers, message);
}

void P2PComm::SetSelfPeer(const Peer& self) { m_selfPeer = self; }
