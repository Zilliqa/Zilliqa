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

#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/Block.h"
#include "libNetwork/Peer.h"

#ifndef __MESSENGER_H__
#define __MESSENGER_H__

class Messenger
{
public:
    // Directory service messages

    static bool
    SetDSPoWSubmission(std::vector<unsigned char>& dst,
                       const unsigned int offset, const uint64_t blockNumber,
                       const uint8_t difficultyLevel, const Peer& submitterPeer,
                       const std::pair<PrivKey, PubKey>& submitterKey,
                       const uint64_t nonce, const std::string& resultingHash,
                       const std::string& mixHash);

    static bool GetDSPoWSubmission(const std::vector<unsigned char>& src,
                                   const unsigned int offset,
                                   uint64_t& blockNumber,
                                   uint8_t& difficultyLevel,
                                   Peer& submitterPeer, PubKey& submitterPubKey,
                                   uint64_t& nonce, std::string& resultingHash,
                                   std::string& mixHash, Signature& signature);

    // Node messages

    static bool SetNodeDSBlock(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const uint32_t shardID, const DSBlock& dsBlock,
        const Peer& powWinnerPeer,
        const std::vector<std::vector<std::pair<PubKey, Peer>>>& shards,
        const std::vector<Peer>& dsReceivers,
        const std::vector<std::vector<Peer>>& shardReceivers,
        const std::vector<std::vector<Peer>>& shardSenders);

    static bool
    GetNodeDSBlock(const std::vector<unsigned char>& src,
                   const unsigned int offset, uint32_t& shardID,
                   DSBlock& dsBlock, Peer& powWinnerPeer,
                   std::vector<std::vector<std::pair<PubKey, Peer>>>& shards,
                   std::vector<Peer>& dsReceivers,
                   std::vector<std::vector<Peer>>& shardReceivers,
                   std::vector<std::vector<Peer>>& shardSenders);

    static bool SetNodeFinalBlock(std::vector<unsigned char>& dst,
                                  const unsigned int offset,
                                  const uint32_t shardID,
                                  const uint64_t dsBlockNumber,
                                  const uint32_t consensusID,
                                  const TxBlock& txBlock,
                                  const std::vector<unsigned char>& stateDelta);

    static bool GetNodeFinalBlock(const std::vector<unsigned char>& src,
                                  const unsigned int offset, uint32_t& shardID,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  std::vector<unsigned char>& stateDelta);

    // Lookup messages

