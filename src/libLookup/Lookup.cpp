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
#include "libData/BlockChainData/BlockChain.h"
#include "libData/BlockData/Block.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

Lookup::Lookup(Mediator& mediator)
    : m_mediator(mediator)
{
    SetLookupNodes();
    if (LOOKUP_NODE_MODE)
    {
        SetDSCommitteInfo();
    }
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
            m_lookupNodes.emplace_back(lookup_node);
        }
    }
}

std::once_flag generateReceiverOnce;

Address GenOneReceiver()
{
    static Address receiverAddr;
    std::call_once(generateReceiverOnce, []() {
        auto receiver = Schnorr::GetInstance().GenKeyPair();
        receiverAddr = Account::GetAddressFromPublicKey(receiver.second);
        LOG_GENERAL(INFO,
                    "Generate testing transaction receiver " << receiverAddr);
    });
    return receiverAddr;
}

Transaction CreateValidTestingTransaction(PrivKey& fromPrivKey,
                                          PubKey& fromPubKey,
                                          const Address& toAddr,
                                          uint256_t amount, uint256_t prevNonce)
{
    unsigned int version = 0;
    auto nonce = prevNonce + 1;

    // LOG_GENERAL("fromPrivKey " << fromPrivKey << " / fromPubKey " << fromPubKey
    // << " / toAddr" << toAddr);

    Transaction txn(version, nonce, toAddr, make_pair(fromPrivKey, fromPubKey),
                    amount, 1, 1, {}, {});

    // std::vector<unsigned char> buf;
    // txn.SerializeWithoutSignature(buf, 0);

    // Signature sig;
    // Schnorr::GetInstance().Sign(buf, fromPrivKey, fromPubKey, sig);

    // vector<unsigned char> sigBuf;
    // sig.Serialize(sigBuf, 0);
    // txn.SetSignature(sigBuf);

    return txn;
}

