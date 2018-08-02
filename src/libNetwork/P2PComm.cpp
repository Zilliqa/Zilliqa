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
#include <event.h>
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

#if 1 //clark
#include "evpp/buffer.h"
#include "evpp/tcp_conn.h"
#include "evpp/tcp_server.h"
#endif

using namespace std;
using namespace boost::multiprecision;

const unsigned char START_BYTE_NORMAL = 0x11;
const unsigned char START_BYTE_BROADCAST = 0x22;
const unsigned int HDR_LEN = 6;
const unsigned int HASH_LEN = 32;

P2PComm::Dispatcher P2PComm::m_dispatcher;
P2PComm::Broadcast_list_func P2PComm::m_broadcast_list_retriever;

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
        uint32_t written_length = 0;

        while (written_length != HDR_LEN)
        {
            ssize_t n = write(cli_sock, buf + written_length,
                              HDR_LEN - written_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket write failed in message header. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address:" << peer);
                return false;
            }
            written_length += n;
        }

        if (start_byte == START_BYTE_BROADCAST)
        {
            written_length = 0;
            while (written_length != HASH_LEN)
            {
                ssize_t n = write(cli_sock, &msg_hash.at(0) + written_length,
                                  HASH_LEN - written_length);
                if (n <= 0)
                {
                    LOG_GENERAL(WARNING,
                                "Socket write failed in hash header. Code = "
                                    << errno
                                    << " Desc: " << std::strerror(errno));
                    return false;
                }
                written_length += n;
            }

            if (written_length == HASH_LEN)
            {
                written_length = HDR_LEN;
            }
            else
            {
                LOG_GENERAL(WARNING, "Wrong message hash length.");
                return false;
            }

            length -= HASH_LEN;
        }

        if (written_length == HDR_LEN)
        {
            written_length = 0;
            while (written_length != length)
            {
                ssize_t n = write(cli_sock, &message.at(0) + written_length,
                                  length - written_length);

                if (errno == EPIPE)
                {
                    LOG_GENERAL(WARNING,
                                " SIGPIPE detected. Error No: "
                                    << errno
                                    << " Desc: " << std::strerror(errno));
                    return true;
                    // No retry as it is likely the other end terminate the conn due to duplicated msg.
                }

                if (n <= 0)
                {
                    LOG_GENERAL(WARNING,
                                "Socket write failed in message body. Code = "
                                    << errno
                                    << " Desc: " << std::strerror(errno));
                    return false;
                }
                written_length += n;
            }
        }
        else
        {
            LOG_GENERAL(INFO, "DEBUG: not written_length == HDR_LEN");
        }

        if (written_length > 1000000)
        {
            LOG_GENERAL(
                INFO, "DEBUG: Sent a total of " << written_length << " bytes");
        }
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

namespace
{
#if 1 //clark
    bool readHeader(unsigned char* buf, evpp::Buffer* msg)
#else
    bool readHeader(unsigned char* buf, int cli_sock, Peer from)
#endif
    {
        assert(buf);
#if 1 //clark
        memcpy(buf, msg->NextString(HDR_LEN).c_str(), HDR_LEN);
#else
        uint32_t read_length = 0;

        // Read out just the header first
        while (read_length != HDR_LEN)
        {
            ssize_t n
                = read(cli_sock, buf + read_length, HDR_LEN - read_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);
                return false;
            }
            read_length += n;
        }
#endif
        return true;
    }

#if 1 //clark
    bool readHash(unsigned char* hash_buf, evpp::Buffer* msg)
#else
    bool readHash(unsigned char* hash_buf, int cli_sock, Peer from)
#endif
    {
        assert(hash_buf);
#if 1 //clark
        memcpy(hash_buf, msg->NextString(HASH_LEN).c_str(), HASH_LEN);
#else
        uint32_t read_length = 0;
        while (read_length != HASH_LEN)
        {
            ssize_t n = read(cli_sock, hash_buf + read_length,
                             HASH_LEN - read_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);
                return false;
            }
            read_length += n;
        }
