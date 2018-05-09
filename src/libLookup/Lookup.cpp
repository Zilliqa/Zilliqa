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

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <exception>
#include <fstream>
#include <netinet/in.h>
#include <random>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_set>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "Lookup.h"
#include "common/Messages.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"
#include "libData/BlockData/Block.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

Lookup::Lookup(Mediator& mediator)
    : m_mediator(mediator)
{
    SetLookupNodes();
#ifdef IS_LOOKUP_NODE
    SetDSCommitteInfo();
#endif // IS_LOOKUP_NODE
}

Lookup::~Lookup() {}

void Lookup::AppendTimestamp(vector<unsigned char>& message,
                             unsigned int& offset)
{
    // Append a sending time to avoid message to be discarded
    uint256_t milliseconds_since_epoch
        = std::chrono::system_clock::now().time_since_epoch()
        / std::chrono::seconds(1);

    Serializable::SetNumber<uint256_t>(message, offset,
                                       milliseconds_since_epoch, UINT256_SIZE);

    offset += UINT256_SIZE;
}

void Lookup::SetLookupNodes()
{
    LOG_MARKER();

    m_lookupNodes.clear();
    m_lookupNodesOffline.clear();
    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml("constants.xml", pt);

    for (const ptree::value_type& v : pt.get_child("node.lookups"))
    {
        if (v.first == "peer")
        {
            struct in_addr ip_addr;
            inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
            Peer lookup_node((uint128_t)ip_addr.s_addr,
                             v.second.get<uint32_t>("port"));
            m_lookupNodes.push_back(lookup_node);
        }
    }
}

vector<Peer> Lookup::GetLookupNodes()
{
    LOG_MARKER();

    return m_lookupNodes;
}

void Lookup::SendMessageToLookupNodes(
    const std::vector<unsigned char>& message) const
{
    LOG_MARKER();

    // LOG_GENERAL(INFO, "i am here " << to_string(m_mediator.m_currentEpochNum).c_str())
    vector<Peer> allLookupNodes;

    for (auto node : m_lookupNodes)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Sending msg to lookup node " << node.GetPrintableIPAddress()
                                                << ":"
                                                << node.m_listenPortHost);

        allLookupNodes.push_back(node);
    }

    P2PComm::GetInstance().SendBroadcastMessage(allLookupNodes, message);
}

void Lookup::SendMessageToLookupNodesSerial(
    const std::vector<unsigned char>& message) const
{
    LOG_MARKER();

    // LOG_GENERAL("i am here " << to_string(m_mediator.m_currentEpochNum).c_str())
    vector<Peer> allLookupNodes;

    for (auto node : m_lookupNodes)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Sending msg to lookup node " << node.GetPrintableIPAddress()
                                                << ":"
                                                << node.m_listenPortHost);

        allLookupNodes.push_back(node);
    }

    P2PComm::GetInstance().SendMessage(allLookupNodes, message);
}

void Lookup::SendMessageToRandomLookupNode(
    const std::vector<unsigned char>& message) const
{
    LOG_MARKER();

    // int index = rand() % (NUM_LOOKUP_USE_FOR_SYNC) + m_lookupNodes.size()
    // - NUM_LOOKUP_USE_FOR_SYNC;
    int index = rand() % m_lookupNodes.size();

    P2PComm::GetInstance().SendMessage(m_lookupNodes[index], message);
}

void Lookup::SendMessageToSeedNodes(
    const std::vector<unsigned char>& message) const
{
    LOG_MARKER();

    for (auto node : m_seedNodes)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Sending msg to seed node " << node.GetPrintableIPAddress()
                                              << ":" << node.m_listenPortHost);
        P2PComm::GetInstance().SendMessage(node, message);
    }
}

bool Lookup::GetSeedPeersFromLookup()
{
    LOG_MARKER();

    vector<unsigned char> getSeedPeersMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETSEEDPEERS};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getSeedPeersMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    SendMessageToRandomLookupNode(getSeedPeersMessage);

    return true;
}

vector<unsigned char> Lookup::ComposeGetDSInfoMessage()
{
    LOG_MARKER();

    // getDSNodesMessage = [Port]
    vector<unsigned char> getDSNodesMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETDSINFOFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getDSNodesMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getDSNodesMessage;
}

vector<unsigned char> Lookup::ComposeGetStateMessage()
{
    LOG_MARKER();

    // getStateMessage = [Port]
    vector<unsigned char> getStateMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETSTATEFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getStateMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getStateMessage;
}

bool Lookup::GetDSInfoFromSeedNodes()
{
    LOG_MARKER();
    SendMessageToSeedNodes(ComposeGetDSInfoMessage());
    return true;
}

bool Lookup::GetDSInfoFromLookupNodes()
{
    LOG_MARKER();
    SendMessageToRandomLookupNode(ComposeGetDSInfoMessage());
    return true;
}

bool Lookup::GetStateFromLookupNodes()
{
    LOG_MARKER();
    SendMessageToRandomLookupNode(ComposeGetStateMessage());

    return true;
}

