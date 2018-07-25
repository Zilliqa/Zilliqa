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
        AccountStore::GetInstance().AddAccount(addr, {bal, nonce});
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
            m_mediator.m_currentEpochNum
                = (uint64_t)m_mediator.m_txBlockChain.GetLastBlock()
                      .GetHeader()
                      .GetBlockNum()
                + 1;
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
        std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
        m_mediator.m_DSCommittee.clear();
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
        = (uint64_t)m_mediator.m_txBlockChain.GetLastBlock()
              .GetHeader()
              .GetBlockNum()
        + 1;
    m_mediator.UpdateDSBlockRand(runInitializeGenesisBlocks);
    m_mediator.UpdateTxBlockRand(runInitializeGenesisBlocks);
    SetState(POW_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        (uint64_t)m_mediator.m_dsBlockChain.GetBlockCount());
}

bool Node::StartRetrieveHistory()
{
    LOG_MARKER();

    m_mediator.m_txBlockChain.Reset();
    m_mediator.m_dsBlockChain.Reset();

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

    SetState(SYNC);
    auto func = [this]() -> void {
        m_synchronizer.FetchOfflineLookups(m_mediator.m_lookup);

        {
            unique_lock<mutex> lock(
                m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
            while (!m_mediator.m_lookup->m_fetchedOfflineLookups)
            {
                if (m_mediator.m_lookup->cv_offlineLookups.wait_for(
                        lock,
                        chrono::seconds(POW_WINDOW_IN_SECONDS
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
                m_mediator.m_lookup,
                // m_mediator.m_dsBlockChain.GetBlockCount());
                m_mediator.m_dsBlockChain.GetLastBlock()
                        .GetHeader()
                        .GetBlockNum()
                    + 1);
            m_synchronizer.FetchLatestTxBlocks(
                m_mediator.m_lookup,
                // m_mediator.m_txBlockChain.GetBlockCount());
                m_mediator.m_txBlockChain.GetLastBlock()
                        .GetHeader()
                        .GetBlockNum()
                    + 1);
            this_thread::sleep_for(
                chrono::seconds(m_mediator.m_lookup->m_startedPoW2
                                    ? BACKUP_POW2_WINDOW_IN_SECONDS
                                        + TXN_SUBMISSION + TXN_BROADCAST
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
                  "I am a DS node. Why am I getting this message? Action: "
                      << GetActionString(action));
        return false;
    }

    static const std::multimap<NodeState, Action> ACTIONS_FOR_STATE
        = {{POW_SUBMISSION, STARTPOW},
           {POW2_SUBMISSION, STARTPOW2},
           {TX_SUBMISSION, PROCESS_SHARDING},
           {TX_SUBMISSION_BUFFER, PROCESS_SHARDING},
           {MICROBLOCK_CONSENSUS, PROCESS_MICROBLOCKCONSENSUS},
           {WAITING_FINALBLOCK, PROCESS_FINALBLOCK}};

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
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Action " << GetActionString(action)
                            << " not allowed in state " << GetStateString());
        return false;
    }

    return true;
}

vector<Peer> Node::GetBroadcastList(unsigned char ins_type,
                                    const Peer& broadcast_originator)
{
    // LOG_MARKER();

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
    //                 peers.emplace_back(m_mediator.m_DSCommitteeNetworkInfo.at(i));
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
    //                 peers.emplace_back(m_myShardMembersNetworkInfo.at(i));
    //                 LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(), "  IP: " << peers.back().GetPrintableIPAddress() << " Port: " << peers.back().m_listenPortHost);

    //             }

    //         }
    //     }

    //     return peers;
    // }

    // Regardless of the instruction type, right now all our "broadcasts" are just redundant multicasts from DS nodes to non-DS nodes
    return vector<Peer>();
}

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
                    amount, 1, 1, {}, {});

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
                    "Generate testing transaction receiver " << receiverAddr);
    });
    return receiverAddr;
}

