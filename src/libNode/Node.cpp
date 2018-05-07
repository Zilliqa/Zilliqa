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
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libPersistence/Retriever.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

void addBalanceToGenesisAccount()
{
    LOG_MARKER();

    const uint256_t bal{100000000000};
    const uint256_t nonce{0};

    for (auto& walletHexStr : GENESIS_WALLETS)
    {
        Address addr{DataConversion::HexStrToUint8Vec(walletHexStr)};
        AccountStore::GetInstance().AddAccount(
            addr, {bal, nonce, dev::h256(), dev::h256()});
        LOG_GENERAL(INFO,
                    "add genesis account " << addr << " with balance " << bal);
    }
}

Node::Node(Mediator& mediator, unsigned int syncType, bool toRetrieveHistory)
    : m_mediator(mediator)
{
    this->Install(syncType, toRetrieveHistory);
}

Node::~Node() {}

void Node::Install(unsigned int syncType, bool toRetrieveHistory)
{
    // m_state = IDLE;
    bool runInitializeGenesisBlocks = true;

    if (toRetrieveHistory)
    {
        if (StartRetrieveHistory())
        {
            m_consensusID = 0;
            m_consensusLeaderID = 0;
            runInitializeGenesisBlocks = false;
        }
        else
        {
            LOG_GENERAL(INFO, "RetrieveHistory cancelled");
        }
    }

    if (runInitializeGenesisBlocks)
    {
        this->Init();
        if (syncType == SyncType::NO_SYNC)
        {
            m_consensusID = 1;
            m_consensusLeaderID = 1;
            addBalanceToGenesisAccount();
        }
        else
        {
            m_consensusID = 0;
            m_consensusLeaderID = 0;
        }
    }

    this->Prepare(runInitializeGenesisBlocks);
}

void Node::Init()
{
    // Zilliqa first epoch start from 1 not 0. So for the first DS epoch, there will be 1 less mini epoch only for the first DS epoch.
    // Hence, we have to set consensusID for first epoch to 1.
    LOG_MARKER();

    m_retriever->CleanAll();
    m_retriever.reset();
    m_mediator.m_dsBlockChain.Reset();
    m_mediator.m_txBlockChain.Reset();
    {
        std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommitteeNetworkInfo);
        m_mediator.m_DSCommitteeNetworkInfo.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommitteePubKeys);
        m_mediator.m_DSCommitteePubKeys.clear();
    }
    m_committedTransactions.clear();
    AccountStore::GetInstance().Init();

    m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                           m_mediator.m_txBlockChain);
}

void Node::Prepare(bool runInitializeGenesisBlocks)
{
    LOG_MARKER();
    m_mediator.m_currentEpochNum
        = (uint64_t)m_mediator.m_txBlockChain.GetBlockCount();
    m_mediator.UpdateDSBlockRand(runInitializeGenesisBlocks);
    m_mediator.UpdateTxBlockRand(runInitializeGenesisBlocks);
    SetState(POW1_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount());
}

bool Node::StartRetrieveHistory()
{
    LOG_MARKER();
    m_retriever = make_shared<Retriever>(m_mediator);

    bool ds_result;
    std::thread tDS(&Retriever::RetrieveDSBlocks, m_retriever.get(),
                    std::ref(ds_result));
    // retriever->RetrieveDSBlocks(ds_result);

    bool tx_result;
    std::thread tTx(&Retriever::RetrieveTxBlocks, m_retriever.get(),
                    std::ref(tx_result));
    // retriever->RetrieveTxBlocks(tx_result);

    bool st_result = m_retriever->RetrieveStates();

    tDS.join();
    tTx.join();

    bool tx_bodies_result = true;
#ifndef IS_LOOKUP_NODE
    tx_bodies_result = m_retriever->RetrieveTxBodiesDB();
#endif //IS_LOOKUP_NODE

    bool res = false;
    if (st_result && ds_result && tx_result && tx_bodies_result)
    {
#ifndef IS_LOOKUP_NODE
        if (m_retriever->ValidateStates())
#else // IS_LOOKUP_NODE
        if (m_retriever->ValidateStates() && m_retriever->CleanExtraTxBodies())
#endif // IS_LOOKUP_NODE
        {
            LOG_GENERAL(INFO, "RetrieveHistory Successed");
            m_mediator.m_isRetrievedHistory = true;
            res = true;
        }
    }
    return res;
}