vector<unsigned char> Lookup::ComposeGetDSBlockMessage(uint256_t lowBlockNum,
                                                       uint256_t highBlockNum)
{
    LOG_MARKER();

    // getDSBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getDSBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETDSBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset,
                                       lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset,
                                       highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint32_t>(getDSBlockMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getDSBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetDSBlockFromSeedNodes(uint256_t lowBlockNum,
                                     uint256_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToSeedNodes(ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetDSBlockFromLookupNodes(uint256_t lowBlockNum,
                                       uint256_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToRandomLookupNode(
        ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

vector<unsigned char> Lookup::ComposeGetTxBlockMessage(uint256_t lowBlockNum,
                                                       uint256_t highBlockNum)
{
    LOG_MARKER();

    // getTxBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getTxBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETTXBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(getTxBlockMessage, curr_offset,
                                       lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint256_t>(getTxBlockMessage, curr_offset,
                                       highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint32_t>(getTxBlockMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getTxBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetTxBlockFromSeedNodes(uint256_t lowBlockNum,
                                     uint256_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToSeedNodes(ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetTxBlockFromLookupNodes(uint256_t lowBlockNum,
                                       uint256_t highBlockNum)
{
    LOG_MARKER();

    SendMessageToRandomLookupNode(
        ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));

    return true;
}

bool Lookup::GetTxBodyFromSeedNodes(string txHashStr)
{
    LOG_MARKER();

    // getTxBodyMessage = [TRAN_HASH_SIZE txHashStr][4-byte Port]
    vector<unsigned char> getTxBodyMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETTXBODYFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    std::array<unsigned char, TRAN_HASH_SIZE> hash
        = DataConversion::HexStrToStdArray(txHashStr);

    getTxBodyMessage.resize(curr_offset + TRAN_HASH_SIZE);

    copy(hash.begin(), hash.end(), getTxBodyMessage.begin() + curr_offset);
    curr_offset += TRAN_HASH_SIZE;

    Serializable::SetNumber<uint32_t>(getTxBodyMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    SendMessageToSeedNodes(getTxBodyMessage);

    return true;
}

#ifdef IS_LOOKUP_NODE
bool Lookup::SetDSCommitteInfo()
{
    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml("config.xml", pt);

    lock(m_mediator.m_mutexDSCommitteeNetworkInfo,
         m_mediator.m_mutexDSCommitteePubKeys);
    lock_guard<mutex> g(m_mediator.m_mutexDSCommitteeNetworkInfo, adopt_lock);
    lock_guard<mutex> g2(m_mediator.m_mutexDSCommitteePubKeys, adopt_lock);

    for (ptree::value_type const& v : pt.get_child("nodes"))
    {
        if (v.first == "peer")
        {
            PubKey key(
                DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")),
                0);
            m_mediator.m_DSCommitteePubKeys.push_back(key);

            struct in_addr ip_addr;
            inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
            Peer peer((uint128_t)ip_addr.s_addr,
                      v.second.get<unsigned int>("port"));
            m_mediator.m_DSCommitteeNetworkInfo.push_back(peer);
        }
    }

    return true;
}

vector<map<PubKey, Peer>> Lookup::GetShardPeers()
{
    lock_guard<mutex> g(m_mutexShards);
    return m_shards;
}

vector<Peer> Lookup::GetNodePeers()
{
    lock_guard<mutex> g(m_mutexNodesInNetwork);
    return m_nodesInNetwork;
}
#endif // IS_LOOKUP_NODE

bool Lookup::ProcessEntireShardingStructure(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();

#ifdef IS_LOOKUP_NODE
    // Sharding structure message format:

    // [4-byte num of shards]
    // [4-byte shard size]
    //   [33-byte public key][16-byte IP][4-byte port]
    //   [33-byte public key][16-byte IP][4-byte port]
    //   ...
    // [4-byte shard size]
    //   [33-byte public key][16-byte IP][4-byte port]
    //   [33-byte public key][16-byte IP][4-byte port]
    //   ...
    // ...
    LOG_GENERAL(INFO, "[LOOKUP received sharding structure]");

    unsigned int length_available = message.size() - offset;

    if (length_available < 4)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    // unsigned int view change count
    unsigned int viewChangeCount = Serializable::GetNumber<uint32_t>(
        message, offset, sizeof(unsigned int));
    offset += sizeof(unsigned int);

    for (unsigned int i = 0; i < viewChangeCount; i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();
        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }

    // 4-byte num of shards
    uint32_t num_shards
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    LOG_GENERAL(INFO, "Number of shards: " << to_string(num_shards));

    lock(m_mutexShards, m_mutexNodesInNetwork);
    lock_guard<mutex> g(m_mutexShards, adopt_lock);
    lock_guard<mutex> h(m_mutexNodesInNetwork, adopt_lock);

    m_shards.clear();
    m_nodesInNetwork.clear();
    std::unordered_set<Peer> t_nodesInNetwork;

    for (unsigned int i = 0; i < num_shards; i++)
    {
        length_available = message.size() - offset;

        if (length_available < 4)
        {
            LOG_GENERAL(WARNING, "Malformed message");
            return false;
        }

        m_shards.push_back(map<PubKey, Peer>());

        // 4-byte shard size
        uint32_t shard_size = Serializable::GetNumber<uint32_t>(
            message, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        length_available = message.size() - offset;

        LOG_GENERAL(INFO,
                    "Size of shard " << to_string(i) << ": "
                                     << to_string(shard_size));

        if (length_available < (33 + 16 + 4) * shard_size)
        {
            LOG_GENERAL(WARNING, "Malformed message");
            return false;
        }

        map<PubKey, Peer>& shard = m_shards.at(i);

        for (unsigned int j = 0; j < shard_size; j++)
        {
            // 33-byte public key
            PubKey key = PubKey(message, offset);
            offset += PUB_KEY_SIZE;

            // 16-byte IP + 4-byte port
            // Peer peer = Peer(message, offset);
            Peer peer;
            if (peer.Deserialize(message, offset) != 0)
            {
                LOG_GENERAL(WARNING, "We failed to deserialize Peer.");
                return false;
            }

            offset += IP_SIZE + PORT_SIZE;

            shard.insert(make_pair(key, peer));

            m_nodesInNetwork.push_back(peer);
            t_nodesInNetwork.insert(peer);

            LOG_GENERAL(INFO,
                        "[SHARD "
                            << to_string(i) << "] "
                            << "[PEER " << to_string(j) << "] "
                            << "Inserting Pubkey to shard : " << string(key));
            LOG_GENERAL(INFO,
                        "[SHARD " << to_string(i) << "] "
                                  << "[PEER " << to_string(j) << "] "
                                  << "Corresponding peer : " << string(peer));
        }
    }

    for (auto peer : t_nodesInNetwork)
    {
        if (!l_nodesInNetwork.erase(peer))
        {
            LOG_STATE("[JOINPEER]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << std::setw(6) << std::left
                      << m_mediator.m_currentEpochNum << "][" << std::setw(4)
                      << std::left << m_mediator.GetNodeMode(peer) << "]"
                      << string(peer));
        }
    }

    for (auto peer : l_nodesInNetwork)
    {
        LOG_STATE("[LOSTPEER]["
                  << std::setw(15) << std::left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << std::setw(6) << std::left << m_mediator.m_currentEpochNum
                  << "][" << std::setw(4) << std::left
                  << m_mediator.GetNodeMode(peer) << "]" << string(peer));
    }

    l_nodesInNetwork = t_nodesInNetwork;

#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           const Peer& from)
{
    LOG_MARKER();

#ifdef IS_LOOKUP_NODE
    // Message = [4-byte listening port]

    const unsigned int length_available = message.size() - offset;
    const unsigned int min_length_needed = sizeof(uint32_t);

    if (min_length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    // 4-byte listening port
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock_guard<mutex> g(m_mutexNodesInNetwork);

    uint32_t numPeersInNetwork = m_nodesInNetwork.size();

    if (numPeersInNetwork < SEED_PEER_LIST_SIZE)
    {
        LOG_GENERAL(WARNING,
                    "[Lookup Node] numPeersInNetwork < SEED_PEER_LIST_SIZE");
        return false;
    }

    vector<unsigned char> seedPeersMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETSEEDPEERS};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(seedPeersMessage, curr_offset,
                                      SEED_PEER_LIST_SIZE, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Which of the following two implementations is more efficient and parallelizable?
    // ================================================

    unordered_set<uint32_t> indicesAlreadyAdded;

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, numPeersInNetwork - 1);

    for (unsigned int i = 0; i < SEED_PEER_LIST_SIZE; i++)
    {
        uint32_t index = dis(gen);
        while (indicesAlreadyAdded.find(index) != indicesAlreadyAdded.end())
        {
            index = dis(gen);
        }
        indicesAlreadyAdded.insert(index);

        Peer candidateSeed = m_nodesInNetwork[index];

        candidateSeed.Serialize(seedPeersMessage, curr_offset);
        curr_offset += (IP_SIZE + PORT_SIZE);
    }

    // ================================================

    // auto nodesInNetworkCopy = m_nodesInNetwork;
    // int upperLimit = numPeersInNetwork-1;
    // random_device rd;
    // mt19937 gen(rd());

    // for(unsigned int i = 0; i < SEED_PEER_LIST_SIZE; ++i, --upperLimit)
    // {
    //     uniform_int_distribution<> dis(0, upperLimit);
    //     uint32_t index = dis(gen);

    //     Peer candidateSeed = m_nodesInNetwork[index];
    //     candidateSeed.Serialize(seedPeersMessage, curr_offset);
    //     curr_offset += (IP_SIZE + PORT_SIZE);

    //     swap(nodesInNetworkCopy[index], nodesInNetworkCopy[upperLimit]);
    // }

    // ================================================

    P2PComm::GetInstance().SendMessage(peer, seedPeersMessage);

#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetDSInfoFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE
    // Message = [Port]
    LOG_MARKER();

    deque<PubKey> dsPubKeys;
    deque<Peer> dsPeers;
    {
        lock(m_mediator.m_mutexDSCommitteeNetworkInfo,
             m_mediator.m_mutexDSCommitteePubKeys);
        lock_guard<mutex> g(m_mediator.m_mutexDSCommitteeNetworkInfo,
                            adopt_lock);
        lock_guard<mutex> g2(m_mediator.m_mutexDSCommitteePubKeys, adopt_lock);

        dsPubKeys = m_mediator.m_DSCommitteePubKeys;
        dsPeers
            = m_mediator
                  .m_DSCommitteeNetworkInfo; // Data::GetInstance().GetDSPeers();
    }

    // dsInfoMessage = [num_ds_peers][DSPeer][DSPeer]... num_ds_peers times
    vector<unsigned char> dsInfoMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETDSINFOFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(dsInfoMessage, curr_offset,
                                      dsPeers.size(), sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    for (unsigned int i = 0; i < dsPeers.size(); i++)
    {
        PubKey& pubKey = dsPubKeys.at(i);
        pubKey.Serialize(dsInfoMessage, curr_offset);
        curr_offset += (PUB_KEY_SIZE);

        Peer& peer = dsPeers.at(i);
        peer.Serialize(dsInfoMessage, curr_offset);
        curr_offset += (IP_SIZE + PORT_SIZE);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "IP:" << peer.GetPrintableIPAddress());
    }

    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, dsInfoMessage);

    //#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][4-byte portNo]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   UINT256_SIZE + UINT256_SIZE
                                       + sizeof(uint32_t)))
    {
        return false;
    }

    // 32-byte lower-limit block number
    boost::multiprecision::uint256_t lowBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    // 32-byte upper-limit block number
    boost::multiprecision::uint256_t highBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    if (highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessGetDSBlockFromSeed requested by "
                  << from << " for blocks " << lowBlockNum.convert_to<string>()
                  << " to " << highBlockNum.convert_to<string>());

    // dsBlockMessage = [lowBlockNum][highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> dsBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETDSBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(dsBlockMessage, curr_offset, lowBlockNum,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint256_t>(dsBlockMessage, curr_offset,
                                       highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    boost::multiprecision::uint256_t blockNum;

    for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Fetching DSBlock " << blockNum.convert_to<string>()
                                          << " for " << from);
            DSBlock dsBlock = m_mediator.m_dsBlockChain.GetBlock(blockNum);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DSBlock " << blockNum.convert_to<string>()
                                 << " serialized for " << from);
            dsBlock.Serialize(dsBlockMessage, curr_offset);
            curr_offset += dsBlock.GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_GENERAL(INFO,
                        "Block Number " + blockNum.convert_to<string>()
                                + " absent. Didn't include it in response "
                                  "message. Reason: "
                            << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if (blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint256_t>(dsBlockMessage, highBlockNumOffset,
                                           blockNum - 1, UINT256_SIZE);
    }

    // 4-byte portNo
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);
    LOG_GENERAL(INFO, requestingNode);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, dsBlockMessage);

    //#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetStateFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    // #ifndef IS_LOOKUP_NODE
    // Message = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txbody]

    // if (IsMessageSizeInappropriate(message.size(), offset,
    //                                TRAN_HASH_SIZE + Transaction::GetSerializedSize()))
    // {
    //     return false;
    // }

    // TxnHash tranHash;
    // copy(message.begin() + offset, message.begin() + offset + TRAN_HASH_SIZE,
    //      tranHash.asArray().begin());
    // offset += TRAN_HASH_SIZE;

    // Transaction transaction(message, offset);

    // vector<unsigned char> serializedTxBody;
    // transaction.Serialize(serializedTxBody, 0);
    // BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);

    // 4-byte listening port

    // [Port number]

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    vector<unsigned char> setStateMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETSTATEFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;
    curr_offset
        += AccountStore::GetInstance().Serialize(setStateMessage, curr_offset);
    AccountStore::GetInstance().PrintAccountState();

    P2PComm::GetInstance().SendMessage(requestingNode, setStateMessage);
    // #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetTxBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][4-byte portNo]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   UINT256_SIZE + UINT256_SIZE
                                       + sizeof(uint32_t)))
    {
        return false;
    }

    // 32-byte lower-limit block number
    boost::multiprecision::uint256_t lowBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    // 32-byte upper-limit block number
    boost::multiprecision::uint256_t highBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    if (highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_txBlockChain.GetBlockCount() - 1;
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessGetTxBlockFromSeed requested by "
                  << from << " for blocks " << lowBlockNum.convert_to<string>()
                  << " to " << highBlockNum.convert_to<string>());

    // txBlockMessage = [lowBlockNum][highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> txBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETTXBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(txBlockMessage, curr_offset, lowBlockNum,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint256_t>(txBlockMessage, curr_offset,
                                       highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    boost::multiprecision::uint256_t blockNum;

    for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Fetching TxBlock " << blockNum.convert_to<string>()
                                          << " for " << from);
            TxBlock txBlock = m_mediator.m_txBlockChain.GetBlock(blockNum);
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "TxBlock " << blockNum.convert_to<string>()
                                 << " serialized for " << from);
            txBlock.Serialize(txBlockMessage, curr_offset);
            curr_offset += txBlock.GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_GENERAL(INFO,
                        "Block Number " + blockNum.convert_to<string>()
                                + " absent. Didn't include it in response "
                                  "message. Reason: "
                            << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if (blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint256_t>(txBlockMessage, highBlockNumOffset,
                                           blockNum - 1, UINT256_SIZE);
    }

    // 4-byte portNo
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);
    LOG_GENERAL(INFO, requestingNode);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, txBlockMessage);

    // #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetTxBodyFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [TRAN_HASH_SIZE txHashStr][4-byte portNo]

    LOG_MARKER();

    TxnHash tranHash;
    copy(message.begin() + offset, message.begin() + offset + TRAN_HASH_SIZE,
         tranHash.asArray().begin());
    offset += TRAN_HASH_SIZE;

    TxBodySharedPtr tx;

    BlockStorage::GetBlockStorage().GetTxBody(tranHash, tx);

    // txBodyMessage = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txBody]
    vector<unsigned char> txBodyMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETTXBODYFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    copy(tranHash.asArray().begin(), tranHash.asArray().end(),
         txBodyMessage.begin() + curr_offset);
    curr_offset += TRAN_HASH_SIZE;

    tx->Serialize(txBodyMessage, curr_offset);
    curr_offset += tx->GetSerializedSize();

    // 4-byte portNo
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);
    LOG_GENERAL(INFO, requestingNode);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, txBodyMessage);

    // #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetNetworkId(const vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    // 4-byte portNo
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    vector<unsigned char> networkIdMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETNETWORKIDFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    string networkId = "TESTNET"; // TODO: later convert it to a enum

    copy(networkId.begin(), networkId.end(),
         networkIdMessage.begin() + curr_offset);

    // TODO: Revamp the sendmessage and sendbroadcastmessage
    // Currently, we use sendbroadcastmessage instead of sendmessage. The reason is a new node who want to
    // join will received similar response from mulitple lookup node. It will process them in full.
    // Currently, we want the duplicated message to be drop so to ensure it do not do redundant processing.
    // In the long term, we need to track all the incoming messages from lookup or seed node more grandularly,.
    // and ensure 2/3 of such identical message is received in order to move on.

    // vector<Peer> node;
    // node.push_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, networkIdMessage);

    return true;
    // #endif // IS_LOOKUP_NODE
}

