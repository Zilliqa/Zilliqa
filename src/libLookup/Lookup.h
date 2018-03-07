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


#ifndef __LOOKUP_H__
#define __LOOKUP_H__

#include <map>
#include <vector>

#include "common/Broadcastable.h"
#include "common/Executable.h"
#include "libCrypto/Schnorr.h"
#include "libNetwork/Peer.h"
#include "libUtils/Logger.h"

class Mediator;
class Synchronizer;

/// Processes requests pertaining to network, transaction, or block information
class Lookup : public Executable, public Broadcastable
{
    Mediator & m_mediator;

#ifndef IS_LOOKUP_NODE    
    // Info about lookup node
    std::vector<Peer> m_lookupNodes;
    std::vector<Peer> m_seedNodes;
#endif // IS_LOOKUP_NODE

#ifdef IS_LOOKUP_NODE
    // Sharding committee members
    std::vector<std::map<PubKey, Peer>> m_shards;
    std::vector<Peer> m_nodesInNetwork;
#endif // IS_LOOKUP_NODE

    bool m_isDSRandUpdated = false;
    std::mutex m_dsRandUpdationMutex;
    std::condition_variable m_dsRandUpdateCondition;

    std::mutex m_mutexSetDSBlockFromSeed;
    std::mutex m_mutexSetTxBlockFromSeed;
    std::mutex m_mutexSetTxBodyFromSeed;
    std::mutex m_mutexSetState;
    
    std::vector<unsigned char> ComposeGetDSInfoMessage();
    std::vector<unsigned char> ComposeGetStateMessage();
    std::vector<unsigned char> ComposeGetDSBlockMessage(
        boost::multiprecision::uint256_t lowBlockNum, boost::multiprecision::uint256_t highBlockNum);    
    std::vector<unsigned char> ComposeGetTxBlockMessage(
        boost::multiprecision::uint256_t lowBlockNum, boost::multiprecision::uint256_t highBlockNum);        

public:

    /// Constructor.  
    Lookup(Mediator & mediator);

    /// Destructor.
    ~Lookup();

#ifndef IS_LOOKUP_NODE
    // Setting the lookup nodes
    // Hardcoded for now -- to be called by constructor
    void SetLookupNodes();

    // Getter for m_lookupNodes
    std::vector<Peer> GetLookupNodes();
    
    // Calls P2PComm::SendMessage serially for every Lookup Node
    void SendMessageToLookupNodes(const std::vector<unsigned char> & message) const;

    // Calls P2PComm::SendMessage serially for every Seed peer
    void SendMessageToSeedNodes(const std::vector<unsigned char> & message) const;

    // TODO: move the Get and ProcessSet functions to Synchronizer
    bool GetSeedPeersFromLookup();
    bool GetDSInfoFromSeedNodes();
    bool GetDSBlockFromSeedNodes(boost::multiprecision::uint256_t lowBlockNum, 
                                 boost::multiprecision::uint256_t highBlockNum);
    bool GetTxBlockFromSeedNodes(boost::multiprecision::uint256_t lowBlockNum, 
                                 boost::multiprecision::uint256_t highBlockNum);
    bool GetDSInfoFromLookupNodes();
    bool GetDSBlockFromLookupNodes(boost::multiprecision::uint256_t lowBlockNum, 
                                   boost::multiprecision::uint256_t highBlockNum);
    bool GetTxBlockFromLookupNodes(boost::multiprecision::uint256_t lowBlockNum, 
                                   boost::multiprecision::uint256_t highBlockNum);
    bool GetTxBodyFromSeedNodes(std::string txHashStr);
    bool GetStateFromLookupNodes();
#else // IS_LOOKUP_NODE 
    bool SetDSCommitteInfo();
#endif // IS_LOOKUP_NODE

    bool ProcessEntireShardingStructure(const std::vector<unsigned char> & message, 
                                        unsigned int offset, const Peer & from);
    bool ProcessGetSeedPeersFromLookup(const std::vector<unsigned char> & message,
                                       unsigned int offset, const Peer & from);
    bool ProcessGetDSInfoFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                  const Peer & from);
    bool ProcessGetDSBlockFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from);
    bool ProcessGetTxBlockFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from);
    bool ProcessGetTxBodyFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                  const Peer & from);
    bool ProcessGetStateFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                 const Peer & from);

    bool ProcessGetNetworkId(const std::vector<unsigned char> & message, unsigned int offset, 
                             const Peer &from);

    bool ProcessSetSeedPeersFromLookup(const std::vector<unsigned char> & message, 
                                       unsigned int offset, const Peer & from);
    bool ProcessSetDSInfoFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                  const Peer & from);
    bool ProcessSetDSBlockFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from);
    bool ProcessSetTxBlockFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from); 
    bool ProcessSetTxBodyFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                  const Peer & from);
    bool ProcessSetStateFromSeed(const std::vector<unsigned char> & message, unsigned int offset, 
                                 const Peer & from);

    bool Execute(const std::vector<unsigned char> & message, unsigned int offset, 
                 const Peer & from);

    bool InitMining();              
    bool AlreadyJoinedNetwork();
};

#endif // __LOOKUP_H__