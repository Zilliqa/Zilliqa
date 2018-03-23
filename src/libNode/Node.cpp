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
#include <chrono>
#include <functional>
#include <thread>

#include <boost/multiprecision/cpp_int.hpp>

#include "Node.h"
#include "common/Serializable.h"
#include "common/Messages.h"
#include "common/Constants.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libPersistence/Retriever.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

Node::Node(Mediator & mediator, bool toRetrieveHistory) : m_mediator(mediator)
{
    // m_state = IDLE;
    bool runInitializeGenesisBlocks = true;

    if(toRetrieveHistory)
    {
        if(StartRetrieveHistory())
        {
            m_consensusID = 0;
            m_consensusLeaderID = 0;
            runInitializeGenesisBlocks = false;
        }
        else
        {
            LOG_MESSAGE("FAIL: RetrieveHistory Failed");
        }
    }
    
    if(runInitializeGenesisBlocks)
    {
        // Zilliqa first epoch start from 1 not 0. So for the first DS epoch, there will be 1 less mini epoch only for the first DS epoch. 
        // Hence, we have to set consensusID for first epoch to 1. 
        m_consensusID = 1;
        m_consensusLeaderID = 1;

        m_mediator.m_dsBlockChain.Reset();
        m_mediator.m_txBlockChain.Reset();
        m_committedTransactions.clear();
        AccountStore::GetInstance().Init();
        
        m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain, m_mediator.m_txBlockChain);
    }
    
    m_mediator.m_currentEpochNum = (uint64_t) m_mediator.m_txBlockChain.GetBlockCount();
    m_mediator.UpdateDSBlockRand(runInitializeGenesisBlocks);
    m_mediator.UpdateTxBlockRand(runInitializeGenesisBlocks);
    SetState(POW1_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient((uint64_t)m_mediator.m_dsBlockChain.GetBlockCount());
}

Node::~Node()
{

}

bool Node::StartRetrieveHistory()
{
    LOG_MARKER();
    Retriever* retriever = new Retriever(m_mediator);
    
    bool ds_result;
    std::thread tDS(&Retriever::RetrieveDSBlocks, retriever, std::ref(ds_result));
    // retriever->RetrieveDSBlocks(ds_result);

    bool tx_result;
    std::thread tTx(&Retriever::RetrieveTxBlocks, retriever, std::ref(tx_result));
    // retriever->RetrieveTxBlocks(tx_result);

    bool st_result = retriever->RetrieveStates();

    tDS.join();
    tTx.join();
    bool res = false;
    if(st_result && ds_result && tx_result)
    {
        if(retriever->ValidateStates() && retriever->CleanExtraTxBodies())
        {
            LOG_MESSAGE("RetrieveHistory Successed");
            m_mediator.m_isRetrievedHistory = true;
            res = true;
        }
    }
    delete retriever;
    return res;
}

#ifndef IS_LOOKUP_NODE

void Node::StartSynchronization()
{
    m_isNewNode = true;
    auto func = [this]() -> void
    {
        while (!m_mediator.m_isConnectedToNetwork)
        {
            m_synchronizer.FetchLatestDSBlocks(m_mediator.m_lookup, m_mediator.m_dsBlockChain.GetBlockCount());
            m_synchronizer.FetchDSInfo(m_mediator.m_lookup);
            // m_synchronizer.AttemptPoW(m_mediator.m_lookup);
            m_synchronizer.FetchLatestTxBlocks(m_mediator.m_lookup, m_mediator.m_txBlockChain.GetBlockCount());
            m_synchronizer.FetchLatestState(m_mediator.m_lookup);
            m_synchronizer.AttemptPoW(m_mediator.m_lookup);

            this_thread::sleep_for(chrono::seconds(NEW_NODE_POW2_TIMEOUT_IN_SECONDS));
        }
    };

    DetachedFunction(1, func);
}

#endif //IS_LOOKUP_NODE