bool Lookup::ProcessSetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [Peer info][Peer info]... SEED_PEER_LIST_SIZE times

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   (IP_SIZE + PORT_SIZE) * SEED_PEER_LIST_SIZE))
    {
        return false;
    }

    for (unsigned int i = 0; i < SEED_PEER_LIST_SIZE; i++)
    {
        // Peer peer = Peer(message, offset);
        Peer peer;
        if (peer.Deserialize(message, offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize Peer.");
            return false;
        }

        m_seedNodes.push_back(peer);
        LOG_GENERAL(INFO, "Peer " + to_string(i) + ": " << string(peer));
        offset += (IP_SIZE + PORT_SIZE);
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetDSInfoFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE
    // Message = [numDSPeers][DSPeer][DSPeer]... numDSPeers times

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    uint32_t numDSPeers
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetDSInfoFromSeed sent by " << from << " for numPeers "
                                                  << numDSPeers);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE)
                                       * numDSPeers))
    {
        return false;
    }

    deque<PubKey> dsPubKeys;
    deque<Peer> dsPeers;

    for (unsigned int i = 0; i < numDSPeers; i++)
    {
        dsPubKeys.push_back(PubKey(message, offset));
        offset += PUB_KEY_SIZE;

        Peer peer(message, offset);
        offset += (IP_SIZE + PORT_SIZE);

        if (m_syncType == SyncType::DS_SYNC && peer == m_mediator.m_selfPeer)
        {
            peer = Peer();
        }
        dsPeers.push_back(peer);

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "ProcessSetDSInfoFromSeed recvd peer " << i << ": " << peer);
    }

    {
        lock(m_mediator.m_mutexDSCommitteeNetworkInfo,
             m_mediator.m_mutexDSCommitteePubKeys);
        lock_guard<mutex> g(m_mediator.m_mutexDSCommitteeNetworkInfo,
                            adopt_lock);
        lock_guard<mutex> g2(m_mediator.m_mutexDSCommitteePubKeys, adopt_lock);
        m_mediator.m_DSCommitteePubKeys = dsPubKeys;
        m_mediator.m_DSCommitteeNetworkInfo = dsPeers;
    }

        //    Data::GetInstance().SetDSPeers(dsPeers);
        //#endif // IS_LOOKUP_NODE