#ifndef IS_LOOKUP_NODE

void Node::StartSynchronization()
{
    LOG_MARKER();

    SetState(POW2_SUBMISSION);
    auto func = [this]() -> void {
        m_synchronizer.FetchOfflineLookups(m_mediator.m_lookup);

        {
            unique_lock<mutex> lock(
                m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
            while (!m_mediator.m_lookup->m_fetchedOfflineLookups)
            {
                if (m_mediator.m_lookup->cv_offlineLookups.wait_for(
                        lock,
                        chrono::seconds(POW1_WINDOW_IN_SECONDS
                                        + BACKUP_POW2_WINDOW_IN_SECONDS))
                    == std::cv_status::timeout)
                {
                    LOG_GENERAL(WARNING, "FetchOfflineLookups Timeout...");
                    return;
                }
            }
            m_mediator.m_lookup->m_fetchedOfflineLookups = false;
        }
        while (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
        {
            m_synchronizer.FetchLatestDSBlocks(
                m_mediator.m_lookup, m_mediator.m_dsBlockChain.GetBlockCount());
            m_synchronizer.FetchLatestTxBlocks(
                m_mediator.m_lookup, m_mediator.m_txBlockChain.GetBlockCount());
            this_thread::sleep_for(
                chrono::seconds(m_mediator.m_lookup->m_startedPoW2
                                    ? BACKUP_POW2_WINDOW_IN_SECONDS
                                    : NEW_NODE_SYNC_INTERVAL));
        }
    };

    DetachedFunction(1, func);
}

#endif //IS_LOOKUP_NODE

bool Node::CheckState(Action action)
{
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  string("I am a DS node.")
                          + string(" Why am I getting this message? Action: ")
                      << ActionString(action));
        return false;
    }

    bool result = true;

    switch (action)
    {
    case STARTPOW1:
        switch (m_state)
        {
        case POW1_SUBMISSION:
            break;
        case POW2_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in POW2_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in TX_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION_BUFFER:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in TX_SUBMISSION_BUFFER");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS_PREP:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in "
                      "MICROBLOCK_CONSENSUS_PREP");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in MICROBLOCK_CONSENSUS");
            result = false;
            break;
        case WAITING_FINALBLOCK:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW1 but already in WAITING_FINALBLOCK");
            result = false;
            break;
        case ERROR:
            LOG_GENERAL(WARNING, "Doing STARTPOW1 but receiving ERROR message");
            result = false;
            break;
        default:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Unrecognized or error state");
            result = false;
            break;
        }
        break;
    case STARTPOW2:
        switch (m_state)
        {
        case POW1_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in POW1_SUBMISSION");
            result = false;
            break;
        case POW2_SUBMISSION:
            break;
        case TX_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in TX_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION_BUFFER:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in TX_SUBMISSION_BUFFER");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS_PREP:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in "
                      "MICROBLOCK_CONSENSUS_PREP");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in MICROBLOCK_CONSENSUS");
            result = false;
            break;
        case WAITING_FINALBLOCK:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing STARTPOW2 but already in WAITING_FINALBLOCK");
            result = false;
            break;
        case ERROR:
            LOG_GENERAL(WARNING, "Doing STARTPOW2 but receiving ERROR message");
            result = false;
            break;
        default:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Unrecognized or error state");
            result = false;
            break;
        }
        break;
    case PROCESS_SHARDING:
        switch (m_state)
        {
        case POW1_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_SHARDING but already in POW1_SUBMISSION");
            result = false;
            break;
        case POW2_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_SHARDING but already in POW2_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION:
            break;
        case TX_SUBMISSION_BUFFER:
            break;
        case MICROBLOCK_CONSENSUS_PREP:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_SHARDING but already in "
                      "MICROBLOCK_CONSENSUS_PREP");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_SHARDING but already in "
                      "MICROBLOCK_CONSENSUS");
            result = false;
            break;
        case WAITING_FINALBLOCK:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_SHARDING but already in "
                      "WAITING_FINALBLOCK");
            result = false;
            break;
        case ERROR:
            LOG_GENERAL(WARNING,
                        "Doing PROCESS_SHARDING but receiving ERROR message");
            result = false;
            break;
        default:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Unrecognized or error state");
            result = false;
            break;
        }
        break;
    case PROCESS_MICROBLOCKCONSENSUS:
        switch (m_state)
        {
        case POW1_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in POW1_SUBMISSION");
            result = false;
            break;
        case POW2_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in POW2_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in TX_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION_BUFFER:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in TX_SUBMISSION_BUFFER");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS_PREP:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in MICROBLOCK_CONSENSUS_PREP");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS:
            break;
        case WAITING_FINALBLOCK:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing PROCESS_MICROBLOCKCONSENSUS but already "
                      "in WAITING_FINALBLOCK");
            result = false;
            break;
        case ERROR:
            LOG_GENERAL(WARNING,
                        "Doing PROCESS_MICROBLOCKSUBMISSION but "
                        "receiving ERROR message");
            result = false;
            break;
        default:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Unrecognized or error state");
            result = false;
            break;
        }
        break;
    case PROCESS_FINALBLOCK:
        switch (m_state)
        {
        case POW1_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in "
                      "POW1_SUBMISSION");
            result = false;
            break;
        case POW2_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in "
                      "POW2_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in TX_SUBMISSION");
            result = false;
            break;
        case TX_SUBMISSION_BUFFER:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in "
                      "TX_SUBMISSION_BUFFER");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS_PREP:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in "
                      "MICROBLOCK_CONSENSUS_PREP");
            result = false;
            break;
        case MICROBLOCK_CONSENSUS:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Doing WAITING_FINALBLOCK but already in "
                      "MICROBLOCK_CONSENSUS");
            result = false;
            break;
        case WAITING_FINALBLOCK:
            break;
        case ERROR:
            LOG_GENERAL(WARNING,
                        "Doing WAITING_FINALBLOCK but receiving ERROR message");
            result = false;
            break;
        default:
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Unrecognized or error state");
            result = false;
            break;
        }
        break;
    default:
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unrecognized action");
        result = false;
        break;
    }

    return result;
}

