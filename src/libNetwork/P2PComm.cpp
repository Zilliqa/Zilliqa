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
const unsigned int HDR_LEN = 6;
const unsigned int HASH_LEN = 32;
const unsigned int BROADCAST_EXPIRY_SECONDS = 600;

static const bool USE_GOSSIP = false;

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
    P2PComm::Dispatcher dispatcher;
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

P2PComm::P2PComm()
{
    if (USE_GOSSIP)
    {
        startTimer();
    }
}

P2PComm::~P2PComm() {}

P2PComm& P2PComm::GetInstance()
{
    static P2PComm comm;
    return comm;
}

class P2PComm::AtomicGossiperActions
{
public:
    Gossiper::Actions actions;
    AtomicGossiperPtr gossiper;
};

class P2PComm::AtomicGossiper
    : public enable_shared_from_this<P2PComm::AtomicGossiper>
{
    AtomicGossiper(unsigned broadcastExpirySeconds,
                   const shared_ptr<vector<unsigned char>>& message,
                   const shared_ptr<vector<unsigned char>>& hash)
        : m_gossiper(broadcastExpirySeconds)
        , m_message(message)
        , m_hash(hash)
    // Should be private to disallow creation on the stack. We want to only
    // have `shared_ptr`-enabled instances, created using `static create()`.
    {
        assert(m_message);
        assert(hash);
    }

public:
    static AtomicGossiperPtr
    create(unsigned broadcastExpirySeconds,
           const shared_ptr<vector<unsigned char>>& message,
           const shared_ptr<vector<unsigned char>>& hash)
    {
        return shared_ptr<AtomicGossiper>(
            new AtomicGossiper(broadcastExpirySeconds, message, hash));
    }

    const AtomicGossiperActions broadcast(Gossiper::Time now)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return {m_gossiper.broadcast(m_peersToIds.size(), now),
                shared_from_this()};
    }

    const AtomicGossiperActions onRumorReceived(int peerId, Gossiper::Time now)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return {m_gossiper.onRumorReceived(peerId, m_peersToIds.size(), now),
                shared_from_this()};
    }

    const AtomicGossiperActions tick(Gossiper::Time now)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return {m_gossiper.tick(now), shared_from_this()};
    }

    template<typename Container> void unionPeers(const Container& peers)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto& peer : peers)
        {
            auto theId = m_peersToIds.size();
            const auto& it = m_peersToIds.insert(make_pair(peer, theId));
            if (it.second)
            {
                m_idsToPeers[theId] = peer;
            }
        }
    }

    int peerId(const Peer& peer) const
    {
        // The only reason this should work without race conditions,
        // in spite of concurrency, is that it's a monotonic container.
        std::lock_guard<std::mutex> guard(m_mutex);
        const auto& it = m_peersToIds.find(peer);
        return (it == m_peersToIds.end() ? -1 : it->second);
    }

    bool peerFromId(Peer* peer, int id) const
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        assert(peer);
        const auto& it = m_idsToPeers.find(id);
        if (it == m_idsToPeers.end())
        {
            return false;
        }
        *peer = it->second;
        return true;
    }

    const shared_ptr<vector<unsigned char>>& message() const
    {
        return m_message;
    };
    const shared_ptr<vector<unsigned char>>& hash() const { return m_hash; };

private:
    mutable std::mutex m_mutex;

    Gossiper m_gossiper;
    const shared_ptr<vector<unsigned char>> m_message;
    const shared_ptr<vector<unsigned char>> m_hash;

    unordered_map<Peer, int> m_peersToIds;
    unordered_map<int, Peer> m_idsToPeers;
};

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
    LOG_MARKER();
    lock_guard<mutex> guard(m_broadcastCoreMutex);

    SendMessagePoolHelper<START_BYTE_BROADCAST>(peers, message, message_hash);
}

void P2PComm::ClearBroadcastHashAsync(const vector<unsigned char>& message_hash)
{
    LOG_MARKER();
    // TODO: are we sure there wont be many threads arising from this, will ThreadPool alleviate it?
    // Launch a separate, detached thread to automatically remove the hash from the list after a long time period has elapsed
    auto func2 = [this, message_hash]() -> void {
        this_thread::sleep_for(chrono::seconds(BROADCAST_EXPIRY_SECONDS));
        lock_guard<mutex> guard2(m_broadcastHashesMutex);
        m_broadcastHashes.erase(message_hash);
        LOG_PAYLOAD(INFO, "Removing msg hash from broadcast list", message_hash,
                    Logger::MAX_BYTES_TO_DISPLAY);
    };

    DetachedFunction(1, func2);
}

namespace
{

    bool readMessage(vector<unsigned char>* message, int cli_sock, Peer from,
                     uint32_t message_length)
    {
        // Read the rest of the message
        assert(message);
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

        LOG_PAYLOAD(INFO, "Message received", *message,
                    Logger::MAX_BYTES_TO_DISPLAY);

        if (read_length != message_length)
        {
            LOG_GENERAL(WARNING, "Incorrect message length.");
            return false;
        }

        return true;
    }

} // anonymous namespace