#ifndef IS_LOOKUP_NODE
    if (m_dsInfoWaitingNotifying
        && m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW
            == m_mediator.m_dsBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum()
                - 1)
    {
        unique_lock<mutex> lock(m_mutexDSInfoUpdation);
        m_fetchedDSInfo = true;
        cv_dsInfoUpdate.notify_one();
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE TODO: uncomment later
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times

    LOG_MARKER();

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetDSBlockFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   UINT256_SIZE + UINT256_SIZE))
    {
        return false;
    }

    // 32-byte lower-limit block number
    boost::multiprecision::uint256_t lowBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    // 32-byte upper-limit block number
    boost::multiprecision::uint256_t highBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetDSBlockFromSeed sent by "
                  << from << " for blocks " << lowBlockNum.convert_to<string>()
                  << " to " << highBlockNum.convert_to<string>());

    // since we will usually only enable sending of 500 blocks max, casting to uint32_t should be safe
    if (IsMessageSizeInappropriate(message.size(), offset,
                                   (uint32_t)(highBlockNum - lowBlockNum + 1)
                                       * DSBlock::GetMinSize()))
    {
        return false;
    }

    uint64_t latestSynBlockNum
        = (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount();

    if (latestSynBlockNum > highBlockNum)
    {
        // TODO: We should get blocks from n nodes.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I already have the block");
    }
    else
    {
        for (boost::multiprecision::uint256_t blockNum = lowBlockNum;
             blockNum <= highBlockNum; blockNum++)
        {
            // DSBlock dsBlock(message, offset);
            DSBlock dsBlock;
            if (dsBlock.Deserialize(message, offset) != 0)
            {
                LOG_GENERAL(WARNING, "We failed to deserialize dsBlock.");
                return false;
            }
            offset += dsBlock.GetSerializedSize();

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "dsblock.GetHeader().GetDifficulty(): "
                          << (int)dsBlock.GetHeader().GetDifficulty());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "dsblock.GetHeader().GetNonce(): "
                          << dsBlock.GetHeader().GetNonce());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "dsblock.GetHeader().GetBlockNum(): "
                          << dsBlock.GetHeader().GetBlockNum());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "dsblock.GetHeader().GetMinerPubKey().hex(): "
                          << dsBlock.GetHeader().GetMinerPubKey());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "dsblock.GetHeader().GetLeaderPubKey().hex(): "
                          << dsBlock.GetHeader().GetLeaderPubKey());

            m_mediator.m_dsBlockChain.AddBlock(dsBlock);

            // Store DS Block to disk
            vector<unsigned char> serializedDSBlock;
            dsBlock.Serialize(serializedDSBlock, 0);
            BlockStorage::GetBlockStorage().PutDSBlock(
                dsBlock.GetHeader().GetBlockNum(), serializedDSBlock);