vector<Peer> Node::GetBroadcastList(unsigned char ins_type,
                                    const Peer& broadcast_originator)
{
    LOG_MARKER();

    // // MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION
    // if (ins_type == NodeInstructionType::FORWARDTRANSACTION)
    // {
    //     LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "Gossip Forward list:");

    //     vector<Peer> peers;
    //     // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "DS size: " << m_mediator.m_DSCommitteeNetworkInfo.size() << " Shard size: " << m_myShardMembersNetworkInfo.size());

    //     if (m_isDSNode)
    //     {
    //         lock_guard<mutex> g(m_mutexFinalBlockProcessing);
    //         // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "I'm a DS node. DS size: " << m_mediator.m_DSCommitteeNetworkInfo.size() << " rand: " << rand() % m_mediator.m_DSCommitteeNetworkInfo.size());
    //         for (unsigned int i = 0; i < m_mediator.m_DSCommitteeNetworkInfo.size(); i++)
    //         {
    //             if (i == m_consensusMyID)
    //             {
    //                 continue;
    //             }
    //             if (rand() % m_mediator.m_DSCommitteeNetworkInfo.size() <= GOSSIP_RATE)
    //             {
    //                 peers.push_back(m_mediator.m_DSCommitteeNetworkInfo.at(i));
    //                 LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "DSNode  IP: " << peers.back().GetPrintableIPAddress() << " Port: " << peers.back().m_listenPortHost);

    //             }

    //         }
    //     }
    //     else
    //     {
    //         // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "I'm a shard node. Shard size: " << m_myShardMembersNetworkInfo.size() << " rand: " << rand() % m_myShardMembersNetworkInfo.size());
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
    //                 LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "  IP: " << peers.back().GetPrintableIPAddress() << " Port: " << peers.back().m_listenPortHost);

    //             }

    //         }
    //     }

    //     return peers;
    // }

    // Regardless of the instruction type, right now all our "broadcasts" are just redundant multicasts from DS nodes to non-DS nodes
    return vector<Peer>();
}

