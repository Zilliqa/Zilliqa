#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/Block/DSBlock.h"
#include "libNetwork/Peer.h"

class Messenger
{
public:
    // Directory service messages
    static bool
    SetDSPoWSubmission(std::vector<unsigned char>& dst, unsigned int offset,
                       const uint64_t blockNumber, const Peer& submitterPeer,
                       const std::pair<PrivKey, PubKey>& submitterKey,
                       const uint64_t nonce, const std::string& resultingHash,
                       const std::string& mixHash);
    static bool GetDSPoWSubmission(const std::vector<unsigned char>& src,
                                   unsigned int offset, uint64_t& blockNumber,
                                   Peer& submitterPeer, PubKey& submitterPubKey,
                                   uint64_t& nonce, std::string& resultingHash,
                                   std::string& mixHash, Signature& signature);

    static bool SetDSDSBlockAnnouncement(
        std::vector<unsigned char>& dst, unsigned int offset,
        const DSBlock& dsBlock, const Peer& powWinnerPeer,
        const std::vector<map<PubKey, Peer>>& shards,
        const std::vector<Peer>& dsReceivers,
        const std::vector<std::vector<Peer>>& shardReceivers,
        const std::vector<std::vector<Peer>>& shardSenders);
    static bool GetDSDSBlockAnnouncement(
        const std::vector<unsigned char>& src, unsigned int offset,
        DSBlock& dsBlock, Peer& powWinnerPeer,
        std::vector<map<PubKey, Peer>>& shards, std::vector<Peer>& dsReceivers,
        std::vector<std::vector<Peer>>& shardReceivers,
        std::vector<std::vector<Peer>>& shardSenders);

    // Node messages
    static bool
    SetNodeDSBlock(std::vector<unsigned char>& dst, unsigned int offset,
                   const uint32_t shardID, const DSBlock& dsBlock,
                   const Peer& powWinnerPeer,
                   const std::vector<map<PubKey, Peer>>& shards,
                   const std::vector<Peer>& dsReceivers,
                   const std::vector<std::vector<Peer>>& shardReceivers,
                   const std::vector<std::vector<Peer>>& shardSenders);
    static bool GetNodeDSBlock(const std::vector<unsigned char>& src,
                               unsigned int offset, uint32_t& shardID,
                               DSBlock& dsBlock, Peer& powWinnerPeer,
                               std::vector<map<PubKey, Peer>>& shards,
                               std::vector<Peer>& dsReceivers,
                               std::vector<std::vector<Peer>>& shardReceivers,
                               std::vector<std::vector<Peer>>& shardSenders);

    // Lookup messages
};
