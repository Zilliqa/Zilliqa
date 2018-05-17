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

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

/** TODO
void Node::StoreDSBlockToDisk(const DSBlock& dsblock)
{
    LOG_MARKER();

    m_mediator.m_dsBlockChain.AddBlock(dsblock);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing DS Block Number: "
                  << dsblock.GetHeader().GetBlockNum()
                  << " with Nonce: " << dsblock.GetHeader().GetNonce()
                  << ", Difficulty: " << dsblock.GetHeader().GetDifficulty()
                  << ", Timestamp: " << dsblock.GetHeader().GetTimestamp()
                  << ", view change count: "
                  << dsblock.GetHeader().GetViewChangeCount());

    // Update the rand1 value for next PoW
    m_mediator.UpdateDSBlockRand();

    // Store DS Block to disk
    vector<unsigned char> serializedDSBlock;
    dsblock.Serialize(serializedDSBlock, 0);

    LOG_GENERAL(
        INFO,
        "View change count:  " << dsblock.GetHeader().GetViewChangeCount());

    for (unsigned int i = 0; i < dsblock.GetHeader().GetViewChangeCount(); i++)
    {
        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();
        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }
    BlockStorage::GetBlockStorage().PutDSBlock(
        dsblock.GetHeader().GetBlockNum(), serializedDSBlock);
#ifndef IS_LOOKUP_NODE
    BlockStorage::GetBlockStorage().PushBackTxBodyDB(
        dsblock.GetHeader().GetBlockNum());
#endif
}
**/

void Node::UpdateDSCommiteeComposition()
{
    LOG_MARKER();

    {
        lock(m_mediator.m_mutexDSCommitteeNetworkInfo,
             m_mediator.m_mutexDSCommitteePubKeys);
        lock_guard<mutex> g2(m_mediator.m_mutexDSCommitteeNetworkInfo,
                             adopt_lock);
        lock_guard<mutex> g3(m_mediator.m_mutexDSCommitteePubKeys, adopt_lock);

        m_mediator.m_DSCommitteeNetworkInfo.push_back(
            m_mediator.m_DSCommitteeNetworkInfo.front());
        m_mediator.m_DSCommitteeNetworkInfo.pop_front();

        m_mediator.m_DSCommitteePubKeys.push_back(
            m_mediator.m_DSCommitteePubKeys.front());
        m_mediator.m_DSCommitteePubKeys.pop_front();
    }
}

bool Node::VerifyVCBlockCoSignature(const VCBlock& vcblock)
{
    LOG_MARKER();

    unsigned int index = 0;
    unsigned int count = 0;

    const vector<bool>& B2 = vcblock.GetB2();
    if (m_mediator.m_DSCommitteePubKeys.size() != B2.size())
    {
        LOG_GENERAL(WARNING,
                    "Mismatch: DS committee size = "
                        << m_mediator.m_DSCommitteePubKeys.size()
                        << ", co-sig bitmap size = " << B2.size());
        return false;
    }

    // Generate the aggregated key
    vector<PubKey> keys;
    for (auto& kv : m_mediator.m_DSCommitteePubKeys)
    {
        if (B2.at(index) == true)
        {
            keys.push_back(kv);
            count++;
        }
        index++;
    }

    if (count != ConsensusCommon::NumForConsensus(B2.size()))
    {
        LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
        return false;
    }

    shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
    if (aggregatedKey == nullptr)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
        return false;
    }

    // Verify the collective signature
    vector<unsigned char> message;
    vcblock.GetHeader().Serialize(message, 0);
    vcblock.GetCS1().Serialize(message, VCBlockHeader::SIZE);
    BitVector::SetBitVector(message, VCBlockHeader::SIZE + BLOCK_SIG_SIZE,
                            vcblock.GetB1());
    if (Schnorr::GetInstance().Verify(message, 0, message.size(),
                                      vcblock.GetCS2(), *aggregatedKey)
        == false)
    {
        LOG_GENERAL(WARNING, "Cosig verification failed");
        return false;
    }

    return true;
}

/**  TODO 
void Node::LogReceivedDSBlockDetails(const DSBlock& dsblock)
{
#ifdef IS_LOOKUP_NODE
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I the lookup node have deserialized the DS Block");
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetDifficulty(): "
                  << (int)dsblock.GetHeader().GetDifficulty());
    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "dsblock.GetHeader().GetNonce(): " << dsblock.GetHeader().GetNonce());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetBlockNum(): "
                  << dsblock.GetHeader().GetBlockNum());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetMinerPubKey(): "
                  << dsblock.GetHeader().GetMinerPubKey());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "dsblock.GetHeader().GetLeaderPubKey(): "
                  << dsblock.GetHeader().GetLeaderPubKey());
#endif // IS_LOOKUP_NODE
}
**/

bool Node::ProcessVCBlock(const vector<unsigned char>& message,
                          unsigned int cur_offset, const Peer& from)
{
    // Message = [VC block]
    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   VCBlock::GetMinSize()))
    {
        LOG_GENERAL(WARNING, "Incoming vc block size too small");
        return false;
    }

    VCBlock vcblock;
    if (vcblock.Deserialize(message, cur_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize vcblock.");
        return false;
    }

    cur_offset += vcblock.GetSerializedSize();

    if (vcblock.GetHeader().GetViewChangeEpochNo()
        != m_mediator.m_currentEpochNum)
    {
        LOG_GENERAL(WARNING,
                    "Received wrong vcblock. cur epoch: "
                        << m_mediator.m_currentEpochNum << "vc epoch: "
                        << vcblock.GetHeader().GetViewChangeEpochNo());
        return false;
    }

    // TODO State machine check

    unsigned int newCandidateLeader
        = 1; // TODO: To be change to a random node using VRF
    if (!(m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeader)
              == vcblock.GetHeader().GetCandidateLeaderNetworkInfo()
          && m_mediator.m_DSCommitteePubKeys.at(newCandidateLeader)
              == vcblock.GetHeader().GetCandidateLeaderPubKey()))
    {

        LOG_GENERAL(
            WARNING,
            "View change expectation mismatched "
            "expected new leader: "
                << m_mediator.m_DSCommitteeNetworkInfo.at(newCandidateLeader)
                << "actual vc new leader "
                << vcblock.GetHeader().GetCandidateLeaderNetworkInfo());
        return false;
    }

    // TODO
    // LogReceivedDSBlockDetails(vcblock);

    // Check the signature of this VC block
    if (!VerifyVCBlockCoSignature(vcblock))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "VCBlock co-sig verification failed");
        return false;
    }

    // TDOO
    // Add to block chain and Store the VC block to disk.
    // StoreVCBlockToDisk(dsblock);

    UpdateDSCommiteeComposition();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am a node and my view of leader is successfully changed.");
    return true;
}