#ifndef IS_LOOKUP_NODE
bool Node::CheckCreatedTransaction(const Transaction& tx)
{
    LOG_MARKER();

    // Check if from account is sharded here
    const PubKey& senderPubKey = tx.GetSenderPubKey();
    Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int correct_shard
        = Transaction::GetShardIndex(fromAddr, m_numShards);

    if (correct_shard != m_myShardID)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "This tx is not sharded to me!");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "From Account  = 0x" << fromAddr);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Correct shard = " << correct_shard);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "This shard    = " << m_myShardID);
        return false;
    }

    // Check if from account exists in local storage
    if (!AccountStore::GetInstance().DoesAccountExist(fromAddr))
    {
        LOG_GENERAL(INFO,
                    "fromAddr not found: " << fromAddr
                                           << ". Transaction rejected: "
                                           << tx.GetTranID());
        return false;
    }

    // Check if to account exists in local storage
    const Address& toAddr = tx.GetToAddr();
    if (!AccountStore::GetInstance().DoesAccountExist(toAddr))
    {
        LOG_GENERAL(INFO, "New account is added: " << toAddr);
        AccountStore::GetInstance().AddAccount(
            toAddr, {0, 0, dev::h256(), dev::h256()});
    }

    // Check if transaction amount is valid
    if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount())
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Insufficient funds in source account!");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "From Account = 0x" << fromAddr);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Balance      = "
                      << AccountStore::GetInstance().GetBalance(fromAddr));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Debit Amount = " << tx.GetAmount());
        return false;
    }

    return true;
}
#endif // IS_LOOKUP_NODE

/// Return a valid transaction from fromKeyPair to toAddr with the specified amount
///
/// TODO: nonce is still no valid yet
Transaction CreateValidTestingTransaction(PrivKey& fromPrivKey,
                                          PubKey& fromPubKey,
                                          const Address& toAddr,
                                          uint256_t amount)
{
    unsigned int version = 0;
    auto nonce = 0;

    // LOG_GENERAL("fromPrivKey " << fromPrivKey << " / fromPubKey " << fromPubKey
    // << " / toAddr" << toAddr);

    Transaction txn(version, nonce, toAddr, make_pair(fromPrivKey, fromPubKey),
                    amount, 0, 0, {0}, {0});

    // std::vector<unsigned char> buf;
    // txn.SerializeWithoutSignature(buf, 0);

    // Signature sig;
    // Schnorr::GetInstance().Sign(buf, fromPrivKey, fromPubKey, sig);

    // vector<unsigned char> sigBuf;
    // sig.Serialize(sigBuf, 0);
    // txn.SetSignature(sigBuf);

    return txn;
}

bool GetOneGoodKeyPair(PrivKey& oPrivKey, PubKey& oPubKey, uint32_t myShard,
                       uint32_t nShard)
{
    for (auto& privKeyHexStr : GENESIS_KEYS)
    {
        auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
        auto privKey = PrivKey{privKeyBytes, 0};
        auto pubKey = PubKey{privKey};
        auto addr = Account::GetAddressFromPublicKey(pubKey);
        auto txnShard = Transaction::GetShardIndex(addr, nShard);

        LOG_GENERAL(INFO,
                    "Genesis Priv Key Str "
                        << privKeyHexStr << " / Priv Key " << privKey
                        << " / Pub Key " << pubKey << " / Addr " << addr
                        << " / txnShard " << txnShard << " / myShard "
                        << myShard << " / nShard " << nShard);
        if (txnShard == myShard)
        {
            oPrivKey = privKey;
            oPubKey = pubKey;
            return true;
        }
    }

    return false;
}

bool GetOneGenesisAddress(Address& oAddr)
{
    if (GENESIS_WALLETS.empty())
    {
        LOG_GENERAL(INFO, "could not get one genensis address");
        return false;
    }

    oAddr = Address{DataConversion::HexStrToUint8Vec(GENESIS_WALLETS.front())};
    return true;
}

std::once_flag generateReceiverOnce;

Address GenOneReceiver()
{
    static Address receiverAddr;
    std::call_once(generateReceiverOnce, []() {
        auto receiver = Schnorr::GetInstance().GenKeyPair();
        receiverAddr = Account::GetAddressFromPublicKey(receiver.second);
        LOG_GENERAL(INFO,
                    "Generate testing transaction receiver" << receiverAddr);
    });
    return receiverAddr;
}

