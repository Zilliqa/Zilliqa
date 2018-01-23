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

#include "ConsensusBackup.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libUtils/Logger.h"
#include "libUtils/DataConversion.h"
#include "libNetwork/P2PComm.h"

using namespace std;

bool ConsensusBackup::CheckState(Action action)
{
    bool result = true;

    switch(action)
    {
        case PROCESS_ANNOUNCE:
            switch(m_state)
            {
                case INITIAL:
                    break;
                case COMMIT_DONE:
                    LOG_MESSAGE("Error: Processing announce but commit already done");
                    result = false;
                    break;
                case RESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing announce but response already done");
                    result = false;
                    break;
                case FINALCOMMIT_DONE:
                    LOG_MESSAGE("Error: Processing announce but finalcommit already done");
                    result = false;
                    break;
                case FINALRESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing announce but finalresponse already done");
                    result = false;
                    break;
                case DONE:
                    LOG_MESSAGE("Error: Processing announce but consensus already done");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE("Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_CHALLENGE:
            switch(m_state)
            {
                case INITIAL:
                    LOG_MESSAGE("Error: Processing challenge but commit not yet done");
                    result = false;
                    break;
                case COMMIT_DONE:
                    break;
                case RESPONSE_DONE:
                    LOG_MESSAGE("Processing challenge but response already done");
                    // LOG_MESSAGE("Error: Processing challenge but response already done");
                    // result = false;
                    break;
                case FINALCOMMIT_DONE:
                    LOG_MESSAGE("Error: Processing challenge but finalcommit already done");
                    result = false;
                    break;
                case FINALRESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing challenge but finalresponse already done");
                    result = false;
                    break;
                case DONE:
                    LOG_MESSAGE("Error: Processing challenge but consensus already done");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE("Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_COLLECTIVESIG:
            switch(m_state)
            {
                case INITIAL:
                    LOG_MESSAGE("Error: Processing collectivesig but commit not yet done");
                    result = false;
                    break;
                case COMMIT_DONE:
                    break;
                case RESPONSE_DONE:
                    break;
                case FINALCOMMIT_DONE:
                    LOG_MESSAGE("Error: Processing collectivesig but finalcommit already done");
                    result = false;
                    break;
                case FINALRESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing collectivesig but finalresponse already done");
                    result = false;
                    break;
                case DONE:
                    LOG_MESSAGE("Error: Processing collectivesig but consensus already done");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE("Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_FINALCHALLENGE:
            switch(m_state)
            {
                case INITIAL:
                    LOG_MESSAGE("Error: Processing finalchallenge but commit not yet done");
                    result = false;
                    break;
                case COMMIT_DONE:
                    LOG_MESSAGE("Error: Processing finalchallenge but response not yet done");
                    result = false;
                    break;
                case RESPONSE_DONE:
                    LOG_MESSAGE("Processing finalchallenge but finalcommit not yet done");
                    // LOG_MESSAGE("Error: Processing finalchallenge but finalcommit not yet done");
                    // result = false;
                    break;
                case FINALCOMMIT_DONE:
                    break;
                case FINALRESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing finalchallenge but finalresponse already done");
                    result = false;
                    break;
                case DONE:
                    LOG_MESSAGE("Error: Processing finalchallenge but consensus already done");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE("Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_FINALCOLLECTIVESIG:
            switch(m_state)
            {
                case INITIAL:
                    LOG_MESSAGE("Error: Processing finalcollectivesig but commit not yet done");
                    result = false;
                    break;
                case COMMIT_DONE:
                    LOG_MESSAGE("Error: Processing finalcollectivesig but response not yet done");
                    // TODO: check this logic again. 
                    // Issue #43
                    // Node cannot proceed if finalcollectivesig arrive earler (and get ignore by the node)
                    //result = false; 
                    break;
                case RESPONSE_DONE:
                    LOG_MESSAGE("Error: Processing finalcollectivesig but finalcommit not yet done");
                    // TODO: check this logic again. 
                    // Issue #43
                    // Node cannot proceed if finalcollectivesig arrive earler (and get ignore by the node)
                    //result = false;
                    break;
                case FINALCOMMIT_DONE:
                    break;
                case FINALRESPONSE_DONE:
                    break;
                case DONE:
                    LOG_MESSAGE("Error: Processing finalcollectivesig but consensus already done");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE("Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        default:
            LOG_MESSAGE("Error: Unrecognized action");
            result = false;
            break;
    }

    return result;
}
bool ConsensusBackup::ProcessMessageAnnounce(const vector<unsigned char> & announcement, unsigned int offset)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (CheckState(PROCESS_ANNOUNCE) == false)
    {
        return false;
    }

    // Extract and check announce message body
    // =======================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [message] [64-byte signature]

    const unsigned int length_available = announcement.size() - offset;
    const unsigned int min_length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE + sizeof(uint16_t) + 1 + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (min_length_needed > length_available)
    {
        LOG_MESSAGE("Error: Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(announcement, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_MESSAGE("Error: Consensus ID in announcement (" << consensus_id << ") does not match instance consensus ID (" << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(), announcement.begin() + curr_offset) == false)
    {
        LOG_MESSAGE("Error: Block hash in announcement does not match instance block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leader_id = Serializable::GetNumber<uint16_t>(announcement, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leader_id != m_leaderID)
    {
        LOG_MESSAGE("Error: Leader ID mismatch. Expected: " << m_leaderID << ". But gotten: " << leader_id);
        return false;
    }

    // message
    const unsigned int message_size = announcement.size() - curr_offset - SIGNATURE_CHALLENGE_SIZE - SIGNATURE_RESPONSE_SIZE;
    m_message.resize(message_size);
    copy(announcement.begin() + curr_offset, announcement.begin() + curr_offset + message_size, m_message.begin());
    curr_offset += message_size;

    // Check the message
    bool msg_valid = m_msgContentValidator(m_message);
    if (msg_valid == false)
    {
        LOG_MESSAGE("Error: Message validation failed");
        m_state = ERROR;
        return false;
    }

    // 64-byte signature
    Signature signature(announcement, curr_offset);

    // Check the signature
    bool sig_valid = VerifyMessage(announcement, offset, curr_offset - offset, signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_MESSAGE("Error: Invalid signature in announce message");
        m_state = ERROR;
        return false;
    }

    // Generate commit
    // ===============

    vector<unsigned char> commit = { m_classByte, m_insByte, static_cast<unsigned char>(ConsensusMessageType::COMMIT) };

    bool result = GenerateCommitMessage(commit, MessageOffset::BODY + sizeof(unsigned char));

    if (result == true)
    {
        // Update internal state
        // =====================
        m_state = COMMIT_DONE;

        // Unicast to the leader
        // =====================

        P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID), commit);

    }

    return result;
}

bool ConsensusBackup::GenerateCommitMessage(vector<unsigned char> & commit, unsigned int offset)
{
    LOG_MARKER();

    // Generate new commit
    // ===================

    m_commitSecret.reset(new CommitSecret());
    m_commitPoint.reset(new CommitPoint(*m_commitSecret));

    // Assemble commit message body
    // ============================
    
    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit]

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(commit, curr_offset, m_consensusID, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    commit.insert(commit.begin() + curr_offset, m_blockHash.begin(), m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte backup id
    Serializable::SetNumber<uint16_t>(commit, curr_offset, m_myID, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 33-byte commit
    m_commitPoint->Serialize(commit, curr_offset);
    curr_offset += COMMIT_POINT_SIZE;

    // 64-byte signature
    Signature signature = SignMessage(commit, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_MESSAGE("Error: Message signing failed");
        m_state = ERROR;
        return false;
    }
    signature.Serialize(commit, curr_offset);

    return true;
}

bool ConsensusBackup::ProcessMessageChallengeCore(const vector<unsigned char> & challenge, unsigned int offset, Action action, ConsensusMessageType returnmsgtype, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (CheckState(action) == false)
    {
        return false;
    }

    // Extract and check challenge message body
    // ========================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge] [64-byte signature]

    const unsigned int length_available = challenge.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE + sizeof(uint16_t) + COMMIT_POINT_SIZE + PUB_KEY_SIZE + CHALLENGE_SIZE + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_MESSAGE("Error: Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(challenge, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_MESSAGE("Error: Consensus ID in challenge (" << consensus_id << ") does not match instance consensus ID (" << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(), challenge.begin() + curr_offset) == false)
    {
        LOG_MESSAGE("Error: Block hash in challenge does not match instance block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leader_id = Serializable::GetNumber<uint16_t>(challenge, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leader_id != m_leaderID)
    {
        LOG_MESSAGE("Error: Leader ID mismatch");
        return false;
    }

    // 33-byte aggregated commit
    CommitPoint aggregated_commit(challenge, curr_offset);
    curr_offset += COMMIT_POINT_SIZE;

    // Check the aggregated commit
    if (aggregated_commit.Initialized() == false)
    {
        LOG_MESSAGE("Error: Invalid aggregated commit received");
        m_state = ERROR;
        return false;
    }

    // 33-byte aggregated key
    PubKey aggregated_key(challenge, curr_offset);
    curr_offset += PUB_KEY_SIZE;

    // Check the aggregated key
    if (aggregated_key.Initialized() == false)
    {
        LOG_MESSAGE("Error: Invalid aggregated key received");
        m_state = ERROR;
        return false;
    }

    // 32-byte challenge
    m_challenge.Deserialize(challenge, curr_offset);
    curr_offset += CHALLENGE_SIZE;

    // Check the challenge
    if (m_challenge.Initialized() == false)
    {
        LOG_MESSAGE("Error: Invalid challenge received");
        m_state = ERROR;
        return false;
    }
    Challenge challenge_verif = GetChallenge(m_message, 0, m_message.size(), aggregated_commit, aggregated_key);

    if (!(challenge_verif == m_challenge))
    {
        LOG_MESSAGE("Error: Generated challenge mismatch");
        m_state = ERROR;
        return false;
    }

    // 64-byte signature
    Signature signature(challenge, curr_offset);

    // Check the signature
    bool sig_valid = VerifyMessage(challenge, offset, curr_offset - offset, signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_MESSAGE("Error: Invalid signature in challenge message");
        m_state = ERROR;
        return false;
    }

    // Generate response
    // =================

    vector<unsigned char> response = { m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype) };
    bool result = GenerateResponseMessage(response, MessageOffset::BODY + sizeof(unsigned char));
    if (result == true)
    {

        // Update internal state
        // =====================

        m_state = nextstate;
        
        // Unicast to the leader
        // =====================

        P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID), response);

    }

    return result;
}

bool ConsensusBackup::ProcessMessageChallenge(const vector<unsigned char> & challenge, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageChallengeCore(challenge, offset, PROCESS_CHALLENGE, RESPONSE, RESPONSE_DONE);
}

bool ConsensusBackup::GenerateResponseMessage(vector<unsigned char> & response, unsigned int offset)
{
    LOG_MARKER();

    // Assemble response message body
    // ==============================
    
    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [32-byte response] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [32-byte response]

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(response, curr_offset, m_consensusID, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    response.insert(response.begin() + curr_offset, m_blockHash.begin(), m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte backup id
    Serializable::SetNumber<uint16_t>(response, curr_offset, m_myID, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 32-byte response
    Response r(*m_commitSecret, m_challenge, m_myPrivKey);
    r.Serialize(response, curr_offset);
    curr_offset += RESPONSE_SIZE;

    // 64-byte signature
    Signature signature = SignMessage(response, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_MESSAGE("Error: Message signing failed");
        m_state = ERROR;
        return false;
    }
    signature.Serialize(response, curr_offset);

    return true;
}

bool ConsensusBackup::ProcessMessageCollectiveSigCore(const vector<unsigned char> & collectivesig, unsigned int offset, Action action, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (CheckState(action) == false)
    {
        return false;
    }

    // Extract and check collective signature message body
    // ===================================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [N-byte bitmap] [64-byte collective signature] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [N-byte bitmap] [64-byte collective signature]
    // Note on N-byte bitmap: N = number of bytes needed to represent all nodes (1 bit = 1 node) + 2 (length indicator)

    const unsigned int length_available = collectivesig.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE + sizeof(uint16_t) + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE + GetBitVectorLengthInBytes(m_pubKeys.size()) + 2 + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_MESSAGE("Error: Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(collectivesig, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_MESSAGE("Error: Consensus ID in challenge (" << consensus_id << ") does not match instance consensus ID (" << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(), collectivesig.begin() + curr_offset) == false)
    {
        LOG_MESSAGE("Error: Block hash in challenge does not match instance block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leader_id = Serializable::GetNumber<uint16_t>(collectivesig, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leader_id != m_leaderID)
    {
        LOG_MESSAGE("Error: Leader ID mismatch");
        return false;
    }

    // N-byte bitmap
    m_responseMap = GetBitVector(collectivesig, curr_offset, GetBitVectorLengthInBytes(m_pubKeys.size()));
    curr_offset += GetBitVectorLengthInBytes(m_pubKeys.size()) + 2;

    // Check the bitmap
    if (m_responseMap.empty())
    {
        LOG_MESSAGE("Error: Response map deserialization failed");
        return false;
    }

    // 64-byte collective signature
    m_collectiveSig.Deserialize(collectivesig, curr_offset);
    curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    // Aggregate keys
    PubKey aggregated_key = AggregateKeys(m_responseMap);
    if (aggregated_key.Initialized() == false)
    {
        LOG_MESSAGE("Error: Aggregated key generation failed");
        m_state = ERROR;
        return false;
    }

    if (Schnorr::GetInstance().Verify(m_message, m_collectiveSig, aggregated_key) == false)
    {
        LOG_MESSAGE("Error: Collective signature verification failed");
        m_state = ERROR;
        return false;
    }

    // 64-byte signature
    Signature signature(collectivesig, curr_offset);

    // Check the signature
    bool sig_valid = VerifyMessage(collectivesig, offset, curr_offset - offset, signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_MESSAGE("Error: Invalid signature in challenge message");
        m_state = ERROR;
        return false;
    }

    // Generate final commit
    // =====================

    bool result = true;

    if (action == PROCESS_COLLECTIVESIG)
    {
        vector<unsigned char> finalcommit = { m_classByte, m_insByte, static_cast<unsigned char>(ConsensusMessageType::FINALCOMMIT) };
        result = GenerateCommitMessage(finalcommit, MessageOffset::BODY + sizeof(unsigned char));
        if (result == true)
        {
            // Update internal state
            // =====================

            m_state = nextstate;

            // First round: consensus over message (e.g., DS block)
            // Second round: consensus over collective sig
            m_message.clear();
            m_collectiveSig.Serialize(m_message, 0);

            // Unicast to the leader
            // =====================

            P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID), finalcommit);

        }
    }
    else
    {
        // Update internal state
        // =====================

        m_state = nextstate;
    }

    return result;
}

bool ConsensusBackup::ProcessMessageCollectiveSig(const vector<unsigned char> & collectivesig, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCollectiveSigCore(collectivesig, offset, PROCESS_COLLECTIVESIG, FINALCOMMIT_DONE);
}

bool ConsensusBackup::ProcessMessageFinalChallenge(const vector<unsigned char> & challenge, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageChallengeCore(challenge, offset, PROCESS_FINALCHALLENGE, FINALRESPONSE, FINALRESPONSE_DONE);
}

bool ConsensusBackup::ProcessMessageFinalCollectiveSig(const vector<unsigned char> & finalcollectivesig, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCollectiveSigCore(finalcollectivesig, offset, PROCESS_FINALCOLLECTIVESIG, DONE);
}

ConsensusBackup::ConsensusBackup
(
    uint32_t consensus_id,
    const vector<unsigned char> & block_hash,
    uint16_t node_id,
    uint16_t leader_id,
    const PrivKey & privkey,
    const deque<PubKey> & pubkeys,
    const deque<Peer> & peer_info,
    unsigned char class_byte,
    unsigned char ins_byte,
    MsgContentValidatorFunc msg_validator
) : ConsensusCommon(consensus_id, block_hash, node_id, privkey, pubkeys, peer_info, class_byte, ins_byte), m_commitSecret(nullptr), m_commitPoint(nullptr)
{
    LOG_MARKER();

    m_state = INITIAL;
    m_leaderID = leader_id;
    m_msgContentValidator = msg_validator;
}

ConsensusBackup::~ConsensusBackup()
{

}

bool ConsensusBackup::ProcessMessage(const vector<unsigned char> & message, unsigned int offset)
{
    LOG_MARKER();

    // Incoming message format (from offset): [1-byte consensus message type] [consensus message]

    bool result = false;

    switch(message.at(offset))
    {
        case ConsensusMessageType::ANNOUNCE:
            result = ProcessMessageAnnounce(message, offset + 1);
            break;
        case ConsensusMessageType::CHALLENGE:
            result = ProcessMessageChallenge(message, offset + 1);
            break;
        case ConsensusMessageType::COLLECTIVESIG:
            result = ProcessMessageCollectiveSig(message, offset + 1);
            break;
        case ConsensusMessageType::FINALCHALLENGE:
            result = ProcessMessageFinalChallenge(message, offset + 1);
            break;
        case ConsensusMessageType::FINALCOLLECTIVESIG:
            result = ProcessMessageFinalCollectiveSig(message, offset + 1);
            break;
        default:
        LOG_MESSAGE("Error: Unknown consensus message received");
            break;
    }

    return result;
}