bool Lookup::GenTxnToSend(size_t n, map<uint32_t, vector<unsigned char>>& mp,
                          uint32_t nShard)
{
    vector<unsigned char> txns;

    if (GENESIS_KEYS.size() == 0)
    {
        LOG_GENERAL(WARNING, "No genesis keys found");
        return false;
    }

    unsigned int NUM_TXN_TO_DS = n / GENESIS_KEYS.size();

    for (auto& privKeyHexStr : GENESIS_KEYS)
    {
        auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
        auto privKey = PrivKey{privKeyBytes, 0};
        auto pubKey = PubKey{privKey};
        auto addr = Account::GetAddressFromPublicKey(pubKey);

        if (nShard == 0)
        {
            return false;
        }
        auto txnShard = Transaction::GetShardIndex(addr, nShard);
        txns.clear();

        uint256_t nonce
            = AccountStore::GetInstance().GetAccount(addr)->GetNonce();

        if (!GetTxnFromFile::GetFromFile(addr, static_cast<uint32_t>(nonce) + 1,
                                         n, txns))
        {
            LOG_GENERAL(WARNING, "Failed to get txns from file");
            return false;
        }

        /*Address receiverAddr = GenOneReceiver();
        unsigned int curr_offset = 0;
        txns.reserve(n);
        for (auto i = 0u; i != n; i++)
        {

            Transaction txn(0, nonce + i + 1, receiverAddr,
                            make_pair(privKey, pubKey), 10 * i + 2, 1, 1, {},
                            {});

            curr_offset = txn.Serialize(txns, curr_offset);
        }*/
        //[Change back here]
        copy(txns.begin(), txns.end(), back_inserter(mp[txnShard]));

        LOG_GENERAL(INFO,
                    "[Batching] Last Nonce sent " << nonce + n << " of Addr "
                                                  << addr.hex());
        txns.clear();

        if (!GetTxnFromFile::GetFromFile(addr,
                                         static_cast<uint32_t>(nonce) + n + 1,
                                         NUM_TXN_TO_DS, txns))
        {
            LOG_GENERAL(WARNING, "Failed to get txns for DS");
        }

        copy(txns.begin(), txns.end(), back_inserter(mp[nShard]));
    }

    return true;
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

        allLookupNodes.emplace_back(node);
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

        allLookupNodes.emplace_back(node);
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

vector<unsigned char> Lookup::ComposeGetDSBlockMessage(uint64_t lowBlockNum,
                                                       uint64_t highBlockNum)
{
    LOG_MARKER();

    // getDSBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getDSBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETDSBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint64_t>(getDSBlockMessage, curr_offset,
                                      lowBlockNum, sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    Serializable::SetNumber<uint64_t>(getDSBlockMessage, curr_offset,
                                      highBlockNum, sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    Serializable::SetNumber<uint32_t>(getDSBlockMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getDSBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetDSBlockFromSeedNodes(uint64_t lowBlockNum,
                                     uint64_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToSeedNodes(ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetDSBlockFromLookupNodes(uint64_t lowBlockNum,
                                       uint64_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToRandomLookupNode(
        ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

vector<unsigned char> Lookup::ComposeGetTxBlockMessage(uint64_t lowBlockNum,
                                                       uint64_t highBlockNum)
{
    LOG_MARKER();

    // getTxBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getTxBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETTXBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint64_t>(getTxBlockMessage, curr_offset,
                                      lowBlockNum, sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    Serializable::SetNumber<uint64_t>(getTxBlockMessage, curr_offset,
                                      highBlockNum, sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    Serializable::SetNumber<uint32_t>(getTxBlockMessage, curr_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getTxBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetTxBlockFromSeedNodes(uint64_t lowBlockNum,
                                     uint64_t highBlockNum)
{
    LOG_MARKER();
    SendMessageToSeedNodes(ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetTxBlockFromLookupNodes(uint64_t lowBlockNum,
                                       uint64_t highBlockNum)
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

bool Lookup::SetDSCommitteInfo()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::SetDSCommitteInfo not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }
    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml("config.xml", pt);

    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

    for (ptree::value_type const& v : pt.get_child("nodes"))
    {
        if (v.first == "peer")
        {
            PubKey key(
                DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")),
                0);

            struct in_addr ip_addr;
            inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
            Peer peer((uint128_t)ip_addr.s_addr,
                      v.second.get<unsigned int>("port"));
            m_mediator.m_DSCommittee->emplace_back(make_pair(key, peer));
        }
    }

    return true;
}

vector<vector<pair<PubKey, Peer>>> Lookup::GetShardPeers()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetShardPeers not expected to be called from "
                    "other than the LookUp node.");
        return vector<vector<pair<PubKey, Peer>>>();
    }

    lock_guard<mutex> g(m_mutexShards);
    return m_mediator.m_ds->m_shards;
}

vector<Peer> Lookup::GetNodePeers()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetNodePeers not expected to be called from other "
                    "than the LookUp node.");
        return vector<Peer>();
    }

    lock_guard<mutex> g(m_mutexNodesInNetwork);
    return m_nodesInNetwork;
}

bool Lookup::ProcessEntireShardingStructure()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessEntireShardingStructure not expected to be "
                    "called from other than the LookUp node.");
        return true;
    }

    LOG_MARKER();

    LOG_GENERAL(INFO, "[LOOKUP received sharding structure]");

    lock(m_mutexShards, m_mutexNodesInNetwork);
    lock_guard<mutex> g(m_mutexShards, adopt_lock);
    lock_guard<mutex> h(m_mutexNodesInNetwork, adopt_lock);

    m_nodesInNetwork.clear();
    unordered_set<Peer> t_nodesInNetwork;

    for (unsigned int i = 0; i < m_mediator.m_ds->m_shards.size(); i++)
    {
        unsigned int index = 0;
        for (auto& j : m_mediator.m_ds->m_shards.at(i))
        {
            const PubKey& key = j.first;
            const Peer& peer = j.second;

            m_nodesInNetwork.emplace_back(peer);
            t_nodesInNetwork.emplace(peer);

            LOG_GENERAL(INFO,
                        "[SHARD "
                            << to_string(i) << "] "
                            << "[PEER " << to_string(index) << "] "
                            << "Inserting Pubkey to shard : " << string(key));
            LOG_GENERAL(INFO,
                        "[SHARD " << to_string(i) << "] "
                                  << "[PEER " << to_string(index) << "] "
                                  << "Corresponding peer : " << string(peer));

            index++;
        }
    }

    for (auto& peer : t_nodesInNetwork)
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

    for (auto& peer : l_nodesInNetwork)
    {
        LOG_STATE("[LOSTPEER]["
                  << std::setw(15) << std::left
                  << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                  << std::setw(6) << std::left << m_mediator.m_currentEpochNum
                  << "][" << std::setw(4) << std::left
                  << m_mediator.GetNodeMode(peer) << "]" << string(peer));
    }

    l_nodesInNetwork = t_nodesInNetwork;

    return true;
}

bool Lookup::ProcessGetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           const Peer& from)
{
    LOG_MARKER();

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessGetSeedPeersFromLookup not expected to be "
                    "called from other than the LookUp node.");
        return true;
    }

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

    return true;
}