/// generate transation from one to many random accounts
vector<Transaction> GenTransactionBulk(PrivKey& fromPrivKey, PubKey& fromPubKey,
                                       size_t n)
{
    vector<Transaction> txns;
    const size_t amountLimitMask = 0xff; // amount will vary from 0 to 255

    // FIXME: it's a workaround to use the first genensis account
    // auto receiver = Schnorr::GetInstance().GenKeyPair();
    // auto receiverAddr = Account::GetAddressFromPublicKey(receiver.second);

    // alternative 1: use first genesis address
    // Address addr;
    // if (not GetOneGenesisAddress(addr))
    // {
    // return txns;
    // }
    // auto receiverAddr = addr;

    // alternative 2: use a fresh address throughout entire lifetime
    auto receiverAddr = GenOneReceiver();

    txns.reserve(n);
    for (auto i = 0u; i != n; i++)
    {
        auto txn = CreateValidTestingTransaction(
            fromPrivKey, fromPubKey, receiverAddr, i & amountLimitMask);
        txns.emplace_back(txn);
    }

    return txns;
}

/// Handle send_txn command with the following message format
///
/// XXX The message format below is no ignored
///     Message = [33-byte from pubkey] [33-byte to pubkey] [32-byte amount]
bool Node::ProcessCreateTransaction(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    // vector<Transaction> txnToCreate;
    size_t nTxnPerAccount{N_PREFILLED_PER_ACCOUNT};
    // size_t nTxnDelta{MAXSUBMITTXNPERNODE};

    // if (not GetOneGoodKeyPair(senderPrivKey, senderPubKey, m_myShardID,
    // m_numShards))
    // {
    // LOG_GENERAL(
    // "No proper genesis account, cannot send testing transactions");
    // return false;
    // }

    // for (auto nTxn = 0u; nTxn < nTxnPerAccount; nTxn += nTxnDelta)
    // {
    unsigned int count = 0;
    for (auto& privKeyHexStr : GENESIS_KEYS)
    {
        auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
        auto privKey = PrivKey{privKeyBytes, 0};
        auto pubKey = PubKey{privKey};
        auto addr = Account::GetAddressFromPublicKey(pubKey);
        auto txns = GenTransactionBulk(privKey, pubKey, nTxnPerAccount);
        m_nRemainingPrefilledTxns += txns.size();
        {
            lock_guard<mutex> lg{m_mutexPrefilledTxns};
            auto& txnsDst = m_prefilledTxns[addr];
            txnsDst.insert(txnsDst.end(), txns.begin(), txns.end());
        }
        count++;
        if (count == 1)
            break;
    }
    // LOG_GENERAL("prefilled " << (nTxn + nTxnDelta) * GENESIS_KEYS.size()
    // << " txns");

    // {
    // lock_guard<mutex> g(m_mutexCreatedTransactions);
    // m_createdTransactions.insert(m_createdTransactions.end(),
    // txnToCreate.begin(), txnToCreate.end());
    // }

    LOG_GENERAL(INFO,
                "Finished prefilling " << nTxnPerAccount * GENESIS_KEYS.size()
                                       << " transactions");

    return true;
#endif // IS_LOOKUP_NODE
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::ProcessSubmitMissingTxn(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from)
{
    auto msgBlockNum
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    auto localBlockNum = (uint256_t)m_mediator.m_currentEpochNum;
    ;

    if (msgBlockNum != localBlockNum)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "untimely delivery of "
                      << "missing txns. received: " << msgBlockNum
                      << " , local: " << localBlockNum);
    }

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   Transaction::GetMinSerializedSize()))
    {
        return false;
    }

    const auto& submittedTransaction = Transaction(message, offset);

    // if (CheckCreatedTransaction(submittedTransaction))
    {
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        auto& receivedTransactions = m_receivedTransactions[msgBlockNum];
        receivedTransactions.insert(
            make_pair(submittedTransaction.GetTranID(), submittedTransaction));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Received missing txn: " << submittedTransaction.GetTranID())
    }

    return true;
}