    static bool SetLookupGetSeedPeers(std::vector<unsigned char>& dst,
                                      const unsigned int offset,
                                      const uint32_t listenPort);
    static bool GetLookupGetSeedPeers(const std::vector<unsigned char>& src,
                                      const unsigned int offset,
                                      uint32_t& listenPort);
    static bool SetLookupSetSeedPeers(std::vector<unsigned char>& dst,
                                      const unsigned int offset,
                                      const std::vector<Peer>& candidateSeeds);
    static bool GetLookupSetSeedPeers(const std::vector<unsigned char>& src,
                                      const unsigned int offset,
                                      std::vector<Peer>& candidateSeeds);
    static bool SetLookupGetDSInfoFromSeed(std::vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort);
    static bool
    GetLookupGetDSInfoFromSeed(const std::vector<unsigned char>& src,
                               const unsigned int offset, uint32_t& listenPort);
    static bool SetLookupSetDSInfoFromSeed(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const std::vector<std::pair<PubKey, Peer>>& dsNodes);
    static bool
    GetLookupSetDSInfoFromSeed(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               std::vector<std::pair<PubKey, Peer>>& dsNodes);
    static bool SetLookupGetDSBlockFromSeed(std::vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort);
    static bool GetLookupGetDSBlockFromSeed(
        const std::vector<unsigned char>& src, const unsigned int offset,
        uint64_t& lowBlockNum, uint64_t& highBlockNum, uint32_t& listenPort);
    static bool SetLookupSetDSBlockFromSeed(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const uint64_t lowBlockNum, const uint64_t highBlockNum,
        const std::vector<DSBlock>& dsBlocks);
    static bool
    GetLookupSetDSBlockFromSeed(const std::vector<unsigned char>& src,
                                const unsigned int offset,
                                uint64_t& lowBlockNum, uint64_t& highBlockNum,
                                std::vector<DSBlock>& dsBlocks);
    static bool SetLookupGetTxBlockFromSeed(std::vector<unsigned char>& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort);
    static bool GetLookupGetTxBlockFromSeed(
        const std::vector<unsigned char>& src, const unsigned int offset,
        uint64_t& lowBlockNum, uint64_t& highBlockNum, uint32_t& listenPort);
    static bool SetLookupSetTxBlockFromSeed(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const uint64_t lowBlockNum, const uint64_t highBlockNum,
        const std::vector<TxBlock>& txBlocks);
    static bool
    GetLookupSetTxBlockFromSeed(const std::vector<unsigned char>& src,
                                const unsigned int offset,
                                uint64_t& lowBlockNum, uint64_t& highBlockNum,
                                std::vector<TxBlock>& txBlocks);
    static bool SetLookupGetTxBodyFromSeed(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const std::vector<unsigned char>& txHash, const uint32_t listenPort);
    static bool GetLookupGetTxBodyFromSeed(
        const std::vector<unsigned char>& src, const unsigned int offset,
        std::vector<unsigned char>& txHash, uint32_t& listenPort);
    static bool SetLookupSetTxBodyFromSeed(
        std::vector<unsigned char>& dst, const unsigned int offset,
        const std::vector<unsigned char>& txHash, const Transaction& txBody);
    static bool GetLookupSetTxBodyFromSeed(
        const std::vector<unsigned char>& src, const unsigned int offset,
        std::vector<unsigned char>& txHash, Transaction& txBody);
    static bool SetLookupSetNetworkIDFromSeed(std::vector<unsigned char>& dst,
                                              const unsigned int offset,
                                              const std::string& networkID);
    static bool
    GetLookupSetNetworkIDFromSeed(const std::vector<unsigned char>& src,
                                  const unsigned int offset,
                                  std::string& networkID);
    static bool SetLookupGetStateFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint32_t listenPort);
    static bool GetLookupGetStateFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort);
    static bool SetLookupSetStateFromSeed(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const AccountStore& accountStore);
    static bool GetLookupSetStateFromSeed(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          AccountStore& accountStore);
    static bool SetLookupSetLookupOffline(std::vector<unsigned char>& dst,
                                          const unsigned int offset,
                                          const uint32_t listenPort);
    static bool GetLookupSetLookupOffline(const std::vector<unsigned char>& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort);
    static bool SetLookupSetLookupOnline(std::vector<unsigned char>& dst,
                                         const unsigned int offset,
                                         const uint32_t listenPort);
    static bool GetLookupSetLookupOnline(const std::vector<unsigned char>& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort);
    static bool SetLookupGetOfflineLookups(std::vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort);
    static bool
    GetLookupGetOfflineLookups(const std::vector<unsigned char>& src,
                               const unsigned int offset, uint32_t& listenPort);
    static bool SetLookupSetOfflineLookups(std::vector<unsigned char>& dst,
                                           const unsigned int offset,
                                           const std::vector<Peer>& nodes);
    static bool
    GetLookupSetOfflineLookups(const std::vector<unsigned char>& src,
                               const unsigned int offset,
                               std::vector<Peer>& nodes);
    static bool SetLookupGetStartPoWFromSeed(std::vector<unsigned char>& dst,
                                             const unsigned int offset,
                                             const uint32_t listenPort);
    static bool
    GetLookupGetStartPoWFromSeed(const std::vector<unsigned char>& src,
                                 const unsigned int offset,
                                 uint32_t& listenPort);
};

#endif // __MESSENGER_H__