bool Lookup::ProcessGetDSInfoFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE
    // Message = [Port]
    LOG_MARKER();
    // dsInfoMessage = [num_ds_peers][DSPeer][DSPeer]... num_ds_peers times
    vector<unsigned char> dsInfoMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETDSINFOFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    {
        lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
        Serializable::SetNumber<uint32_t>(dsInfoMessage, curr_offset,
                                          m_mediator.m_DSCommittee->size(),
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        for (auto& i : *m_mediator.m_DSCommittee)
        {
            PubKey& pubKey = i.first;
            pubKey.Serialize(dsInfoMessage, curr_offset);
            curr_offset += (PUB_KEY_SIZE);

            Peer& peer = i.second;
            peer.Serialize(dsInfoMessage, curr_offset);
            curr_offset += (IP_SIZE + PORT_SIZE);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "IP:" << peer.GetPrintableIPAddress());
        }
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
    // node.emplace_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, dsInfoMessage);

    //#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    //#ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [8-byte lowBlockNum][8-byte highBlockNum][4-byte portNo]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(uint64_t) + sizeof(uint64_t)
                                       + sizeof(uint32_t)))
    {
        return false;
    }

    // 8-byte lower-limit block number
    uint64_t lowBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // 8-byte upper-limit block number
    uint64_t highBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    if (lowBlockNum == 1)
    {
        lowBlockNum = m_mediator.m_dsBlockChain.GetLastBlock()
                          .GetHeader()
                          .GetBlockNum();
    }
    else if (lowBlockNum == 0)
    {
        //give all the blocks in the ds blockchain
        lowBlockNum = 1;
    }

    if (highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_dsBlockChain.GetLastBlock()
                           .GetHeader()
                           .GetBlockNum();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessGetDSBlockFromSeed requested by "
                  << from << " for blocks " << lowBlockNum << " to "
                  << highBlockNum);

    // dsBlockMessage = [lowBlockNum][highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> dsBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETDSBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint64_t>(dsBlockMessage, curr_offset, lowBlockNum,
                                      sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint64_t>(dsBlockMessage, curr_offset, highBlockNum,
                                      sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    uint64_t blockNum;

    for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "Fetching DSBlock " << blockNum.convert_to<string>()
            //                               << " for " << from);
            DSBlock dsBlock = m_mediator.m_dsBlockChain.GetBlock(blockNum);
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "DSBlock " << blockNum.convert_to<string>()
            //                      << " serialized for " << from);
            dsBlock.Serialize(dsBlockMessage, curr_offset);
            curr_offset += dsBlock.GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_GENERAL(INFO,
                        "Block Number " << blockNum
                                        << " absent. Didn't include it in "
                                           "response message. Reason: "
                                        << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if (blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint64_t>(dsBlockMessage, highBlockNumOffset,
                                          blockNum - 1, sizeof(uint64_t));
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
    // node.emplace_back(requestingNode);

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
    // node.emplace_back(requestingNode);

    vector<unsigned char> setStateMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETSTATEFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;
    curr_offset
        += AccountStore::GetInstance().Serialize(setStateMessage, curr_offset);
    // AccountStore::GetInstance().PrintAccountState();

    P2PComm::GetInstance().SendMessage(requestingNode, setStateMessage);
    // #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetTxBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [8-byte lowBlockNum][8-byte highBlockNum][4-byte portNo]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(uint64_t) + sizeof(uint64_t)
                                       + sizeof(uint32_t)))
    {
        return false;
    }

    // 8-byte lower-limit block number
    uint64_t lowBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // 8-byte upper-limit block number
    uint64_t highBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    if (lowBlockNum == 1)
    {
        lowBlockNum = m_mediator.m_txBlockChain.GetLastBlock()
                          .GetHeader()
                          .GetBlockNum();
    }
    else if (lowBlockNum == 0)
    {
        //give all the blocks till now in blockchain
        lowBlockNum = 1;
    }

    if (highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_txBlockChain.GetLastBlock()
                           .GetHeader()
                           .GetBlockNum();
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessGetTxBlockFromSeed requested by "
                  << from << " for blocks " << lowBlockNum << " to "
                  << highBlockNum);

    // txBlockMessage = [lowBlockNum][highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> txBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETTXBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint64_t>(txBlockMessage, curr_offset, lowBlockNum,
                                      sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint64_t>(txBlockMessage, curr_offset, highBlockNum,
                                      sizeof(uint64_t));
    curr_offset += sizeof(uint64_t);

    uint64_t blockNum;

    for (blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "Fetching TxBlock " << blockNum.convert_to<string>()
            //                               << " for " << from);
            TxBlock txBlock = m_mediator.m_txBlockChain.GetBlock(blockNum);
            // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            //           "TxBlock " << blockNum.convert_to<string>()
            //                      << " serialized for " << from);
            txBlock.Serialize(txBlockMessage, curr_offset);
            curr_offset += txBlock.GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_GENERAL(INFO,
                        "Block Number " << blockNum
                                        << " absent. Didn't include it in "
                                           "response message. Reason: "
                                        << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if (blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint64_t>(txBlockMessage, highBlockNumOffset,
                                          blockNum - 1, sizeof(uint64_t));
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
    // node.emplace_back(requestingNode);

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

    TxBodySharedPtr tptr;

    BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);

    // txBodyMessage = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txBody]
    vector<unsigned char> txBodyMessage
        = {MessageType::LOOKUP, LookupInstructionType::SETTXBODYFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    copy(tranHash.asArray().begin(), tranHash.asArray().end(),
         txBodyMessage.begin() + curr_offset);
    curr_offset += TRAN_HASH_SIZE;

    tptr->Serialize(txBodyMessage, curr_offset);
    curr_offset += tptr->GetSerializedSize();

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
    // node.emplace_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, txBodyMessage);

    // #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetShardFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    //Message = [Port]
    uint32_t port;
    if (!Messenger::GetLookupGetShardsFromSeed(message, offset, port))
    {
        LOG_GENERAL(WARNING, "Failed to process");
        return false;
    }
    Peer requestingNode(from.m_ipAddress, port);
    vector<unsigned char> msg
        = {MessageType::LOOKUP, LookupInstructionType::SETSHARDSFROMSEED};
    lock_guard<mutex> g(m_mutexShards);
    if (!Messenger::SetLookupSetShardsFromSeed(msg, MessageOffset::BODY,
                                               m_mediator.m_ds->m_shards))
    {
        LOG_GENERAL(WARNING, "Failed to Process");
        return false;
    }

    P2PComm::GetInstance().SendMessage(requestingNode, msg);

    return true;
}

bool Lookup::ProcessSetShardFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    VectorOfShard shards;

    if (!Messenger::GetLookupSetShardsFromSeed(message, offset, shards))
    {
        LOG_GENERAL(WARNING, "Failed to Process");
        return false;
    }
    LOG_GENERAL(INFO, "Sharding Structure Recvd from " << from);

    uint32_t i = 0;
    for (const auto& shard : shards)
    {
        LOG_GENERAL(INFO, "Size of shard " << i << " " << shard.size());
        i++;
    }
    lock_guard<mutex> g(m_mutexShards);

    m_mediator.m_ds->m_shards = move(shards);

    return true;
}

bool Lookup::GetShardFromLookup()
{
    LOG_MARKER();
    vector<unsigned char> msg
        = {MessageType::LOOKUP, LookupInstructionType::GETSHARDSFROMSEED};

    if (!Messenger::SetLookupGetShardsFromSeed(
            msg, MessageOffset::BODY, m_mediator.m_selfPeer.m_listenPortHost))
    {
        LOG_GENERAL(WARNING, "Failed to process");
        return false;
    }

    SendMessageToRandomLookupNode(msg);

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
    // node.emplace_back(requestingNode);

    P2PComm::GetInstance().SendMessage(requestingNode, networkIdMessage);

    return true;
    // #endif // IS_LOOKUP_NODE
}

bool Lookup::ProcessSetSeedPeersFromLookup(const vector<unsigned char>& message,
                                           unsigned int offset,
                                           [[gnu::unused]] const Peer& from)
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessSetSeedPeersFromLookup not expected to be "
                    "called from LookUp node.");
        return true;
    }
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

        m_seedNodes.emplace_back(peer);
        LOG_GENERAL(INFO, "Peer " + to_string(i) + ": " << string(peer));
        offset += (IP_SIZE + PORT_SIZE);
    }

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

    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
    m_mediator.m_DSCommittee->clear();

    for (unsigned int i = 0; i < numDSPeers; i++)
    {
        PubKey pubkey(message, offset);

        offset += PUB_KEY_SIZE;

        Peer peer(message, offset);
        offset += (IP_SIZE + PORT_SIZE);

        if (m_syncType == SyncType::DS_SYNC && peer == m_mediator.m_selfPeer)
        {
            peer = Peer();
        }

        m_mediator.m_DSCommittee->emplace_back(make_pair(pubkey, peer));

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "ProcessSetDSInfoFromSeed recvd peer " << i << ": " << peer);
    }
    //    Data::GetInstance().SetDSPeers(dsPeers);
    //#endif // IS_LOOKUP_NODE

    if (!LOOKUP_NODE_MODE && m_dsInfoWaitingNotifying
        && m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW
            == m_mediator.m_dsBlockChain.GetLastBlock()
                   .GetHeader()
                   .GetBlockNum())
    {
        LOG_EPOCH(
            INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Notifying ProcessSetStateFromSeed that DSInfo has been received");
        unique_lock<mutex> lock(m_mutexDSInfoUpdation);
        m_fetchedDSInfo = true;
        cv_dsInfoUpdate.notify_one();
    }

    return true;
}