bool Node::ProcessSubmitTxnSharing(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from)
{
    //LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   Transaction::GetMinSerializedSize()))
    {
        return false;
    }

    const auto& submittedTransaction = Transaction(message, offset);
    // if (CheckCreatedTransaction(submittedTransaction))
    {
        boost::multiprecision::uint256_t blockNum
            = (uint256_t)m_mediator.m_currentEpochNum;
        lock_guard<mutex> g(m_mutexReceivedTransactions);
        auto& receivedTransactions = m_receivedTransactions[blockNum];

        receivedTransactions.insert(
            make_pair(submittedTransaction.GetTranID(), submittedTransaction));
        //LOG_EPOCH(to_string(m_mediator.m_currentEpochNum).c_str(),
        //             "Received txn: " << submittedTransaction.GetTranID())
    }

    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessSubmitTransaction(const vector<unsigned char>& message,
                                    unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // This message is sent by my shard peers
    // Message = [204-byte transaction]

    //LOG_MARKER();

    unsigned int cur_offset = offset;

    auto submitTxnType = Serializable::GetNumber<uint32_t>(message, cur_offset,
                                                           sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    if (submitTxnType == SUBMITTRANSACTIONTYPE::MISSINGTXN)
    {
        if (m_state != MICROBLOCK_CONSENSUS)
        {
            LOG_EPOCH(
                INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Not in a microblock consensus state: don't want missing txns")
        }

        ProcessSubmitMissingTxn(message, cur_offset, from);
    }
    else if (submitTxnType == SUBMITTRANSACTIONTYPE::TXNSHARING)
    {
        while (m_state != TX_SUBMISSION && m_state != TX_SUBMISSION_BUFFER)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Not in ProcessSubmitTxn state -- waiting!")
            this_thread::sleep_for(chrono::milliseconds(200));
        }

        ProcessSubmitTxnSharing(message, cur_offset, from);
    }
#endif // IS_LOOKUP_NODE
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::CheckCreatedTransactionFromLookup(const Transaction& tx)
{
    LOG_MARKER();

    // Check if from account is sharded here
    const PubKey& senderPubKey = tx.GetSenderPubKey();
    Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int correct_shard
        = Transaction::GetShardIndex(fromAddr, m_numShards);

    if (correct_shard != m_myShardID)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "This tx is not sharded to me!");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "From Account  = 0x" << fromAddr);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Correct shard = " << correct_shard);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "This shard    = " << m_myShardID);
        return false;
    }

    // Check if from account exists in local storage
    if (!AccountStore::GetInstance().DoesAccountExist(fromAddr))
    {
        LOG_GENERAL(INFO,
                    "fromAddr not found: " << fromAddr
                                           << ". Transaction rejected: "
                                           << tx.GetTranID());
        return false;
    }

    {
        // Check from account nonce
        lock_guard<mutex> g(m_mutexTxnNonceMap);
        if (m_txnNonceMap.find(fromAddr) == m_txnNonceMap.end())
        {
            LOG_GENERAL(INFO, "Txn from " << fromAddr << "is new.");

            if (tx.GetNonce()
                != AccountStore::GetInstance().GetNonce(fromAddr) + 1)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Tx nonce not in line with account state!");
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "From Account      = 0x" << fromAddr);
                LOG_EPOCH(
                    INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Account Nonce     = "
                        << AccountStore::GetInstance().GetNonce(fromAddr));
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Expected Tx Nonce = "
                              << AccountStore::GetInstance().GetNonce(fromAddr)
                                  + 1);
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Actual Tx Nonce   = " << tx.GetNonce());
                return false;
            }
            m_txnNonceMap.insert(make_pair(fromAddr, tx.GetNonce()));
        }
        else
        {
            if (tx.GetNonce() != m_txnNonceMap.at(fromAddr) + 1)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Tx nonce not in line with account state!");
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "From Account      = 0x" << fromAddr);
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Account Nonce     = " << m_txnNonceMap.at(fromAddr));
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Expected Tx Nonce = " << m_txnNonceMap.at(fromAddr)
                                  + 1);
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Actual Tx Nonce   = " << tx.GetNonce());
                return false;
            }
            m_txnNonceMap.at(fromAddr) += 1;
        }
    }

    // Check if to account exists in local storage
    const Address& toAddr = tx.GetToAddr();
    if (!AccountStore::GetInstance().DoesAccountExist(toAddr))
    {
        LOG_GENERAL(INFO, "New account is added: " << toAddr);
        AccountStore::GetInstance().AddAccount(
            toAddr, {0, 0, dev::h256(), dev::h256()});
    }

    // Check if transaction amount is valid
    if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount())
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Insufficient funds in source account!");
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "From Account = 0x" << fromAddr);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Balance      = "
                      << AccountStore::GetInstance().GetBalance(fromAddr));
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Debit Amount = " << tx.GetAmount());
        return false;
    }

    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessCreateTransactionFromLookup(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE

    //LOG_MARKER();

    if (IsMessageSizeInappropriate(message.size(), offset,
                                   Transaction::GetMinSerializedSize()))
    {
        return false;
    }

    unsigned int curr_offset = offset;

    // Transaction tx(message, curr_offset);
    Transaction tx;
    if (tx.Deserialize(message, curr_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize Transaction.");
        return false;
    }

    lock_guard<mutex> g(m_mutexCreatedTransactions);

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Recvd txns: " << tx.GetTranID()
                             << " Signature: " << tx.GetSignature()
                             << " toAddr: " << tx.GetToAddr().hex());
    if (CheckCreatedTransactionFromLookup(tx))
    {
        m_createdTransactions.push_back(tx);
    }
    else
    {
        LOG_GENERAL(WARNING, "Txn is not valid.");
        return false;
    }

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
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Node State is now " << m_state << " at epoch "
                                   << m_mediator.m_currentEpochNum);
}