void P2PComm::HandleAcceptedConnection(
    int cli_sock, Peer from, Dispatcher dispatcher,
    broadcast_list_func broadcast_list_retriever)
{
    LOG_MARKER();
    LOG_GENERAL(INFO, "Incoming message from " << from);

    unique_ptr<int, void (*)(int*)> cli_sock_closer(&cli_sock, close_socket);

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

    unsigned char buf[HDR_LEN] = {0};
    {

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
                return;
            }
            read_length += n;
        }

        if (read_length == HDR_LEN)
        {
            // If received version doesn't match expected version (defined in constant file), drop this message
            if (buf[0] != (unsigned char)(MSG_VERSION & 0xFF))
            {
                LOG_GENERAL(WARNING,
                            "Header version wrong, received ["
                                << buf[0] - 0x00 << "] while expected ["
                                << MSG_VERSION << "].");
                return;
            }

            // If received start byte is not allowed, drop this message
            if ((buf[1] != START_BYTE_NORMAL)
                && (buf[1] != START_BYTE_BROADCAST))
            {
                LOG_GENERAL(WARNING, "Header length or type wrong.");
                return;
            }
        }
    }

    uint32_t message_length
        = (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8) + buf[5];

    if (buf[1] == START_BYTE_BROADCAST)
    {
        HandleAcceptedConnectionBroadcast(
            cli_sock, from, dispatcher, broadcast_list_retriever,
            message_length, move(cli_sock_closer));
        return;
    }

    // Non-broadcast case - regular messages
    vector<unsigned char> message;
    if (!readMessage(&message, cli_sock, from, message_length))
    {
        return;
    }

    cli_sock_closer.reset(); // close socket now so it can be reused

    dispatcher(message, from);
}

void P2PComm::HandleAcceptedConnectionBroadcast(
    int cli_sock, Peer from, Dispatcher dispatcher,
    broadcast_list_func broadcast_list_retriever, uint32_t message_length,
    unique_ptr<int, void (*)(int*)> cli_sock_closer)
{
    unsigned char hash_buf[HASH_LEN];
    {
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
                return;
            }
            read_length += n;
        }
    }

    const vector<unsigned char> msg_hash(hash_buf, hash_buf + HASH_LEN);

    P2PComm& instance = P2PComm::GetInstance();

    // Check if this message has been received before
    if (!USE_GOSSIP)
    {
        lock_guard<mutex> guard(instance.m_broadcastHashesMutex);
        if (instance.m_broadcastHashes.find(msg_hash)
            != instance.m_broadcastHashes.end())
        {
            // We already sent and/or received this message before -> discard
            LOG_GENERAL(INFO, "Discarding duplicate broadcast message");
            return;
        }
    }

    vector<unsigned char> message;
    if (!readMessage(&message, cli_sock, from, message_length - HASH_LEN))
    {
        return;
    }

    cli_sock_closer.reset(); // close socket now so it can be reused

    const auto this_msg_hash = shaMessage(message);

    if (this_msg_hash != msg_hash)
    {
        LOG_GENERAL(WARNING, "Incorrect message hash.");
        return;
    }

    auto populateList = [&]() {
        unsigned char msg_type = 0xFF;
        unsigned char ins_type = 0xFF;
        if (message.size() > MessageOffset::INST)
        {
            msg_type = message.at(MessageOffset::TYPE);
            ins_type = message.at(MessageOffset::INST);
        }
        return broadcast_list_retriever(msg_type, ins_type, from);
    };

    if (USE_GOSSIP)
    {
        const auto hash = shaMessage(message);

        auto gossiper = instance.gossiperForMessage(message, hash);
        assert(gossiper);

        gossiper->unionPeers(populateList());

        instance.perform(gossiper->onRumorReceived(
            gossiper->peerId(from), std::chrono::steady_clock::now()));

        return;
    }

    // Non-gossip (legacy) case follows:
    {
        lock_guard<mutex> guard(instance.m_broadcastHashesMutex);
        const auto res = instance.m_broadcastHashes.insert(this_msg_hash);
        if (!res.second)
        {
            // We already sent and/or received this message before -> discard
            LOG_GENERAL(INFO, "Discarding duplicate broadcast message");
            return;
        }
    }

    const vector<Peer> broadcast_list = populateList();
    if (broadcast_list.size() > 0)
    {
        instance.SendBroadcastMessageCore(broadcast_list, message, msg_hash);
    }

    instance.ClearBroadcastHashAsync(this_msg_hash);

    LOG_STATE(
        "[BROAD][" << std::setw(15) << std::left << instance.m_selfPeer << "]["
                   << DataConversion::Uint8VecToHexStr(msg_hash).substr(0, 6)
                   << "] RECV");

    // Dispatch message normally
    dispatcher(message, from);
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

        Dispatcher dispatcher = ((ConnectionData*)arg)->dispatcher;
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