#ifndef IS_LOOKUP_NODE
            if (!BlockStorage::GetBlockStorage().PushBackTxBodyDB(
                    dsBlock.GetHeader().GetBlockNum()))
            {
                if (BlockStorage::GetBlockStorage().PopFrontTxBodyDB()
                    && BlockStorage::GetBlockStorage().PushBackTxBodyDB(
                           dsBlock.GetHeader().GetBlockNum()))
                {
                    // Do nothing
                }
                else
                {
                    LOG_GENERAL(WARNING,
                                "Cannot push txBodyDB even after pop, "
                                "investigate why!");
                    throw std::exception();
                }
            }
#endif // IS_LOOKUP_NODE
        }

        if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
        {
            GetDSInfoFromLookupNodes();
        }

        if (m_syncType == SyncType::DS_SYNC
            || m_syncType == SyncType::LOOKUP_SYNC)
        {
            if (!m_isFirstLoop)
            {
                m_currDSExpired = true;
            }
            else
            {
                m_isFirstLoop = false;
            }
        }
    }
    m_mediator.UpdateDSBlockRand();

    return true;
}

bool Lookup::ProcessSetTxBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times
    LOG_MARKER();

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetTxBlockFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   UINT256_SIZE + UINT256_SIZE))
    {
        return false;
    }

    // 32-byte lower-limit block number
    boost::multiprecision::uint256_t lowBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    // 32-byte upper-limit block number
    boost::multiprecision::uint256_t highBlockNum
        = Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE;

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetTxBlockFromSeed sent by "
                  << from << " for blocks " << lowBlockNum.convert_to<string>()
                  << " to " << highBlockNum.convert_to<string>());

    uint64_t latestSynBlockNum
        = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();

    if (latestSynBlockNum > highBlockNum)
    {
        // TODO: We should get blocks from n nodes.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I already have the block");
        return false;
    }
    else
    {
        for (boost::multiprecision::uint256_t blockNum = lowBlockNum;
             blockNum <= highBlockNum; blockNum++)
        {
            TxBlock txBlock(message, offset);
            offset += txBlock.GetSerializedSize();

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetType(): "
                          << txBlock.GetHeader().GetType());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetVersion(): "
                          << txBlock.GetHeader().GetVersion());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetGasLimit(): "
                          << txBlock.GetHeader().GetGasLimit());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetGasUsed(): "
                          << txBlock.GetHeader().GetGasUsed());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetBlockNum(): "
                          << txBlock.GetHeader().GetBlockNum());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetNumMicroBlockHashes(): "
                          << txBlock.GetHeader().GetNumMicroBlockHashes());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetNumTxs(): "
                          << txBlock.GetHeader().GetNumTxs());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetMinerPubKey(): "
                          << txBlock.GetHeader().GetMinerPubKey());
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "txBlock.GetHeader().GetStateRootHash(): "
                          << txBlock.GetHeader().GetStateRootHash());

            m_mediator.m_txBlockChain.AddBlock(txBlock);

            // Store Tx Block to disk
            vector<unsigned char> serializedTxBlock;
            txBlock.Serialize(serializedTxBlock, 0);
            BlockStorage::GetBlockStorage().PutTxBlock(
                txBlock.GetHeader().GetBlockNum(), serializedTxBlock);
        }

        m_mediator.m_currentEpochNum
            = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();
        m_mediator.UpdateTxBlockRand();

        if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)
        {
            GetStateFromLookupNodes();
        }
    }

    return true;
}

