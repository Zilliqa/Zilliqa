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

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "common/Messages.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/SanityChecks.h"
#include "Lookup.h"

using namespace std;
using namespace boost::multiprecision;

Lookup::Lookup(Mediator & mediator) : m_mediator(mediator)
{
#ifndef IS_LOOKUP_NODE
    SetLookupNodes();
#else // IS_LOOKUP_NODE
    SetDSCommitteInfo();   
#endif // IS_LOOKUP_NODE    
}

Lookup::~Lookup()
{

}

#ifndef IS_LOOKUP_NODE
void Lookup::SetLookupNodes()
{
    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml("constants.xml", pt);

    for(const ptree::value_type & v : pt.get_child("node.lookups"))
    {
        if (v.first == "peer")
        {
            struct in_addr ip_addr;
            inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
            Peer lookup_node((uint128_t)ip_addr.s_addr, v.second.get<uint32_t>("port"));
            m_lookupNodes.push_back(lookup_node);
        }
    }  
}

vector<Peer> Lookup::GetLookupNodes()
{
    return m_lookupNodes;
}

void Lookup::SendMessageToLookupNodes(const std::vector<unsigned char> & message) const
{
    // LOG_MESSAGE("i am here " << to_string(m_mediator.m_currentEpochNum).c_str())
    for(auto node: m_lookupNodes) 
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Sending msg to lookup node " << node.GetPrintableIPAddress() <<
                     ":" << node.m_listenPortHost);
        P2PComm::GetInstance().SendMessage(node, message);
    }
}

void Lookup::SendMessageToSeedNodes(const std::vector<unsigned char> &message) const
{
    for(auto node: m_seedNodes)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
                     "Sending msg to seed node " << node.GetPrintableIPAddress() <<
                     ":" << node.m_listenPortHost);
        P2PComm::GetInstance().SendMessage(node, message);
    }
}

bool Lookup::GetSeedPeersFromLookup()
{
    vector<unsigned char> getSeedPeersMessage = { MessageType::LOOKUP, 
                                                  LookupInstructionType::GETSEEDPEERS };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getSeedPeersMessage, curr_offset, 
                                      m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    SendMessageToLookupNodes(getSeedPeersMessage);

    return true;   
}