bool Node::CheckState(Action action)
{
    if(m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), string("Error: I am a DS node.") +
                string(" Why am I getting this message? Action: ") << ActionString(action));
        return false;
    }

    bool result = true;

    switch(action)
    {
        case STARTPOW1:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in TX_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION_BUFFER:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in TX_SUBMISSION_BUFFER");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in MICROBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in MICROBLOCK_CONSENSUS");
                    result = false;
                    break;
                case WAITING_FINALBLOCK:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW1 but already in WAITING_FINALBLOCK");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case STARTPOW2:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    break;
                case TX_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in TX_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION_BUFFER:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in TX_SUBMISSION_BUFFER");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in MICROBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in MICROBLOCK_CONSENSUS");
                    result = false;
                    break;
                case WAITING_FINALBLOCK:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing STARTPOW2 but already in WAITING_FINALBLOCK");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_SHARDING:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDING but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDING but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION:
                    break;
                case TX_SUBMISSION_BUFFER:
                    break;
                case MICROBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDING but already in MICROBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDING but already in MICROBLOCK_CONSENSUS");
                    result = false;
                    break;
                case WAITING_FINALBLOCK:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDING but already in WAITING_FINALBLOCK");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_MICROBLOCKCONSENSUS:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in TX_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION_BUFFER:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in TX_SUBMISSION_BUFFER");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in MICROBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS:
                    break;
                case WAITING_FINALBLOCK:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKCONSENSUS but already in WAITING_FINALBLOCK");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_FINALBLOCK:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in TX_SUBMISSION");
                    result = false;
                    break;
                case TX_SUBMISSION_BUFFER:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in TX_SUBMISSION_BUFFER");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in MICROBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case MICROBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing WAITING_FINALBLOCK but already in MICROBLOCK_CONSENSUS");
                    result = false;
                    break;
                case WAITING_FINALBLOCK:
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        default:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized action");
            result = false;
            break;
    }

    return result;
}

vector<Peer> Node::GetBroadcastList(unsigned char ins_type, const Peer & broadcast_originator)
{
    LOG_MARKER();

    // // MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION
    // if (ins_type == NodeInstructionType::FORWARDTRANSACTION)
    // {
    //     LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Gossip Forward list:");

    //     vector<Peer> peers;
    //     // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "DS size: " << m_mediator.m_DSCommitteeNetworkInfo.size() << " Shard size: " << m_myShardMembersNetworkInfo.size());

    //     if (m_isDSNode)
    //     {
    //         lock_guard<mutex> g(m_mutexFinalBlockProcessing);
    //         // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I'm a DS node. DS size: " << m_mediator.m_DSCommitteeNetworkInfo.size() << " rand: " << rand() % m_mediator.m_DSCommitteeNetworkInfo.size());
    //         for (unsigned int i = 0; i < m_mediator.m_DSCommitteeNetworkInfo.size(); i++)
    //         {
    //             if (i == m_consensusMyID)
    //             {
    //                 continue;
    //             }
    //             if (rand() % m_mediator.m_DSCommitteeNetworkInfo.size() <= GOSSIP_RATE)
    //             {
    //                 peers.push_back(m_mediator.m_DSCommitteeNetworkInfo.at(i));
    //                 LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "DSNode  IP: " << peers.back().GetPrintableIPAddress() << " Port: " << peers.back().m_listenPortHost);

    //             }

    //         }
    //     }
    //     else
    //     {
    //         // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I'm a shard node. Shard size: " << m_myShardMembersNetworkInfo.size() << " rand: " << rand() % m_myShardMembersNetworkInfo.size());
    //         lock_guard<mutex> g(m_mutexFinalBlockProcessing);
    //         for (unsigned int i = 0; i < m_myShardMembersNetworkInfo.size(); i++)
    //         {
    //             if (i == m_consensusMyID)
    //             {
    //                 continue;
    //             }
    //             if (rand() % m_myShardMembersNetworkInfo.size() <= GOSSIP_RATE)
    //             {
    //                 peers.push_back(m_myShardMembersNetworkInfo.at(i));
    //                 LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "  IP: " << peers.back().GetPrintableIPAddress() << " Port: " << peers.back().m_listenPortHost);

    //             }

    //         }
    //     }


    //     return peers;
    // }

    // Regardless of the instruction type, right now all our "broadcasts" are just redundant multicasts from DS nodes to non-DS nodes
    return vector<Peer>();
}