bool Lookup::ProcessSetStateFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    bool ret = true;
    // Message = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txbody]

    LOG_MARKER();

    // if (IsMessageSizeInappropriate(message.size(), offset,
    //                                TRAN_HASH_SIZE + Transaction::GetSerializedSize()))
    // {
    //     return false;
    // }

    // TxnHash tranHash;
    // copy(message.begin() + offset, message.begin() + offset + TRAN_HASH_SIZE,
    //      tranHash.asArray().begin());
    // offset += TRAN_HASH_SIZE;

    // Transaction transaction(message, offset);

    // vector<unsigned char> serializedTxBody;
    // transaction.Serialize(serializedTxBody, 0);
    // BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetState);

    unsigned int curr_offset = offset;
    // AccountStore::GetInstance().Deserialize(message, curr_offset);
    if (AccountStore::GetInstance().Deserialize(message, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize AccountStore.");
        ret = false;
    }

#ifndef IS_LOOKUP_NODE
    if (m_syncType == SyncType::NEW_SYNC || m_syncType == SyncType::NORMAL_SYNC)
    {
        m_dsInfoWaitingNotifying = true;
        {
            unique_lock<mutex> lock(m_mutexDSInfoUpdation);
            while (!m_fetchedDSInfo)
            {
                if (cv_dsInfoUpdate.wait_for(
                        lock,
                        chrono::seconds(POW1_WINDOW_IN_SECONDS
                                        + BACKUP_POW2_WINDOW_IN_SECONDS))
                    == std::cv_status::timeout)
                {
                    // timed out
                    LOG_GENERAL(WARNING, "Timed out for waiting ProcessDSInfo");
                    m_dsInfoWaitingNotifying = false;
                    return false;
                }
                LOG_GENERAL(INFO, "Get ProcessDsInfo Notified");
                m_dsInfoWaitingNotifying = false;
            }
            m_fetchedDSInfo = false;
        }
        InitMining();
    }
    else if (m_syncType == SyncType::DS_SYNC)
    {
        if (!m_currDSExpired)
        {
            m_isFirstLoop = true;
            m_syncType = SyncType::NO_SYNC;
            m_mediator.m_ds->FinishRejoinAsDS();
        }
        m_currDSExpired = false;
    }
#else // IS_LOOKUP_NODE
    if (m_syncType == SyncType::LOOKUP_SYNC)
    {
        // rsync the txbodies here
        if (RsyncTxBodies() && !m_currDSExpired)
        {
            if (FinishRejoinAsLookup())
            {
                m_syncType = SyncType::NO_SYNC;
            }
        }
        m_currDSExpired = false;
    }
#endif // IS_LOOKUP_NODE

    return ret;
}

bool Lookup::ProcessSetTxBodyFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    // Message = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txbody]

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetTxBodyFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   Transaction::GetMinSerializedSize()))
    {
        return false;
    }

    TxnHash tranHash;
    copy(message.begin() + offset, message.begin() + offset + TRAN_HASH_SIZE,
         tranHash.asArray().begin());
    offset += TRAN_HASH_SIZE;

    // Transaction transaction(message, offset);
    Transaction transaction;
    if (transaction.Deserialize(message, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize Transaction.");
        return false;
    }

    vector<unsigned char> serializedTxBody;
    transaction.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);
    AccountStore::GetInstance().UpdateAccounts(transaction);

    return true;
}

#ifndef IS_LOOKUP_NODE

bool Lookup::CheckStateRoot()
{
    StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();
    StateHash rootInFinalBlock = m_mediator.m_txBlockChain.GetLastBlock()
                                     .GetHeader()
                                     .GetStateRootHash();

    if (stateRoot == rootInFinalBlock)
    {
        LOG_GENERAL(INFO, "CheckStateRoot match");
        return true;
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "State root doesn't match. Calculated = "
                        << stateRoot << ". "
                        << "StoredInBlock = " << rootInFinalBlock);

        return false;
    }
}

bool Lookup::InitMining()
{
    LOG_MARKER();

    m_mediator.m_currentEpochNum
        = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();

    // General check
    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW != 0)
    {
        return false;
    }

    if (m_mediator.m_currentEpochNum != 0)
    {
        m_mediator.m_node->m_consensusID = 0;
    }

    uint256_t curDsBlockNum
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

    m_mediator.UpdateDSBlockRand();
    auto dsBlockRand = m_mediator.m_dsBlockRand;
    array<unsigned char, 32> txBlockRand{};

    // if (m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW == curDsBlockNum)
    // {
    //     // DS block for the epoch has not been generated.
    //     // Attempt PoW1
    //     m_mediator.UpdateTxBlockRand();
    //     dsBlockRand = m_mediator.m_dsBlockRand;

    //     m_mediator.m_node->SetState(Node::POW1_SUBMISSION);
    //     POW::GetInstance().EthashConfigureLightClient((uint64_t)m_mediator.m_dsBlockChain.GetBlockCount());
    //     m_mediator.m_node->StartPoW1(m_mediator.m_dsBlockChain.GetBlockCount(),
    //                                     POW1_DIFFICULTY, dsBlockRand, m_mediator.m_txBlockRand);
    // }
    //else if
    if (m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW
        == curDsBlockNum - 1)
    {
        if (CheckStateRoot())
        {
            // DS block has been generated.
            // Attempt PoW2
            m_startedPoW2 = true;
            m_mediator.UpdateDSBlockRand();
            dsBlockRand = m_mediator.m_dsBlockRand;
            txBlockRand = {};

            m_mediator.m_node->SetState(Node::POW2_SUBMISSION);
            POW::GetInstance().EthashConfigureLightClient(
                (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount());

            this_thread::sleep_for(chrono::seconds(NEW_NODE_POW_DELAY));

            m_mediator.m_node->StartPoW2(
                m_mediator.m_dsBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum(),
                POW2_DIFFICULTY, dsBlockRand, txBlockRand);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    // Check whether is the new node connected to the network. Else, initiate re-sync process again.
    this_thread::sleep_for(chrono::seconds(BACKUP_POW2_WINDOW_IN_SECONDS));
    m_startedPoW2 = false;
    if (m_syncType != SyncType::NO_SYNC)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not yet connected to network");
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I have successfully join the network");
        LOG_GENERAL(INFO, "Clean TxBodyDB except the last one");
        int size_txBodyDBs
            = (int)BlockStorage::GetBlockStorage().GetTxBodyDBSize();
        for (int i = 0; i < size_txBodyDBs - 1; i++)
        {
            BlockStorage::GetBlockStorage().PopFrontTxBodyDB(true);
        }
    }

    return true;
}
#endif // IS_LOOKUP_NODE

bool Lookup::ProcessSetLookupOffline(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();
#ifdef IS_LOOKUP_NODE
    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    {
        lock_guard<mutex> lock(m_mutexOfflineLookups);
        auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(),
                              requestingNode);
        if (iter != m_lookupNodes.end())
        {
            m_lookupNodesOffline.push_back(requestingNode);
            m_lookupNodes.erase(iter);
        }
        else
        {
            LOG_GENERAL(WARNING, "The Peer Info is not in m_lookupNodes");
            return false;
        }
    }
#endif // IS_LOOKUP_NODE
    return true;
}

