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

#include "Messenger.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"
#include <random>
#include <unordered_set>

using namespace std;
using namespace ZilliqaMessage;

namespace
{
    void SerializableToProtobufByteArray(const Serializable& serializable,
                                         ByteArray& byteArray)
    {
        vector<unsigned char> tmp;
        serializable.Serialize(tmp, 0);
        byteArray.set_data(tmp.data(), tmp.size());
    }

    void ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                         Serializable& serializable)
    {
        vector<unsigned char> tmp;
        copy(byteArray.data().begin(), byteArray.data().end(),
             back_inserter(tmp));
        serializable.Deserialize(tmp, 0);
    }

    template<class T>
    bool SerializeToArray(const T& protoMessage, vector<unsigned char>& dst,
                          const unsigned int offset)
    {
        const int length_available = dst.size() - offset;

        if (length_available < protoMessage.ByteSize())
        {
            dst.resize(dst.size() + protoMessage.ByteSize() - length_available);
        }

        return protoMessage.SerializeToArray(dst.data() + offset,
                                             protoMessage.ByteSize());
    }
}

bool Messenger::SetDSPoWSubmission(
    vector<unsigned char>& dst, const unsigned int offset,
    const uint64_t blockNumber, const uint8_t difficultyLevel,
    const Peer& submitterPeer, const pair<PrivKey, PubKey>& submitterKey,
    const uint64_t nonce, const string& resultingHash, const string& mixHash)
{
    LOG_MARKER();

    DSPoWSubmission result;

    result.mutable_data()->set_blocknumber(blockNumber);
    result.mutable_data()->set_difficultylevel(difficultyLevel);

    SerializableToProtobufByteArray(
        submitterPeer, *result.mutable_data()->mutable_submitterpeer());
    SerializableToProtobufByteArray(
        submitterKey.second, *result.mutable_data()->mutable_submitterpubkey());

    result.mutable_data()->set_nonce(nonce);
    result.mutable_data()->set_resultinghash(resultingHash);
    result.mutable_data()->set_mixhash(mixHash);

    if (result.data().IsInitialized())
    {
        vector<unsigned char> tmp(result.data().ByteSize());
        result.data().SerializeToArray(tmp.data(), tmp.size());

        Signature signature;
        if (!Schnorr::GetInstance().Sign(tmp, submitterKey.first,
                                         submitterKey.second, signature))
        {
            LOG_GENERAL(WARNING, "Failed to sign PoW.");
            return false;
        }

        SerializableToProtobufByteArray(signature, *result.mutable_signature());
    }
    else
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission.Data initialization failed.");
        return false;
    }

    if (!result.IsInitialized())
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSPoWSubmission(const vector<unsigned char>& src,
                                   const unsigned int offset,
                                   uint64_t& blockNumber,
                                   uint8_t& difficultyLevel,
                                   Peer& submitterPeer, PubKey& submitterPubKey,
                                   uint64_t& nonce, string& resultingHash,
                                   string& mixHash, Signature& signature)
{
    LOG_MARKER();

    DSPoWSubmission result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (!result.IsInitialized() || !result.data().IsInitialized())
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed.");
        return false;
    }

    blockNumber = result.data().blocknumber();
    difficultyLevel = result.data().difficultylevel();
    ProtobufByteArrayToSerializable(result.data().submitterpeer(),
                                    submitterPeer);
    ProtobufByteArrayToSerializable(result.data().submitterpubkey(),
                                    submitterPubKey);
    nonce = result.data().nonce();
    resultingHash = result.data().resultinghash();
    mixHash = result.data().mixhash();
    ProtobufByteArrayToSerializable(result.signature(), signature);

    vector<unsigned char> tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                       submitterPubKey))
    {
        LOG_GENERAL(WARNING, "PoW submission signature wrong.");
        return false;
    }

    return true;
}