#endif
        return true;
    }

#if 1 //clark
    bool readMessage(vector<unsigned char>* message, evpp::Buffer* msg,
                     uint32_t message_length)
#else
    bool readMessage(vector<unsigned char>* message, int cli_sock, Peer from,
                     uint32_t message_length)
#endif
    {
        // Read the rest of the message
        assert(message);
#if 1 //clark
        string read = msg->NextString(message_length);

        if (read.size() != message_length)
        {
            LOG_GENERAL(WARNING, "Incorrect message length.");
            return false;
        }

        message->resize(message_length);

        for (size_t i = 0; i < read.size(); ++i)
        {
            message->at(i) = read[i];
        }
#else
        uint32_t read_length = 0;
        message->resize(message_length);
        while (read_length != message_length)
        {
            ssize_t n = read(cli_sock, &message->at(read_length),
                             message_length - read_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);
                return false;
            }
            read_length += n;
        }
#endif
        LOG_PAYLOAD(INFO, "Message received", *message,
                    Logger::MAX_BYTES_TO_DISPLAY);
#if 0 //clark
        if (read_length != message_length)
        {
            LOG_GENERAL(WARNING, "Incorrect message length.");
            return false;
        }
#endif
        return true;
    }

    uint32_t messageLength(unsigned char* buf)
    {
        assert(buf);
        return (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8) + buf[5];
    }

} // anonymous namespace

#if 1 //clark
void P2PComm::HandleAcceptedConnection(const evpp::TCPConnPtr& conn,
                                       evpp::Buffer* msg)
#else
void P2PComm::HandleAcceptedConnection(int cli_sock, Peer from)
#endif
{
//LOG_MARKER();
#if 1 //clark
    int cli_sock = conn->fd();
    Peer from(
        uint128_t(inet_addr(conn->remote_addr()
                                .substr(0, conn->remote_addr().find(':'))
                                .c_str())),
        stoull(conn->remote_addr().substr(conn->remote_addr().find(':') + 1)));
#endif

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

    unsigned char header[HDR_LEN] = {0};

#if 1 //clark
    if (!readHeader(header, msg))
#else
    if (!readHeader(header, cli_sock, from))
#endif
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
#if 1 //clark
        HandleAcceptedConnectionBroadcast(msg, from, messageLength(header),
                                          move(cli_sock_closer));
#else
        HandleAcceptedConnectionBroadcast(cli_sock, from, messageLength(header),
                                          move(cli_sock_closer));
#endif
    }
    else if (startByte == START_BYTE_NORMAL)
    {
#if 1 //clark
        HandleAcceptedConnectionNormal(msg, from, messageLength(header),
                                       move(cli_sock_closer));
#else
        HandleAcceptedConnectionNormal(cli_sock, from, messageLength(header),
                                       move(cli_sock_closer));
#endif
    }
    else
    {
        // Unexpected start byte. Drop this message
        LOG_GENERAL(WARNING, "Header length or type wrong.");
    }
}
#if 1 //clark
void P2PComm::HandleAcceptedConnectionNormal(evpp::Buffer* msg, Peer from,
                                             uint32_t message_length,
                                             SocketCloser cli_sock_closer)
#else
void P2PComm::HandleAcceptedConnectionNormal(int cli_sock, Peer from,
                                             uint32_t message_length,
                                             SocketCloser cli_sock_closer)
#endif
{
    vector<unsigned char> message;

#if 1 //clark
    if (!readMessage(&message, msg, message_length))
#else
    if (!readMessage(&message, cli_sock, from, message_length))
#endif
    {
        return;
    }

    cli_sock_closer.reset(); // close socket now so it can be reused
    m_dispatcher(message, from);
}

#if 1 //clark
void P2PComm::HandleAcceptedConnectionBroadcast(evpp::Buffer* msg, Peer from,
                                                uint32_t message_length,
                                                SocketCloser cli_sock_closer)