bool Lookup::ProcessSetDSBlockFromSeed(const vector<unsigned char>& message,
                                       unsigned int offset, const Peer& from)
{
    // #ifndef IS_LOOKUP_NODE TODO: uncomment later
    // Message = [8-byte lowBlockNum][8-byte highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times

    LOG_MARKER();

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetDSBlockFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(uint64_t) + sizeof(uint64_t)))
    {
        return false;
    }

    // 8-byte lower-limit block number
    uint64_t lowBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // 8-byte upper-limit block number
    uint64_t highBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetDSBlockFromSeed sent by " << from << " for blocks "
                                                   << lowBlockNum << " to "
                                                   << highBlockNum);

    // since we will usually only enable sending of 500 blocks max, casting to uint32_t should be safe
    if (IsMessageSizeInappropriate(message.size(), offset,
                                   (uint32_t)(highBlockNum - lowBlockNum + 1)
                                       * DSBlock::GetMinSize()))
    {
        return false;
    }

    uint64_t latestSynBlockNum
        // = (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount();
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;

    if (latestSynBlockNum > highBlockNum)
    {
        // TODO: We should get blocks from n nodes.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I already have the block");
    }
    else
    {
        for (uint64_t blockNum = lowBlockNum; blockNum <= highBlockNum;
             blockNum++)
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
            if (!LOOKUP_NODE_MODE
                && !BlockStorage::GetBlockStorage().PushBackTxBodyDB(
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
    // Message = [8-byte lowBlockNum][8-byte highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times
    LOG_MARKER();

    if (AlreadyJoinedNetwork())
    {
        return true;
    }

    unique_lock<mutex> lock(m_mutexSetTxBlockFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   sizeof(uint64_t) + sizeof(uint64_t)))
    {
        return false;
    }

    // 8-byte lower-limit block number
    uint64_t lowBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // 8-byte upper-limit block number
    uint64_t highBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "ProcessSetTxBlockFromSeed sent by " << from << " for blocks "
                                                   << lowBlockNum << " to "
                                                   << highBlockNum);

    uint64_t latestSynBlockNum
        // = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;

    if (latestSynBlockNum > highBlockNum)
    {
        // TODO: We should get blocks from n nodes.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I already have the block");
        return false;
    }
    else
    {
        for (uint64_t blockNum = lowBlockNum; blockNum <= highBlockNum;
             blockNum++)
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

            m_mediator.m_node->AddBlock(txBlock);

            // Store Tx Block to disk
            vector<unsigned char> serializedTxBlock;
            txBlock.Serialize(serializedTxBlock, 0);
            BlockStorage::GetBlockStorage().PutTxBlock(
                txBlock.GetHeader().GetBlockNum(), serializedTxBlock);
        }

        m_mediator.m_currentEpochNum
            = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            + 1;

        m_mediator.UpdateTxBlockRand();

        if ((m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0))
        {
            GetStateFromLookupNodes();
        }
    }

    return true;
}