bool Lookup::ProcessSetLookupOnline(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from)
{
    LOG_MARKER();
#ifdef IS_LOOKUP_NODE
    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    {
        lock_guard<mutex> lock(m_mutexOfflineLookups);
        auto iter = std::find(m_lookupNodesOffline.begin(),
                              m_lookupNodesOffline.end(), requestingNode);
        if (iter != m_lookupNodes.end())
        {
            m_lookupNodes.push_back(requestingNode);
            m_lookupNodesOffline.erase(iter);
        }
        else
        {
            LOG_GENERAL(WARNING,
                        "The Peer Info is not in m_lookupNodesOffline");
            return false;
        }
    }
#endif // IS_LOOKUP_NODE
    return true;
}

bool Lookup::ProcessGetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    LOG_MARKER();
#ifdef IS_LOOKUP_NODE
    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);
    LOG_GENERAL(INFO, requestingNode);

    // vector<Peer> node;
    // node.push_back(requestingNode);

    // curLookupMessage = [num_offline_lookups][LookupPeer][LookupPeer]... num_offline_lookups times
    vector<unsigned char> offlineLookupsMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETOFFLINELOOKUPS};
    unsigned int curr_offset = MessageOffset::BODY;

    {
        lock_guard<mutex> lock(m_mutexOfflineLookups);
        Serializable::SetNumber<uint32_t>(offlineLookupsMessage, curr_offset,
                                          m_lookupNodesOffline.size(),
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        for (unsigned int i = 0; i < m_lookupNodesOffline.size(); i++)
        {
            Peer& peer = m_lookupNodesOffline.at(i);
            peer.Serialize(offlineLookupsMessage, curr_offset);
            curr_offset += (IP_SIZE + PORT_SIZE);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "IP:" << peer.GetPrintableIPAddress());
        }
    }

    P2PComm::GetInstance().SendMessage(requestingNode, offlineLookupsMessage);
#endif // IS_LOOKUP_NODE
    return true;
}

bool Lookup::ProcessSetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    // Message = [num_offline_lookups][LookupPeer][LookupPeer]... num_offline_lookups times
    LOG_MARKER();
#ifndef IS_LOOKUP_NODE
    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    uint32_t numOfflineLookups
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetOfflineLookups sent by "
                  << from << " for numOfflineLookups " << numOfflineLookups);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   (IP_SIZE + PORT_SIZE) * numOfflineLookups))
    {
        return false;
    }

    for (unsigned int i = 0; i < numOfflineLookups; i++)
    {
        Peer peer(message, offset);
        offset += (IP_SIZE + PORT_SIZE);

        // Remove selfPeerInfo from m_lookupNodes
        auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(), peer);
        if (iter != m_lookupNodes.end())
        {
            m_lookupNodesOffline.push_back(peer);
            m_lookupNodes.erase(iter);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "ProcessSetOfflineLookups recvd offline lookup "
                          << i << ": " << peer);
        }
    }

    {
        unique_lock<mutex> lock(m_mutexOfflineLookupsUpdation);
        m_fetchedOfflineLookups = true;
        cv_offlineLookups.notify_one();
    }
#endif // IS_LOOKUP_NODE
    return true;
}

#ifdef IS_LOOKUP_NODE
void Lookup::StartSynchronization()
{
    LOG_MARKER();

    this->CleanVariables();

    auto func = [this]() -> void {
        GetMyLookupOffline();
        GetDSInfoFromLookupNodes();
        while (m_syncType != SyncType::NO_SYNC)
        {
            GetDSBlockFromLookupNodes(m_mediator.m_dsBlockChain.GetBlockCount(),
                                      0);
            GetTxBlockFromLookupNodes(m_mediator.m_txBlockChain.GetBlockCount(),
                                      0);
            this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
        }
    };
    DetachedFunction(1, func);
}

Peer Lookup::GetLookupPeerToRsync()
{
    LOG_MARKER();

    std::vector<Peer> t_Peers;
    for (auto p : m_lookupNodes)
    {
        if (p != m_mediator.m_selfPeer)
        {
            t_Peers.push_back(p);
        }
    }

    int index = rand() % t_Peers.size();

    return t_Peers[index];
}

std::vector<unsigned char> Lookup::ComposeGetLookupOfflineMessage()
{
    LOG_MARKER();

    // getLookupOfflineMessage = [Port]
    vector<unsigned char> getLookupOfflineMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETLOOKUPOFFLINE};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getLookupOfflineMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getLookupOfflineMessage;
}

std::vector<unsigned char> Lookup::ComposeGetLookupOnlineMessage()
{
    LOG_MARKER();

    // getLookupOnlineMessage = [Port]
    vector<unsigned char> getLookupOnlineMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETLOOKUPONLINE};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getLookupOnlineMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getLookupOnlineMessage;
}

bool Lookup::GetMyLookupOffline()
{
    LOG_MARKER();

    // Remove selfPeerInfo from m_lookupNodes
    auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(),
                          m_mediator.m_selfPeer);
    if (iter != m_lookupNodes.end())
    {
        m_lookupNodesOffline.push_back(m_mediator.m_selfPeer);
        m_lookupNodes.erase(iter);
    }
    else
    {
        LOG_GENERAL(WARNING, "My Peer Info is not in m_lookupNodes");
        return false;
    }

    SendMessageToLookupNodesSerial(ComposeGetLookupOfflineMessage());
    return true;
}