#ifndef IS_LOOKUP_NODE
bool Node::CheckCreatedTransaction(const Transaction & tx)
{
    LOG_MARKER();

    // Check if from account is sharded here
    const PubKey & senderPubKey = tx.GetSenderPubKey();
    Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int correct_shard = Transaction::GetShardIndex(fromAddr, m_numShards);

    if (correct_shard != m_myShardID)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: This tx is not sharded to me!");
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "From Account  = 0x" << fromAddr);
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Correct shard = " << correct_shard);
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "This shard    = " << m_myShardID);
        return false;
    }

    // Check if from account exists in local storage
    if (!AccountStore::GetInstance().DoesAccountExist(fromAddr))
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "To-do: What to do if from account is not in my account store?");
        throw exception();
    }

    // Check from account nonce
    if (tx.GetNonce() != AccountStore::GetInstance().GetNonce(fromAddr) + 1)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: Tx nonce not in line with account state!");
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "From Account      = 0x" << fromAddr);
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Account Nonce     = " << AccountStore::GetInstance().GetNonce(fromAddr));
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Expected Tx Nonce = " << AccountStore::GetInstance().GetNonce(fromAddr) + 1);
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Actual Tx Nonce   = " << tx.GetNonce());
        return false;
    }

    // Check if to account exists in local storage
    const Address & toAddr = tx.GetToAddr();
    if (!AccountStore::GetInstance().DoesAccountExist(toAddr))
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "To-do: What to do if to account is not in my account store?");
        throw exception();
    }

    // Check if transaction amount is valid
    if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount())
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Error: Insufficient funds in source account!");
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "From Account = 0x" << fromAddr);
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Balance      = " << AccountStore::GetInstance().GetBalance(fromAddr));
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Debit Amount = " << tx.GetAmount());
        return false;
    }
    
    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessCreateTransaction(const vector<unsigned char> & message, unsigned int offset, 
                                    const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // This message is sent by the test script and is used to generate a new transaction for submitting to the network
    // Message = [33-byte from pubkey] [33-byte to pubkey] [32-byte amount]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE))
    {
        return false;
    }

    unsigned int cur_offset = offset;

    // To-do: Put in the checks (e.g., are these pubkeys known, is the amount good, etc)

    // 33-byte from pubkey
    PubKey fromPubKey(message, cur_offset);

    // TODO: remove this
    fromPubKey = m_mediator.m_selfKey.second;
    vector<unsigned char> msg;
    fromPubKey.Serialize(msg, 0);

    // Generate from account
    Address fromAddr;
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    // TODO: replace this by what follows
    sha2.Update(msg, 0, PUB_KEY_SIZE);
    // sha2.Update(message, cur_offset, PUB_KEY_SIZE);
    const vector<unsigned char> & tmp1 = sha2.Finalize();
    copy(tmp1.end() - ACC_ADDR_SIZE, tmp1.end(), fromAddr.asArray().begin());

    cur_offset += PUB_KEY_SIZE;

    // 33-byte to pubkey
    PubKey toPubkey(message, cur_offset);

    // Generate to account
    Address toAddr;
    sha2.Reset();
    sha2.Update(message, cur_offset, PUB_KEY_SIZE);
    // const vector<unsigned char> & tmp2 = sha2.Finalize();
    // copy(tmp2.end() - ACC_ADDR_SIZE, tmp2.end(), toAddr.asArray().begin());

    cur_offset += PUB_KEY_SIZE;

    // 32-byte amount
    uint256_t amount = Serializable::GetNumber<uint256_t>(message, cur_offset, UINT256_SIZE);

    // Create the transaction object

    // To-do: Replace dummy values with the required ones
    //uint32_t version = 0;
    uint32_t version = (uint32_t) m_consensusMyID; //hack
    uint256_t nonce = 0;

    array<unsigned char, TRAN_SIG_SIZE> signature;
    fill(signature.begin(), signature.end(), 0x0F);

    lock_guard<mutex> g(m_mutexCreatedTransactions);

    // if(!CheckCreatedTransaction(txn))
    // {
    //     return false;
    // }

    // TODO: Remove this before production. This is to reduce time spent on aws testnet. 
    for (unsigned i=0; i < 10000; i++)
    {
        Transaction txn(version, nonce, toAddr, fromPubKey, amount, signature);
        // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     // "Created txns: " << txn.GetTranID())
        // LOG_MESSAGE(txn.GetSerializedSize());
        m_createdTransactions.push_back(txn);
        nonce++;
        amount++;
    }
#endif // IS_LOOKUP_NODE
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::ProcessSubmitMissingTxn(const vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from)
{
    auto msgBlockNum = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    auto localBlockNum = (uint256_t) m_mediator.m_currentEpochNum;;

    if (msgBlockNum != localBlockNum)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "untimely delivery of " <<
                     "missing txns. received: " << msgBlockNum << " , local: " << localBlockNum);
    }

    const auto & submittedTransaction = Transaction(message, offset);

    // if(CheckCreatedTransaction(submittedTransaction))
    // {
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        auto & receivedTransactions = m_receivedTransactions[msgBlockNum];
        receivedTransactions.insert(make_pair(submittedTransaction.GetTranID(), 
                                              submittedTransaction));
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                     "Received missing txn: " << submittedTransaction.GetTranID())
    // }
    return true;
}