bool Lookup::ProcessSetStateFromSeed(const vector<unsigned char>& message,
                                     unsigned int offset,
                                     [[gnu::unused]] const Peer& from)
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

    if (!LOOKUP_NODE_MODE)
    {
        if (m_syncType == SyncType::NEW_SYNC
            || m_syncType == SyncType::NORMAL_SYNC)
        {
            m_dsInfoWaitingNotifying = true;

            GetDSInfoFromLookupNodes();

            {
                unique_lock<mutex> lock(m_mutexDSInfoUpdation);
                while (!m_fetchedDSInfo)
                {
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "Waiting for DSInfo");

                    if (cv_dsInfoUpdate.wait_for(
                            lock, chrono::seconds(NEW_NODE_SYNC_INTERVAL))
                        == std::cv_status::timeout)
                    {
                        // timed out
                        LOG_EPOCH(
                            INFO,
                            to_string(m_mediator.m_currentEpochNum).c_str(),
                            "Timed out waiting for DSInfo");
                        m_dsInfoWaitingNotifying = false;
                        return false;
                    }
                    LOG_EPOCH(INFO,
                              to_string(m_mediator.m_currentEpochNum).c_str(),
                              "Get ProcessDsInfo Notified");
                    m_dsInfoWaitingNotifying = false;
                }
                m_fetchedDSInfo = false;
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "DSInfo received -> Ask lookup to let me know when to "
                      "start PoW");

            // Ask lookup to inform me when it's time to do PoW
            vector<unsigned char> getpowsubmission_message
                = {MessageType::LOOKUP,
                   LookupInstructionType::GETSTARTPOWFROMSEED};
            Serializable::SetNumber<uint32_t>(
                getpowsubmission_message, MessageOffset::BODY,
                m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
            m_mediator.m_lookup->SendMessageToRandomLookupNode(
                getpowsubmission_message);
        }
        else if (m_syncType == SyncType::DS_SYNC)
        {
            if (!m_currDSExpired
                && m_mediator.m_ds->m_latestActiveDSBlockNum
                    < m_mediator.m_dsBlockChain.GetLastBlock()
                          .GetHeader()
                          .GetBlockNum())
            {
                m_isFirstLoop = true;
                m_syncType = SyncType::NO_SYNC;
                m_mediator.m_ds->FinishRejoinAsDS();
            }
            m_currDSExpired = false;
        }
    }
    else if (m_syncType == SyncType::LOOKUP_SYNC)
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

    return ret;
}

bool Lookup::ProcessSetTxBodyFromSeed(const vector<unsigned char>& message,
                                      unsigned int offset,
                                      [[gnu::unused]] const Peer& from)
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
    TransactionWithReceipt twr;
    if (twr.Deserialize(message, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize Transaction.");
        return false;
    }

    // if (!AccountStore::GetInstance().UpdateAccounts(
    //         m_mediator.m_currentEpochNum - 1, transaction))
    // {
    //     LOG_GENERAL(WARNING, "UpdateAccounts failed");
    //     return false;
    // }
    vector<unsigned char> serializedTxBody;
    twr.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);

    return true;
}

bool Lookup::CheckStateRoot()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::CheckStateRoot not expected to be called from "
                    "LookUp node.");
        return true;
    }

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
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(
            WARNING,
            "Lookup::InitMining not expected to be called from LookUp node.");
        return true;
    }

    LOG_MARKER();

    // General check
    if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW != 0)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Epoch num check failed");
        return false;
    }

    if (m_mediator.m_currentEpochNum != 0)
    {
        m_mediator.m_node->m_consensusID = 0;
    }

    uint64_t curDsBlockNum
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

    m_mediator.UpdateDSBlockRand();
    auto dsBlockRand = m_mediator.m_dsBlockRand;
    array<unsigned char, 32> txBlockRand{};

    if (m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW == curDsBlockNum)
    {
        if (CheckStateRoot())
        {
            // Attempt PoW
            m_startedPoW = true;
            dsBlockRand = m_mediator.m_dsBlockRand;
            txBlockRand = m_mediator.m_txBlockRand;

            m_mediator.m_node->SetState(Node::POW_SUBMISSION);
            POW::GetInstance().EthashConfigureLightClient(
                m_mediator.m_dsBlockChain.GetLastBlock()
                    .GetHeader()
                    .GetBlockNum()
                + 1);

            this_thread::sleep_for(chrono::seconds(NEW_NODE_POW_DELAY));

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Starting PoW for new ds block number "
                          << curDsBlockNum + 1);

            m_mediator.m_node->StartPoW(curDsBlockNum + 1,
                                        m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetDSDifficulty(),
                                        m_mediator.m_dsBlockChain.GetLastBlock()
                                            .GetHeader()
                                            .GetDifficulty(),
                                        dsBlockRand, txBlockRand);
        }
        else
        {
            LOG_GENERAL(WARNING, "State root check failed");
            return false;
        }
    }
    else
    {
        LOG_EPOCH(
            WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DS block num check failed. Current DS epoch = "
                << (m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW)
                << " Stored DS block = " << curDsBlockNum);
        return false;
    }
    // Check whether is the new node connected to the network. Else, initiate re-sync process again.
    this_thread::sleep_for(chrono::seconds(POW_BACKUP_WINDOW_IN_SECONDS));
    m_startedPoW = false;
    if (m_syncType != SyncType::NO_SYNC)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not yet connected to network");
        m_mediator.m_node->SetState(Node::SYNC);
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

