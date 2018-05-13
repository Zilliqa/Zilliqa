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

#include "ConsensusLeader.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;

// To avoid the linking errors in Mac compiler
const unsigned int ConsensusLeader::COMMIT_WINDOW_IN_SECONDS;
const unsigned int ConsensusLeader::DELAY_BEFORE_STARTING_SUBSET_IN_SECONDS;
const unsigned int ConsensusLeader::NUM_CONSENSUS_SUBSETS;

bool ConsensusLeader::CheckState(Action action)
{
    bool result = true;

    switch (action)
    {
    case SEND_ANNOUNCEMENT:
        switch (m_state)
        {
        case INITIAL:
            break;
        case ANNOUNCE_DONE:
        case COMMIT_TIMER_EXPIRED:
        case COMMIT_LISTS_GENERATED:
        case CHALLENGE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_COMMITFAILURE:
        switch (m_state)
        {
        case INITIAL:
            result = false;
            break;
        case ANNOUNCE_DONE:
        case COMMIT_TIMER_EXPIRED:
        case COMMIT_LISTS_GENERATED:
        case CHALLENGE_DONE:
            break;
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_COMMIT:
        switch (m_state)
        {
        case INITIAL:
            result = false;
            break;
        case ANNOUNCE_DONE:
            break;
        case COMMIT_TIMER_EXPIRED:
        case COMMIT_LISTS_GENERATED:
        case CHALLENGE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_RESPONSE:
    case PROCESS_FINALCOMMIT:
    case PROCESS_FINALRESPONSE:
        switch (m_state)
        {
        case INITIAL:
        case ANNOUNCE_DONE:
        case COMMIT_TIMER_EXPIRED:
        case COMMIT_LISTS_GENERATED:
            result = false;
            break;
        case CHALLENGE_DONE:
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

bool ConsensusLeader::CheckStateSubset(unsigned int subsetID, Action action)
{
    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    bool result = true;

    switch (action)
    {
    case PROCESS_RESPONSE:
        switch (subset.m_state)
        {
        case INITIAL:
        case ANNOUNCE_DONE:
            result = false;
            break;
        case CHALLENGE_DONE:
            break;
        case COLLECTIVESIG_DONE:
        case FINALCHALLENGE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_FINALCOMMIT:
        switch (subset.m_state)
        {
        case INITIAL:
        case ANNOUNCE_DONE:
        case CHALLENGE_DONE:
            result = false;
            break;
        case COLLECTIVESIG_DONE:
            break;
        case FINALCHALLENGE_DONE:
        case DONE:
        case ERROR:
        default:
            result = false;
            break;
        }
        break;
    case PROCESS_FINALRESPONSE:
        switch (subset.m_state)
        {
        case INITIAL:
        case ANNOUNCE_DONE:
        case CHALLENGE_DONE:
        case COLLECTIVESIG_DONE:
            result = false;
            break;
        case FINALCHALLENGE_DONE:
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

void ConsensusLeader::SetStateSubset(unsigned int subsetID, State newState)
{
    LOG_MARKER();

    if ((newState == INITIAL)
        || (newState > m_consensusSubsets.at(subsetID).m_state))
    {
        m_consensusSubsets.at(subsetID).m_state = newState;
    }
}

void ConsensusLeader::GenerateConsensusSubsets()
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutex);

    // Get the list of all the peers who committed, by peer index
    vector<unsigned int> peersWhoCommitted;
    for (unsigned int index = 0; index < m_commitPointMap.size(); index++)
    {
        if (m_commitPointMap.at(index).Initialized())
        {
            peersWhoCommitted.push_back(index);
        }
    }

    // Generate NUM_CONSENSUS_SUBSETS lists (= subsets of peersWhoCommitted)
    // If we have exactly the minimum num required for consensus, no point making more than 1 subset
    const unsigned int numSubsets
        = (peersWhoCommitted.size() == m_numForConsensus)
        ? 1
        : NUM_CONSENSUS_SUBSETS;
    m_consensusSubsets.clear();
    m_consensusSubsets.resize(numSubsets);
    for (unsigned int i = 0; i < numSubsets; i++)
    {
        ConsensusSubset& subset = m_consensusSubsets.at(i);
        subset.m_commitPointMap.resize(m_pubKeys.size());
        subset.m_commitOrResponseCounter = 0;
        subset.m_responseDataMap.resize(m_pubKeys.size());
        subset.m_state = ANNOUNCE_DONE;

        for (unsigned int j = 0; j < m_numForConsensus; j++)
        {
            unsigned int index = peersWhoCommitted.at(j);
            subset.m_commitPointMap.at(index) = m_commitPointMap.at(index);
        }

        random_shuffle(peersWhoCommitted.begin(), peersWhoCommitted.end());
    }

    // Clear out the original commit map, we don't need it anymore at this point
    m_commitPointMap.clear();

    LOG_GENERAL(INFO,
                "Generated " << numSubsets << " subsets of "
                             << m_numForConsensus
                             << " backups each for this consensus");

    SetState(COMMIT_LISTS_GENERATED);
}

void ConsensusLeader::StartConsensusSubsets()
{
    LOG_MARKER();

    {
        // Update overall internal state
        lock_guard<mutex> g(m_mutex);
        SetState(CHALLENGE_DONE);
    }

    m_numSubsetsRunning = m_consensusSubsets.size();

    for (unsigned int index = 0; index < m_consensusSubsets.size(); index++)
    {
        // If overall state has somehow transitioned from CHALLENGE_DONE then it means
        // consensus has ended and there's no point in starting another subset
        if (m_state != CHALLENGE_DONE)
        {
            break;
        }

        ConsensusSubset& subset = m_consensusSubsets.at(index);
        vector<unsigned char> challenge
            = {m_classByte, m_insByte,
               static_cast<unsigned char>(ConsensusMessageType::CHALLENGE)};
        bool result = GenerateChallengeMessage(
            challenge, MessageOffset::BODY + sizeof(unsigned char), index,
            PROCESS_COMMIT);

        if (result == true)
        {
            // Update subset's internal state
            SetStateSubset(index, CHALLENGE_DONE);

            // Multicast to all nodes in this subset who sent validated commits
            vector<Peer> commit_peers;
            deque<Peer>::const_iterator j = m_peerInfo.begin();
            for (unsigned int i = 0; i < subset.m_commitPointMap.size();
                 i++, j++)
            {
                if (subset.m_commitPointMap.at(i).Initialized())
                {
                    commit_peers.push_back(*j);
                }
            }
            P2PComm::GetInstance().SendMessage(commit_peers, challenge);
        }
        else
        {
            SetStateSubset(index, ERROR);
            SubsetEnded(index);
        }

        // Throttle initiating next subset by a few seconds to avoid unnecessary flooding
        this_thread::sleep_for(
            chrono::seconds(DELAY_BEFORE_STARTING_SUBSET_IN_SECONDS));
    }
}

void ConsensusLeader::SubsetEnded(unsigned int subsetID)
{
    LOG_MARKER();

    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    if (subset.m_state == DONE)
    {
        // We've achieved consensus!
        LOG_GENERAL(
            INFO, "[Subset " << subsetID << "] Subset has finished consensus!");

        // Reset all other subsets to INITIAL so they reject any further messages from their backups
        for (unsigned int i = 0; i < m_consensusSubsets.size(); i++)
        {
            if (i == subsetID)
            {
                continue;
            }

            SetStateSubset(i, INITIAL);
        }

        // Set overall state to DONE
        SetState(DONE);

        // Set the final consensus data
        // B1 and B2 will be equal
        m_CS1 = subset.m_CS1;
        m_CS2 = subset.m_CS2;
        m_B1.resize(subset.m_responseDataMap.size(), false);
        m_B2.resize(subset.m_responseDataMap.size(), false);
        for (unsigned int i = 0; i < subset.m_responseDataMap.size(); i++)
        {
            if (subset.m_responseDataMap.at(i).Initialized())
            {
                m_B1.at(i) = true;
                m_B2.at(i) = true;
            }
        }
    }
    else if (--m_numSubsetsRunning == 0)
    {
        // All subsets have ended and not one reached consensus!
        LOG_GENERAL(
            INFO,
            "[Subset " << subsetID
                       << "] Last remaining subset failed to reach consensus!");

        // Set overall state to ERROR
        SetState(ERROR);
    }
    else
    {
        LOG_GENERAL(INFO,
                    "[Subset " << subsetID
                               << "] Subset has failed to reach consensus!");
    }
}

bool ConsensusLeader::ProcessMessageCommitCore(
    const vector<unsigned char>& commit, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (!CheckState(action))
    {
        return false;
    }

    // Extract and check commit message body
    // =====================================

    // Format Commit: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit] [64-byte signature]
    // Format FinalCommit: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [33-byte commit] [64-byte signature]

    const unsigned int length_available = commit.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t)
        + (action == PROCESS_FINALCOMMIT ? sizeof(uint8_t) : 0)
        + COMMIT_POINT_SIZE + SIGNATURE_CHALLENGE_SIZE
        + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(
        commit, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensusID != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in commitment ("
                        << consensusID
                        << ") does not match instance consensus ID ("
                        << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              commit.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "Block hash in commitment does not match instance "
                    "block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte backup id
    unsigned int backupID = Serializable::GetNumber<uint16_t>(
        commit, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    unsigned int subsetID = 0;

    if (action == PROCESS_COMMIT)
    {
        // Check the backup id
        if (backupID >= m_commitPointMap.size())
        {
            LOG_GENERAL(WARNING,
                        "Backup ID (" << backupID << ") beyond backup count");
            return false;
        }
        if (m_commitPointMap.at(backupID).Initialized() == true)
        {
            LOG_GENERAL(WARNING,
                        "Backup " << backupID
                                  << " has already sent validated commit");
            return false;
        }
    }
    else
    {
        // 1-byte subset id
        subsetID = Serializable::GetNumber<uint8_t>(commit, curr_offset,
                                                    sizeof(uint8_t));
        curr_offset += sizeof(uint8_t);

        // Check the subset id
        if (subsetID >= NUM_CONSENSUS_SUBSETS)
        {
            LOG_GENERAL(WARNING,
                        "Error: Subset ID (" << subsetID
                                             << ") >= NUM_CONSENSUS_SUBSETS");
            return false;
        }

        LOG_GENERAL(INFO,
                    "Msg is for subset " << subsetID << " backup " << backupID);

        ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

        // Check subset state
        if (!CheckStateSubset(subsetID, action))
        {
            return false;
        }

        // Check the backup id
        if (backupID >= subset.m_commitPointMap.size())
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID << "] [Backup " << backupID
                                   << "] Backup ID beyond backup count");
            return false;
        }
        if (subset.m_commitPointMap.at(backupID).Initialized())
        {
            LOG_GENERAL(
                WARNING,
                "[Subset "
                    << subsetID << "] [Backup " << backupID
                    << "] Backup has already sent validated final commit");
            return false;
        }

        // Check if this backup was part of this subset's first round commit
        if (subset.m_responseDataMap.at(backupID).Initialized() == false)
        {
            LOG_GENERAL(
                WARNING,
                "[Subset "
                    << subsetID << "] [Backup " << backupID
                    << "] Backup has not participated in the commit phase");
            return false;
        }
    }

    // 33-byte commit - skip for now, deserialize later below
    curr_offset += COMMIT_POINT_SIZE;

    // 64-byte signature
    Signature signature;
    if (signature.Deserialize(commit, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize signature.");
        return false;
    }

    // Check the signature
    if (VerifyMessage(commit, offset, curr_offset - offset, signature,
                      (uint16_t)backupID)
        == false)
    {
        LOG_GENERAL(WARNING, "Invalid signature in commit message");
        return false;
    }

    bool result = true;
    {
        lock_guard<mutex> g(m_mutex);

        if (action == PROCESS_COMMIT)
        {
            // 33-byte commit
            m_commitPointMap.at(backupID)
                = CommitPoint(commit, curr_offset - COMMIT_POINT_SIZE);
            if (!m_commitPointMap.at(backupID).Initialized())
            {
                LOG_GENERAL(WARNING, "Failed to set commit");
                return false;
            }
            m_commitCounter++;

            if (m_commitCounter % 10 == 0)
            {
                LOG_GENERAL(INFO,
                            "Received " << m_commitCounter
                                        << " out of a possible "
                                        << m_pubKeys.size()
                                        << " commits (min for consensus = "
                                        << m_numForConsensus << ")");
            }
        }
        else
        {
            ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

            if (!CheckStateSubset(subsetID, action))
            {
                return false;
            }

            // 33-byte commit
            subset.m_commitPointMap.at(backupID)
                = CommitPoint(commit, curr_offset - COMMIT_POINT_SIZE);
            if (!subset.m_commitPointMap.at(backupID).Initialized())
            {
                LOG_GENERAL(WARNING,
                            "[Subset " << subsetID << "] Failed to set commit");
                return false;
            }
            subset.m_commitOrResponseCounter++;

            if (subset.m_commitOrResponseCounter % 10 == 0)
            {
                LOG_GENERAL(INFO,
                            "[Subset " << subsetID << "] Received "
                                       << subset.m_commitOrResponseCounter
                                       << " out of " << m_numForConsensus);
            }

            if (subset.m_commitOrResponseCounter == m_numForConsensus)
            {
                LOG_GENERAL(INFO,
                            "[Subset " << subsetID << "] All "
                                       << m_numForConsensus
                                       << " commits obtained");

                vector<unsigned char> challenge
                    = {m_classByte, m_insByte,
                       static_cast<unsigned char>(
                           ConsensusMessageType::FINALCHALLENGE)};
                result = GenerateChallengeMessage(
                    challenge, MessageOffset::BODY + sizeof(unsigned char),
                    subsetID, PROCESS_FINALCOMMIT);

                if (result == true)
                {
                    vector<Peer> commit_peers;
                    deque<Peer>::const_iterator j = m_peerInfo.begin();
                    for (unsigned int i = 0; i < subset.m_commitPointMap.size();
                         i++, j++)
                    {
                        if (subset.m_commitPointMap.at(i).Initialized())
                        {
                            commit_peers.push_back(*j);
                        }
                    }

                    // Update subset internal state before multicasting
                    SetStateSubset(subsetID, FINALCHALLENGE_DONE);
                    subset.m_responseDataMap.clear();
                    subset.m_responseDataMap.resize(m_pubKeys.size());
                    subset.m_commitOrResponseCounter = 0;

                    // Multicast to all nodes in this subset
                    // FIXME: quick fix: 0106'08' comes to the backup ealier than 0106'04'
                    this_thread::sleep_for(chrono::milliseconds(1000));
                    P2PComm::GetInstance().SendMessage(commit_peers, challenge);
                }
                else
                {
                    SetStateSubset(subsetID, ERROR);
                    SubsetEnded(subsetID);
                }
            }
        }
    }

    return result;
}

bool ConsensusLeader::ProcessMessageCommit(const vector<unsigned char>& commit,
                                           unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCommitCore(commit, offset, PROCESS_COMMIT, CHALLENGE,
                                    CHALLENGE_DONE);
}

bool ConsensusLeader::ProcessMessageCommitFailure(
    const vector<unsigned char>& commitFailureMsg, unsigned int offset,
    const Peer& from)
{
    LOG_MARKER();

    if (!CheckState(PROCESS_COMMITFAILURE))
    {
        return false;
    }

    const unsigned int length_available = commitFailureMsg.size() - offset;
    const unsigned int length_needed
        = sizeof(uint32_t) + BLOCK_HASH_SIZE + sizeof(uint16_t);

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(
        commitFailureMsg, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensusID != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in commitment ("
                        << consensusID << ") does not match "
                        << "instance consensus ID (" << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              commitFailureMsg.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "Block hash in commitment does not match instance "
                    "block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte backup id
    uint16_t backupID = Serializable::GetNumber<uint16_t>(
        commitFailureMsg, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the backup id
    if (backupID
        >= m_commitPointMap
               .size()) // using commitMap instead of commitFailureMap knowingly since its size = puKeys.size() for sure
    {
        LOG_GENERAL(WARNING, "Backup ID beyond backup count");
        return false;
    }

    if (m_commitFailureMap.find(backupID) != m_commitFailureMap.end())
    {
        LOG_GENERAL(WARNING, "Backup has already sent commit failure message");
        return false;
    }

    m_commitFailureCounter++;
    m_commitFailureMap[backupID] = vector<unsigned char>(
        commitFailureMsg.begin() + curr_offset, commitFailureMsg.end());

    m_nodeCommitFailureHandlerFunc(commitFailureMsg, curr_offset, from);

    if (m_commitFailureCounter == m_numForConsensusFailure)
    {
        SetState(ERROR);

        vector<unsigned char> consensusFailureMsg
            = {m_classByte, m_insByte, CONSENSUSFAILURE};
        P2PComm::GetInstance().SendMessage(m_peerInfo, consensusFailureMsg);

        auto main_func = [this]() mutable -> void {
            m_shardCommitFailureHandlerFunc(m_commitFailureMap);
        };
        DetachedFunction(1, main_func);
    }

    return true;
}

bool ConsensusLeader::GenerateChallengeMessage(vector<unsigned char>& challenge,
                                               unsigned int offset,
                                               unsigned int subsetID,
                                               Action action)
{
    LOG_MARKER();

    // Generate challenge object
    // =========================

    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    // Aggregate commits
    CommitPoint aggregated_commit = AggregateCommits(subset.m_commitPointMap);
    if (aggregated_commit.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] AggregateCommits failed");
        return false;
    }

    // Aggregate keys
    vector<bool> commitBitmap(subset.m_commitPointMap.size(), false);
    for (unsigned int i = 0; i < subset.m_commitPointMap.size(); i++)
    {
        if (subset.m_commitPointMap.at(i).Initialized())
        {
            commitBitmap.at(i) = true;
        }
    }
    PubKey aggregated_key = AggregateKeys(commitBitmap);
    if (aggregated_key.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Aggregated key generation failed");
        return false;
    }

    // Generate the challenge
    if (action == PROCESS_COMMIT)
    {
        // First round: consensus over part of message (e.g., DS block header)
        subset.m_challenge = GetChallenge(m_message, 0, m_lengthToCosign,
                                          aggregated_commit, aggregated_key);
    }
    else
    {
        // Second round: consensus over part of message + CS1 + B1
        vector<unsigned char> msg(
            m_lengthToCosign + BLOCK_SIG_SIZE
            + BitVector::GetBitVectorSerializedSize(commitBitmap.size()));
        copy(m_message.begin(), m_message.begin() + m_lengthToCosign,
             msg.begin());
        subset.m_CS1.Serialize(msg, m_lengthToCosign);
        BitVector::SetBitVector(msg, m_lengthToCosign + BLOCK_SIG_SIZE,
                                commitBitmap);
        subset.m_challenge = GetChallenge(msg, 0, msg.size(), aggregated_commit,
                                          aggregated_key);
    }

    if (subset.m_challenge.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Challenge generation failed");
        return false;
    }

    // Assemble challenge message body
    // ===============================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge]

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(challenge, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    challenge.insert(challenge.begin() + curr_offset, m_blockHash.begin(),
                     m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte leader id
    Serializable::SetNumber<uint16_t>(challenge, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 1-byte subset id
    Serializable::SetNumber<uint8_t>(challenge, curr_offset, (uint8_t)subsetID,
                                     sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // 33-byte aggregated commit
    aggregated_commit.Serialize(challenge, curr_offset);
    curr_offset += COMMIT_POINT_SIZE;

    // 33-byte aggregated key
    aggregated_key.Serialize(challenge, curr_offset);
    curr_offset += PUB_KEY_SIZE;

    // 32-byte challenge
    subset.m_challenge.Serialize(challenge, curr_offset);
    curr_offset += CHALLENGE_SIZE;

    // 64-byte signature
    Signature signature = SignMessage(challenge, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Message signing failed");
        return false;
    }
    signature.Serialize(challenge, curr_offset);

    return true;
}

bool ConsensusLeader::ProcessMessageResponseCore(
    const vector<unsigned char>& response, unsigned int offset, Action action,
    ConsensusMessageType returnmsgtype, State nextstate)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (!CheckState(action))
    {
        return false;
    }

    // Extract and check response message body
    // =======================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [1-byte subset id] [32-byte response] [64-byte signature]

    const unsigned int length_available = response.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + sizeof(uint8_t) + RESPONSE_SIZE
        + SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensusID = Serializable::GetNumber<uint32_t>(
        response, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensusID != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in response ("
                        << consensusID
                        << ") does not match instance consensus ID ("
                        << m_consensusID << ")");
        return false;
    }

    // 32-byte blockhash

    // Check the block hash
    if (equal(m_blockHash.begin(), m_blockHash.end(),
              response.begin() + curr_offset)
        == false)
    {
        LOG_GENERAL(
            WARNING,
            "Block hash in response does not match instance block hash");
        return false;
    }
    curr_offset += BLOCK_HASH_SIZE;

    // 2-byte backup id
    unsigned int backupID = Serializable::GetNumber<uint16_t>(
        response, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 1-byte subset id
    unsigned int subsetID = Serializable::GetNumber<uint8_t>(
        response, curr_offset, sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // Check the subset id
    if (subsetID >= NUM_CONSENSUS_SUBSETS)
    {
        LOG_GENERAL(WARNING,
                    "Error: Subset ID (" << subsetID
                                         << ") >= NUM_CONSENSUS_SUBSETS");
        return false;
    }

    LOG_GENERAL(INFO,
                "Msg is for subset " << subsetID << " backup " << backupID);

    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    // Check subset state
    if (!CheckStateSubset(subsetID, action))
    {
        return false;
    }

    // Check the backup id
    if (backupID >= subset.m_responseDataMap.size())
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] [Backup " << backupID
                               << "] Backup ID beyond backup count");
        return false;
    }
    if (subset.m_commitPointMap.at(backupID).Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset "
                        << subsetID << "] [Backup " << backupID
                        << "] Backup has not participated in the commit phase");
        return false;
    }

    for (unsigned int i = 0; i < subset.m_responseDataMap.size(); i++)
        LOG_GENERAL(INFO,
                    "Response map "
                        << i << " = "
                        << subset.m_responseDataMap.at(i).Initialized());

    if (subset.m_responseDataMap.at(backupID).Initialized())
    {
        LOG_GENERAL(WARNING,
                    "[Subset "
                        << subsetID << "] [Backup " << backupID
                        << "] Backup has already sent validated response");
        return false;
    }

    // 32-byte response
    Response tmp_response = Response(response, curr_offset);
    if (!tmp_response.Initialized())
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] [Backup " << backupID
                               << "] Failed to deserialize response");
        return false;
    }
    curr_offset += RESPONSE_SIZE;

    if (MultiSig::VerifyResponse(tmp_response, subset.m_challenge,
                                 m_pubKeys.at(backupID),
                                 subset.m_commitPointMap.at(backupID))
        == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] [Backup " << backupID
                               << "] Invalid response for this backup");
        return false;
    }

    // 64-byte signature
    Signature signature;
    if (signature.Deserialize(response, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] [Backup " << backupID
                               << "] Failed to deserialize signature");
        return false;
    }

    // Check the signature
    if (VerifyMessage(response, offset, curr_offset - offset, signature,
                      backupID)
        == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] [Backup " << backupID
                               << "] Invalid signature in response message");
        return false;
    }

    // Update internal state
    // =====================

    lock_guard<mutex> g(m_mutex);

    if (!CheckState(action))
    {
        return false;
    }

    if (!CheckStateSubset(subsetID, action))
    {
        return false;
    }

    // 32-byte response
    subset.m_responseDataMap.at(backupID) = tmp_response;
    subset.m_commitOrResponseCounter++;

    if (subset.m_commitOrResponseCounter % 10 == 0)
    {
        LOG_GENERAL(INFO,
                    "[Subset " << subsetID << "] Received "
                               << subset.m_commitOrResponseCounter << " out of "
                               << m_numForConsensus);
    }

    // Generate collective sig if everyone has responded

    bool result = true;

    if (subset.m_commitOrResponseCounter == m_numForConsensus)
    {
        LOG_GENERAL(INFO,
                    "[Subset " << subsetID << "] All responses obtained.");

        vector<unsigned char> collectivesig = {
            m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype)};
        result = GenerateCollectiveSigMessage(
            collectivesig, MessageOffset::BODY + sizeof(unsigned char),
            subsetID, action);

        if (result == true)
        {
            // Update subset's internal state
            SetStateSubset(subsetID, nextstate);

            if (action == PROCESS_RESPONSE)
            {
                vector<Peer> subset_peers;
                deque<Peer>::const_iterator j = m_peerInfo.begin();
                for (unsigned int i = 0; i < subset.m_commitPointMap.size();
                     i++, j++)
                {
                    if (subset.m_commitPointMap.at(i).Initialized())
                    {
                        subset_peers.push_back(*j);
                    }
                }

                // Update subset internal state before multicasting
                subset.m_commitPointMap.clear();
                subset.m_commitPointMap.resize(m_pubKeys.size());
                subset.m_commitOrResponseCounter = 0;

                // Multicast to all nodes in the subset
                P2PComm::GetInstance().SendMessage(subset_peers, collectivesig);
            }
            else
            {
                // Subset has finished consensus!
                SubsetEnded(subsetID);

                // Multicast to all nodes in the committee
                P2PComm::GetInstance().SendMessage(m_peerInfo, collectivesig);
            }
        }
        else
        {
            SetStateSubset(subsetID, ERROR);
            SubsetEnded(subsetID);
        }
    }

    return result;
}

bool ConsensusLeader::ProcessMessageResponse(
    const vector<unsigned char>& response, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageResponseCore(response, offset, PROCESS_RESPONSE,
                                      COLLECTIVESIG, COLLECTIVESIG_DONE);
}

bool ConsensusLeader::GenerateCollectiveSigMessage(
    vector<unsigned char>& collectivesig, unsigned int offset,
    unsigned int subsetID, Action action)
{
    LOG_MARKER();

    // Generate collective signature object
    // ====================================

    ConsensusSubset& subset = m_consensusSubsets.at(subsetID);

    // Aggregate responses
    vector<bool> responseBitmap(subset.m_responseDataMap.size(), false);
    for (unsigned int i = 0; i < subset.m_responseDataMap.size(); i++)
    {
        if (subset.m_responseDataMap.at(i).Initialized())
        {
            responseBitmap.at(i) = true;
        }
    }

    Response aggregated_response = AggregateResponses(subset.m_responseDataMap);
    if (aggregated_response.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] AggregateResponses failed");
        return false;
    }

    // Aggregate keys
    PubKey aggregated_key = AggregateKeys(responseBitmap);
    if (aggregated_key.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID
                               << "] Aggregated key generation failed");
        return false;
    }

    // Generate and verify the collective signature
    if (action == PROCESS_RESPONSE)
    {
        subset.m_CS1 = AggregateSign(subset.m_challenge, aggregated_response);
        if (subset.m_CS1.Initialized() == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Collective sig generation failed");
            return false;
        }

        // First round: consensus over part of message (e.g., DS block header)

        if (Schnorr::GetInstance().Verify(m_message, 0, m_lengthToCosign,
                                          subset.m_CS1, aggregated_key)
            == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Collective sig verification failed");
            return false;
        }
    }
    else
    {
        subset.m_CS2 = AggregateSign(subset.m_challenge, aggregated_response);
        if (subset.m_CS2.Initialized() == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Collective sig generation failed");
            return false;
        }

        // Second round: consensus over part of message + CS1 + B1

        vector<unsigned char> msg(
            m_lengthToCosign + BLOCK_SIG_SIZE
            + BitVector::GetBitVectorSerializedSize(responseBitmap.size()));
        copy(m_message.begin(), m_message.begin() + m_lengthToCosign,
             msg.begin());
        subset.m_CS1.Serialize(msg, m_lengthToCosign);
        BitVector::SetBitVector(msg, m_lengthToCosign + BLOCK_SIG_SIZE,
                                responseBitmap);

        if (Schnorr::GetInstance().Verify(msg, subset.m_CS2, aggregated_key)
            == false)
        {
            LOG_GENERAL(WARNING,
                        "[Subset " << subsetID
                                   << "] Collective sig verification failed");
            //return false;
        }
    }

    // Assemble collective signature message body
    // ==========================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [N-byte bitmap] [64-byte collective signature] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [1-byte subset id] [N-byte bitmap] [64-byte collective signature]
    // Note on N-byte bitmap: N = number of bytes needed to represent all nodes (1 bit = 1 node) + 2 (length indicator)

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(collectivesig, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 32-byte blockhash
    collectivesig.insert(collectivesig.begin() + curr_offset,
                         m_blockHash.begin(), m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte leader id
    Serializable::SetNumber<uint16_t>(collectivesig, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // 1-byte subset id
    Serializable::SetNumber<uint8_t>(collectivesig, curr_offset,
                                     (uint8_t)subsetID, sizeof(uint8_t));
    curr_offset += sizeof(uint8_t);

    // N-byte bitmap
    curr_offset
        += BitVector::SetBitVector(collectivesig, curr_offset, responseBitmap);

    // 64-byte collective signature
    if (action == PROCESS_RESPONSE)
    {
        subset.m_CS1.Serialize(collectivesig, curr_offset);
    }
    else
    {
        subset.m_CS2.Serialize(collectivesig, curr_offset);
    }
    curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    // 64-byte signature
    Signature signature
        = SignMessage(collectivesig, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING,
                    "[Subset " << subsetID << "] Message signing failed");
        return false;
    }
    signature.Serialize(collectivesig, curr_offset);

    return true;
}

bool ConsensusLeader::ProcessMessageFinalCommit(
    const vector<unsigned char>& finalcommit, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageCommitCore(finalcommit, offset, PROCESS_FINALCOMMIT,
                                    FINALCHALLENGE, FINALCHALLENGE_DONE);
}

bool ConsensusLeader::ProcessMessageFinalResponse(
    const vector<unsigned char>& finalresponse, unsigned int offset)
{
    LOG_MARKER();
    return ProcessMessageResponseCore(
        finalresponse, offset, PROCESS_FINALRESPONSE, FINALCOLLECTIVESIG, DONE);
}

ConsensusLeader::ConsensusLeader(
    uint32_t consensusID, const vector<unsigned char>& block_hash,
    uint16_t node_id, const PrivKey& privkey, const deque<PubKey>& pubkeys,
    const deque<Peer>& peer_info, unsigned char class_byte,
    unsigned char ins_byte,
    NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
    ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc)
    : ConsensusCommon(consensusID, block_hash, node_id, privkey, pubkeys,
                      peer_info, class_byte, ins_byte)
    , m_commitPointMap(pubkeys.size())
    , m_responseDataMap(pubkeys.size())
{
    LOG_MARKER();

    m_state = INITIAL;
    // m_numForConsensus = (floor(TOLERANCE_FRACTION * (pubkeys.size() - 1)) + 1);
    m_numForConsensus = ConsensusCommon::NumForConsensus(pubkeys.size());
    m_numForConsensusFailure = pubkeys.size() - m_numForConsensus;
    LOG_GENERAL(INFO,
                "TOLERANCE_FRACTION "
                    << TOLERANCE_FRACTION << " pubkeys.size() "
                    << pubkeys.size() << " m_numForConsensus "
                    << m_numForConsensus << " m_numForConsensusFailure "
                    << m_numForConsensusFailure);

    m_nodeCommitFailureHandlerFunc = nodeCommitFailureHandlerFunc;
    m_shardCommitFailureHandlerFunc = shardCommitFailureHandlerFunc;
}

ConsensusLeader::~ConsensusLeader() {}

bool ConsensusLeader::StartConsensus(const vector<unsigned char>& message,
                                     uint32_t lengthToCosign)
{
    LOG_MARKER();

    // Initial checks
    // ==============

    if (message.size() == 0)
    {
        LOG_GENERAL(WARNING, "Empty message");
        return false;
    }

    if (lengthToCosign > message.size())
    {
        LOG_GENERAL(WARNING, "lengthToCosign > message size");
        return false;
    }

    if (!CheckState(SEND_ANNOUNCEMENT))
    {
        return false;
    }

    // Assemble announcement message body
    // ==================================

    // Format: [CLA] [INS] [1-byte consensus message type] [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [message] [4-byte length to co-sign] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [message] [4-byte length to co-sign]

    LOG_GENERAL(INFO,
                "DEBUG: my ip is "
                    << m_peerInfo.at(m_myID).GetPrintableIPAddress());
    LOG_GENERAL(INFO,
                "DEBUG: my pub is " << DataConversion::SerializableToHexStr(
                    m_pubKeys.at(m_myID)));

    vector<unsigned char> announcement
        = {m_classByte, m_insByte,
           static_cast<unsigned char>(ConsensusMessageType::ANNOUNCE)};
    unsigned int curr_offset = MessageOffset::BODY + sizeof(unsigned char);

    // 4-byte consensus id
    Serializable::SetNumber<uint32_t>(announcement, curr_offset, m_consensusID,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);
    LOG_GENERAL(INFO, "DEBUG: consensus id is " << m_consensusID);

    // 32-byte blockhash
    announcement.insert(announcement.begin() + curr_offset, m_blockHash.begin(),
                        m_blockHash.end());
    curr_offset += m_blockHash.size();

    // 2-byte leader id
    Serializable::SetNumber<uint16_t>(announcement, curr_offset, m_myID,
                                      sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);
    LOG_GENERAL(INFO, "DEBUG: consensus leader id is " << m_myID);

    // message
    announcement.insert(announcement.begin() + curr_offset, message.begin(),
                        message.end());
    curr_offset += message.size();

    // 4-byte length to co-sign
    Serializable::SetNumber<uint32_t>(announcement, curr_offset, lengthToCosign,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // 64-byte signature
    Signature signature = SignMessage(
        announcement, MessageOffset::BODY + sizeof(unsigned char),
        curr_offset - MessageOffset::BODY - sizeof(unsigned char));
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Message signing failed");
        SetState(ERROR);
        return false;
    }
    signature.Serialize(announcement, curr_offset);

    // Update internal state
    // =====================

    SetState(ANNOUNCE_DONE);
    m_commitCounter = 0;
    m_commitFailureCounter = 0;
    m_responseCounter = 0;
    m_message = message;
    m_lengthToCosign = lengthToCosign;

    // Multicast to all nodes in the committee
    // =======================================

    P2PComm::GetInstance().SendMessage(m_peerInfo, announcement);

    // Start timer for accepting commits
    // =================================
    unsigned int wait_time = COMMIT_WINDOW_IN_SECONDS;
    auto func = [this, wait_time]() -> void {
        this_thread::sleep_for(chrono::seconds(wait_time));
        {
            lock_guard<mutex> g(m_mutex);
            SetState(COMMIT_TIMER_EXPIRED);
        }
        LOG_GENERAL(INFO, "Commit window closed");
        if (m_commitCounter < m_numForConsensus)
        {
            LOG_GENERAL(
                WARNING,
                "Insufficient commits obtained after timeout. Required = "
                    << m_numForConsensus << " Actual = " << m_commitCounter);
            SetState(ERROR);
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Sufficient commits obtained after timeout. Required = "
                            << m_numForConsensus
                            << " Actual = " << m_commitCounter);
            GenerateConsensusSubsets();
            StartConsensusSubsets();
        }
    };
    DetachedFunction(1, func);

    return true;
}

bool ConsensusLeader::ProcessMessage(const vector<unsigned char>& message,
                                     unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    // Incoming message format (from offset): [1-byte consensus message type] [consensus message]

    bool result = false;

    switch (message.at(offset))
    {
    case ConsensusMessageType::COMMIT:
        result = ProcessMessageCommit(message, offset + 1);
        break;
    case ConsensusMessageType::COMMITFAILURE:
        result = ProcessMessageCommitFailure(message, offset + 1, from);
        break;
    case ConsensusMessageType::RESPONSE:
        result = ProcessMessageResponse(message, offset + 1);
        break;
    case ConsensusMessageType::FINALCOMMIT:
        result = ProcessMessageFinalCommit(message, offset + 1);
        break;
    case ConsensusMessageType::FINALRESPONSE:
        result = ProcessMessageFinalResponse(message, offset + 1);
        break;
    default:
        LOG_GENERAL(WARNING,
                    "Unknown consensus message received. No: "
                        << (unsigned int)message.at(offset));
    }

    return result;
}

ConsensusCommon::State ConsensusLeader::GetState() const
{
    State result = INITIAL;

    if ((m_state < CHALLENGE_DONE) || (m_state >= DONE))
    {
        result = m_state;
    }
    else
    {
        for (auto& subset : m_consensusSubsets)
        {
            if (subset.m_state > result)
            {
                result = subset.m_state;
            }
        }
    }

    return result;
}

std::ostream& operator<<(std::ostream& out, const ConsensusLeader::Action value)
{
    const char* s = 0;
#define PROCESS_VAL(p)                                                         \
    case (ConsensusLeader::p):                                                 \
        s = #p;                                                                \
        break;
    switch (value)
    {
        PROCESS_VAL(SEND_ANNOUNCEMENT);
        PROCESS_VAL(PROCESS_COMMITFAILURE);
        PROCESS_VAL(PROCESS_COMMIT);
        PROCESS_VAL(PROCESS_RESPONSE);
        PROCESS_VAL(PROCESS_FINALCOMMIT);
        PROCESS_VAL(PROCESS_FINALRESPONSE);
    }
#undef PROCESS_VAL

    return out << s;
}
