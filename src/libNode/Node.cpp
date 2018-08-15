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
using namespace boost::multi_index;

void addBalanceToGenesisAccount()
{
    LOG_MARKER();

    const uint256_t bal{std::numeric_limits<uint64_t>::max()};
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
        m_mediator.m_DSCommittee->clear();
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
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum()
        + 1;
    m_mediator.UpdateDSBlockRand(runInitializeGenesisBlocks);
    m_mediator.UpdateTxBlockRand(runInitializeGenesisBlocks);
    SetState(POW_SUBMISSION);
    POW::GetInstance().EthashConfigureLightClient(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1);
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
                        lock, chrono::seconds(POW_WINDOW_IN_SECONDS))
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
            this_thread::sleep_for(chrono::seconds(
                m_mediator.m_lookup->m_startedPoW ? POW_BACKUP_WINDOW_IN_SECONDS
                                                  : NEW_NODE_SYNC_INTERVAL));
        }
    };

    DetachedFunction(1, func);
}

#endif //IS_LOOKUP_NODE

bool Node::CheckState(Action action)
{
    if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE
        && action != PROCESS_MICROBLOCKCONSENSUS)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am a DS node. Why am I getting this message? Action: "
                      << GetActionString(action));
        return false;
    }

    static const std::multimap<NodeState, Action> ACTIONS_FOR_STATE
        = {{POW_SUBMISSION, STARTPOW},
           {MICROBLOCK_CONSENSUS_PREP, PROCESS_DSBLOCK},
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

vector<Peer>
    Node::GetBroadcastList([[gnu::unused]] unsigned char ins_type,
                           [[gnu::unused]] const Peer& broadcast_originator)
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

    /// generate transation from one to many random accounts
    /*vector<Transaction> GenTransactionBulk(PrivKey& fromPrivKey, PubKey& fromPubKey,
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
}*/

#ifndef IS_LOOKUP_NODE
bool Node::ProcessSubmitMissingTxn(const vector<unsigned char>& message,
                                   unsigned int offset,
                                   [[gnu::unused]] const Peer& from)
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

        lock_guard<mutex> g(m_mutexCreatedTransactions);
        // m_createdTransactions.push_back(submittedTransaction);
        auto& listIdx = m_createdTransactions.get<MULTI_INDEX_KEY::GAS_PRICE>();
        listIdx.insert(submittedTransaction);
    }

    vector<TxnHash> missingTxnHashes;
    if (!ProcessTransactionWhenShardBackup(m_txnsOrdering, missingTxnHashes))
    {
        LOG_GENERAL(WARNING, "Wrong order after receiving missing txns");
        return false;
    }
    if (!missingTxnHashes.empty())
    {
        LOG_GENERAL(WARNING, "Still missed txns");
        return false;
    }

    AccountStore::GetInstance().SerializeDelta();
    cv_MicroBlockMissingTxn.notify_all();
    return true;
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessSubmitTransaction(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
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
#endif // IS_LOOKUP_NODE
    return true;
}

bool Node::ProcessCreateTransactionFromLookup(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
#ifndef IS_LOOKUP_NODE

    LOG_MARKER();

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

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Recvd txns: " << tx.GetTranID()
                             << " Signature: " << tx.GetSignature()
                             << " toAddr: " << tx.GetToAddr().hex());

    lock_guard<mutex> g(m_mutexCreatedTransactions);
    if (m_mediator.m_validator->CheckCreatedTransactionFromLookup(tx))
    {
        auto& compIdx
            = m_createdTransactions.get<MULTI_INDEX_KEY::ADDR_NONCE>();
        auto it = compIdx.find(make_tuple(tx.GetSenderAddr(), tx.GetNonce()));
        if (it != compIdx.end())
        {
            if (it->GetGasPrice() < tx.GetGasPrice())
            {
                compIdx.replace(it, tx);
                return true;
            }
            else
            {
                LOG_GENERAL(WARNING,
                            "Txn with same address and nonce already "
                            "exists with higher gas price");
                return false;
            }
        }

        auto& listIdx = m_createdTransactions.get<MULTI_INDEX_KEY::GAS_PRICE>();
        listIdx.insert(tx);
    }
    else
    {
        LOG_GENERAL(WARNING, "Txn is not valid.");
        return false;
    }

#endif //IS_LOOKUP_NOD

    return true;
}

bool Node::ProcessTxnPacketFromLookup(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
{
    LOG_MARKER();
#ifndef IS_LOOKUP_NODE
    // check it's at inappropriate timing
    // vacuous epoch -> reject
    // new ds epoch but didn't received ds block yet -> buffer
    // else -> process

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (isVacuousEpoch)
    {
        return false;
    }
    else if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0)

    {
        // check for recieval of new ds block
        // need to wait the ProcessDSBlock finish
        lock_guard<mutex> g(m_mutexDSBlock);
        if (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()
            < (m_mediator.m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW) + 1)
        {
            lock_guard<mutex> g(m_mutexTxnPacketBuffer);
            m_txnPacketBuffer.emplace_back(message);
        }
    }
    else
    {
        return ProcessTxnPacketFromLookupCore(message, offset);
    }
#endif //IS_LOOKUP_NOD
    return true;
}