bool Messenger::SetNodeDSBlock(vector<unsigned char>& dst,
                               const unsigned int offset,
                               const uint32_t shardID, const DSBlock& dsBlock,
                               const Peer& powWinnerPeer,
                               const vector<map<PubKey, Peer>>& shards,
                               const vector<Peer>& dsReceivers,
                               const vector<vector<Peer>>& shardReceivers,
                               const vector<vector<Peer>>& shardSenders)
{
    LOG_MARKER();

    NodeDSBlock result;

    result.set_shardid(shardID);
    SerializableToProtobufByteArray(dsBlock, *result.mutable_dsblock());
    SerializableToProtobufByteArray(powWinnerPeer,
                                    *result.mutable_powwinnerpeer());

    for (const auto& shard : shards)
    {
        ShardingStructure::Shard* proto_shard
            = result.mutable_sharding()->add_shards();

        for (const auto& node : shard)
        {
            ShardingStructure::Member* proto_member
                = proto_shard->add_members();

            SerializableToProtobufByteArray(node.first,
                                            *proto_member->mutable_pubkey());
            SerializableToProtobufByteArray(node.second,
                                            *proto_member->mutable_peerinfo());
        }
    }

    TxSharingAssignments* proto_assignments = result.mutable_assignments();

    for (const auto& dsnode : dsReceivers)
    {
        SerializableToProtobufByteArray(dsnode,
                                        *proto_assignments->add_dsnodes());
    }

    for (unsigned int i = 0; i < shardReceivers.size(); i++)
    {
        TxSharingAssignments::AssignedNodes* proto_shard
            = proto_assignments->add_shardnodes();

        for (const auto& receiver : shardReceivers.at(i))
        {
            SerializableToProtobufByteArray(receiver,
                                            *proto_shard->add_receivers());
        }
        for (const auto& sender : shardSenders.at(i))
        {
            SerializableToProtobufByteArray(sender,
                                            *proto_shard->add_senders());
        }
    }

    if (!result.IsInitialized())
    {
        LOG_GENERAL(WARNING, "NodeDSBlock initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeDSBlock(const vector<unsigned char>& src,
                               const unsigned int offset, uint32_t& shardID,
                               DSBlock& dsBlock, Peer& powWinnerPeer,
                               vector<map<PubKey, Peer>>& shards,
                               vector<Peer>& dsReceivers,
                               vector<vector<Peer>>& shardReceivers,
                               vector<vector<Peer>>& shardSenders)
{
    LOG_MARKER();

    NodeDSBlock result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (!result.IsInitialized())
    {
        LOG_GENERAL(WARNING, "NodeDSBlock initialization failed.");
        return false;
    }

    shardID = result.shardid();
    ProtobufByteArrayToSerializable(result.dsblock(), dsBlock);
    ProtobufByteArrayToSerializable(result.powwinnerpeer(), powWinnerPeer);

    for (int i = 0; i < result.sharding().shards_size(); i++)
    {
        shards.emplace_back();

        const ShardingStructure::Shard& proto_shard
            = result.sharding().shards(i);

        for (int j = 0; j < proto_shard.members_size(); j++)
        {
            const ShardingStructure::Member& proto_member
                = proto_shard.members(j);

            PubKey key;
            Peer peer;

            ProtobufByteArrayToSerializable(proto_member.pubkey(), key);
            ProtobufByteArrayToSerializable(proto_member.peerinfo(), peer);

            shards.back().emplace(key, peer);
        }
    }

    const TxSharingAssignments& proto_assignments = result.assignments();

    for (int i = 0; i < proto_assignments.dsnodes_size(); i++)
    {
        Peer peer;
        ProtobufByteArrayToSerializable(proto_assignments.dsnodes(i), peer);
        dsReceivers.emplace_back(peer);
    }

    for (int i = 0; i < proto_assignments.shardnodes_size(); i++)
    {
        const TxSharingAssignments::AssignedNodes& proto_shard
            = result.assignments().shardnodes(i);

        shardReceivers.emplace_back();

        for (int j = 0; j < proto_shard.receivers_size(); j++)
        {
            Peer peer;
            ProtobufByteArrayToSerializable(proto_shard.receivers(j), peer);
            shardReceivers.back().emplace_back(peer);
        }

        shardSenders.emplace_back();

        for (int j = 0; j < proto_shard.senders_size(); j++)
        {
            Peer peer;
            ProtobufByteArrayToSerializable(proto_shard.senders(j), peer);
            shardSenders.back().emplace_back(peer);
        }
    }

    return true;
}

bool Messenger::SetNodeFinalBlock(vector<unsigned char>& dst,
                                  const unsigned int offset,
                                  const uint32_t shardID,
                                  const uint64_t dsBlockNumber,
                                  const uint32_t consensusID,
                                  const TxBlock& txBlock,
                                  const vector<unsigned char>& stateDelta)
{
    LOG_MARKER();

    NodeFinalBlock result;

    result.set_shardid(shardID);
    result.set_dsblocknumber(dsBlockNumber);
    result.set_consensusid(consensusID);
    SerializableToProtobufByteArray(txBlock, *result.mutable_txblock());
    result.set_statedelta(stateDelta.data(), stateDelta.size());

    if (!result.IsInitialized())
    {
        LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFinalBlock(const vector<unsigned char>& src,
                                  const unsigned int offset, uint32_t& shardID,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  vector<unsigned char>& stateDelta)
{
    LOG_MARKER();

    NodeFinalBlock result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (!result.IsInitialized())
    {
        LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed.");
        return false;
    }

    shardID = result.shardid();
    dsBlockNumber = result.dsblocknumber();
    consensusID = result.consensusid();
    ProtobufByteArrayToSerializable(result.txblock(), txBlock);
    stateDelta.resize(result.statedelta().size());
    copy(result.statedelta().begin(), result.statedelta().end(),
         stateDelta.begin());

    return true;
}

bool Messenger::SetLookupGetSeedPeers(vector<unsigned char>& dst,
                                      const unsigned int offset,
                                      const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetSeedPeers result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetSeedPeers(const vector<unsigned char>& src,
                                      const unsigned int offset,
                                      uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetSeedPeers result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetSeedPeers(vector<unsigned char>& dst,
                                      const unsigned int offset,
                                      const vector<Peer>& candidateSeeds)
{
    LOG_MARKER();

    LookupSetSeedPeers result;

    unordered_set<uint32_t> indicesAlreadyAdded;

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, candidateSeeds.size() - 1);

    for (unsigned int i = 0; i < candidateSeeds.size(); i++)
    {
        uint32_t index = dis(gen);
        while (indicesAlreadyAdded.find(index) != indicesAlreadyAdded.end())
        {
            index = dis(gen);
        }
        indicesAlreadyAdded.insert(index);

        SerializableToProtobufByteArray(candidateSeeds.at(index),
                                        *result.add_candidateseeds());
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetSeedPeers(const vector<unsigned char>& src,
                                      const unsigned int offset,
                                      vector<Peer>& candidateSeeds)
{
    LOG_MARKER();

    LookupSetSeedPeers result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed.");
        return false;
    }

    for (int i = 0; i < result.candidateseeds_size(); i++)
    {
        Peer seedPeer;
        ProtobufByteArrayToSerializable(result.candidateseeds(i), seedPeer);
        candidateSeeds.emplace_back(seedPeer);
    }

    return true;
}

bool Messenger::SetLookupGetDSInfoFromSeed(vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetDSInfoFromSeed result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSInfoFromSeed(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetDSInfoFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetDSInfoFromSeed(
    vector<unsigned char>& dst, const unsigned int offset,
    const vector<pair<PubKey, Peer>>& dsNodes)
{
    LOG_MARKER();

    LookupSetDSInfoFromSeed result;

    for (const auto& node : dsNodes)
    {
        LookupSetDSInfoFromSeed::DSNode* protodsnode = result.add_dsnodes();
        SerializableToProtobufByteArray(node.first,
                                        *protodsnode->mutable_pubkey());
        SerializableToProtobufByteArray(node.second,
                                        *protodsnode->mutable_peer());
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSInfoFromSeed(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           vector<pair<PubKey, Peer>>& dsNodes)
{
    LOG_MARKER();

    LookupSetDSInfoFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed.");
        return false;
    }

    for (int i = 0; i < result.dsnodes_size(); i++)
    {
        PubKey pubkey;
        Peer peer;

        ProtobufByteArrayToSerializable(result.dsnodes(i).pubkey(), pubkey);
        ProtobufByteArrayToSerializable(result.dsnodes(i).peer(), peer);
        dsNodes.emplace_back(make_pair(pubkey, peer));
    }

    return true;
}

bool Messenger::SetLookupGetDSBlockFromSeed(vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetDSBlockFromSeed result;

    result.set_lowblocknum(lowBlockNum);
    result.set_highblocknum(highBlockNum);
    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSBlockFromSeed(const vector<unsigned char>& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetDSBlockFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed.");
        return false;
    }

    lowBlockNum = result.lowblocknum();
    highBlockNum = result.highblocknum();
    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetDSBlockFromSeed(vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const vector<DSBlock>& dsBlocks)
{
    LOG_MARKER();

    LookupSetDSBlockFromSeed result;

    result.set_lowblocknum(lowBlockNum);
    result.set_highblocknum(highBlockNum);

    for (const auto& dsblock : dsBlocks)
    {
        SerializableToProtobufByteArray(
            dsblock, *result.add_dsblocks()->mutable_dsblock());
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSBlockFromSeed(const vector<unsigned char>& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            vector<DSBlock>& dsBlocks)
{
    LOG_MARKER();

    LookupSetDSBlockFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed.");
        return false;
    }

    lowBlockNum = result.lowblocknum();
    highBlockNum = result.highblocknum();

    for (int i = 0; i < result.dsblocks_size(); i++)
    {
        DSBlock dsblock;
        ProtobufByteArrayToSerializable(result.dsblocks(i).dsblock(), dsblock);
        dsBlocks.emplace_back(dsblock);
    }

    return true;
}

bool Messenger::SetLookupGetTxBlockFromSeed(vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetTxBlockFromSeed result;

    result.set_lowblocknum(lowBlockNum);
    result.set_highblocknum(highBlockNum);
    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetTxBlockFromSeed(const vector<unsigned char>& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetTxBlockFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
        return false;
    }

    lowBlockNum = result.lowblocknum();
    highBlockNum = result.highblocknum();
    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetTxBlockFromSeed(vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const vector<TxBlock>& txBlocks)
{
    LOG_MARKER();

    LookupSetTxBlockFromSeed result;

    result.set_lowblocknum(lowBlockNum);
    result.set_highblocknum(highBlockNum);

    for (const auto& txblock : txBlocks)
    {
        SerializableToProtobufByteArray(
            txblock, *result.add_txblocks()->mutable_txblock());
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxBlockFromSeed(const vector<unsigned char>& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            vector<TxBlock>& txBlocks)
{
    LOG_MARKER();

    LookupSetTxBlockFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed.");
        return false;
    }

    lowBlockNum = result.lowblocknum();
    highBlockNum = result.highblocknum();

    for (int i = 0; i < result.txblocks_size(); i++)
    {
        TxBlock txblock;
        ProtobufByteArrayToSerializable(result.txblocks(i).txblock(), txblock);
        txBlocks.emplace_back(txblock);
    }

    return true;
}

bool Messenger::SetLookupGetTxBodyFromSeed(vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const vector<unsigned char>& txHash,
                                           const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetTxBodyFromSeed result;

    result.set_txhash(txHash.data(), txHash.size());
    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetTxBodyFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetTxBodyFromSeed(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           vector<unsigned char>& txHash,
                                           uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetTxBodyFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetTxBodyFromSeed initialization failed.");
        return false;
    }

    txHash.resize(result.txhash().size());
    copy(result.txhash().begin(), result.txhash().end(), txHash.begin());
    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetTxBodyFromSeed(vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const vector<unsigned char>& txHash,
                                           const Transaction& txBody)
{
    LOG_MARKER();

    LookupSetTxBodyFromSeed result;

    result.set_txhash(txHash.data(), txHash.size());
    SerializableToProtobufByteArray(txBody, *result.mutable_txbody());

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetTxBodyFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxBodyFromSeed(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           vector<unsigned char>& txHash,
                                           Transaction& txBody)
{
    LOG_MARKER();

    LookupSetTxBodyFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetTxBodyFromSeed initialization failed.");
        return false;
    }

    txHash.resize(result.txhash().size());
    copy(result.txhash().begin(), result.txhash().end(), txHash.begin());
    ProtobufByteArrayToSerializable(result.txbody(), txBody);

    return true;
}

bool Messenger::SetLookupSetNetworkIDFromSeed(vector<unsigned char>& dst,
                                              const unsigned int offset,
                                              const string& networkID)
{
    LOG_MARKER();

    LookupSetNetworkIDFromSeed result;

    result.set_networkid(networkID);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "LookupSetNetworkIDFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetNetworkIDFromSeed(const vector<unsigned char>& src,
                                              const unsigned int offset,
                                              string& networkID)
{
    LOG_MARKER();

    LookupSetNetworkIDFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "LookupSetNetworkIDFromSeed initialization failed.");
        return false;
    }

    networkID = result.networkid();

    return true;
}

bool Messenger::SetLookupGetStateFromSeed(vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetStateFromSeed result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStateFromSeed(const vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetStateFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetStateFromSeed(vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const AccountStore& accountStore)
{
    LOG_MARKER();

    LookupSetStateFromSeed result;

    SerializableToProtobufByteArray(accountStore, *result.mutable_accounts());

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetStateFromSeed(const vector<unsigned char>& src,
                                          const unsigned int offset,
                                          AccountStore& accountStore)
{
    LOG_MARKER();

    LookupSetStateFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed.");
        return false;
    }

    ProtobufByteArrayToSerializable(result.accounts(), accountStore);

    return true;
}

bool Messenger::SetLookupSetLookupOffline(vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint32_t listenPort)
{
    LOG_MARKER();

    LookupSetLookupOffline result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOffline(const vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort)
{
    LOG_MARKER();

    LookupSetLookupOffline result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetLookupOnline(vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort)
{
    LOG_MARKER();

    LookupSetLookupOnline result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOnline(const vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort)
{
    LOG_MARKER();

    LookupSetLookupOnline result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupGetOfflineLookups(vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetOfflineLookups result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetOfflineLookups(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetOfflineLookups result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}

bool Messenger::SetLookupSetOfflineLookups(vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const vector<Peer>& nodes)
{
    LOG_MARKER();

    LookupSetOfflineLookups result;

    for (const auto& node : nodes)
    {
        SerializableToProtobufByteArray(node, *result.add_nodes());
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetOfflineLookups(const vector<unsigned char>& src,
                                           const unsigned int offset,
                                           vector<Peer>& nodes)
{
    LOG_MARKER();

    LookupSetOfflineLookups result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed.");
        return false;
    }

    for (int i = 0; i < result.nodes_size(); i++)
    {
        Peer node;
        ProtobufByteArrayToSerializable(result.nodes(i), node);
        nodes.emplace_back(node);
    }

    return true;
}

bool Messenger::SetLookupGetStartPoWFromSeed(vector<unsigned char>& dst,
                                             const unsigned int offset,
                                             const uint32_t listenPort)
{
    LOG_MARKER();

    LookupGetStartPoWFromSeed result;

    result.set_listenport(listenPort);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "LookupGetStartPoWFromSeed initialization failed.");
        return false;
    }

    return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStartPoWFromSeed(const vector<unsigned char>& src,
                                             const unsigned int offset,
                                             uint32_t& listenPort)
{
    LOG_MARKER();

    LookupGetStartPoWFromSeed result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "LookupGetStartPoWFromSeed initialization failed.");
        return false;
    }

    listenPort = result.listenport();

    return true;
}