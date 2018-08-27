#include "Messenger.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace ZilliqaMessage;

namespace
{
    void SerializableToProtobufByteArray(const Serializable& serializable,
                                         ByteArray* byteArray)
    {
        vector<unsigned char> tmp;
        serializable.Serialize(tmp, 0);
        byteArray->set_data(tmp.data(), tmp.size());
    }

    void ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                         Serializable& serializable)
    {
        vector<unsigned char> tmp;
        copy(byteArray.data().begin(), byteArray.data().end(),
             back_inserter(tmp));
        serializable.Deserialize(tmp, 0);
    }
}

bool Messenger::SetDSPoWSubmission(
    vector<unsigned char>& dst, unsigned int offset, const uint64_t blockNumber,
    const Peer& submitterPeer, const pair<PrivKey, PubKey>& submitterKey,
    const uint64_t nonce, const string& resultingHash, const string& mixHash)
{
    LOG_MARKER();

    DSPoWSubmission result;

    result.mutable_data()->set_blocknumber(blockNumber);

    SerializableToProtobufByteArray(
        submitterPeer, result.mutable_data()->mutable_submitterpeer());
    SerializableToProtobufByteArray(
        submitterKey.second, result.mutable_data()->mutable_submitterpubkey());

    result.mutable_data()->set_nonce(nonce);
    result.mutable_data()->set_resultinghash(resultingHash);
    result.mutable_data()->set_mixhash(mixHash);

    if (result.data().IsInitialized())
    {
        vector<unsigned char> tmp(result.data().ByteSize());
        result.data().SerializeToArray(tmp.data(), tmp.size());

        Signature signature;
        if (Schnorr::GetInstance().Sign(tmp, submitterKey.first,
                                        submitterKey.second, signature)
            == false)
        {
            LOG_GENERAL(WARNING, "Failed to sign PoW");
            return false;
        }

        SerializableToProtobufByteArray(signature, result.mutable_signature());
    }
    else
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission.Data initialization failed");
        return false;
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed");
        return false;
    }

    const int length_available = dst.size() - offset;

    if (length_available < result.ByteSize())
    {
        dst.resize(dst.size() + result.ByteSize() - length_available);
    }

    return result.SerializeToArray(dst.data() + offset, result.ByteSize());
}

bool Messenger::GetDSPoWSubmission(const vector<unsigned char>& src,
                                   unsigned int offset, uint64_t& blockNumber,
                                   Peer& submitterPeer, PubKey& submitterPubKey,
                                   uint64_t& nonce, string& resultingHash,
                                   string& mixHash, Signature& signature)
{
    LOG_MARKER();

    DSPoWSubmission result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if ((result.IsInitialized() == false)
        || (result.data().IsInitialized() == false))
    {
        LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed");
        return false;
    }

    blockNumber = result.data().blocknumber();
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
        LOG_GENERAL(WARNING, "PoW submission signature wrong");
        return false;
    }

    return true;
}

bool Messenger::SetDSDSBlockAnnouncement(
    vector<unsigned char>& dst, unsigned int offset, const DSBlock& dsBlock,
    const Peer& powWinnerPeer, const vector<map<PubKey, Peer>>& shards,
    const vector<Peer>& dsReceivers, const vector<vector<Peer>>& shardReceivers,
    const vector<vector<Peer>>& shardSenders)
{
    LOG_MARKER();

    DSDSBlockAnnouncement result;

    SerializableToProtobufByteArray(dsBlock, result.mutable_dsblock());
    SerializableToProtobufByteArray(powWinnerPeer,
                                    result.mutable_powwinnerpeer());

    for (const auto& shard : shards)
    {
        ShardingStructure::Shard* proto_shard
            = result.mutable_sharding()->add_shards();

        for (const auto& node : shard)
        {
            ShardingStructure::Member* proto_member
                = proto_shard->add_members();

            SerializableToProtobufByteArray(node.first,
                                            proto_member->mutable_pubkey());
            SerializableToProtobufByteArray(node.second,
                                            proto_member->mutable_peerinfo());
        }
    }

    TxSharingAssignments* proto_assignments = result.mutable_assignments();

    for (const auto& dsnode : dsReceivers)
    {
        SerializableToProtobufByteArray(dsnode,
                                        proto_assignments->add_dsnodes());
    }

    for (unsigned int i = 0; i < shardReceivers.size(); i++)
    {
        TxSharingAssignments::AssignedNodes* proto_shard
            = proto_assignments->add_shardnodes();

        for (const auto& receiver : shardReceivers.at(i))
        {
            SerializableToProtobufByteArray(receiver,
                                            proto_shard->add_receivers());
        }
        for (const auto& sender : shardSenders.at(i))
        {
            SerializableToProtobufByteArray(sender, proto_shard->add_senders());
        }
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "DSDSBlockAnnouncement initialization failed");
        return false;
    }

    const int length_available = dst.size() - offset;

    if (length_available < result.ByteSize())
    {
        dst.resize(dst.size() + result.ByteSize() - length_available);
    }

    return result.SerializeToArray(dst.data() + offset, result.ByteSize());
}