#ifndef IS_LOOKUP_NODE
bool Node::ProcessTxnPacketFromLookupCore(const vector<unsigned char>& message,
                                          unsigned int offset)
{
    LOG_MARKER();

    // core part:
    if (IsMessageSizeInappropriate(message.size(), offset,
                                   2 * sizeof(uint32_t)))
    {
        return false;
    }
    unsigned int curr_offset = offset;
    uint32_t shardId = Serializable::GetNumber<uint32_t>(message, curr_offset,
                                                         sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    if (shardId != m_myShardID)
    {
        LOG_GENERAL(WARNING, "Wrong Shard");
        return false;
    }

    uint32_t num = Serializable::GetNumber<uint32_t>(message, curr_offset,
                                                     sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    if (IsMessageSizeInappropriate(message.size(), curr_offset,
                                   Transaction::GetMinSerializedSize() * num))
    {
        return false;
    }

    unsigned int txn_sent_count = 0;
    lock_guard<mutex> g(m_mutexCreatedTransactions);

    for (unsigned int i = 0; i < num; i++)
    {
        Transaction tx;
        if (tx.Deserialize(message, curr_offset) != 0)
        {
            LOG_GENERAL(WARNING, "Failed to deserialize");
            return false;
        }

        if (m_mediator.m_validator->CheckCreatedTransactionFromLookup(tx))
        {

            auto& compIdx
                = m_createdTransactions.get<MULTI_INDEX_KEY::ADDR_NONCE>();
            auto it
                = compIdx.find(make_tuple(tx.GetSenderAddr(), tx.GetNonce()));
            if (it != compIdx.end())
            {
                if (it->GetGasPrice() < tx.GetGasPrice())
                {
                    compIdx.replace(it, tx);
                }
                else
                {
                    LOG_GENERAL(WARNING,
                                "Txn with same address and nonce already "
                                "exists with higher gas price");
                }
            }
            else
            {
                auto& listIdx
                    = m_createdTransactions.get<MULTI_INDEX_KEY::GAS_PRICE>();
                listIdx.insert(tx);
                txn_sent_count++;
            }
        }
        else
        {
            LOG_GENERAL(WARNING, "Txn is not valid.");
        }

        curr_offset += tx.GetSerializedSize();
    }
    LOG_GENERAL(INFO, "TXN COUNT" << txn_sent_count);
    vector<Peer> toSend;

    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE)
    {
        for (auto it = m_myShardMembers->begin(); it != m_myShardMembers->end();
             it++)
        {
            toSend.push_back(it->second);
        }
        if (txn_sent_count > 0)
        {
            LOG_GENERAL(INFO,
                        "[Batching] Broadcast my txns to other shard members");
            P2PComm::GetInstance().SendBroadcastMessage(toSend, message);
        }
    }
    else
    {
        for (auto it = m_mediator.m_DSCommittee->begin();
             it != m_mediator.m_DSCommittee->end(); it++)
        {
            toSend.push_back(it->second);
        }
        if (txn_sent_count > 0)
        {
            LOG_GENERAL(INFO,
                        "[DSMB] "
                            << " Send to other DS committee");

            P2PComm::GetInstance().SendBroadcastMessage(toSend, message);
        }
    }

    return true;
}

void Node::CommitTxnPacketBuffer()
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexTxnPacketBuffer);

    for (const auto& msg : m_txnPacketBuffer)
    {
        ProcessTxnPacketFromLookupCore(msg, MessageOffset::BODY);
    }

    m_txnPacketBuffer.clear();
}
#endif //IS_LOOKUP_NOD

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
}

#ifndef IS_LOOKUP_NODE
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
    m_myShardMembers->clear();
    m_isPrimary = false;
    m_isMBSender = false;
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
        std::lock_guard<mutex> lock(m_mutexProcessedTransactions);
        m_processedTransactions.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexCommittedTransactions);
        m_committedTransactions.clear();
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
    m_mediator.m_lookup->m_startedPoW = false;

    return true;
}
void Node::SetMyShardID(uint32_t shardID) { m_myShardID = shardID; }
#endif // IS_LOOKUP_NODE

void Node::CleanCreatedTransaction()
{
    std::lock_guard<mutex> lock(m_mutexCreatedTransactions);
    // m_createdTransactions.clear();
    m_createdTransactions.get<0>().clear();
}

bool Node::ProcessDoRejoin(
    [[gnu::unused]] const std::vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from)
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

bool Node::ToBlockMessage([[gnu::unused]] unsigned char ins_byte)
{
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
#ifndef IS_LOOKUP_NODE
    {
        if (m_mediator.m_lookup->m_syncType == SyncType::DS_SYNC)
        {
            return true;
        }
        if (!m_fromNewProcess)
        {
            if (ins_byte != NodeInstructionType::DSBLOCK
                && ins_byte != NodeInstructionType::CREATETRANSACTIONFROMLOOKUP)
            {
                return true;
            }
        }
        else
        {
            if (m_runFromLate && ins_byte != NodeInstructionType::DSBLOCK
                && ins_byte != NodeInstructionType::CREATETRANSACTIONFROMLOOKUP)
            {
                return true;
            }
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
           &Node::ProcessSubmitTransaction,
           &Node::ProcessMicroblockConsensus,
           &Node::ProcessFinalBlock,
           &Node::ProcessForwardTransaction,
           &Node::ProcessCreateTransactionFromLookup,
           &Node::ProcessVCBlock,
           &Node::ProcessDoRejoin,
           &Node::ProcessTxnPacketFromLookup};

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
       MAKE_LITERAL_PAIR(PROCESS_DSBLOCK),
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