void P2PComm::StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
                               broadcast_list_func broadcast_list_retriever)
{
    LOG_MARKER();

    assert(!m_dispatcher); // Implicitly. This method can be called only once.

    m_dispatcher = dispatcher;

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
    static ConnectionData* pConnData = new struct ConnectionData;
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

P2PComm::ShaMessage P2PComm::shaMessage(const vector<unsigned char>& message)
{
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
    sha256.Update(message);
    return sha256.Finalize();
}

template<typename Container>
void P2PComm::SendBroadcastMessageHelper(
    const Container& peers, const std::vector<unsigned char>& message)
{
    if (peers.empty())
    {
        return;
    }

    const auto this_msg_hash = shaMessage(message);

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

void P2PComm::startTimer()
{
    DetachedFunction(1, [this]() -> void {
        while (true)
        {
            this_thread::sleep_for(chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();

            vector<AtomicGossiperActions> todo;

            {
                lock_guard<mutex> guard(m_gossipersMutex);
                for (auto& i : m_gossipers)
                {
                    auto& gossiper = i.second;
                    assert(gossiper);
                    todo.push_back(gossiper->tick(now));
                }
            }

            for (auto& actions : todo)
            {
                perform(actions);
            }
        }
    });
}

P2PComm::AtomicGossiperPtr
P2PComm::gossiperForMessage(const std::vector<unsigned char>& message,
                            const ShaMessage& hash)
{
    lock_guard<mutex> guard(m_gossipersMutex);

    const auto& it = m_gossipers.find(hash);
    if (it != m_gossipers.end())
    {
        return it->second;
    }

    return m_gossipers[hash]
        = AtomicGossiper::create(BROADCAST_EXPIRY_SECONDS,
                                 make_shared<vector<unsigned char>>(message),
                                 make_shared<vector<unsigned char>>(hash));
}

void P2PComm::perform(const AtomicGossiperActions& actions)
{
    auto& gossiper = actions.gossiper;
    assert(gossiper);

    for (auto& action : actions.actions)
    {
        switch (action.what)
        {
        case Gossiper::Action::SendToPeer:
        {
            Peer peer;
            const bool found = gossiper->peerFromId(&peer, action.peerId);
            // Don't send message to unknown peers. (Most likely a be a bug).
            assert(found);
            if (found)
            {
                m_SendPool.AddJob([this, peer, gossiper]() mutable -> void {
                    SendMessageCore(peer, *gossiper->message(),
                                    START_BYTE_BROADCAST, *gossiper->hash());
                });
            }
        }
        break;
        case Gossiper::Action::DropDuplicate:
        {
            LOG_PAYLOAD(WARNING, "Attempted to send twice",
                        *gossiper->message(), Logger::MAX_BYTES_TO_DISPLAY);
        }
        break;
        case Gossiper::Action::Reset:
        {
            lock_guard<mutex> guard(m_gossipersMutex);

            // Note: Despite erasing here, it won't be instantly destructed.
            //       There is at least one further reference, since we hold
            //       a reference in the local `gossiper` variable. This means
            //       that the first opportunity for gossiper to be destructed
            //       is after the end of this method (unless there are even
            //       further references).
            const size_t erased = m_gossipers.erase(*gossiper->hash());
            assert(erased == 1); // It's gossiper's responsibility to not
            (void)erased; // double-erase.

            if (!action.success)
            {
                LOG_PAYLOAD(WARNING, "Failed to broadcast",
                            *gossiper->message(), Logger::MAX_BYTES_TO_DISPLAY);
            }
        }
        break;
        case Gossiper::Action::Dispatch:
        {
            Peer from;
            const bool found = gossiper->peerFromId(&from, action.peerId);
            assert(
                found); // Don't dispatch using unknown peers. (Most likely a be a bug).
            if (found)
            {
                m_dispatcher(*gossiper->message(), from);
            }
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

template<typename Container>
void P2PComm::SendBroadcastMessageGossip(
    const Container& peers, const std::vector<unsigned char>& message)
{
    const auto hash = shaMessage(message);

    auto gossiper = gossiperForMessage(message, hash);
    assert(gossiper);

    gossiper->unionPeers(peers);

    perform(gossiper->broadcast(std::chrono::steady_clock::now()));
}

void P2PComm::SendBroadcastMessage(const vector<Peer>& peers,
                                   const vector<unsigned char>& message)
{
    LOG_MARKER();
    if (USE_GOSSIP)
    {
        SendBroadcastMessageGossip(peers, message);
    }
    else
    {
        SendBroadcastMessageHelper(peers, message);
    }
}

void P2PComm::SendBroadcastMessage(const deque<Peer>& peers,
                                   const vector<unsigned char>& message)
{
    LOG_MARKER();
    if (USE_GOSSIP)
    {
        SendBroadcastMessageGossip(peers, message);
    }
    else
    {
        SendBroadcastMessageHelper(peers, message);
    }
}

void P2PComm::SetSelfPeer(const Peer& self) { m_selfPeer = self; }