bool Lookup::ProcessSetLookupOffline(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessSetLookupOffline not expected to be called "
                    "from other than the LookUp node.");
        return true;
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

    {
        lock_guard<mutex> lock(m_mutexOfflineLookups);
        auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(),
                              requestingNode);
        if (iter != m_lookupNodes.end())
        {
            m_lookupNodesOffline.emplace_back(requestingNode);
            m_lookupNodes.erase(iter);
        }
        else
        {
            LOG_GENERAL(WARNING, "The Peer Info is not in m_lookupNodes");
            return false;
        }
    }
    return true;
}

bool Lookup::ProcessSetLookupOnline(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessSetLookupOnline not expected to be called "
                    "from other than the LookUp node.");
        return true;
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

    {
        lock_guard<mutex> lock(m_mutexOfflineLookups);
        auto iter = std::find(m_lookupNodesOffline.begin(),
                              m_lookupNodesOffline.end(), requestingNode);
        if (iter != m_lookupNodesOffline.end())
        {
            m_lookupNodes.emplace_back(requestingNode);
            m_lookupNodesOffline.erase(iter);
        }
        else
        {
            LOG_GENERAL(WARNING,
                        "The Peer Info is not in m_lookupNodesOffline");
            return false;
        }
    }
    return true;
}

bool Lookup::ProcessGetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessGetOfflineLookups not expected to be "
                    "called from other than the LookUp node.");
        return true;
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
    LOG_GENERAL(INFO, requestingNode);

    // vector<Peer> node;
    // node.emplace_back(requestingNode);

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

        for (auto& peer : m_lookupNodesOffline)
        {
            peer.Serialize(offlineLookupsMessage, curr_offset);
            curr_offset += (IP_SIZE + PORT_SIZE);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "IP:" << peer.GetPrintableIPAddress());
        }
    }

    P2PComm::GetInstance().SendMessage(requestingNode, offlineLookupsMessage);
    return true;
}

bool Lookup::ProcessSetOfflineLookups(const std::vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
    // Message = [num_offline_lookups][LookupPeer][LookupPeer]... num_offline_lookups times
    LOG_MARKER();
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessSetOfflineLookups not expected to be "
                    "called from the LookUp node.");
        return true;
    }
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

        std::lock_guard<std::mutex> lock(m_mutexOfflineLookups);
        // Remove selfPeerInfo from m_lookupNodes
        auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(), peer);
        if (iter != m_lookupNodes.end())
        {
            m_lookupNodesOffline.emplace_back(peer);
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
    return true;
}

bool Lookup::ProcessRaiseStartPoW(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
    // Message = empty

    LOG_MARKER();

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessRaiseStartPoW not expected to be called "
                    "from other than the LookUp node.");
        return true;
    }

    // DS leader has informed me that it's time to start PoW
    m_receivedRaiseStartPoW = true;
    cv_startPoWSubmission.notify_all();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "Threads running ProcessGetStartPoWFromSeed notified to start PoW");

    // Sleep for a while, then let all remaining threads running ProcessGetStartPoWFromSeed know that it's too late to do PoW
    // Sleep time = time it takes for new node to try getting DSInfo + actual PoW window
    this_thread::sleep_for(
        chrono::seconds(NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS));
    m_receivedRaiseStartPoW = false;
    cv_startPoWSubmission.notify_all();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Threads running ProcessGetStartPoWFromSeed notified it's too "
              "late to start PoW");

    return true;
}

bool Lookup::ProcessGetStartPoWFromSeed(const vector<unsigned char>& message,
                                        unsigned int offset, const Peer& from)
{
    // Message = [Peer listen port]

    LOG_MARKER();

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessGetStartPoWFromSeed not expected to be "
                    "called from other than the LookUp node.");
        return true;
    }

    // Normally I'll get this message from new nodes at the vacuous epoch
    // Wait a while if I haven't received RAISESTARTPOW from DS leader yet
    // Wait time = time it takes to finish the vacuous epoch (or at least part of it) + actual PoW window
    if (!m_receivedRaiseStartPoW)
    {
        std::unique_lock<std::mutex> cv_lk(m_MutexCVStartPoWSubmission);

        if (cv_startPoWSubmission.wait_for(
                cv_lk, std::chrono::seconds(POW_WINDOW_IN_SECONDS))
            == std::cv_status::timeout)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Timed out waiting for DS leader to raise startPoW");
            return false;
        }

        if (!m_receivedRaiseStartPoW)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "PoW duration already passed");
            return false;
        }
    }

    // Tell the new node that it's time to start PoW
    vector<unsigned char> setstartpow_message
        = {MessageType::LOOKUP, LookupInstructionType::SETSTARTPOWFROMSEED};
    P2PComm::GetInstance().SendMessage(
        Peer(from.m_ipAddress,
             Serializable::GetNumber<uint32_t>(message, offset,
                                               sizeof(uint32_t))),
        setstartpow_message);

    return true;
}