#ifndef IS_LOOKUP_NODE
void Node::SubmitTransactions()
{
    //LOG_MARKER();

    unsigned int txn_sent_count = 0;

    boost::multiprecision::uint256_t blockNum
        = (uint256_t)m_mediator.m_currentEpochNum;

    // TODO: remove the condition on txn_sent_count -- temporary hack to artificially limit number of
    // txns needed to be shared within shard members so that it completes in the time limit
    while (txn_sent_count < MAXSUBMITTXNPERNODE)
    {
        // shared_lock<shared_timed_mutex> lock(m_mutexProducerConsumer);
        if (m_state != TX_SUBMISSION)
        {
            break;
        }

        Transaction t;

        auto findOneFromPrefilled = [this](Transaction& t) -> bool {
            lock_guard<mutex> g{m_mutexPrefilledTxns};

            for (auto& txns : m_prefilledTxns)
            {
                auto& txnsList = txns.second;
                if (txnsList.empty())
                {
                    continue;
                }

                // auto& addr = txns.first;
                // auto shard = Transaction::GetShardIndex(addr, m_numShards);
                // if (shard != m_myShardID)
                // {
                // continue;
                // }

                t = move(txnsList.front());
                txnsList.pop_front();
                m_nRemainingPrefilledTxns--;

                return true;
            }

            return false;
        };

        auto findOneFromCreated = [this](Transaction& t) -> bool {
            lock_guard<mutex> g(m_mutexCreatedTransactions);

            if (m_createdTransactions.empty())
            {
                return false;
            }

            t = move(m_createdTransactions.front());
            m_createdTransactions.pop_front();
            return true;
        };

        auto submitOne = [this, &blockNum](Transaction& t) {
            vector<unsigned char> tx_message
                = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
            Serializable::SetNumber<uint32_t>(tx_message, MessageOffset::BODY,
                                              SUBMITTRANSACTIONTYPE::TXNSHARING,
                                              sizeof(uint32_t));
            t.Serialize(tx_message, MessageOffset::BODY + sizeof(uint32_t));
            P2PComm::GetInstance().SendMessage(m_myShardMembersNetworkInfo,
                                               tx_message);

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Sent txn: " << t.GetTranID())

            lock_guard<mutex> g(m_mutexSubmittedTransactions);
            auto& submittedTransactions = m_submittedTransactions[blockNum];
            submittedTransactions.insert(make_pair(t.GetTranID(), t));
        };

        if (findOneFromCreated(t))
        {
            submitOne(t);
        }
        else if (findOneFromPrefilled(t))
        {
            submitOne(t);
            txn_sent_count++;
        }
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "added " << txn_sent_count << " to submittedTransactions");

    // Clear m_txnNonceMap
    {
        lock_guard<mutex> g(m_mutexTxnNonceMap);
        m_txnNonceMap.clear();
    }

#ifdef STAT_TEST
    LOG_STATE("[TXNSE][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "]["
                         << m_myShardID << "][" << txn_sent_count << "] CONT");
#endif // STAT_TEST
}