bool Node::ProcessSubmitTxnSharing(const vector<unsigned char> & message, unsigned int offset, 
                                   const Peer & from)
{
    const auto & submittedTransaction = Transaction(message, offset);
    // if(CheckCreatedTransaction(submittedTransaction))
    // {
        boost::multiprecision::uint256_t blockNum = (uint256_t) m_mediator.m_currentEpochNum;
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        auto & receivedTransactions = m_receivedTransactions[blockNum];
        // if(m_mediator.m_selfPeer.m_listenPortHost != 5015 &&
        //    m_mediator.m_selfPeer.m_listenPortHost != 5016 &&
        //    m_mediator.m_selfPeer.m_listenPortHost != 5017 &&
        //    m_mediator.m_selfPeer.m_listenPortHost != 5018 &&
        //    m_mediator.m_selfPeer.m_listenPortHost != 5019 &&
        //    m_mediator.m_selfPeer.m_listenPortHost != 5020)
        // { 
            receivedTransactions.insert(make_pair(submittedTransaction.GetTranID(), 
                                                  submittedTransaction));
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "Received txn: " << submittedTransaction.GetTranID())
        // }
    // }

    return true;        
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessSubmitTransaction(const vector<unsigned char> & message, unsigned int offset, 
                                    const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // This message is sent by my shard peers
    // Message = [204-byte transaction]

    LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset, Transaction::GetSerializedSize()))
    {
        return false;
    }

    unsigned int cur_offset = offset;

    auto submitTxnType = Serializable::GetNumber<uint32_t>(message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t); 

    if (submitTxnType == SUBMITTRANSACTIONTYPE::MISSINGTXN)
    {
        if (m_state != MICROBLOCK_CONSENSUS)
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "Not in a microblock consensus state: don't want missing txns")   
        }

        ProcessSubmitMissingTxn(message, cur_offset, from);
    }
    else if (submitTxnType == SUBMITTRANSACTIONTYPE::TXNSHARING)
    {
        while (m_state != TX_SUBMISSION && m_state != TX_SUBMISSION_BUFFER)
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                         "Not in ProcessSubmitTxn state -- waiting!")
            this_thread::sleep_for(chrono::milliseconds(200));
        }

        ProcessSubmitTxnSharing(message, cur_offset, from);
    }
#endif // IS_LOOKUP_NODE
    return true;
}

bool Node::ProcessCreateTransactionFromLookup(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
#ifndef IS_LOOKUP_NODE

    LOG_MARKER();

    if(IsMessageSizeInappropriate(message.size(), offset, Transaction::GetSerializedSize()))
    {
        return false;
    }

    unsigned int curr_offset = offset;

    Transaction tx(message, curr_offset);

    lock_guard<mutex> g(m_mutexCreatedTransactions);

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),"Recvd txns: "<<tx.GetTranID()<<" Signature: "<<DataConversion::charArrToHexStr(tx.GetSignature())<<" toAddr: "<<tx.GetToAddr().hex());

    m_createdTransactions.push_back(tx);

#endif //IS_LOOKUP_NODE

    return true;

}

// Used by Zilliqa in pow branch. This will be useful for us when doing the accounts and wallet in the future.
// bool Node::ProcessCreateAccounts(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
// {
// #ifndef IS_LOOKUP_NODE
//     // Message = [117-byte account 1] ... [117-byte account n]

//     LOG_MARKER();