bool Lookup::ProcessSetStartPoWFromSeed(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
    // Message = empty

    LOG_MARKER();

    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ProcessSetStartPoWFromSeed not expected to be "
                    "called from the LookUp node.");
        return true;
    }

    InitMining();

    if (m_syncType == SyncType::DS_SYNC)
    {
        if (!m_currDSExpired
            && m_mediator.m_ds->m_latestActiveDSBlockNum
                < m_mediator.m_dsBlockChain.GetLastBlock()
                      .GetHeader()
                      .GetBlockNum())
        {
            m_isFirstLoop = true;
            m_syncType = SyncType::NO_SYNC;
            m_mediator.m_ds->FinishRejoinAsDS();
        }

        m_currDSExpired = false;
    }

    return true;
}

void Lookup::StartSynchronization()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::StartSynchronization not expected to be called "
                    "from other than the LookUp node.");
        return;
    }

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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetLookupPeerToRsync not expected to be called "
                    "from other than the LookUp node.");
        return Peer();
    }

    LOG_MARKER();

    std::vector<Peer> t_Peers;
    for (auto p : m_lookupNodes)
    {
        if (p != m_mediator.m_selfPeer)
        {
            t_Peers.emplace_back(p);
        }
    }

    int index = rand() % t_Peers.size();

    return t_Peers[index];
}

std::vector<unsigned char> Lookup::ComposeGetLookupOfflineMessage()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ComposeGetLookupOfflineMessage not expected to be "
                    "called from other than the LookUp node.");
        return std::vector<unsigned char>();
    }

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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ComposeGetLookupOnlineMessage not expected to be "
                    "called from other than the LookUp node.");
        return std::vector<unsigned char>();
    }

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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetMyLookupOffline not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    LOG_MARKER();

    std::lock_guard<std::mutex> lock(m_mutexOfflineLookups);
    // Remove selfPeerInfo from m_lookupNodes
    auto iter = std::find(m_lookupNodes.begin(), m_lookupNodes.end(),
                          m_mediator.m_selfPeer);
    if (iter != m_lookupNodes.end())
    {
        m_lookupNodesOffline.emplace_back(m_mediator.m_selfPeer);
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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetMyLookupOnline not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    LOG_MARKER();

    std::lock_guard<std::mutex> lock(m_mutexOfflineLookups);
    auto iter = std::find(m_lookupNodesOffline.begin(),
                          m_lookupNodesOffline.end(), m_mediator.m_selfPeer);
    if (iter != m_lookupNodesOffline.end())
    {
        SendMessageToLookupNodesSerial(ComposeGetLookupOnlineMessage());
        m_lookupNodes.emplace_back(m_mediator.m_selfPeer);
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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::RsyncTxBodies not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

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

    string output;
    if (!SysCommand::ExecuteCmdWithOutput(cmdStr, output))
    {
        return false;
    }
    LOG_GENERAL(INFO, "RunRsync: " << output);
    return true;
}

void Lookup::RejoinAsLookup()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::RejoinAsLookup not expected to be called from "
                    "other than the LookUp node.");
        return;
    }

    LOG_MARKER();
    if (m_syncType == SyncType::NO_SYNC)
    {
        auto func = [this]() mutable -> void {
            m_syncType = SyncType::LOOKUP_SYNC;
            AccountStore::GetInstance().InitSoft();
            m_mediator.m_node->Install(SyncType::LOOKUP_SYNC, true);
            this->StartSynchronization();
        };
        DetachedFunction(1, func);
    }
}

bool Lookup::FinishRejoinAsLookup()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::FinishRejoinAsLookup not expected to be called "
                    "from other than the LookUp node.");
        return true;
    }

    return GetMyLookupOnline();
}

bool Lookup::CleanVariables()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::CleanVariables not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    m_seedNodes.clear();
    m_currDSExpired = false;
    m_isFirstLoop = true;
    {
        std::lock_guard<mutex> lock(m_mutexShards);
        m_mediator.m_ds->m_shards.clear();
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
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ToBlockMessage not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    return m_syncType != SyncType::NO_SYNC
        && (ins_byte != LookupInstructionType::SETDSBLOCKFROMSEED
            && ins_byte != LookupInstructionType::SETDSINFOFROMSEED
            && ins_byte != LookupInstructionType::SETTXBLOCKFROMSEED
            && ins_byte != LookupInstructionType::SETSTATEFROMSEED
            && ins_byte != LookupInstructionType::SETLOOKUPOFFLINE
            && ins_byte != LookupInstructionType::SETLOOKUPONLINE);
}

std::vector<unsigned char> Lookup::ComposeGetOfflineLookupNodes()
{
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::ComposeGetOfflineLookupNodes not expected to be "
                    "called from the LookUp node.");
        return std::vector<unsigned char>();
    }

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
    if (LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetOfflineLookupNodes not expected to be called "
                    "from the LookUp node.");
        return true;
    }

    LOG_MARKER();
    // Reset m_lookupNodes/m_lookupNodesOffline
    SetLookupNodes();
    SendMessageToLookupNodesSerial(ComposeGetOfflineLookupNodes());
    return true;
}