void Node::RejoinAsNormal()
{
    LOG_MARKER();
    if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC)
    {
        m_mediator.m_lookup->m_syncType = SyncType::NORMAL_SYNC;
        this->CleanVariables();
        this->Install(true);
        this->StartSynchronization();
    }
}

bool Node::CleanVariables()
{
    m_myShardMembersPubKeys.clear();
    m_myShardMembersNetworkInfo.clear();
    m_isPrimary = false;
    m_isMBSender = false;
    m_myShardID = 0;

    m_consensusObject.reset();
    m_consensusBlockHash.clear();
    {
        std::lock_guard<mutex> lock(m_mutexMicroBlock);
        m_microblock.reset();
    }
    // {
    //     std::lock_guard<mutex> lock(m_mutexCreatedTransactions);
    //     m_createdTransactions.clear();
    // }
    {
        std::lock_guard<mutex> lock(m_mutexTxnNonceMap);
        m_txnNonceMap.clear();
    }
    // {
    //     std::lock_guard<mutex> lock(m_mutexPrefilledTxns);
    //     m_nRemainingPrefilledTxns = 0;
    //     m_prefilledTxns.clear();
    // }
    {
        std::lock_guard<mutex> lock(m_mutexSubmittedTransactions);
        m_submittedTransactions.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexReceivedTransactions);
        m_receivedTransactions.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexCommittedTransactions);
        m_committedTransactions.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexForwardingAssignment);
        m_forwardingAssignment.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexAllMicroBlocksRecvd);
        m_allMicroBlocksRecvd = true;
    }
    {
        std::lock_guard<mutex> lock(m_mutexUnavailableMicroBlocks);
        m_unavailableMicroBlocks.clear();
    }
    // On Lookup
    {
        std::lock_guard<mutex> lock(
            m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
        m_mediator.m_lookup->m_fetchedOfflineLookups = false;
    }
    m_mediator.m_lookup->m_startedPoW2 = false;

    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ToBlockMessage(unsigned char ins_byte)
{
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
#ifndef IS_LOOKUP_NODE
    {
        if (!m_fromNewProcess)
        {
            if (ins_byte != NodeInstructionType::SHARDING)
            {
                return true;
            }
        }
        else
        {
            if (m_runFromLate && ins_byte != NodeInstructionType::SHARDING)
            {
                return true;
            }
        }
        if (m_mediator.m_lookup->m_syncType == SyncType::DS_SYNC)
        {
            return true;
        }
    }
#else // IS_LOOKUP_NODE
    {
        return true;
    }
#endif // IS_LOOKUP_NODE
    return false;
}

bool Node::Execute(const vector<unsigned char>& message, unsigned int offset,
                   const Peer& from)
{
    //LOG_MARKER();

    bool result = true;

    typedef bool (Node::*InstructionHandler)(const vector<unsigned char>&,
                                             unsigned int, const Peer&);

    InstructionHandler ins_handlers[]
        = {&Node::ProcessStartPoW1,
           &Node::ProcessDSBlock,
           &Node::ProcessSharding,
           &Node::ProcessCreateTransaction,
           &Node::ProcessSubmitTransaction,
           &Node::ProcessMicroblockConsensus,
           &Node::ProcessFinalBlock,
           &Node::ProcessForwardTransaction,
           &Node::ProcessCreateTransactionFromLookup};

    const unsigned char ins_byte = message.at(offset);
    const unsigned int ins_handlers_count
        = sizeof(ins_handlers) / sizeof(InstructionHandler);

    // If the node failed and waiting for recovery, block the unwanted msg
    if (ToBlockMessage(ins_byte))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Node not connected to network yet, ignore message");
        return false;
    }

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
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unknown instruction byte " << hex << (unsigned int)ins_byte);
    }

    return result;
}