//     if (IsMessageSizeInappropriate(message.size(), offset, 0, ACCOUNT_SIZE))
//     {
//         return false;
//     }

//     const unsigned int numOfAccounts = (message.size() - offset) / ACCOUNT_SIZE;
//     unsigned int cur_offset = offset;

//     for (unsigned int i = 0; i < numOfAccounts; i++)
//     {
//         AccountStore::GetInstance().AddAccount(Account(message, cur_offset));
//         cur_offset += ACCOUNT_SIZE;
//     }
// #endif // IS_LOOKUP_NODE
//     // Do any post-processing on the final block
//     return true;
// }

void Node::SetState(NodeState state)
{
    m_state = state;
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Node State is now " << m_state <<
                 " at epoch " << m_mediator.m_currentEpochNum);
}

#ifndef IS_LOOKUP_NODE
void Node::SubmitTransactions()
{
    LOG_MARKER();

    // TODO: This is a manual trottle of txns rate for stability testing.
    //uint64_t upper_id_limit = m_mediator.m_currentEpochNum * 20 + 20;
    //uint64_t lower_id_limit = m_mediator.m_currentEpochNum * 20;
    uint64_t upper_id_limit = 600;
    uint64_t lower_id_limit = 0;

    unsigned int txn_sent_count = 0;

    if (m_consensusMyID >= lower_id_limit && m_consensusMyID <= upper_id_limit)
    {
        boost::multiprecision::uint256_t blockNum = (uint256_t) m_mediator.m_currentEpochNum;
        while (true && txn_sent_count < 4) 
        // TODO: remove the condition on txn_sent_count -- temporary hack to artificially limit number of
        // txns needed to be shared within shard members so that it completes in the time limit    
        {
            shared_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);
            if(m_state != TX_SUBMISSION)
            {
                break;
            }
            
            Transaction t;

            bool found = false;

            {
                lock_guard<mutex> g(m_mutexCreatedTransactions);
                found = (m_createdTransactions.size() > 0);
                if (found)
                {
                    t = move(m_createdTransactions.front());
                    m_createdTransactions.pop_front();
                }
            }

            if (found)
            {
                vector<unsigned char> tx_message = { MessageType::NODE, 
                                                     NodeInstructionType::SUBMITTRANSACTION };
                Serializable::SetNumber<uint32_t>(tx_message, MessageOffset::BODY,
                                                  SUBMITTRANSACTIONTYPE::TXNSHARING, 
                                                  sizeof(uint32_t));
                t.Serialize(tx_message, MessageOffset::BODY + sizeof(uint32_t));
                P2PComm::GetInstance().SendMessage(m_myShardMembersNetworkInfo, tx_message);

                LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), 
                             "Sent txn: " << t.GetTranID())

                lock_guard<mutex> g(m_mutexSubmittedTransactions);
                auto & submittedTransactions = m_submittedTransactions[blockNum];
                submittedTransactions.insert(make_pair(t.GetTranID(), t));
                txn_sent_count++; 
            }
        }
    }

#ifdef STAT_TEST
    LOG_STATE("[TXNSE][" << std::setw(15) << std::left << 
              m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" << 
              m_mediator.m_currentEpochNum << "][" << m_myShardID << "][" << txn_sent_count << 
              "] CONT");
#endif // STAT_TEST
}
#endif // IS_LOOKUP_NODE

bool Node::Execute(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
    LOG_MARKER();

    bool result = true; 

    typedef bool(Node::*InstructionHandler)(const vector<unsigned char> &, 
                                            unsigned int, const Peer &);
    
    InstructionHandler ins_handlers[] =
    {
        &Node::ProcessStartPoW1,
        &Node::ProcessDSBlock,
        &Node::ProcessSharding,
        &Node::ProcessCreateTransaction,
        &Node::ProcessSubmitTransaction,
        &Node::ProcessMicroblockConsensus,
        &Node::ProcessFinalBlock,
        &Node::ProcessForwardTransaction,
        &Node::ProcessCreateTransactionFromLookup
    };

    const unsigned char ins_byte = message.at(offset);
    const unsigned int ins_handlers_count = sizeof(ins_handlers) / sizeof(InstructionHandler);

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
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Unknown instruction byte " << 
                     hex << (unsigned int)ins_byte);
    }

    return result;
}
