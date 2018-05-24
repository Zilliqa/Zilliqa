/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
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

#include "ConsensusUser.h"
#include "common/Messages.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;

bool ConsensusUser::ProcessSetLeader(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    // Message = 2-byte ID of leader (0 to num nodes - 1)

    LOG_MARKER();

    if ((m_consensus != nullptr)
        && (m_consensus->GetState() != ConsensusCommon::State::DONE)
        && (m_consensus->GetState() != ConsensusCommon::State::ERROR))
    {
        LOG_GENERAL(WARNING,
                    "You're trying to set me again but my consensus is "
                    "still not finished");
        return false;
    }

    uint16_t leader_id
        = Serializable::GetNumber<uint16_t>(message, offset, sizeof(uint16_t));

    uint32_t dummy_consensus_id = 0xFACEFACE;

    vector<unsigned char> dummy_block_hash(BLOCK_HASH_SIZE);
    fill(dummy_block_hash.begin(), dummy_block_hash.end(), 0x88);

    // For this test class, we assume the committee = everyone in the peer store

    // The peer store is sorted by PubKey
    // This means everyone can have a consistent view of a sorted list of public keys and IP addresses for this committee
    // We can then assign IDs to each one of us, starting from 0 to num nodes - 1
    // But first I need to add my own PubKey into the peer store so it can be sorted with the rest

    // In real usage, we don't expect to use the peerstore to assemble the list of pub keys
    // The DS block should have the info we need, and the peerstore only needs to be used to get the IP addresses

    PeerStore& peerstore = PeerStore::GetStore();
    peerstore.AddPeer(m_selfKey.second,
                      Peer()); // Add myself, but with dummy IP info

    vector<Peer> tmp1 = peerstore.GetAllPeers();
    deque<Peer> peer_info(tmp1.size());
    copy(tmp1.begin(), tmp1.end(),
         peer_info.begin()); // This will be sorted by PubKey

    vector<PubKey> tmp2 = peerstore.GetAllKeys();
    deque<PubKey> pubkeys(tmp2.size());
    copy(tmp2.begin(), tmp2.end(),
         pubkeys.begin()); // These are the sorted PubKeys

    peerstore.RemovePeer(m_selfKey.second); // Remove myself

    // Now I need to find my index in the sorted list (this will be my ID for the consensus)
    uint16_t my_id = 0;
    for (auto i = pubkeys.begin(); i != pubkeys.end(); i++)
    {
        if (*i == m_selfKey.second)
        {
            LOG_GENERAL(INFO, "My node ID for this consensus is " << my_id);
            break;
        }
        my_id++;
    }

    LOG_GENERAL(INFO, "The leader is using " << peer_info.at(leader_id));

    m_leaderOrBackup = (leader_id != my_id);

    if (m_leaderOrBackup == false) // Leader
    {
        m_consensus.reset(new ConsensusLeader(
            dummy_consensus_id, dummy_block_hash, my_id, m_selfKey.first,
            pubkeys, peer_info,
            static_cast<unsigned char>(MessageType::CONSENSUSUSER),
            static_cast<unsigned char>(InstructionType::CONSENSUS),
            std::function<bool(const vector<unsigned char>& errorMsg,
                               unsigned int, const Peer& from)>(),
            std::function<bool(map<unsigned int, vector<unsigned char>>)>()));
    }
    else // Backup
    {
        auto func = [this](const vector<unsigned char>& message,
                           vector<unsigned char>& errorMsg) mutable -> bool {
            return MyMsgValidatorFunc(message, errorMsg);
        };

        m_consensus.reset(new ConsensusBackup(
            dummy_consensus_id, dummy_block_hash, my_id, leader_id,
            m_selfKey.first, pubkeys, peer_info,
            static_cast<unsigned char>(MessageType::CONSENSUSUSER),
            static_cast<unsigned char>(InstructionType::CONSENSUS), func));
    }

    if (m_consensus == nullptr)
    {
        LOG_GENERAL(WARNING, "Consensus object creation failed");
        return false;
    }

    return true;
}

bool ConsensusUser::ProcessStartConsensus(const vector<unsigned char>& message,
                                          unsigned int offset, const Peer& from)
{
    // Message = [message for consensus]

    LOG_MARKER();

    if (m_consensus == nullptr)
    {
        LOG_GENERAL(WARNING, "You didn't set me yet");
        return false;
    }

    if (m_consensus->GetState() != ConsensusCommon::State::INITIAL)
    {
        LOG_GENERAL(WARNING,
                    "You already called me before. Set me again first.");
        return false;
    }

    ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensus.get());
    if (cl == NULL)
    {
        LOG_GENERAL(WARNING,
                    "I'm a backup, you can't start consensus "
                    "(announcement) thru me");
        return false;
    }

    vector<unsigned char> m(message.size() - offset);
    copy(message.begin() + offset, message.end(), m.begin());

    cl->StartConsensus(m, m.size());

    return true;
}

bool ConsensusUser::ProcessConsensusMessage(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    bool result = m_consensus->ProcessMessage(message, offset, from);

    if (m_consensus->GetState() == ConsensusCommon::State::DONE)
    {
        LOG_GENERAL(INFO, "Consensus is DONE!!!");

        vector<unsigned char> tmp;
        m_consensus->GetCS2().Serialize(tmp, 0);
        LOG_PAYLOAD(INFO, "Final collective signature", tmp, 100);

        tmp.clear();
        BitVector::SetBitVector(tmp, 0, m_consensus->GetB2());
        LOG_PAYLOAD(INFO, "Final collective signature bitmap", tmp, 100);
    }

    return result;
}

ConsensusUser::ConsensusUser(const pair<PrivKey, PubKey>& key, const Peer& peer)
    : m_selfKey(key)
    , m_selfPeer(peer)
    , m_consensus(nullptr)
{
    m_leaderOrBackup = false;
}

ConsensusUser::~ConsensusUser() {}

bool ConsensusUser::Execute(const vector<unsigned char>& message,
                            unsigned int offset, const Peer& from)
{
    //LOG_MARKER();

    bool result = false;

    typedef bool (ConsensusUser::*InstructionHandler)(
        const vector<unsigned char>&, unsigned int, const Peer&);

    InstructionHandler ins_handlers[] = {
        &ConsensusUser::ProcessSetLeader, &ConsensusUser::ProcessStartConsensus,
        &ConsensusUser::ProcessConsensusMessage};

    const unsigned char ins_byte = message.at(offset);

    const unsigned int ins_handlers_count
        = sizeof(ins_handlers) / sizeof(InstructionHandler);

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
        LOG_GENERAL(
            INFO, "Unknown instruction byte " << hex << (unsigned int)ins_byte);
    }

    return result;
}

bool ConsensusUser::MyMsgValidatorFunc(const vector<unsigned char>& message,
                                       vector<unsigned char>& errorMsg)
{
    LOG_MARKER();
    LOG_PAYLOAD(INFO, "Message", message, Logger::MAX_BYTES_TO_DISPLAY);
    LOG_GENERAL(INFO, "Message is valid. I don't really care...");

    return true;
}