bool Messenger::GetDSDSBlockAnnouncement(const vector<unsigned char>& src,
                                         unsigned int offset, DSBlock& dsBlock,
                                         Peer& powWinnerPeer,
                                         vector<map<PubKey, Peer>>& shards,
                                         vector<Peer>& dsReceivers,
                                         vector<vector<Peer>>& shardReceivers,
                                         vector<vector<Peer>>& shardSenders)
{
    LOG_MARKER();

    DSDSBlockAnnouncement result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "DSDSBlockAnnouncement initialization failed");
        return false;
    }

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

bool Messenger::SetNodeDSBlock(vector<unsigned char>& dst, unsigned int offset,
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
    SerializableToProtobufByteArray(dsBlock, result.mutable_dsblock());
    SerializableToProtobufByteArray(powWinnerPeer,
                                    result.mutable_powwinnerpeer());

    for (const auto& shard : shards)
    {
        ShardingStructure::Shard* proto_shard
            = result.mutable_sharding()->add_shards();

        for (const auto& node : shard)
        {
            ShardingStructure::Member* proto_member
                = proto_shard->add_members();

            SerializableToProtobufByteArray(node.first,
                                            proto_member->mutable_pubkey());
            SerializableToProtobufByteArray(node.second,
                                            proto_member->mutable_peerinfo());
        }
    }

    TxSharingAssignments* proto_assignments = result.mutable_assignments();

    for (const auto& dsnode : dsReceivers)
    {
        SerializableToProtobufByteArray(dsnode,
                                        proto_assignments->add_dsnodes());
    }

    for (unsigned int i = 0; i < shardReceivers.size(); i++)
    {
        TxSharingAssignments::AssignedNodes* proto_shard
            = proto_assignments->add_shardnodes();

        for (const auto& receiver : shardReceivers.at(i))
        {
            SerializableToProtobufByteArray(receiver,
                                            proto_shard->add_receivers());
        }
        for (const auto& sender : shardSenders.at(i))
        {
            SerializableToProtobufByteArray(sender, proto_shard->add_senders());
        }
    }

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "NodeDSBlock initialization failed");
        return false;
    }

    const int length_available = dst.size() - offset;

    if (length_available < result.ByteSize())
    {
        dst.resize(dst.size() + result.ByteSize() - length_available);
    }

    return result.SerializeToArray(dst.data() + offset, result.ByteSize());
}

bool Messenger::GetNodeDSBlock(const vector<unsigned char>& src,
                               unsigned int offset, uint32_t& shardID,
                               DSBlock& dsBlock, Peer& powWinnerPeer,
                               vector<map<PubKey, Peer>>& shards,
                               vector<Peer>& dsReceivers,
                               vector<vector<Peer>>& shardReceivers,
                               vector<vector<Peer>>& shardSenders)
{
    LOG_MARKER();

    NodeDSBlock result;

    result.ParseFromArray(src.data() + offset, src.size() - offset);

    if (result.IsInitialized() == false)
    {
        LOG_GENERAL(WARNING, "NodeDSBlock initialization failed");
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