bool Lookup::Execute(const vector<unsigned char>& message, unsigned int offset,
                     const Peer& from)
{
    LOG_MARKER();

    bool result = true;

    typedef bool (Lookup::*InstructionHandler)(const vector<unsigned char>&,
                                               unsigned int, const Peer&);

    InstructionHandler ins_handlers[] = {&Lookup::ProcessGetSeedPeersFromLookup,
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
                                         &Lookup::ProcessRaiseStartPoW,
                                         &Lookup::ProcessGetStartPoWFromSeed,
                                         &Lookup::ProcessSetStartPoWFromSeed,
                                         &Lookup::ProcessGetShardFromSeed,
                                         &Lookup::ProcessSetShardFromSeed};

    const unsigned char ins_byte = message.at(offset);
    const unsigned int ins_handlers_count
        = sizeof(ins_handlers) / sizeof(InstructionHandler);

    if (LOOKUP_NODE_MODE)
    {
        if (ToBlockMessage(ins_byte))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Ignore lookup message");
            return false;
        }
    }

    if (ins_byte < ins_handlers_count)
    {
        result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);
        if (!result)
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

bool Lookup::AlreadyJoinedNetwork() { return m_syncType == SyncType::NO_SYNC; }

bool Lookup::AddToTxnShardMap(const Transaction& tx, uint32_t shardId)
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::AddToTxnShardMap not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    lock_guard<mutex> g(m_txnShardMapMutex);

    m_txnShardMap[shardId].push_back(tx);

    return true;
}

bool Lookup::DeleteTxnShardMap(uint32_t shardId)
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::DeleteTxnShardMap not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    lock_guard<mutex> g(m_txnShardMapMutex);

    m_txnShardMap[shardId].clear();

    return true;
}

void Lookup::SenderTxnBatchThread()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(
            WARNING,
            "Lookup::SenderTxnBatchThread not expected to be called from "
            "other than the LookUp node.");
        return;
    }
    LOG_MARKER();

    auto main_func = [this]() mutable -> void {
        uint32_t nShard;
        while (true)
        {
            if ((m_mediator.m_currentEpochNum + 1) % NUM_FINAL_BLOCK_PER_POW
                != 0)
            {
                {
                    lock_guard<mutex> g(m_mutexShards);
                    nShard = m_mediator.m_ds->m_shards.size();
                }
                if (nShard == 0)
                {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    continue;
                }
                SendTxnPacketToNodes(nShard);
            }
            break;
        }

    };
    DetachedFunction(1, main_func);
}

void Lookup::SendTxnPacketToNodes(uint32_t nShard)
{
    LOG_MARKER();

    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(
            WARNING,
            "Lookup::SendTxnPacketToNodes not expected to be called from "
            "other than the LookUp node.");
        return;
    }

    map<uint32_t, vector<unsigned char>> mp;

    if (!GenTxnToSend(NUM_TXN_TO_SEND_PER_ACCOUNT, mp, nShard))
    {
        LOG_GENERAL(WARNING, "GenTxnToSend failed");
        // return;
    }

    for (unsigned int i = 0; i < nShard + 1; i++)
    {
        vector<unsigned char> msg
            = {MessageType::NODE, NodeInstructionType::FORWARDTXNBLOCK};
        bool result = false;

        {
            lock_guard<mutex> g(m_txnShardMapMutex);

            unsigned int size_dummy
                = (mp.find(i) != mp.end()) ? mp.at(i).size() : 0;
            size_dummy = size_dummy / Transaction::GetMinSerializedSize();

            if (size_dummy > 0)
            {
                result = Messenger::SetNodeForwardTxnBlock(
                    msg, MessageOffset::BODY, m_mediator.m_currentEpochNum, i,
                    m_txnShardMap[i], mp.at(i));
            }
            else
            {
                result = Messenger::SetNodeForwardTxnBlock(
                    msg, MessageOffset::BODY, m_mediator.m_currentEpochNum, i,
                    m_txnShardMap[i], {});
            }
        }

        if (!result)
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Messenger::SetNodeForwardTxnBlock failed.");
            LOG_GENERAL(WARNING, "Cannot create packet for " << i << " shard");
            continue;
        }
        vector<Peer> toSend;
        if (i < nShard)
        {

            {
                lock_guard<mutex> g(m_mutexShards);
                auto it = m_mediator.m_ds->m_shards.at(i).begin();

                for (unsigned int j = 0; j < NUM_NODES_TO_SEND_LOOKUP
                     && it != m_mediator.m_ds->m_shards.at(i).end();
                     j++, it++)
                {
                    toSend.push_back(it->second);
                }
            }

            P2PComm::GetInstance().SendBroadcastMessage(toSend, msg);

            LOG_GENERAL(INFO, "Packet disposed off to " << i << " shard");

            DeleteTxnShardMap(i);
        }
        else if (i == nShard)
        {
            //To send DS
            {
                lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
                auto it = m_mediator.m_DSCommittee->begin();

                for (unsigned int j = 0; j < NUM_NODES_TO_SEND_LOOKUP
                     && it != m_mediator.m_DSCommittee->end();
                     j++, it++)
                {
                    toSend.push_back(it->second);
                }
            }

            P2PComm::GetInstance().SendBroadcastMessage(toSend, msg);

            LOG_GENERAL(INFO,
                        "[DSMB]"
                            << " Sent DS the txns");
        }
    }
}

void Lookup::SetServerTrue()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::SetServerTrue not expected to be called from "
                    "other than the LookUp node.");
        return;
    }

    m_isServer = true;
}

bool Lookup::GetIsServer()
{
    if (!LOOKUP_NODE_MODE)
    {
        LOG_GENERAL(WARNING,
                    "Lookup::GetIsServer not expected to be called from "
                    "other than the LookUp node.");
        return true;
    }

    return m_isServer;
}
