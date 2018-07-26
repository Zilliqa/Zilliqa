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

bool ConsensusLeader::CheckState(Action action)
{
    static const std::multimap<ConsensusCommon::State, Action> ACTIONS_FOR_STATE
        = {{INITIAL, SEND_ANNOUNCEMENT},
           {INITIAL, PROCESS_COMMITFAILURE},
           {ANNOUNCE_DONE, PROCESS_COMMIT},
           {ANNOUNCE_DONE, PROCESS_COMMITFAILURE},
           {CHALLENGE_DONE, PROCESS_RESPONSE},
           {CHALLENGE_DONE, PROCESS_COMMITFAILURE},
           {COLLECTIVESIG_DONE, PROCESS_FINALCOMMIT},
           {COLLECTIVESIG_DONE, PROCESS_COMMITFAILURE},
           {FINALCHALLENGE_DONE, PROCESS_FINALRESPONSE},
           {FINALCHALLENGE_DONE, PROCESS_COMMITFAILURE},
           {DONE, PROCESS_COMMITFAILURE}};

    bool found = false;

    for (auto pos = ACTIONS_FOR_STATE.lower_bound(m_state);
         pos != ACTIONS_FOR_STATE.upper_bound(m_state); pos++)
    {
        if (pos->second == action)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        LOG_GENERAL(WARNING,
                    "Action " << GetActionString(action)
                              << " not allowed in state " << GetStateString());
        return false;
    }

    return true;
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

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [33-byte commit] [64-byte signature]

    const unsigned int length_available = commit.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + COMMIT_POINT_SIZE + SIGNATURE_CHALLENGE_SIZE
        + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        commit, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in commitment ("
                        << consensus_id
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
    uint16_t backup_id = Serializable::GetNumber<uint16_t>(commit, curr_offset,
                                                           sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the backup id
    if (backup_id >= m_commitMap.size())
    {
        LOG_GENERAL(WARNING, "Backup ID beyond backup count");
        return false;
    }
    if (m_commitMap.at(backup_id) == true)
    {
        LOG_GENERAL(WARNING, "Backup has already sent validated commit");
        return false;
    }

    // 33-byte commit - skip for now, deserialize later below
    curr_offset += COMMIT_POINT_SIZE;

    // 64-byte signature
    // Signature signature(commit, curr_offset);
    Signature signature;
    if (signature.Deserialize(commit, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize signature.");
        return false;
    }

    // Check the signature
    bool sig_valid = VerifyMessage(commit, offset, curr_offset - offset,
                                   signature, backup_id);
    if (sig_valid == false)
    {
        LOG_GENERAL(WARNING, "Invalid signature in commit message");
        return false;
    }

    bool result = false;
    {
        // Update internal state
        // =====================
        lock_guard<mutex> g(m_mutex);

        if (!CheckState(action))
        {
            return false;
        }

        // 33-byte commit
        if (m_commitCounter < m_numForConsensus)
        {
            m_commitPoints.emplace_back(commit,
                                        curr_offset - COMMIT_POINT_SIZE);
            m_commitPointMap.at(backup_id)
                = CommitPoint(commit, curr_offset - COMMIT_POINT_SIZE);
            m_commitMap.at(backup_id) = true;
        }
        m_commitCounter++;

        if (m_commitCounter % 10 == 0)
        {
            LOG_GENERAL(INFO,
                        "Received " << m_commitCounter << " out of "
                                    << m_numForConsensus << ".");
        }

        // Generate challenge if sufficient commits have been obtained
        // ===========================================================

        if (m_commitCounter == m_numForConsensus)
        {
            LOG_GENERAL(INFO,
                        "Sufficient " << m_numForConsensus
                                      << " commits obtained");

            vector<unsigned char> challenge
                = {m_classByte, m_insByte,
                   static_cast<unsigned char>(returnmsgtype)};
            result = GenerateChallengeMessage(
                challenge, MessageOffset::BODY + sizeof(unsigned char));
            if (result == true)
            {
                // Update internal state
                // =====================

                m_state = nextstate;

                // Add the leader to the responses
                Response r(*m_commitSecret, m_challenge, m_myPrivKey);
                m_responseData.emplace_back(r);
                m_responseDataMap.at(m_myID) = r;
                m_responseMap.at(m_myID) = true;
                m_responseCounter = 1;

                // Multicast to all nodes who send validated commits
                // =================================================

                vector<Peer> commit_peers;
                deque<pair<PubKey, Peer>>::const_iterator j
                    = m_committee.begin();

                for (unsigned int i = 0; i < m_commitMap.size(); i++, j++)
                {
                    if ((m_commitMap.at(i) == true) && (i != m_myID))
                    {
                        commit_peers.emplace_back(j->second);
                    }
                }

                P2PComm::GetInstance().SendMessage(commit_peers, challenge);
            }
        }

        // Redundant commits
        if (m_commitCounter > m_numForConsensus)
        {
            m_commitRedundantPointMap.at(backup_id)
                = CommitPoint(commit, curr_offset - COMMIT_POINT_SIZE);
            m_commitRedundantMap.at(backup_id) = true;
            m_commitRedundantCounter++;
        }
    }
#if 0
    if (m_commitCounter == m_numForConsensus)
    {
        // Set a timer for collecting responses
        // auto main_func = []() -> void {
        //     LOG_GENERAL(INFO, "Timer for collecting responses is triggered");
        // };
        // auto check_response_func = [this]() mutable -> void {
        //     if (m_responseCounter < m_numForConsensus)
        //     {
        //         LOG_GENERAL(INFO, "# responses not reaches the threshold");
        //     }
        //     LOG_GENERAL(INFO, "# responses reaches the threshold");
        // };
        // TimeLockedFunction tlf(1, main_func, check_response_func, true);

        LOG_GENERAL(INFO, "Process the threshold");

        for (unsigned int i = 0; i < 6000; i++)
        {
            this_thread::sleep_for(chrono::milliseconds(1000));

            if (a_State > nextstate)
            {
                LOG_GENERAL(INFO, "# responses reaches the threshold");
                break;
            }
        }

        if (a_State == nextstate)
        {
            if (m_responseCounter < m_numForConsensus)
            {
                LOG_GENERAL(INFO, "# responses does not reach the threshold");
                LOG_GENERAL(INFO, "Insufficient responses obtained");

                // Update internal state
                // =====================
                lock_guard<mutex> g(m_mutex);

                if (m_commitRedundantCounter <= 0)
                {
                    LOG_GENERAL(INFO, "No redundant commit messages");
                    return false;
                }

                // Regenerate challenge
                m_commitCounter = 0;
                m_commitPoints.clear();

                m_responseCounter = 0;
                m_responseData.clear();
                fill(m_responseMap.begin(), m_responseMap.end(), false);

                for (unsigned int i = 0; i < m_commitMap.size(); i++)
                {
                    if ((m_commitMap.at(i) == true) && (m_responseMap.at(i) == true))
                    {
                        m_commitPoints.emplace_back(m_commitPointMap.at(i));
                        m_commitCounter++;
                    }
                    if ((m_commitMap.at(i) == true) && (m_responseMap.at(i) == false))
                    {
                        // Put node i into the blacklist
                        LOG_GENERAL(INFO, "Peer " << i << " is malicious");
                        m_commitMap.at(i) = false;
                    }
                }
                for (unsigned int i = 0; i < m_commitRedundantMap.size(); i++)
                {
                    if (m_commitCounter < m_numForConsensus)
                    {
                        if (m_commitRedundantMap.at(i) == true)
                        {
                            m_commitPoints.emplace_back(m_commitRedundantPointMap.at(i));
                            m_commitCounter++;
                            m_commitMap.at(i) = true;
                            m_commitRedundantMap.at(i) = false;
                            m_commitRedundantCounter--;
                        }
                    }
                }

                if (m_commitCounter < m_numForConsensus)
                {
                    LOG_GENERAL(INFO, "No sufficient redundant commit messages");
                    return false;
                }

                if (m_commitCounter == m_numForConsensus)
                {
                    LOG_GENERAL(INFO, "Sufficient redundant commit messages");
                    // result = true;
                    vector<unsigned char> challenge = { m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype) };
                    result = GenerateChallengeMessage(challenge, MessageOffset::BODY + sizeof(unsigned char));
                    if (result == true)
                    {
                        // Update internal state
                        // =====================

                        a_State = nextstate;

                        // Multicast to all nodes who send validated commits
                        // =================================================

                        vector<Peer> commit_peers;
                        for (unsigned int i = 0; i < m_commitMap.size(); i++)
                        {
                            if (m_commitMap.at(i) == true)
                            {
                                commit_peers.emplace_back(m_peerInfo.at(i));
                            }
                        }
                        P2PComm::GetInstance().SendMessage(commit_peers, challenge);
                    }
                }
            }
        }
    }
#endif
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
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        commitFailureMsg, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in commitment ("
                        << consensus_id << ") does not match "
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
    uint16_t backup_id = Serializable::GetNumber<uint16_t>(
        commitFailureMsg, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the backup id
    if (backup_id
        >= m_commitMap
               .size()) // using commitMap instead of commitFailureMap knowingly since its size = puKeys.size() for sure
    {
        LOG_GENERAL(WARNING, "Backup ID beyond backup count");
        return false;
    }

    if (m_commitFailureMap.find(backup_id) != m_commitFailureMap.end())
    {
        LOG_GENERAL(WARNING, "Backup has already sent commit failure message");
        return false;
    }

    m_commitFailureCounter++;
    m_commitFailureMap[backup_id] = vector<unsigned char>(
        commitFailureMsg.begin() + curr_offset, commitFailureMsg.end());

    m_nodeCommitFailureHandlerFunc(commitFailureMsg, curr_offset, from);

    if (m_commitFailureCounter == m_numForConsensusFailure)
    {
        m_state = INITIAL;

        vector<unsigned char> consensusFailureMsg
            = {m_classByte, m_insByte, CONSENSUSFAILURE};
        deque<Peer> peerInfo;

        for (auto const& i : m_committee)
        {
            peerInfo.push_back(i.second);
        }

        P2PComm::GetInstance().SendMessage(peerInfo, consensusFailureMsg);
        auto main_func = [this]() mutable -> void {
            m_shardCommitFailureHandlerFunc(m_commitFailureMap);
        };
        DetachedFunction(1, main_func);
        // LOG_GENERAL(INFO, "Sufficient " << m_numForConsensus << " commits obtained");

        // vector<unsigned char> challenge = { m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype) };
        // result = GenerateChallengeMessage(challenge, MessageOffset::BODY + sizeof(unsigned char));
        // if (result == true)
        // {
        //     // Update internal state
        //     // =====================

        //     m_state = nextstate;

        //     // Multicast to all nodes who send validated commits
        //     // =================================================

        //     vector<Peer> commit_peers;
        //     deque<Peer>::const_iterator j = m_peerInfo.begin();
        //     for (unsigned int i = 0; i < m_commitMap.size(); i++, j++)
        //     {
        //         if (m_commitMap.at(i) == true)
        //         {
        //             commit_peers.emplace_back(*j);
        //         }
        //     }
        //     P2PComm::GetInstance().SendMessage(commit_peers, challenge);
        // }
    }

    return true;
}

bool ConsensusLeader::GenerateChallengeMessage(vector<unsigned char>& challenge,
                                               unsigned int offset)
{
    LOG_MARKER();

    // Generate challenge object
    // =========================

    // Aggregate commits
    CommitPoint aggregated_commit = AggregateCommits(m_commitPoints);
    if (aggregated_commit.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "AggregateCommits failed");
        m_state = ERROR;
        return false;
    }

    // Aggregate keys
    PubKey aggregated_key = AggregateKeys(m_commitMap);
    if (aggregated_key.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
        m_state = ERROR;
        return false;
    }

    // Generate the challenge
    m_challenge = GetChallenge(m_message, 0, m_lengthToCosign,
                               aggregated_commit, aggregated_key);
    if (m_challenge.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Challenge generation failed");
        m_state = ERROR;
        return false;
    }

    // Assemble challenge message body
    // ===============================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [33-byte aggregated commit] [33-byte aggregated key] [32-byte challenge]

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

    // 33-byte aggregated commit
    aggregated_commit.Serialize(challenge, curr_offset);
    curr_offset += COMMIT_POINT_SIZE;

    // 33-byte aggregated key
    aggregated_key.Serialize(challenge, curr_offset);
    curr_offset += PUB_KEY_SIZE;

    // 32-byte challenge
    m_challenge.Serialize(challenge, curr_offset);
    curr_offset += CHALLENGE_SIZE;

    // 64-byte signature
    Signature signature = SignMessage(challenge, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Message signing failed");
        m_state = ERROR;
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

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte backup id] [32-byte response] [64-byte signature]

    const unsigned int length_available = response.size() - offset;
    const unsigned int length_needed = sizeof(uint32_t) + BLOCK_HASH_SIZE
        + sizeof(uint16_t) + RESPONSE_SIZE + SIGNATURE_CHALLENGE_SIZE
        + SIGNATURE_RESPONSE_SIZE;

    if (length_needed > length_available)
    {
        LOG_GENERAL(WARNING, "Malformed message");
        return false;
    }

    unsigned int curr_offset = offset;

    // 4-byte consensus id
    uint32_t consensus_id = Serializable::GetNumber<uint32_t>(
        response, curr_offset, sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    // Check the consensus id
    if (consensus_id != m_consensusID)
    {
        LOG_GENERAL(WARNING,
                    "Consensus ID in response ("
                        << consensus_id
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
    uint32_t backup_id = Serializable::GetNumber<uint16_t>(
        response, curr_offset, sizeof(uint16_t));
    curr_offset += sizeof(uint16_t);

    // Check the backup id
    if (backup_id >= m_responseMap.size())
    {
        LOG_GENERAL(WARNING, "Backup ID beyond backup count");
        return false;
    }
    if (m_commitMap.at(backup_id) == false)
    {
        LOG_GENERAL(WARNING, "Backup has not participated in the commit phase");
        return false;
    }
    if (m_responseMap.at(backup_id) == true)
    {
        LOG_GENERAL(WARNING, "Backup has already sent validated response");
        return false;
    }

    // 32-byte response
    Response tmp_response = Response(response, curr_offset);
    curr_offset += RESPONSE_SIZE;

    if (MultiSig::VerifyResponse(tmp_response, m_challenge,
                                 m_committee.at(backup_id).first,
                                 m_commitPointMap.at(backup_id))
        == false)
    {
        LOG_GENERAL(WARNING, "Invalid response for this backup");
        return false;
    }

    // 64-byte signature
    // Signature signature(response, curr_offset);
    Signature signature;
    if (signature.Deserialize(response, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize signature.");
        return false;
    }

    // Check the signature
    bool sig_valid = VerifyMessage(response, offset, curr_offset - offset,
                                   signature, backup_id);
    if (sig_valid == false)
    {
        LOG_GENERAL(WARNING, "Invalid signature in response message");
        return false;
    }

    // Update internal state
    // =====================

    lock_guard<mutex> g(m_mutex);

    if (!CheckState(action))
    {
        return false;
    }

    // 32-byte response
    m_responseData.emplace_back(tmp_response);
    m_responseDataMap.at(backup_id) = tmp_response;
    m_responseMap.at(backup_id) = true;
    m_responseCounter++;

    // Generate collective sig if sufficient responses have been obtained
    // ==================================================================

    bool result = true;

    if (m_responseCounter == m_numForConsensus)
    {
        LOG_GENERAL(INFO, "Sufficient responses obtained");

        vector<unsigned char> collectivesig = {
            m_classByte, m_insByte, static_cast<unsigned char>(returnmsgtype)};
        result = GenerateCollectiveSigMessage(
            collectivesig, MessageOffset::BODY + sizeof(unsigned char));

        if (result == true)
        {
            // Update internal state
            // =====================

            m_state = nextstate;

            if (action == PROCESS_RESPONSE)
            {
                // First round: consensus over part of message (e.g., DS block header)
                // Second round: consensus over part of message + CS1 + B1
                m_message.resize(m_lengthToCosign);
                m_collectiveSig.Serialize(m_message, m_lengthToCosign);
                BitVector::SetBitVector(m_message,
                                        m_lengthToCosign + BLOCK_SIG_SIZE,
                                        m_responseMap);
                m_lengthToCosign = m_message.size();

                // Save the collective sig over the first round
                m_CS1 = m_collectiveSig;
                m_B1 = m_responseMap;

                m_commitPoints.clear();
                fill(m_commitMap.begin(), m_commitMap.end(), false);

                // Add the leader to the commits
                m_commitMap.at(m_myID) = true;
                m_commitPoints.emplace_back(*m_commitPoint);
                m_commitPointMap.at(m_myID) = *m_commitPoint;
                m_commitCounter = 1;

                m_commitFailureCounter = 0;
                m_commitFailureMap.clear();

                m_commitRedundantCounter = 0;
                fill(m_commitRedundantMap.begin(), m_commitRedundantMap.end(),
                     false);

                m_responseCounter = 0;
                m_responseData.clear();
                fill(m_responseMap.begin(), m_responseMap.end(), false);
            }
            else
            {
                // Save the collective sig over the second round
                m_CS2 = m_collectiveSig;
                m_B2 = m_responseMap;
            }

            // Multicast to all nodes in the committee
            // =======================================

            // FIXME: quick fix: 0106'08' comes to the backup ealier than 0106'04'
            // if (action == FINALCOMMIT)
            // {
            //     this_thread::sleep_for(chrono::milliseconds(1000));
            // }
            //this_thread::sleep_for(chrono::seconds(CONSENSUS_COSIG_WINDOW));

            deque<Peer> peerInfo;

            for (auto const& i : m_committee)
            {
                peerInfo.push_back(i.second);
            }

            P2PComm::GetInstance().SendMessage(peerInfo, collectivesig);
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
    vector<unsigned char>& collectivesig, unsigned int offset)
{
    LOG_MARKER();

    // Generate collective signature object
    // ====================================

    // Aggregate responses
    Response aggregated_response = AggregateResponses(m_responseData);
    if (aggregated_response.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "AggregateCommits failed");
        m_state = ERROR;
        return false;
    }

    // Aggregate keys
    PubKey aggregated_key = AggregateKeys(m_responseMap);
    if (aggregated_key.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Aggregated key generation failed");
        m_state = ERROR;
        return false;
    }

    // Generate the collective signature
    m_collectiveSig = AggregateSign(m_challenge, aggregated_response);
    if (m_collectiveSig.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Collective sig generation failed");
        m_state = ERROR;
        return false;
    }

    // Verify the collective signature
    if (Schnorr::GetInstance().Verify(m_message, 0, m_lengthToCosign,
                                      m_collectiveSig, aggregated_key)
        == false)
    {
        LOG_GENERAL(WARNING, "Collective sig verification failed");
        m_state = ERROR;

        LOG_GENERAL(INFO,
                    "num of pub keys: " << m_committee.size() << " "
                                        << "num of peer_info keys: "
                                        << m_committee.size());
        return false;
    }

    // Assemble collective signature message body
    // ==========================================

    // Format: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [N-byte bitmap] [64-byte collective signature] [64-byte signature]
    // Signature is over: [4-byte consensus id] [32-byte blockhash] [2-byte leader id] [N-byte bitmap] [64-byte collective signature]
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

    // N-byte bitmap
    curr_offset
        += BitVector::SetBitVector(collectivesig, curr_offset, m_responseMap);

    // 64-byte collective signature
    m_collectiveSig.Serialize(collectivesig, curr_offset);
    curr_offset += SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE;

    // 64-byte signature
    Signature signature
        = SignMessage(collectivesig, offset, curr_offset - offset);
    if (signature.Initialized() == false)
    {
        LOG_GENERAL(WARNING, "Message signing failed");
        m_state = ERROR;
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
    uint32_t consensus_id, const vector<unsigned char>& block_hash,
    uint16_t node_id, const PrivKey& privkey,
    const deque<pair<PubKey, Peer>>& committee, unsigned char class_byte,
    unsigned char ins_byte,
    NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
    ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc)
    : ConsensusCommon(consensus_id, block_hash, node_id, privkey, committee,
                      class_byte, ins_byte)
    , m_commitMap(committee.size(), false)
    , m_commitPointMap(committee.size(), CommitPoint())
    , m_commitRedundantMap(committee.size(), false)
    , m_commitRedundantPointMap(committee.size(), CommitPoint())
    , m_responseDataMap(committee.size(), Response())
{
    LOG_MARKER();

    m_state = INITIAL;
    // m_numForConsensus = (floor(TOLERANCE_FRACTION * (pubkeys.size() - 1)) + 1);
    m_numForConsensus = ConsensusCommon::NumForConsensus(committee.size());
    m_numForConsensusFailure = committee.size() - m_numForConsensus;
    LOG_GENERAL(INFO,
                "TOLERANCE_FRACTION "
                    << TOLERANCE_FRACTION << " pubkeys.size() "
                    << committee.size() << " m_numForConsensus "
                    << m_numForConsensus << " m_numForConsensusFailure "
                    << m_numForConsensusFailure);

    m_nodeCommitFailureHandlerFunc = nodeCommitFailureHandlerFunc;
    m_shardCommitFailureHandlerFunc = shardCommitFailureHandlerFunc;

    m_commitSecret.reset(new CommitSecret());
    m_commitPoint.reset(new CommitPoint(*m_commitSecret));

    // Add the leader to the commits
    m_commitMap.at(m_myID) = true;
    m_commitPoints.emplace_back(*m_commitPoint);
    m_commitPointMap.at(m_myID) = *m_commitPoint;
    m_commitCounter = 1;
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
        LOG_GENERAL(WARNING,
                    "lengthToCosign > message size "
                        << "m_lengthToCosign : " << m_lengthToCosign << " "
                        << "m_message : " << m_message.size());
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
        m_state = ERROR;
        return false;
    }
    signature.Serialize(announcement, curr_offset);

    // Update internal state
    // =====================

    m_state = ANNOUNCE_DONE;
    m_commitRedundantCounter = 0;
    m_commitFailureCounter = 0;
    m_message = message;
    m_lengthToCosign = lengthToCosign;

    // Multicast to all nodes in the committee
    // =======================================

    deque<Peer> peer;

    for (auto const& i : m_committee)
    {
        peer.push_back(i.second);
    }

    P2PComm::GetInstance().SendMessage(peer, announcement);

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

#define MAKE_LITERAL_PAIR(s)                                                   \
    {                                                                          \
        s, #s                                                                  \
    }

map<ConsensusLeader::Action, string> ConsensusLeader::ActionStrings
    = {MAKE_LITERAL_PAIR(SEND_ANNOUNCEMENT),
       MAKE_LITERAL_PAIR(PROCESS_COMMIT),
       MAKE_LITERAL_PAIR(PROCESS_RESPONSE),
       MAKE_LITERAL_PAIR(PROCESS_FINALCOMMIT),
       MAKE_LITERAL_PAIR(PROCESS_FINALRESPONSE),
       MAKE_LITERAL_PAIR(PROCESS_COMMITFAILURE)};

std::string ConsensusLeader::GetActionString(Action action) const
{
    return (ActionStrings.find(action) == ActionStrings.end())
        ? "Unknown"
        : ActionStrings.at(action);
}