vector<unsigned char> Lookup::ComposeGetDSInfoMessage()
{
    // getDSNodesMessage = [Port]
    vector<unsigned char> getDSNodesMessage = { MessageType::LOOKUP, 
                                                LookupInstructionType::GETDSINFOFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(getDSNodesMessage, curr_offset, 
                                      m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    return getDSNodesMessage;
}

bool Lookup::GetDSInfoFromSeedNodes()
{
    SendMessageToSeedNodes(ComposeGetDSInfoMessage());
    return true;
}

bool Lookup::GetDSInfoFromLookupNodes()
{
    SendMessageToLookupNodes(ComposeGetDSInfoMessage());
    return true;
}

vector<unsigned char> Lookup::ComposeGetDSBlockMessage(uint256_t lowBlockNum, 
                                                       uint256_t highBlockNum)
{
    // getDSBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getDSBlockMessage = { MessageType::LOOKUP, 
                                                LookupInstructionType::GETDSBLOCKFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset, 
        lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset, 
        highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint32_t>(getDSBlockMessage, curr_offset, 
        m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    
    return getDSBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetDSBlockFromSeedNodes(uint256_t lowBlockNum, uint256_t highBlockNum)
{
    SendMessageToSeedNodes(ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetDSBlockFromLookupNodes(uint256_t lowBlockNum, uint256_t highBlockNum)
{
    SendMessageToLookupNodes(ComposeGetDSBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

vector<unsigned char> Lookup::ComposeGetTxBlockMessage(uint256_t lowBlockNum, 
                                                       uint256_t highBlockNum)
{
    // getTxBlockMessage = [lowBlockNum][highBlockNum][Port]
    vector<unsigned char> getTxBlockMessage = { MessageType::LOOKUP, 
                                                LookupInstructionType::GETTXBLOCKFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(getTxBlockMessage, curr_offset, 
        lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint256_t>(getTxBlockMessage, curr_offset, 
        highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint32_t>(getTxBlockMessage, curr_offset, 
        m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    
    return getTxBlockMessage;
}

// low and high denote the range of blocknumbers being requested(inclusive).
// use 0 to denote the latest blocknumber since obviously no one will request for the genesis block
bool Lookup::GetTxBlockFromSeedNodes(uint256_t lowBlockNum, uint256_t highBlockNum)
{
    SendMessageToSeedNodes(ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetTxBlockFromLookupNodes(uint256_t lowBlockNum, uint256_t highBlockNum)
{
    SendMessageToLookupNodes(ComposeGetTxBlockMessage(lowBlockNum, highBlockNum));
    return true;
}

bool Lookup::GetTxBodyFromSeedNodes(string txHashStr)
{
    // getTxBodyMessage = [TRAN_HASH_SIZE txHashStr][4-byte Port]
    vector<unsigned char> getTxBodyMessage = { MessageType::LOOKUP, 
                                               LookupInstructionType::GETTXBODYFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    std::array<unsigned char, TRAN_HASH_SIZE> hash = DataConversion::HexStrToStdArray(txHashStr);

    getTxBodyMessage.resize(curr_offset + TRAN_HASH_SIZE);

    copy(hash.begin(), hash.end(), getTxBodyMessage.begin() + curr_offset);
    curr_offset += TRAN_HASH_SIZE;

    Serializable::SetNumber<uint32_t>(getTxBodyMessage, curr_offset, 
                                      m_mediator.m_selfPeer.m_listenPortHost, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    SendMessageToSeedNodes(getTxBodyMessage);

    return true;
}
#else // IS_LOOKUP_NODE

    bool Lookup::SetDSCommitteInfo()
    {
        // Populate tree structure pt
        using boost::property_tree::ptree;
        ptree pt;
        read_xml("config.xml", pt);

        for(ptree::value_type const & v : pt.get_child("nodes"))
        {
            if (v.first == "peer")
            {
                PubKey key(DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")), 0);
                m_mediator.m_DSCommitteePubKeys.push_back(key);

                struct in_addr ip_addr;
                inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
                Peer peer((uint128_t)ip_addr.s_addr, v.second.get<unsigned int>("port"));
                m_mediator.m_DSCommitteeNetworkInfo.push_back(peer);
            }
        }  

        return true;      
    }

#endif // IS_LOOKUP_NODE

bool Lookup::ProcessEntireShardingStructure(const vector<unsigned char> & message, 
                                            unsigned int offset, const Peer & from)
{
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

    LOG_MARKER();

    LOG_MESSAGE("[LOOKUP received sharding structure]");

    unsigned int length_available = message.size() - offset;

    if (length_available < 4)
    {
        LOG_MESSAGE("Error: Malformed message");
        return false;
    }

    // 4-byte num of shards
    uint32_t num_shards = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    LOG_MESSAGE("Number of shards: " << to_string(num_shards));

    m_shards.clear();
    m_nodesInNetwork.clear();

    for (unsigned int i = 0; i < num_shards; i++)
    {
        length_available = message.size() - offset;

        if (length_available < 4)
        {
            LOG_MESSAGE("Error: Malformed message");
            return false;
        }

        m_shards.push_back(map<PubKey, Peer>());

        // 4-byte shard size
        uint32_t shard_size = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        length_available = message.size() - offset;

        LOG_MESSAGE("Size of shard " << to_string(i) << ": " << to_string(shard_size));

        if (length_available < (33+16+4) * shard_size)
        {
            LOG_MESSAGE("Error: Malformed message");
            return false;
        }

        map<PubKey, Peer> & shard = m_shards.at(i);

        for (unsigned int j = 0; j < shard_size; j++)
        {
            // 33-byte public key
            PubKey key = PubKey(message, offset);
            offset += PUB_KEY_SIZE;

            // 16-byte IP + 4-byte port
            Peer peer = Peer(message, offset);
            offset += IP_SIZE + PORT_SIZE;

            shard.insert(make_pair(key, peer));

            m_nodesInNetwork.push_back(peer);

            LOG_MESSAGE("[SHARD " << to_string(i) << "] " << "[PEER " << to_string(j) << "] " <<
                                  "Inserting Pubkey to shard : " << string(key));
            LOG_MESSAGE("[SHARD " << to_string(i) << "] " << "[PEER " << to_string(j) << "] " <<
                                  "Corresponding peer : " << string(peer));
        }
    }

#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetSeedPeersFromLookup(const vector<unsigned char> & message, 
                                           unsigned int offset, const Peer & from)
{
#ifdef IS_LOOKUP_NODE
    // Message = [4-byte listening port]

    LOG_MARKER();

    const unsigned int length_available = message.size() - offset;
    const unsigned int min_length_needed = sizeof(uint32_t);

    if (min_length_needed > length_available)
    {
        LOG_MESSAGE("Error: Malformed message");
        return false;
    }

    // 4-byte listening port
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    uint32_t numPeersInNetwork = m_nodesInNetwork.size();

    if(numPeersInNetwork < SEED_PEER_LIST_SIZE)
    {
        LOG_MESSAGE("Error: [Lookup Node] numPeersInNetwork < SEED_PEER_LIST_SIZE");
        return false;
    }

    vector<unsigned char> seedPeersMessage = { MessageType::LOOKUP, 
                                               LookupInstructionType::SETSEEDPEERS };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(seedPeersMessage, curr_offset, SEED_PEER_LIST_SIZE, 
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Which of the following two implementations is more efficient and parallelizable?
    // ================================================

    unordered_set<uint32_t> indicesAlreadyAdded;

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, numPeersInNetwork-1);

    for(unsigned int i = 0; i < SEED_PEER_LIST_SIZE; i++)
    {
        uint32_t index = dis(gen);
        while(indicesAlreadyAdded.find(index) != indicesAlreadyAdded.end())
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

bool Lookup::ProcessGetDSInfoFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                      const Peer & from)
{
//#ifndef IS_LOOKUP_NODE
    // Message = [Port]

    LOG_MARKER();

    deque<PubKey> dsPubKeys = m_mediator.m_DSCommitteePubKeys;
    deque<Peer> dsPeers = m_mediator.m_DSCommitteeNetworkInfo; // Data::GetInstance().GetDSPeers();

    // dsInfoMessage = [num_ds_peers][DSPeer][DSPeer]... num_ds_peers times
    vector<unsigned char> dsInfoMessage = { MessageType::LOOKUP, 
                                            LookupInstructionType::SETDSINFOFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(dsInfoMessage, curr_offset, dsPeers.size(), sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    for(unsigned int i = 0; i < dsPeers.size(); i++)
    {
        PubKey & pubKey = dsPubKeys.at(i);
        pubKey.Serialize(dsInfoMessage, curr_offset);
        curr_offset += (PUB_KEY_SIZE);

        Peer & peer = dsPeers.at(i);     
        peer.Serialize(dsInfoMessage, curr_offset);
        curr_offset += (IP_SIZE + PORT_SIZE);
    }

    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    // 4-byte listening port
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);    

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    P2PComm::GetInstance().SendMessage(requestingNode, dsInfoMessage);

//#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetDSBlockFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                       const Peer & from)
{
//#ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][4-byte portNo] 

    LOG_MARKER();


    if (IsMessageSizeInappropriate(message.size(), offset, UINT256_SIZE + UINT256_SIZE + 
                                   sizeof(uint32_t)))
    {
        return false;
    }

    // 32-byte lower-limit block number 
    boost::multiprecision::uint256_t lowBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    // 32-byte upper-limit block number 
    boost::multiprecision::uint256_t highBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    if(highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    }

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                 "ProcessGetDSBlockFromSeed requested by " << from << " for blocks " <<
                 lowBlockNum.convert_to<string>() << " to " << highBlockNum.convert_to<string>());

    // dsBlockMessage = [lowBlockNum][highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> dsBlockMessage = { MessageType::LOOKUP, 
                                             LookupInstructionType::SETDSBLOCKFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(dsBlockMessage, curr_offset, lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint256_t>(dsBlockMessage, curr_offset, highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    boost::multiprecision::uint256_t blockNum; 

    for(blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "Fetching DSBlock " << blockNum.convert_to<string>() << " for " << from);
            DSBlock dsBlock = m_mediator.m_dsBlockChain.GetBlock(blockNum);
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "DSBlock " << blockNum.convert_to<string>() << " serialized for " << from);
            dsBlock.Serialize(dsBlockMessage, curr_offset);
            curr_offset += DSBlock::GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_MESSAGE("Block Number " + blockNum.convert_to<string>() + 
                        " absent. Didn't include it in response message. Reason: " << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if(blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint256_t>(dsBlockMessage, highBlockNumOffset, blockNum - 1, 
                                           UINT256_SIZE);
    }

    // 4-byte portNo
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);    

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    P2PComm::GetInstance().SendMessage(requestingNode, dsBlockMessage);

//#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetTxBlockFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                       const Peer & from)
{
// #ifndef IS_LOOKUP_NODE // TODO: remove the comment
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][4-byte portNo] 

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, UINT256_SIZE + UINT256_SIZE + 
                                   sizeof(uint32_t)))
    {
        return false;
    }

    // 32-byte lower-limit block number 
    boost::multiprecision::uint256_t lowBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    // 32-byte upper-limit block number 
    boost::multiprecision::uint256_t highBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    if(highBlockNum == 0)
    {
        highBlockNum = m_mediator.m_txBlockChain.GetBlockCount() - 1;
    }

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                 "ProcessGetTxBlockFromSeed requested by " << from << " for blocks " <<
                 lowBlockNum.convert_to<string>() << " to " << highBlockNum.convert_to<string>());

    // txBlockMessage = [lowBlockNum][highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times
    vector<unsigned char> txBlockMessage = { MessageType::LOOKUP, 
                                             LookupInstructionType::SETTXBLOCKFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(txBlockMessage, curr_offset, lowBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    unsigned int highBlockNumOffset = curr_offset;

    Serializable::SetNumber<uint256_t>(txBlockMessage, curr_offset, highBlockNum, UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    boost::multiprecision::uint256_t blockNum; 

    for(blockNum = lowBlockNum; blockNum <= highBlockNum; blockNum++)
    {
        try
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "Fetching TxBlock " << blockNum.convert_to<string>() << " for " << from);
            TxBlock txBlock = m_mediator.m_txBlockChain.GetBlock(blockNum);
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "TxBlock " << blockNum.convert_to<string>() << " serialized for " << from);
            txBlock.Serialize(txBlockMessage, curr_offset);
            curr_offset += txBlock.GetSerializedSize();
        }
        catch (const char* e)
        {
            LOG_MESSAGE("Block Number " + blockNum.convert_to<string>() + 
                        " absent. Didn't include it in response message. Reason: " << e);
            break;
        }
    }

    // if serialization got interrupted in between, reset the highBlockNum value in msg
    if(blockNum != highBlockNum + 1)
    {
        Serializable::SetNumber<uint256_t>(txBlockMessage, highBlockNumOffset, blockNum - 1, 
                                           UINT256_SIZE);
    }

    // 4-byte portNo
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);    

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    P2PComm::GetInstance().SendMessage(requestingNode, txBlockMessage);

// #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetTxBodyFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                      const Peer & from)
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
    vector<unsigned char> txBodyMessage = { MessageType::LOOKUP, 
                                            LookupInstructionType::SETTXBODYFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    copy(tranHash.asArray().begin(), tranHash.asArray().end(), txBodyMessage.begin() + curr_offset);
    curr_offset += TRAN_HASH_SIZE;

    tx->Serialize(txBodyMessage, curr_offset);
    curr_offset += Transaction::GetSerializedSize();

    // 4-byte portNo
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);    

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    P2PComm::GetInstance().SendMessage(requestingNode, txBodyMessage);

// #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessGetNetworkId(const vector<unsigned char> & message, unsigned int offset,
                                 const Peer &from)
{
// #ifndef IS_LOOKUP_NODE  
    LOG_MARKER();

    // 4-byte portNo
    uint32_t portNo = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);    

    uint128_t ipAddr = from.m_ipAddress;
    Peer requestingNode(ipAddr, portNo);

    vector<unsigned char> networkIdMessage = { MessageType::LOOKUP, 
                                               LookupInstructionType::SETNETWORKIDFROMSEED };
    unsigned int curr_offset = MessageOffset::BODY;

    string networkId = "TESTNET"; // TODO: later convert it to a enum

    copy(networkId.begin(), networkId.end(), networkIdMessage.begin() + curr_offset);

    P2PComm::GetInstance().SendMessage(requestingNode, networkIdMessage);

    return true;
// #endif // IS_LOOKUP_NODE
}

bool Lookup::ProcessSetSeedPeersFromLookup(const vector<unsigned char> &message, 
                                           unsigned int offset, const Peer &from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [Peer info][Peer info]... SEED_PEER_LIST_SIZE times

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, 
                                   (IP_SIZE + PORT_SIZE) * SEED_PEER_LIST_SIZE))
    {
        return false;
    }

    for(unsigned int i = 0; i < SEED_PEER_LIST_SIZE; i++)
    {
        Peer peer = Peer(message, offset);
        m_seedNodes.push_back(peer);
        LOG_MESSAGE("Peer " + to_string(i) + ": " << string(peer));
        offset += (IP_SIZE + PORT_SIZE);
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetDSInfoFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                      const Peer & from)
{
//#ifndef IS_LOOKUP_NODE
    // Message = [numDSPeers][DSPeer][DSPeer]... numDSPeers times

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, sizeof(uint32_t)))
    {
        return false;
    }

    uint32_t numDSPeers = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t); 

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                 "ProcessSetDSInfoFromSeed sent by " << from << " for numPeers " << numDSPeers);


    if (IsMessageSizeInappropriate(message.size(), offset, 
                                   (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE) * numDSPeers))
    {
        return false;
    }

    deque<PubKey> dsPubKeys;
    deque<Peer> dsPeers;

    for(unsigned int i = 0; i < numDSPeers; i++)
    {
        dsPubKeys.push_back(PubKey(message, offset));
        offset += PUB_KEY_SIZE;

        Peer peer(message, offset);     
        offset += (IP_SIZE + PORT_SIZE);

        dsPeers.push_back(peer);

        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "ProcessSetDSInfoFromSeed recvd peer " << i << ": " << peer);
    }

    m_mediator.m_DSCommitteePubKeys = dsPubKeys;
    m_mediator.m_DSCommitteeNetworkInfo = dsPeers;
//    Data::GetInstance().SetDSPeers(dsPeers);
//#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetDSBlockFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                       const Peer & from)
{
// #ifndef IS_LOOKUP_NODE TODO: uncomment later
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][DSBlock][DSBlock]... (highBlockNum - lowBlockNum + 1) times

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, UINT256_SIZE + UINT256_SIZE))
    {
        return false;
    }

    // 32-byte lower-limit block number 
    boost::multiprecision::uint256_t lowBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    // 32-byte upper-limit block number 
    boost::multiprecision::uint256_t highBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                 "ProcessSetDSBlockFromSeed sent by " << from << " for blocks " <<
                 lowBlockNum.convert_to<string>() << " to " << highBlockNum.convert_to<string>());

    // since we will usually only enable sending of 500 blocks max, casting to uint32_t should be safe
    if (IsMessageSizeInappropriate(message.size(), offset, 
        (uint32_t)(highBlockNum - lowBlockNum + 1) * DSBlock::GetSerializedSize()))
    {
        return false;
    }

    for(boost::multiprecision::uint256_t blockNum = lowBlockNum; 
        blockNum <= highBlockNum; 
        blockNum++)
    {
        DSBlock dsBlock(message, offset);
        offset += DSBlock::GetSerializedSize();

        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "I the lookup node have deserialized the DS Block"); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "dsblock.GetHeader().GetDifficulty(): " << 
                     (int) dsBlock.GetHeader().GetDifficulty()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "dsblock.GetHeader().GetNonce(): " << dsBlock.GetHeader().GetNonce()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "dsblock.GetHeader().GetBlockNum(): " << dsBlock.GetHeader().GetBlockNum()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "dsblock.GetHeader().GetMinerPubKey().hex(): " << 
                     dsBlock.GetHeader().GetMinerPubKey()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "dsblock.GetHeader().GetLeaderPubKey().hex(): " << 
                     dsBlock.GetHeader().GetLeaderPubKey());    

        m_mediator.m_dsBlockChain.AddBlock(dsBlock);

        // Store DS Block to disk
        vector<unsigned char> serializedDSBlock;
        dsBlock.Serialize(serializedDSBlock, 0);
        BlockStorage::GetBlockStorage().PutDSBlock(dsBlock.GetHeader().GetBlockNum(), 
                                                   serializedDSBlock);
    }

    m_mediator.UpdateDSBlockRand();

    {
        unique_lock<mutex> lock(m_dsRandUpdationMutex);
        m_isDSRandUpdated = true;
        m_dsRandUpdateCondition.notify_one();
    }
// #endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetTxBlockFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                       const Peer & from)
{
//#ifndef IS_LOOKUP_NODE
    // Message = [32-byte lowBlockNum][32-byte highBlockNum][TxBlock][TxBlock]... (highBlockNum - lowBlockNum + 1) times

    LOG_MARKER();
    unique_lock<mutex> lock(m_mutexSetTxBlockFromSeed);

    if (IsMessageSizeInappropriate(message.size(), offset, UINT256_SIZE + UINT256_SIZE))
    {
        return false;
    }

    // 32-byte lower-limit block number 
    boost::multiprecision::uint256_t lowBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    // 32-byte upper-limit block number 
    boost::multiprecision::uint256_t highBlockNum = 
        Serializable::GetNumber<uint256_t>(message, offset, UINT256_SIZE);
    offset += UINT256_SIZE; 

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                 "ProcessSetTxBlockFromSeed sent by " << from << " for blocks " <<
                 lowBlockNum.convert_to<string>() << " to " << highBlockNum.convert_to<string>());


    uint64_t latestSynBlockNum = (uint64_t) m_mediator.m_txBlockChain.GetBlockCount(); 
    if (latestSynBlockNum >= highBlockNum)
    {
        // TODO: We should get blocks from n nodes.
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                    "I already have the block"); 
        return false; 
    }

    for(boost::multiprecision::uint256_t blockNum = lowBlockNum; 
        blockNum <= highBlockNum; 
        blockNum++)
    {
        TxBlock txBlock(message, offset);
        offset += txBlock.GetSerializedSize();

        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "I the lookup node have deserialized the TxBlock"); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetType(): " << txBlock.GetHeader().GetType()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetVersion(): " << txBlock.GetHeader().GetVersion()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetGasLimit(): " << txBlock.GetHeader().GetGasLimit()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetGasUsed(): " << txBlock.GetHeader().GetGasUsed()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetBlockNum(): " << txBlock.GetHeader().GetBlockNum());
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetNumMicroBlockHashes(): " << 
                     txBlock.GetHeader().GetNumMicroBlockHashes()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetNumTxs(): " << txBlock.GetHeader().GetNumTxs()); 
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "txBlock.GetHeader().GetMinerPubKey(): " << 
                     txBlock.GetHeader().GetMinerPubKey());

        m_mediator.m_txBlockChain.AddBlock(txBlock);

        // Store Tx Block to disk
        vector<unsigned char> serializedTxBlock;
        txBlock.Serialize(serializedTxBlock, 0);
        BlockStorage::GetBlockStorage().PutTxBlock(txBlock.GetHeader().GetBlockNum(), 
                                                   serializedTxBlock);
    }
#ifndef IS_LOOKUP_NODE // TODO : remove from here to top
    m_mediator.m_currentEpochNum = (uint64_t) m_mediator.m_txBlockChain.GetBlockCount();
    m_mediator.UpdateTxBlockRand();

    {
        unique_lock<mutex> lock(m_dsRandUpdationMutex);
        while(!m_isDSRandUpdated)            
        {
            m_dsRandUpdateCondition.wait(lock);
        }
        m_isDSRandUpdated = false;
    }

    auto dsBlockRand = m_mediator.m_dsBlockRand;
    array<unsigned char, 32> txBlockRand = {0};

    m_mediator.m_node->SetState(Node::POW1_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(m_mediator.m_currentEpochNum);
    // for(int i=0; i<5; i++)
    // {
    m_mediator.m_node->StartPoW2(m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(), 
                                 uint8_t(0x3), dsBlockRand, txBlockRand);
    //     this_thread::sleep_for(chrono::seconds(15));
    // }
    this_thread::sleep_for(chrono::seconds(NEW_NODE_POW2_TIMEOUT_IN_SECONDS));
    if(!m_mediator.m_isConnectedToNetwork)
    {
        Synchronizer synchronizer;
        synchronizer.FetchDSInfo(this);
        synchronizer.FetchLatestDSBlocks(this, m_mediator.m_dsBlockChain.GetBlockCount());
        synchronizer.FetchLatestTxBlocks(this, m_mediator.m_txBlockChain.GetBlockCount());        
    }
#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::ProcessSetTxBodyFromSeed(const vector<unsigned char> & message, unsigned int offset, 
                                      const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // Message = [TRAN_HASH_SIZE txHashStr][Transaction::GetSerializedSize() txbody]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, 
                                   TRAN_HASH_SIZE + Transaction::GetSerializedSize()))
    {
        return false;
    }

    TxnHash tranHash;
    copy(message.begin() + offset, message.begin() + offset + TRAN_HASH_SIZE, 
         tranHash.asArray().begin());
    offset += TRAN_HASH_SIZE;

    Transaction transaction(message, offset);

    vector<unsigned char> serializedTxBody;
    transaction.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tranHash, serializedTxBody);

#endif // IS_LOOKUP_NODE

    return true;
}

bool Lookup::Execute(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
    LOG_MARKER();

    bool result = true; 

    typedef bool(Lookup::*InstructionHandler)(const vector<unsigned char> &, unsigned int, 
                                              const Peer &);
    
    InstructionHandler ins_handlers[] =
    {
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
        &Lookup::ProcessGetNetworkId
    };

    const unsigned char ins_byte = message.at(offset);
    const unsigned int ins_handlers_count = sizeof(ins_handlers) / sizeof(InstructionHandler);

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
        LOG_MESSAGE("Unknown instruction byte " << hex << (unsigned int)ins_byte);
    }

    return result;
}