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

using namespace std;
using namespace boost::multiprecision;

const unsigned char START_BYTE_NORMAL = 0x11;
const unsigned char START_BYTE_BROADCAST = 0x22;
const unsigned int HDR_LEN = 5;
const unsigned int HASH_LEN = 32;
const unsigned int BROADCAST_EXPIRY_SECONDS = 600;

/// Comparison operator for ordering the list of message hashes.
struct hash_compare
{
    bool operator()(const vector<unsigned char>& l,
                    const vector<unsigned char>& r)
    {
        return equal(l.begin(), l.end(), r.begin());
    }
};

struct ConnectionData
{
    std::function<void(const std::vector<unsigned char>&, const Peer&)>
        dispatcher;
    broadcast_list_func broadcast_list_retriever;
};

static void close_socket(int* cli_sock)
{
    if (cli_sock != NULL)
    {
        shutdown(*cli_sock, SHUT_RDWR);
        close(*cli_sock);
    }
}

P2PComm::P2PComm() {}

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
    LOG_MARKER();
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
        // 0x11 - start byte
        // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
        // <message>

        // 0x22 - start byte (broadcast)
        // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
        // <32-byte hash> <message>

        // 0x33 - start byte (report)
        // 0x00 0x00 0x00 0x01 - 4-byte length of message
        // 0x00
        uint32_t length = message.size();
        if (start_byte == START_BYTE_BROADCAST)
        {
            length += HASH_LEN;
        }
        unsigned char buf[HDR_LEN]
            = {start_byte, (unsigned char)((length >> 24) & 0xFF),
               (unsigned char)((length >> 16) & 0xFF),
               (unsigned char)((length >> 8) & 0xFF),
               (unsigned char)(length & 0xFF)};
        uint32_t written_length = 0;

        while (written_length != HDR_LEN)
        {
            int n = write(cli_sock, buf + written_length,
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
                int n = write(cli_sock, &msg_hash.at(0) + written_length,
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
                int n = write(cli_sock, &message.at(0) + written_length,
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
    LOG_MARKER();
    lock_guard<mutex> guard(m_broadcastCoreMutex);

    SendMessagePoolHelper<START_BYTE_BROADCAST>(peers, message, message_hash);
    // TODO: are we sure there wont be many threads arising from this, will ThreadPool alleviate it?
    // Launch a separate, detached thread to automatically remove the hash from the list after a long time period has elapsed
    auto func2 = [this, message_hash]() -> void {
        vector<unsigned char> msg_hash_copy(message_hash);
        this_thread::sleep_for(chrono::seconds(BROADCAST_EXPIRY_SECONDS));
        lock_guard<mutex> guard(m_broadcastHashesMutex);
        m_broadcastHashes.erase(msg_hash_copy);
        LOG_PAYLOAD(INFO, "Removing msg hash from broadcast list",
                    msg_hash_copy, Logger::MAX_BYTES_TO_DISPLAY);
    };

    DetachedFunction(1, func2);
}

void P2PComm::HandleAcceptedConnection(
    int cli_sock, Peer from,
    function<void(const vector<unsigned char>&, const Peer&)> dispatcher,
    broadcast_list_func broadcast_list_retriever)
{
    LOG_MARKER();

    unique_ptr<int, void (*)(int*)> cli_sock_closer(&cli_sock, close_socket);

    LOG_GENERAL(INFO, "Incoming message from " << from);

    vector<unsigned char> message;

    // Reception format:
    // 0x11 - start byte
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
    // <message>

    // 0x22 - start byte (broadcast)
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
    // <32-byte hash> <message>

    // 0x33 - start byte (report)
    // 0x00 0x00 0x00 0x01 - 4-byte length of message
    // 0x00

    unsigned char buf[HDR_LEN] = {0};
    uint32_t read_length = 0;

    // Read out just the header first
    while (read_length != HDR_LEN)
    {
        int n = read(cli_sock, buf + read_length, HDR_LEN - read_length);
        if (n <= 0)
        {
            LOG_GENERAL(WARNING,
                        "Socket read failed. Code = "
                            << errno << " Desc: " << std::strerror(errno)
                            << ". IP address: " << from);
            return;
        }
        read_length += n;
    }

    if (!((read_length == HDR_LEN)
          && ((buf[0] == START_BYTE_NORMAL)
              || (buf[0] == START_BYTE_BROADCAST))))
    {
        LOG_GENERAL(WARNING, "Header length or type wrong.");
        return;
    }

    uint32_t message_length = 0;
    message_length = (buf[1] << 24) + (buf[2] << 16) + (buf[3] << 8) + buf[4];

    unsigned char hash_buf[HASH_LEN];
    if (buf[0] == START_BYTE_BROADCAST)
    {
        read_length = 0;
        while (read_length != HASH_LEN)
        {
            int n = read(cli_sock, hash_buf + read_length,
                         HASH_LEN - read_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);
                return;
            }
            read_length += n;
        }

        // Check if this message has been received before
        bool found = false;
        {
            lock_guard<mutex> guard(
                P2PComm::GetInstance().m_broadcastHashesMutex);
            vector<unsigned char> msg_hash(hash_buf, hash_buf + HASH_LEN);
            found = (P2PComm::GetInstance().m_broadcastHashes.find(msg_hash)
                     != P2PComm::GetInstance().m_broadcastHashes.end());
            // While we have the lock, we should quickly add the hash
            if (!found)
            {
                // Read the rest of the message
                read_length = 0;
                message.resize(message_length - HASH_LEN);
                while (read_length != message_length - HASH_LEN)
                {
                    int n = read(cli_sock, &message.at(read_length),
                                 message_length - HASH_LEN - read_length);
                    if (n <= 0)
                    {
                        LOG_GENERAL(WARNING,
                                    "Socket read failed. Code = "
                                        << errno
                                        << " Desc: " << std::strerror(errno)
                                        << ". IP address: " << from);
                        return;
                    }
                    read_length += n;
                }

                LOG_PAYLOAD(INFO, "Message received", message,
                            Logger::MAX_BYTES_TO_DISPLAY);

                if (read_length != message_length - HASH_LEN)
                {
                    LOG_GENERAL(WARNING, "Incorrect message length.");
                    return;
                }

                SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
                sha256.Update(message);
                vector<unsigned char> this_msg_hash = sha256.Finalize();

                if (this_msg_hash == msg_hash)
                {
                    P2PComm::GetInstance().m_broadcastHashes.insert(
                        this_msg_hash);
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
        else
        {
            unsigned char msg_type = 0xFF;
            unsigned char ins_type = 0xFF;
            if (message.size() > MessageOffset::INST)
            {
                msg_type = message.at(MessageOffset::TYPE);
                ins_type = message.at(MessageOffset::INST);
            }
            vector<Peer> broadcast_list
                = broadcast_list_retriever(msg_type, ins_type, from);
            if (broadcast_list.size() > 0)
            {
                vector<unsigned char> this_msg_hash(hash_buf,
                                                    hash_buf + HASH_LEN);
                P2PComm::GetInstance().SendBroadcastMessageCore(
                    broadcast_list, message, this_msg_hash);
            }

#ifdef STAT_TEST
            vector<unsigned char> this_msg_hash(hash_buf, hash_buf + HASH_LEN);
            LOG_STATE(
                "[BROAD]["
                << std::setw(15) << std::left
                << P2PComm::GetInstance().m_selfPeer << "]["
                << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
                << "] RECV");
#endif // STAT_TEST

            // Dispatch message normally
            dispatcher(message, from);
        }
    }
    else
    {
        // Read the rest of the message
        read_length = 0;
        message.resize(message_length);
        while (read_length != message_length)
        {
            int n = read(cli_sock, &message.at(read_length),
                         message_length - read_length);
            if (n <= 0)
            {
                LOG_GENERAL(WARNING,
                            "Socket read failed. Code = "
                                << errno << " Desc: " << std::strerror(errno)
                                << ". IP address: " << from);
                return;
            }
            read_length += n;
        }

        LOG_PAYLOAD(INFO, "Message received", message,
                    Logger::MAX_BYTES_TO_DISPLAY);
        if (read_length != message_length)
        {
            LOG_GENERAL(WARNING, "Incorrect message length.");
            return;
        }

        cli_sock_closer.reset(); // close socket now so it can be reused
        dispatcher(message, from);
    }
}

void P2PComm::ConnectionAccept(int serv_sock, short event, void* arg)
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

        LOG_GENERAL(INFO,
                    "DEBUG: I got an incoming message from "
                        << from.GetPrintableIPAddress());

        function<void(const vector<unsigned char>&, const Peer&)> dispatcher
            = ((ConnectionData*)arg)->dispatcher;
        broadcast_list_func broadcast_list_retriever
            = ((ConnectionData*)arg)->broadcast_list_retriever;
        auto func
            = [cli_sock, from, dispatcher, broadcast_list_retriever]() -> void {
            HandleAcceptedConnection(cli_sock, from, dispatcher,
                                     broadcast_list_retriever);
        };

        P2PComm::GetInstance().m_RecvPool.AddJob(func);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING, "Socket accept error" << ' ' << e.what());
    }
}

void P2PComm::StartMessagePump(
    uint32_t listen_port_host,
    function<void(const vector<unsigned char>&, const Peer&)> dispatcher,
    broadcast_list_func broadcast_list_retriever)
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
    ConnectionData* pConnData = new struct ConnectionData;
    pConnData->dispatcher = dispatcher;
    pConnData->broadcast_list_retriever = broadcast_list_retriever;
    event_set(&ev, serv_sock, EV_READ | EV_PERSIST, ConnectionAccept,
              pConnData);
    event_base_set(base, &ev);
    event_add(&ev, nullptr);
    event_base_dispatch(base);

    close(serv_sock);
    delete pConnData;
    event_del(&ev);
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

#ifdef STAT_TEST
    LOG_STATE("[BROAD]["
              << std::setw(15) << std::left
              << m_selfPeer.GetPrintableIPAddress() << "]["
              << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
              << "] BEGN");
#endif // STAT_TEST

    SendBroadcastMessageCore(peers, message, this_msg_hash);

#ifdef STAT_TEST
    LOG_STATE("[BROAD]["
              << std::setw(15) << std::left
              << m_selfPeer.GetPrintableIPAddress() << "]["
              << DataConversion::Uint8VecToHexStr(this_msg_hash).substr(0, 6)
              << "] DONE");
#endif // STAT_TEST
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

#ifdef STAT_TEST
void P2PComm::SetSelfPeer(const Peer& self) { m_selfPeer = self; }
#endif // STAT_TEST
