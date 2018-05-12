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
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;

bool ConsensusBackup::CheckState(Action action)
{
    bool result = true;

    switch (action)
    {
    case PROCESS_ANNOUNCE:
        switch (m_state)
        {
        case INITIAL:
            break;
        case COMMIT_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_CHALLENGE:
    case PROCESS_COLLECTIVESIG:
    case PROCESS_FINALCHALLENGE:
    case PROCESS_FINALCOLLECTIVESIG:
        switch (m_state)
        {
        case INITIAL:
            result = false;
            break;
        case COMMIT_DONE:
            break;
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    default:
        result = false;
        break;
    }

    if (result == false)
    {
        LOG_GENERAL(WARNING,
                    "Action " << action << " not allowed in state " << m_state);
    }

    return result;
}

bool ConsensusBackup::CheckStateSubset(unsigned int subsetID, Action action)
{
    ConsensusSubset& subset = m_consensusSubsets[subsetID];

    bool result = true;

    switch (action)
    {
    case PROCESS_CHALLENGE:
        switch (subset.m_state)
        {
        case COMMIT_DONE:
            break;
        case RESPONSE_DONE:
        case FINALCOMMIT_DONE:
        case FINALRESPONSE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_COLLECTIVESIG:
        switch (subset.m_state)
        {
        case COMMIT_DONE:
            result = false;
            break;
        case RESPONSE_DONE:
            break;
        case FINALCOMMIT_DONE:
        case FINALRESPONSE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_FINALCHALLENGE:
        switch (subset.m_state)
        {
        case COMMIT_DONE:
        case RESPONSE_DONE:
            result = false;
            break;
        case FINALCOMMIT_DONE:
            break;
        case FINALRESPONSE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_FINALCOLLECTIVESIG:
        switch (subset.m_state)
        {
        case COMMIT_DONE:
        case RESPONSE_DONE:
        case FINALCOMMIT_DONE:
            result = false;
            break;
        case FINALRESPONSE_DONE:
            break;
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    default:
        result = false;
        break;
    }

    if (result == false)
    {
        LOG_GENERAL(WARNING,
                    "Action " << action << " not allowed in state "
                              << subset.m_state);
    }

    return result;
}

void ConsensusBackup::SetStateSubset(unsigned int subsetID, State newState)
{
    ConsensusSubset& subset = m_consensusSubsets[subsetID];

    if ((newState == INITIAL) || (newState > subset.m_state))
    {
        subset.m_state = newState;

        if (newState == DONE)
        {
            LOG_GENERAL(INFO,
                        "[Subset " << subsetID
                                   << "] Subset has finished consensus!");
            SetState(DONE);

            // Set the final consensus data
            // B1 and B2 will be equal
            m_CS1 = subset.m_CS1;
            m_CS2 = subset.m_CS2;
            m_B1 = subset.m_B1;
            m_B2 = subset.m_B2;
        }
    }
}

bool ConsensusBackup::ProcessMessageAnnounce(
    const vector<unsigned char>& announcement, unsigned int offset)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (!CheckState(PROCESS_ANNOUNCE))
    {
        return false;
    }

    // Extract and check announce message body
    // =======================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [message] [4-byte length to co-sign] [64-byte signature]

    const unsigned int length_available = announcement.size() - offset;
    const unsigned int min_length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + 1 + sizeof(uint32_t) + SIGNATURE_CHALLENGE_SIZE
        + SIGNATURE_RESPONSE_SIZE;

    if (min_length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        announcement, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in announcement ("
                        << consensus_id
                        << ") does not match instance consensus ID ("
                        << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              announcement.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "Block hash in announcement does not match instance "
                    "block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leaderID = Serializable::GetNumber<uint16_t>(
        announcement, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leaderID != m_leaderID)
    {
        LOG_GENERAL(WARNING,
                    "Leader ID mismatch. Expected: "
                        << m_leaderID << ". But gotten: " << leaderID);
        return false;
    }

    // message
    const unsigned int message_size = announcement.size() - curr_offset
        - sizeof(uint32_t) - SIGNATURE_CHALLENGE_SIZE - SIGNATURE_RESPONSE_SIZE;
    m_message.resize(message_size);
    copy(announcement.begin() + curr_offset,
         announcement.begin() + curr_offset + message_size, m_message.begin());
    curr_offset += message_size;

    // 4-byte length to co-sign
    m_lengthToCosign = Serializable::GetNumber<uint32_t>(
        announcement, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the length to co-sign
    if (m_lengthToCosign > m_message.size())
    {
        LOG_GENERAL(WARNING, "m_lengthToCosign > message size");
    }

    // Check the message
    std::vector<unsigned char> errorMsg;
    bool msg_valid = m_msgContentValidator(m_message, errorMsg);
    if (msg_valid == false)
    {
        LOG_GENERAL(WARNING, "Message validation failed");

        if (!errorMsg.empty())
        {
            vector<unsigned char> commitFailureMsg
                = {m_classByte, m_insByte,
                   static_cast<unsigned char>(
                       ConsensusMessageType::COMMITFAILURE)};

            bool result = GenerateCommitFailureMessage(
                commitFailureMsg, MessageOffset::BODY + sizeof(unsigned char),
                errorMsg);

            if (result == true)
            {
                // Update internal state
                // =====================
                SetState(INITIAL); // TODO: replace it by a more specific state

                // Unicast to the leader
                // =====================
                P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID),
                                                   commitFailureMsg);

                return true;
            }
        }

        SetState(ERROR);
        return false;
    }

    // 64-byte signature
    // Signature signature(announcement, curr_offset);
    Signature signature;
    if (signature.Deserialize(announcement, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize signature.");
        return false;
    }

    // Check the signature
    bool sig_valid = VerifyMessage(announcement, offset, curr_offset - offset,
                                   signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_GENERAL(WARNING, "Invalid signature in announce message");
        SetState(ERROR);
        return false;
    }

    // Generate commit
    // ===============

    vector<unsigned char> commit
        = {m_classByte, m_insByte,
           static_cast<unsigned char>(ConsensusMessageType::COMMIT)};

    bool result = GenerateCommitMessage(
        commit, MessageOffset::BODY + sizeof(unsigned char), 0,
        PROCESS_ANNOUNCE);
    if (result == true)
    {
        // Update internal state
        // =====================
        SetState(COMMIT_DONE);

        // Unicast to the leader
        // =====================
        P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID), commit);
    }

    return result;
}

bool ConsensusBackup::ProcessMessageConsensusFailure(
    const vector<unsigned char>& announcement, unsigned int offset)
{
    LOG_MARKER();

    SetState(INITIAL);

    m_commitSecret.reset();
    m_commitPoint.reset();

    m_consensusSubsets.clear();

    return true;
}

bool ConsensusBackup::GenerateCommitFailureMessage(
    vector<unsigned char>& commitFailure, unsigned int offset,
    const vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(commitFailure, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    commitFailure.insert(commitFailure.begin() + curr_offset,
                         m_blockHash.begin(), m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte backup id
    Serializable::SetNumber<uint16_t>(commitFailure, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    commitFailure.resize(curr_offset + errorMsg.size());
    copy(errorMsg.begin(), errorMsg.end(), commitFailure.begin() + curr_offset);

    return true;
}

bool ConsensusBackup::GenerateCommitMessage(vector<unsigned char>& commit,
                                            unsigned int offset,
                                            unsigned int subsetID,
                                            Action action)
{
    LOG_MARKER();

    // Generate new commit
    // ===================
    if (action == PROCESS_ANNOUNCE)
    {
        m_commitSecret.reset(new CommitSecret());
        m_commitPoint.reset(new CommitPoint(*m_commitSecret));
    }
    else
    {
        m_consensusSubsets[subsetID].m_commitSecret.reset(new CommitSecret());
        m_consensusSubsets[subsetID].m_commitPoint.reset(
            new CommitPoint(*m_consensusSubsets[subsetID].m_commitSecret));
    }

    // Assemble commit message body
    // ============================

    // Format Commit: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit]

    // Format FinalCommit: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [33-byte commit] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [33-byte commit]

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(commit, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    // 32-byte blockhash
    commit.insert(commit.begin() + curr_offset, m_blockHash.begin(),
                  m_blockHash.end());
    curr_offset += m_blockHash.size();
    // 2-byte backup id
    Serializable::SetNumber<uint16_t>(commit, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);
    if (action == PROCESS_COLLECTIVESIG)
    {
        // 1-byte subset id
        Serializable::SetNumber<uint8_t>(commit, curr_offset, (uint8_t)subsetID,
                                         sizeof(uint8_t));
        curr_offset += sizeof(uint8_t);
    }
    // 33-byte commit
    if (action == PROCESS_ANNOUNCE)
    {
        m_commitPoint->Serialize(commit, curr_offset);
    }
    else
    {
        m_consensusSubsets[subsetID].m_commitPoint->Serialize(commit,
                                                              curr_offset);
    }
    curr_offset += COMMIT_POINT_SIZE;
    // 64-byte signature
    Signature signature = SignMessage(commit, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Message signing failed");
        return false;
    }
    signature.Serialize(commit, curr_offset);
    return true;
}

bool ConsensusBackup::ProcessMessageChallengeCore(
    const vector<unsigned char>& challenge, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (!CheckState(action))
    {
        return false;
    }

    // Extract and check challenge message body
    // ========================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge] [64-byte signature]

    const unsigned int length_available = challenge.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + sizeof(uint8_t) + COMMIT_POINT_SIZE + PUB_KEY_SIZE
        + CHALLENGE_SIZE + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        challenge, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in challenge ("
                        << consensus_id
                        << ") does not match instance consensus ID ("
                        << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              challenge.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "Block hash in challenge does not match instance "
                    "block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leaderID = Serializable::GetNumber<uint16_t>(
        challenge, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leaderID != m_leaderID)
    {
        LOG_GENERAL(WARNING, "Leader ID mismatch");
        return false;
    }

    // 1-byte subset id
    unsigned int subsetID = Serializable::GetNumber<uint8_t>(
        challenge, curr_offset, sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // Check the subset id
    if (m_consensusSubsets.find(subsetID) != m_consensusSubsets.end())
    {
        if (CheckStateSubset(subsetID, action) == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Subset state check failed");
            return false;
        }
    }
    else
    {
        // Create entry
        m_consensusSubsets[subsetID].m_commitPoint = m_commitPoint;
        m_consensusSubsets[subsetID].m_commitSecret = m_commitSecret;
        SetStateSubset(subsetID, COMMIT_DONE);
    }

    LOG_GENERAL(INFO, "This msg is for subset with ID = " << subsetID);

    ConsensusSubset& subset = m_consensusSubsets[subsetID];

    // 33-byte aggregated commit
    CommitPoint aggregated_commit(challenge, curr_offset);
    curr_offset += COMMIT_POINT_SIZE;

    // Check the aggregated commit
    if (aggregated_commit.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Invalid aggregated commit received");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    // 33-byte aggregated key
    PubKey aggregated_key(challenge, curr_offset);
    curr_offset += PUB_KEY_SIZE;

    // Check the aggregated key
    if (aggregated_key.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Invalid aggregated key received");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    // 32-byte challenge
    // m_challenge.Deserialize(challenge, curr_offset);
    if (subset.m_challenge.Deserialize(challenge, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] We failed to deserialize m_challenge.");
        SetStateSubset(subsetID, ERROR);
        return false;
    }
    curr_offset += CHALLENGE_SIZE;

    // Check the challenge
    if (subset.m_challenge.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Invalid challenge received");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    if (action == PROCESS_CHALLENGE)
    {
        // First round: consensus over part of message (e.g., DS block header)
        Challenge challenge_verif = GetChallenge(
            m_message, 0, m_lengthToCosign, aggregated_commit, aggregated_key);

        if (!(challenge_verif == subset.m_challenge))
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Generated challenge mismatch");
            SetStateSubset(subsetID, ERROR);
            return false;
        }
    }
    else
    {
        // Second round: consensus over part of message + CS1 + B1
        vector<unsigned char> msg(
            m_lengthToCosign + BLOCK_SIG_SIZE
            + BitVector::GetBitVectorSerializedSize(subset.m_B1.size()));
        copy(m_message.begin(), m_message.begin() + m_lengthToCosign,
             msg.begin());
        subset.m_CS1.Serialize(msg, m_lengthToCosign);
        BitVector::SetBitVector(msg, m_lengthToCosign + BLOCK_SIG_SIZE,
                                subset.m_B1);

        Challenge challenge_verif = GetChallenge(
            msg, 0, msg.size(), aggregated_commit, aggregated_key);

        if (!(challenge_verif == subset.m_challenge))
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Generated challenge mismatch");
            SetStateSubset(subsetID, ERROR);
            return false;
        }
    }

    // 64-byte signature
    // Signature signature(challenge, curr_offset);
    Signature signature;
    if (signature.Deserialize(challenge, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] We failed to deserialize signature.");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    // Check the signature
    bool sig_valid = VerifyMessage(challenge, offset, curr_offset - offset,
                                   signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Invalid signature in challenge message");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    // Generate response
    // =================

    vector<unsigned char> response
        = {m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype)};
    bool result = GenerateResponseMessage(
        response, MessageOffset::BODY + sizeof(unsigned char), subsetID);
    if (result == true)
    {

        // Update internal state
        // =====================

        SetStateSubset(subsetID, nextstate);

        // Unicast to the leader
        // =====================

        P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID), response);
    }
    else
    {
        SetStateSubset(subsetID, ERROR);
    }

    return result;
}

bool ConsensusBackup::ProcessMessageChallenge(
    const vector<unsigned char>& challenge, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageChallengeCore(challenge, offset, PROCESS_CHALLENGE,
                                       RESPONSE, RESPONSE_DONE);
}

bool ConsensusBackup::GenerateResponseMessage(vector<unsigned char>& response,
                                              unsigned int offset,
                                              unsigned int subsetID)
{
    LOG_MARKER();

    // Assemble response message body
    // ==============================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [32-byte response] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [32-byte response]

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(response, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    response.insert(response.begin() + curr_offset, m_blockHash.begin(),
                    m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte backup id
    Serializable::SetNumber<uint16_t>(response, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 1-byte subset id
    Serializable::SetNumber<uint8_t>(response, curr_offset, (uint8_t)subsetID,
                                     sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // 32-byte response
    Response r(*(m_consensusSubsets[subsetID].m_commitSecret),
               m_consensusSubsets[subsetID].m_challenge, m_myPrivKey);
    r.Serialize(response, curr_offset);
    curr_offset += RESPONSE_SIZE;

    // 64-byte signature
    Signature signature = SignMessage(response, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Message signing failed");
        return false;
    }
    signature.Serialize(response, curr_offset);

    return true;
}

bool ConsensusBackup::ProcessMessageCollectiveSigCore(
    const vector<unsigned char>& collectivesig, unsigned int offset,
    Action action, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (!CheckState(action))
    {
        return false;
    }

    // Extract and check collective signature message body
    // ===================================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [N-byte bitmap] [64-byte collective signature] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [N-byte bitmap] [64-byte collective signature]
    // Note on N-byte bitmap: N = number of bytes needed to represent all nodes (1 bit = 1 node) + 2 (length indicator)

    const unsigned int length_available = collectivesig.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + sizeof(uint8_t) + SIGNATURE_CHALLENGE_SIZE
        + SIGNATURE_RESPONSE_SIZE
        + BitVector::GetBitVectorSerializedSize(m_pubKeys.size())
        + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        collectivesig, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in challenge ("
                        << consensus_id
                        << ") does not match instance consensus ID ("
                        << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              collectivesig.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "Block hash in challenge does not match instance "
                    "block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte leader id
    uint16_t leaderID = Serializable::GetNumber<uint16_t>(
        collectivesig, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the leader id
    if (leaderID != m_leaderID)
    {
        LOG_GENERAL(WARNING, "Leader ID mismatch");
        return false;
    }

    // 1-byte subset id
    unsigned int subsetID = Serializable::GetNumber<uint8_t>(
        collectivesig, curr_offset, sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // Check the subset id
    if (m_consensusSubsets.find(subsetID) != m_consensusSubsets.end())
    {
        if (!CheckStateSubset(subsetID, action))
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Subset state check failed");
            return false;
        }
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Subset info not available");
        return false;
    }

    LOG_GENERAL(INFO, "This msg is for subset with ID = " << subsetID);

    ConsensusSubset& subset = m_consensusSubsets[subsetID];

    // N-byte bitmap
    if (action == PROCESS_COLLECTIVESIG)
    {
        subset.m_B1 = BitVector::GetBitVector(
            collectivesig, curr_offset,
            BitVector::GetBitVectorLengthInBytes(m_pubKeys.size()));

        // Check the bitmap
        if (subset.m_B1.empty())
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Response map deserialization failed");
            return false;
        }

        curr_offset += BitVector::GetBitVectorSerializedSize(m_pubKeys.size());

        // 64-byte collective signature
        if (subset.m_CS1.Deserialize(collectivesig, curr_offset) != 0)
        {
            LOG_GENERAL(WARNING,
                        "[Subset "
                            << subsetID
                            << "] We failed to deserialize m_collectiveSig.");
            return false;
        }
        curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

        // Aggregate keys
        PubKey aggregated_key = AggregateKeys(subset.m_B1);
        if (aggregated_key.Initialized() == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Aggregated key generation failed");
            return false;
        }

        // First round: consensus over part of message (e.g., DS block header)
        if (Schnorr::GetInstance().Verify(m_message, 0, m_lengthToCosign,
                                          subset.m_CS1, aggregated_key)
            == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset "
                            << subsetID
                            << "] Collective signature verification failed");
            return false;
        }
    }
    else
    {
        subset.m_B2 = BitVector::GetBitVector(
            collectivesig, curr_offset,
            BitVector::GetBitVectorLengthInBytes(m_pubKeys.size()));

        // Check the bitmap
        if (subset.m_B2.empty())
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Response map deserialization failed");
            return false;
        }

        curr_offset += BitVector::GetBitVectorSerializedSize(m_pubKeys.size());

        // 64-byte collective signature
        if (subset.m_CS2.Deserialize(collectivesig, curr_offset) != 0)
        {
            LOG_GENERAL(WARNING,
                        "[Subset "
                            << subsetID
                            << "] We failed to deserialize m_collectiveSig.");
            return false;
        }
        curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

        // Aggregate keys
        PubKey aggregated_key = AggregateKeys(subset.m_B2);
        if (aggregated_key.Initialized() == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Aggregated key generation failed");
            return false;
        }

        // Second round: consensus over part of message + CS1 + B1
        vector<unsigned char> msg(
            m_lengthToCosign + BLOCK_SIG_SIZE
            + BitVector::GetBitVectorSerializedSize(subset.m_B1.size()));
        copy(m_message.begin(), m_message.begin() + m_lengthToCosign,
             msg.begin());
        subset.m_CS1.Serialize(msg, m_lengthToCosign);
        BitVector::SetBitVector(msg, m_lengthToCosign + BLOCK_SIG_SIZE,
                                subset.m_B1);

        if (Schnorr::GetInstance().Verify(msg, 0, msg.size(), subset.m_CS2,
                                          aggregated_key)
            == false)
        {
            LOG_GENERAL(
                WARNING,
                "[Subset "
                    << subsetID
                    << "] Final collective signature verification failed");
            return false;
        }
    }

    // 64-byte signature
    // Signature signature(collectivesig, curr_offset);
    Signature signature;
    if (signature.Deserialize(collectivesig, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] We failed to deserialize signature.");
        return false;
    }

    // Check the signature
    bool sig_valid = VerifyMessage(collectivesig, offset, curr_offset - offset,
                                   signature, m_leaderID);
    if (sig_valid == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Invalid signature in challenge message");
        SetStateSubset(subsetID, ERROR);
        return false;
    }

    bool result = true;

    if (action == PROCESS_COLLECTIVESIG)
    {
        // Generate final commit
        vector<unsigned char> finalcommit
            = {m_classByte, m_insByte,
               static_cast<unsigned char>(ConsensusMessageType::FINALCOMMIT)};
        result = GenerateCommitMessage(
            finalcommit, MessageOffset::BODY + sizeof(unsigned char), subsetID,
            action);
        if (result == true)
        {
            // Update internal state
            SetStateSubset(subsetID, nextstate);

            // Unicast to the leader
            P2PComm::GetInstance().SendMessage(m_peerInfo.at(m_leaderID),
                                               finalcommit);
        }
    }
    else
    {
        // Update internal state
        SetStateSubset(subsetID, nextstate);
    }

    return result;
}

bool ConsensusBackup::ProcessMessageCollectiveSig(
    const vector<unsigned char>& collectivesig, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCollectiveSigCore(
        collectivesig, offset, PROCESS_COLLECTIVESIG, FINALCOMMIT_DONE);
}

bool ConsensusBackup::ProcessMessageFinalChallenge(
    const vector<unsigned char>& challenge, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageChallengeCore(challenge, offset,
                                       PROCESS_FINALCHALLENGE, FINALRESPONSE,
                                       FINALRESPONSE_DONE);
}

bool ConsensusBackup::ProcessMessageFinalCollectiveSig(
    const vector<unsigned char>& finalcollectivesig, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCollectiveSigCore(finalcollectivesig, offset,
                                           PROCESS_FINALCOLLECTIVESIG, DONE);
}

ConsensusBackup::ConsensusBackup(
    uint32_t consensus_id, const vector<unsigned char>& block_hash,
    uint16_t node_id, uint16_t leaderID, const PrivKey& privkey,
    const deque<PubKey>& pubkeys, const deque<Peer>& peer_info,
    unsigned char class_byte, unsigned char ins_byte,
    MsgContentValidatorFunc msg_validator)
    : ConsensusCommon(consensus_id, block_hash, node_id, privkey, pubkeys,
                      peer_info, class_byte, ins_byte)
{
    LOG_MARKER();

    m_state = INITIAL;
    m_leaderID = leaderID;
    m_msgContentValidator = msg_validator;
}

ConsensusBackup::~ConsensusBackup() {}

bool ConsensusBackup::ProcessMessage(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    // Incoming message format (from offset): [1-byte consensus message type] [consensus message]

    bool result = false;

    switch (message.at(offset))
    {
    case ConsensusMessageType::ANNOUNCE:
        result = ProcessMessageAnnounce(message, offset + 1);
        break;
    case ConsensusMessageType::CONSENSUSFAILURE:
        result = ProcessMessageConsensusFailure(message, offset + 1);
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
        LOG_GENERAL(WARNING, "Unknown consensus message received");
    }

    return result;
}

ConsensusCommon::State ConsensusBackup::GetState() const
{
    State result = INITIAL;

    if ((m_state < COMMIT_DONE) || (m_state >= DONE))
    {
        result = m_state;
    }
    else
    {
        for (auto& subset : m_consensusSubsets)
        {
            if (subset.second.m_state > result)
            {
                result = subset.second.m_state;
            }
        }
    }

    return result;
}

std::ostream& operator<<(std::ostream& out, const ConsensusBackup::Action value)
{
    const char* s = 0;
#define PROCESS_VAL(p)                                                         \
    case (ConsensusBackup::p):                                                 \
        s = #p;                                                                \
        break;
    switch (value)
    {
        PROCESS_VAL(PROCESS_ANNOUNCE);
        PROCESS_VAL(PROCESS_CHALLENGE);
        PROCESS_VAL(PROCESS_COLLECTIVESIG);
        PROCESS_VAL(PROCESS_FINALCHALLENGE);
        PROCESS_VAL(PROCESS_FINALCOLLECTIVESIG);
    }
#undef PROCESS_VAL

    return out << s;
}