#else
void P2PComm::HandleAcceptedConnectionBroadcast(int cli_sock, Peer from,
                                                uint32_t message_length,
                                                SocketCloser cli_sock_closer)
#endif
{
    unsigned char hash_buf[HASH_LEN];

#if 1 //clark
    if (!readHash(hash_buf, msg))
#else
    if (!readHash(hash_buf, cli_sock, from))
#endif
    {
        return;
    }

    const vector<unsigned char> msg_hash(hash_buf, hash_buf + HASH_LEN);

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
#if 1 //clark
            if (!readMessage(&message, msg, message_length - HASH_LEN))
#else
            if (!readMessage(&message, cli_sock, from,
                             message_length - HASH_LEN))
#endif
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

#if 1 //clark
/*
void OnMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* msg)
{
    LOG_MARKER();

    std::string s = msg->NextAllString();
    LOG_GENERAL(INFO, "Received a message [" << s << "]");
    conn->Send(s);

    if (s == "quit" || s == "exit")
    {
        conn->Close();
    }
}
*/
void OnConnection(const evpp::TCPConnPtr& conn)
{
    LOG_MARKER();

    if (conn->IsConnected())
    {
        LOG_GENERAL(INFO,
                    "Accept a new connection from " << conn->remote_addr());
    }
    else
    {
        LOG_GENERAL(INFO, "Disconnected from " << conn->remote_addr());
    }
}
#else
void P2PComm::ConnectionAccept(int serv_sock, [[gnu::unused]] short event,
                               [[gnu::unused]] void* arg)
{
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(struct sockaddr_in);

    try
    {
        int cli_sock = accept(serv_sock, (struct sockaddr*)&cli_addr, &cli_len);

        if (cli_sock < 0)
        {
            LOG_GENERAL(WARNING,
                        "Socket accept failed. Socket ret code: "
                            << cli_sock << ". TCP error code = " << errno
                            << " Desc: " << std::strerror(errno));
            LOG_GENERAL(INFO,
                        "DEBUG: I can't accept any incoming conn. I am "
                        "sleeping for "
                            << PUMPMESSAGE_MILLISECONDS << "ms");
            return;
        }

        Peer from(uint128_t(cli_addr.sin_addr.s_addr), cli_addr.sin_port);

        // LOG_GENERAL(INFO,
        //             "DEBUG: I got an incoming message from "
        // << from.GetPrintableIPAddress());

        auto func = [cli_sock, from]() -> void {
            HandleAcceptedConnection(cli_sock, from);
        };

        P2PComm::GetInstance().m_RecvPool.AddJob(func);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING, "Socket accept error" << ' ' << e.what());
    }
}
#endif

void P2PComm::StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
                               Broadcast_list_func broadcast_list_retriever)
{
    LOG_MARKER();
#if 1 //clark
    m_dispatcher = dispatcher;
    m_broadcast_list_retriever = broadcast_list_retriever;

    std::string addr = std::string("0.0.0.0:") + to_string(listen_port_host);
    evpp::EventLoop loop;
    evpp::TCPServer server(&loop, addr, "ZilliqaServer", MAXMESSAGE);
    server.SetMessageCallback(&HandleAcceptedConnection);
    server.SetConnectionCallback(&OnConnection);
    server.Init();
    server.Start();
    loop.Run();
#else
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

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(listen_port_host);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int bind_ret
        = ::bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (bind_ret < 0)
    {
        LOG_GENERAL(WARNING,
                    "Socket bind failed. Code = " << errno << " Desc: "
                                                  << std::strerror(errno));
        return;
    }

    listen(serv_sock, 5000);

    struct event_base* base = event_base_new();
    struct event ev;
    m_dispatcher = dispatcher;
    m_broadcast_list_retriever = broadcast_list_retriever;
    event_set(&ev, serv_sock, EV_READ | EV_PERSIST, ConnectionAccept, nullptr);
    event_base_set(base, &ev);
    event_add(&ev, nullptr);
    event_base_dispatch(base);

    close(serv_sock);
    event_del(&ev);
    event_base_free(base);
#endif
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