/// generate transation from one to many random accounts
vector<Transaction> GenTransactionBulk(PrivKey& fromPrivKey, PubKey& fromPubKey,
                                       size_t n)
{
    vector<Transaction> txns;

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
        auto txn = CreateValidTestingTransaction(fromPrivKey, fromPubKey,
                                                 receiverAddr, i);
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
    // m_createdTransactions.emplace(m_createdTransactions.end(),
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
    unsigned int cur_offset = offset;

    auto msgBlockNum
        = Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    if (msgBlockNum != m_mediator.m_currentEpochNum)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "untimely delivery of "
                      << "missing txns. received: " << msgBlockNum
                      << " , local: " << m_mediator.m_currentEpochNum);
    }

    while (cur_offset < message.size())
    {
        Transaction submittedTransaction;
        if (submittedTransaction.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING,
                        "Deserialize transactions failed, stop at the previous "
                        "successful one");
            return false;
        }
        cur_offset += submittedTransaction.GetSerializedSize();

        if (m_mediator.m_validator->CheckCreatedTransaction(
                submittedTransaction))
        {
            boost::multiprecision::uint256_t blockNum
                = (uint256_t)m_mediator.m_currentEpochNum;
            lock_guard<mutex> g(m_mutexReceivedTransactions);
            auto& receivedTransactions = m_receivedTransactions[blockNum];

            receivedTransactions.insert(make_pair(
                submittedTransaction.GetTranID(), submittedTransaction));
            //LOG_EPOCH(to_string(m_mediator.m_currentEpochNum).c_str(),
            //             "Received txn: " << submittedTransaction.GetTranID())
        }
    }

    AccountStore::GetInstance().SerializeDelta();
    cv_MicroBlockMissingTxn.notify_all();
    return true;
}

bool Node::ProcessSubmitTxnSharing(const vector<unsigned char>& message,
                                   unsigned int offset, const Peer& from)
{
    //LOG_MARKER();

    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
    {
        if (m_state != TX_SUBMISSION)
        {
            return false;
        }
    }

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    if (!isVacuousEpoch)
    {
        unique_lock<mutex> g(m_mutexNewRoundStarted);
        if (!m_newRoundStarted)
        {
            // LOG_GENERAL(INFO, "Wait for new consensus round started");
            if (m_cvNewRoundStarted.wait_for(
                    g, std::chrono::seconds(TXN_SUBMISSION + TXN_BROADCAST))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Waiting for new round started timeout, ignore");
                return false;
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "New consensus round started, moving to "
                      "ProcessSubmitTxnSharing");
            if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
            {
                LOG_GENERAL(WARNING, "The node started rejoin, ignore");
                return false;
            }
        }
        else
        {
            // LOG_GENERAL(INFO, "No need to wait for newRoundStarted");
        }
    }

    unsigned int cur_offset = offset;

    while (cur_offset < message.size())
    {
        Transaction submittedTransaction;
        if (submittedTransaction.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING,
                        "Deserialize transactions failed, stop at the previous "
                        "successful one");
            return false;
        }
        cur_offset += submittedTransaction.GetSerializedSize();

        if (m_mediator.m_validator->CheckCreatedTransaction(
                submittedTransaction))
        {
            boost::multiprecision::uint256_t blockNum
                = (uint256_t)m_mediator.m_currentEpochNum;
            lock_guard<mutex> g(m_mutexReceivedTransactions);
            auto& receivedTransactions = m_receivedTransactions[blockNum];

            receivedTransactions.emplace(submittedTransaction.GetTranID(),
                                         submittedTransaction);
            //LOG_EPOCH(to_string(m_mediator.m_currentEpochNum).c_str(),
            //             "Received txn: " << submittedTransaction.GetTranID())
        }
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

    LOG_MARKER();

    unsigned int cur_offset = offset;

    unsigned char submitTxnType = message[cur_offset];
    cur_offset += MessageOffset::INST;

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
        ProcessSubmitTxnSharing(message, cur_offset, from);
    }
#endif // IS_LOOKUP_NODE
    return true;
}

bool Node::ProcessCreateTransactionFromLookup(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE

    LOG_MARKER();

    // bool isVacuousEpoch
    //     = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));

    // if (!isVacuousEpoch)
    // {
    //     unique_lock<mutex> g(m_mutexNewRoundStarted);
    //     if (!m_newRoundStarted)
    //     {
    //         LOG_GENERAL(INFO, "Wait for new consensus round started");
    //         m_cvNewRoundStarted.wait(g, [this] { return m_newRoundStarted; });
    //         LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //                   "New consensus round started, moving to "
    //                   "ProcessSubmitTxnSharing");
    //     }
    //     else
    //     {
    //         LOG_GENERAL(INFO, "No need to wait for newRoundStarted");
    //     }
    // }

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
    if (m_mediator.m_validator->CheckCreatedTransactionFromLookup(tx))
    {
        m_createdTransactions.emplace_back(tx);
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
              "Node State is now " << GetStateString() << " at epoch "
                                   << m_mediator.m_currentEpochNum);
}