bool Lookup::GetMyLookupOnline()
{
    LOG_MARKER();

    auto iter = std::find(m_lookupNodesOffline.begin(),
                          m_lookupNodesOffline.end(), m_mediator.m_selfPeer);
    if (iter != m_lookupNodesOffline.end())
    {
        SendMessageToLookupNodesSerial(ComposeGetLookupOnlineMessage());
        m_lookupNodes.push_back(m_mediator.m_selfPeer);
        m_lookupNodesOffline.erase(iter);
    }
    else
    {
        LOG_GENERAL(WARNING, "My Peer Info is not in m_lookupNodesOffline");
        return false;
    }
    return true;
}

bool Lookup::RsyncTxBodies()
{
    LOG_MARKER();
    const Peer& p = GetLookupPeerToRsync();
    string ipAddr = std::string(p.GetPrintableIPAddress());
    string port = std::to_string(p.m_listenPortHost);
    string dbNameStr
        = BlockStorage::GetBlockStorage().GetDBName(BlockStorage::TX_BODY)[0];
    string cmdStr;
    if (ipAddr == "127.0.0.1" || ipAddr == "localhost")
    {
        string indexStr = port;
        indexStr.erase(indexStr.begin());
        cmdStr = "rsync -iraz --size-only ../node_0" + indexStr + "/"
            + PERSISTENCE_PATH + "/" + dbNameStr + "/* " + PERSISTENCE_PATH
            + "/" + dbNameStr + "/";
    }
    else
    {
        cmdStr = "rsync -iraz --size-only -e \"ssh -o "
                 "StrictHostKeyChecking=no\" ubuntu@"
            + ipAddr + ":" + REMOTE_TEST_DIR + "/" + PERSISTENCE_PATH + "/"
            + dbNameStr + "/* " + PERSISTENCE_PATH + "/" + dbNameStr + "/";
    }
    LOG_GENERAL(INFO, cmdStr);

    ExecuteCmd(cmdStr);
    return true;
}

void Lookup::RejoinAsLookup()
{
    LOG_MARKER();
    if (m_syncType == SyncType::NO_SYNC)
    {
        m_syncType = SyncType::LOOKUP_SYNC;
        m_mediator.m_node->Install(SyncType::LOOKUP_SYNC, true);
        this->StartSynchronization();
    }
}

bool Lookup::FinishRejoinAsLookup() { return GetMyLookupOnline(); }

bool Lookup::CleanVariables()
{
    m_seedNodes.clear();
    m_currDSExpired = false;
    m_isFirstLoop = true;
    {
        std::lock_guard<mutex> lock(m_mutexShards);
        m_shards.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexNodesInNetwork);
        m_nodesInNetwork.clear();
        l_nodesInNetwork.clear();
    }

    return true;
}

bool Lookup::ToBlockMessage(unsigned char ins_byte)
{
    if (m_syncType != SyncType::NO_SYNC
        && (ins_byte != LookupInstructionType::SETDSBLOCKFROMSEED
            && ins_byte != LookupInstructionType::SETDSINFOFROMSEED
            && ins_byte != LookupInstructionType::SETTXBLOCKFROMSEED
            && ins_byte != LookupInstructionType::SETSTATEFROMSEED
            && ins_byte != LookupInstructionType::SETLOOKUPOFFLINE
            && ins_byte != LookupInstructionType::SETLOOKUPONLINE))
    {
        return true;
    }
    return false;
}
#endif // IS_LOOKUP_NODE

#ifndef IS_LOOKUP_NODE
std::vector<unsigned char> Lookup::ComposeGetOfflineLookupNodes()
{
    LOG_MARKER();

    // getLookupNodesMessage
    vector<unsigned char> getCurrLookupsMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETOFFLINELOOKUPS};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getCurrLookupsMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getCurrLookupsMessage;
}

bool Lookup::GetOfflineLookupNodes()
{
    LOG_MARKER();
    // Reset m_lookupNodes/m_lookupNodesOffline
    SetLookupNodes();
    SendMessageToLookupNodesSerial(ComposeGetOfflineLookupNodes());
    return true;
}
#endif // IS_LOOKUP_NODE

bool Lookup::Execute(const vector<unsigned char>& message, unsigned int offset,
                     const Peer& from)
{
    LOG_MARKER();

    bool result = true;

    typedef bool (Lookup::*InstructionHandler)(const vector<unsigned char>&,
                                               unsigned int, const Peer&);

    InstructionHandler ins_handlers[] = {
        &Lookup::ProcessEntireShardingStructure,
        &Lookup::ProcessGetSeedPeersFromLookup,
        &Lookup::ProcessSetSeedPeersFromLookup,
        &Lookup::ProcessGetDSInfoFromSeed,
        &Lookup::ProcessSetDSInfoFromSeed,
        &Lookup::ProcessGetDSBlockFromSeed,
        &Lookup::ProcessSetDSBlockFromSeed,
        &Lookup::ProcessGetTxBlockFromSeed,
        &Lookup::ProcessSetTxBlockFromSeed,
        &Lookup::ProcessGetTxBodyFromSeed,
        &Lookup::ProcessSetTxBodyFromSeed,
        &Lookup::ProcessGetNetworkId,
        &Lookup::ProcessGetNetworkId,
        &Lookup::ProcessGetStateFromSeed,
        &Lookup::ProcessSetStateFromSeed,
        &Lookup::ProcessSetLookupOffline,
        &Lookup::ProcessSetLookupOnline,
        &Lookup::ProcessGetOfflineLookups,
        &Lookup::ProcessSetOfflineLookups,
    };

    const unsigned char ins_byte = message.at(offset);
    const unsigned int ins_handlers_count
        = sizeof(ins_handlers) / sizeof(InstructionHandler);

#ifdef IS_LOOKUP_NODE
    if (ToBlockMessage(ins_byte))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignore lookup message");
        return false;
    }
#endif // IS_LOOKUP_NODE

    if (ins_byte < ins_handlers_count)
    {
        result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);
        if (result == false)
        {
            // To-do: Error recovery
        }
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Unknown instruction byte " << hex
                                                << (unsigned int)ins_byte);
    }

    return result;
}

bool Lookup::AlreadyJoinedNetwork()
{
    if (m_syncType == SyncType::NO_SYNC)
    {
        return true;
    }
    else
    {
        return false;
    }
}