void Node::AddBlock(const TxBlock& block)
{
    m_mediator.m_txBlockChain.AddBlock(block);

    if (block.GetHeader().GetBlockNum() == m_latestForwardBlockNum)
    {
        m_cvForwardBlockNumSync.notify_all();
    }
}

#ifndef IS_LOOKUP_NODE
void Node::SubmitTransactions()
{
    //LOG_MARKER();

    unsigned int txn_sent_count = 0;
    boost::multiprecision::uint256_t blockNum
        = (uint256_t)m_mediator.m_currentEpochNum;

    unsigned int cur_offset = 0;

    m_txMessage = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
    cur_offset += MessageOffset::BODY;

    m_txMessage.push_back(SUBMITTRANSACTIONTYPE::TXNSHARING);
    cur_offset += MessageOffset::INST;

    // TODO: remove the condition on txn_sent_count -- temporary hack to artificially limit number of
    // txns needed to be shared within shard members so that it completes in the time limit
    while (txn_sent_count < MAXSUBMITTXNPERNODE)
    {
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

        auto appendOne = [this, &blockNum, &cur_offset](Transaction& t) {
            t.Serialize(m_txMessage, cur_offset);
            cur_offset += t.GetSerializedSize();

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Append txn: " << t.GetTranID())

            lock_guard<mutex> g(m_mutexSubmittedTransactions);
            auto& submittedTransactions = m_submittedTransactions[blockNum];
            submittedTransactions.emplace(t.GetTranID(), t);
        };

        if (findOneFromCreated(t))
        {
            if (m_mediator.m_validator->CheckCreatedTransaction(t)
                || !t.GetCode().empty() || !t.GetData().empty())
            {
                appendOne(t);
            }
        }
        else if (findOneFromPrefilled(t))
        {
            if (m_mediator.m_validator->CheckCreatedTransaction(t)
                || !t.GetCode().empty() || !t.GetData().empty())
            {
                appendOne(t);
            }
        }
        else
        {
            break;
        }
        txn_sent_count++;
    }

    if (txn_sent_count > 0)
    {
        LOG_GENERAL(INFO, "Broadcast my txns to other shard members");
        P2PComm::GetInstance().SendMessage(m_myShardMembersNetworkInfo,
                                           m_txMessage);
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "added " << txn_sent_count << " to submittedTransactions");

    m_mediator.m_validator->CleanVariables();

    LOG_STATE("[TXNSE][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "]["
                         << m_myShardID << "][" << txn_sent_count << "] CONT");
}

void Node::RejoinAsNormal()
{
    LOG_MARKER();
    if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC)
    {
        auto func = [this]() mutable -> void {
            m_mediator.m_lookup->m_syncType = SyncType::NORMAL_SYNC;
            this->CleanVariables();
            this->Install(true);
            this->StartSynchronization();
            this->ResetRejoinFlags();
        };
        DetachedFunction(1, func);
    }
}

void Node::ResetRejoinFlags()
{
    m_doRejoinAtNextRound = false;
    m_doRejoinAtStateRoot = false;
    m_doRejoinAtFinalBlock = false;
}

bool Node::CleanVariables()
{
    AccountStore::GetInstance().InitSoft();
    m_myShardMembersPubKeys.clear();
    m_myShardMembersNetworkInfo.clear();
    m_isPrimary = false;
    m_isMBSender = false;
    m_tempStateDeltaCommitted = true;
    m_myShardID = 0;

    {
        std::lock_guard<mutex> lock(m_mutexConsensus);
        m_consensusObject.reset();
    }

    m_consensusBlockHash.clear();
    {
        std::lock_guard<mutex> lock(m_mutexMicroBlock);
        m_microblock.reset();
    }
    // {
    //     std::lock_guard<mutex> lock(m_mutexCreatedTransactions);
    //     m_createdTransactions.clear();
    // }
    m_mediator.m_validator->CleanVariables();
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
    {
        std::lock_guard<mutex> lock(m_mutexTempCommitted);
        m_tempStateDeltaCommitted = true;
    }
    // On Lookup
    {
        std::lock_guard<mutex> lock(
            m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
        m_mediator.m_lookup->m_fetchedOfflineLookups = false;
    }
    m_mediator.m_lookup->m_startedPoW2 = false;
    m_latestForwardBlockNum = 0;

    return true;
}

void Node::CleanCreatedTransaction()
{
    std::lock_guard<mutex> lock(m_mutexCreatedTransactions);
    m_createdTransactions.clear();
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessDoRejoin(const std::vector<unsigned char>& message,
                           unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE

    LOG_MARKER();

    if (!ENABLE_DO_REJOIN)
    {
        return false;
    }

    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
    {
        LOG_GENERAL(WARNING, "Already in rejoining!");
        return false;
    }

    unsigned int cur_offset = offset;

    if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                   MessageOffset::INST))
    {
        return false;
    }

    unsigned char rejoinType = message[cur_offset];
    cur_offset += MessageOffset::INST;

    switch (rejoinType)
    {
    case REJOINTYPE::ATFINALBLOCK:
        m_doRejoinAtFinalBlock = true;
        break;
    case REJOINTYPE::ATNEXTROUND:
        m_doRejoinAtNextRound = true;
        break;
    case REJOINTYPE::ATSTATEROOT:
        m_doRejoinAtStateRoot = true;
        break;
    default:
        return false;
    }
#endif // IS_LOOKUP_NODE
    return true;
}

bool Node::ToBlockMessage(unsigned char ins_byte)
{
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
#ifndef IS_LOOKUP_NODE
    {
        if (!m_fromNewProcess)
        {
            if (ins_byte != NodeInstructionType::SHARDING
                && ins_byte != NodeInstructionType::SUBMITTRANSACTION)
            {
                return true;
            }
        }
        else
        {
            if (m_runFromLate && ins_byte != NodeInstructionType::SHARDING
                && ins_byte != NodeInstructionType::CREATETRANSACTION
                && ins_byte != NodeInstructionType::SUBMITTRANSACTION)
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
        = {&Node::ProcessStartPoW,
           &Node::ProcessDSBlock,
           &Node::ProcessSharding,
           &Node::ProcessCreateTransaction,
           &Node::ProcessSubmitTransaction,
           &Node::ProcessMicroblockConsensus,
           &Node::ProcessFinalBlock,
           &Node::ProcessForwardTransaction,
           &Node::ProcessCreateTransactionFromLookup,
           &Node::ProcessVCBlock,
           &Node::ProcessForwardStateDelta,
           &Node::ProcessDoRejoin};

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

#define MAKE_LITERAL_PAIR(s)                                                   \
    {                                                                          \
        s, #s                                                                  \
    }

map<Node::NodeState, string> Node::NodeStateStrings
    = {MAKE_LITERAL_PAIR(POW_SUBMISSION),
       MAKE_LITERAL_PAIR(POW2_SUBMISSION),
       MAKE_LITERAL_PAIR(TX_SUBMISSION),
       MAKE_LITERAL_PAIR(TX_SUBMISSION_BUFFER),
       MAKE_LITERAL_PAIR(MICROBLOCK_CONSENSUS_PREP),
       MAKE_LITERAL_PAIR(MICROBLOCK_CONSENSUS),
       MAKE_LITERAL_PAIR(WAITING_FINALBLOCK),
       MAKE_LITERAL_PAIR(ERROR),
       MAKE_LITERAL_PAIR(SYNC)};

string Node::GetStateString() const
{
    if (NodeStateStrings.find(m_state) == NodeStateStrings.end())
    {
        return "Unknown";
    }
    else
    {
        return NodeStateStrings.at(m_state);
    }
}

map<Node::Action, string> Node::ActionStrings
    = {MAKE_LITERAL_PAIR(STARTPOW),
       MAKE_LITERAL_PAIR(STARTPOW2),
       MAKE_LITERAL_PAIR(PROCESS_SHARDING),
       MAKE_LITERAL_PAIR(PROCESS_MICROBLOCKCONSENSUS),
       MAKE_LITERAL_PAIR(PROCESS_FINALBLOCK),
       MAKE_LITERAL_PAIR(PROCESS_TXNBODY),
       MAKE_LITERAL_PAIR(NUM_ACTIONS)};

std::string Node::GetActionString(Action action) const
{
    return (ActionStrings.find(action) == ActionStrings.end())
        ? "Unknown"
        : ActionStrings.at(action);
}