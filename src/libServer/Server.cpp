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

#ifdef IS_LOOKUP_NODE

#include "JSONConversion.h"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>
#include <jsonrpccpp/server.h>

#include "Server.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace jsonrpc;
using namespace std;

CircularArray<std::string> Server::m_RecentTransactions;
std::mutex Server::m_mutexRecentTxns;

const unsigned int PAGE_SIZE = 10;
const unsigned int NUM_PAGES_CACHE = 2;
const unsigned int TXN_PAGE_SIZE = 100;

//[warning] do not make this constant too big as it loops over blockchain
const unsigned int REF_BLOCK_DIFF = 5;

Server::Server(Mediator& mediator, HttpServer& httpserver)
    : AbstractZServer(httpserver)
    , m_mediator(mediator)
{
    m_StartTimeTx = 0;
    m_StartTimeDs = 0;
    m_DSBlockCache.first = 0;
    m_DSBlockCache.second.resize(NUM_PAGES_CACHE * PAGE_SIZE);
    m_TxBlockCache.first = 0;
    m_TxBlockCache.second.resize(NUM_PAGES_CACHE * PAGE_SIZE);
    m_RecentTransactions.resize(TXN_PAGE_SIZE);
    m_TxBlockCountSumPair.first = 0;
    m_TxBlockCountSumPair.second = 0;
}

Server::~Server()
{
    // destructor
}

string Server::GetClientVersion() { return "Hello"; }

string Server::GetNetworkId() { return "TestNet"; }

string Server::GetProtocolVersion() { return "Hello"; }

string Server::CreateTransaction(const Json::Value& _json)
{
    LOG_MARKER();

    try
    {

        if (!JSONConversion::checkJsonTx(_json))
        {
            return "Invalid Tx Json";
        }

        Transaction tx = JSONConversion::convertJsontoTx(_json);

        if (!Transaction::Verify(tx))
        {
            return "Signature incorrect";
        }

        //LOG_GENERAL(INFO, "Nonce: "<<tx.GetNonce().str()<<" toAddr: "<<tx.GetToAddr().hex()<<" senderPubKey: "<<static_cast<string>(tx.GetSenderPubKey());<<" amount: "<<tx.GetAmount().str());

        unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

        const PubKey& senderPubKey = tx.GetSenderPubKey();
        const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
        unsigned int curr_offset = 0;

        if (num_shards > 0)
        {
            unsigned int shard
                = Transaction::GetShardIndex(fromAddr, num_shards);
            map<PubKey, Peer> shardMembers
                = m_mediator.m_lookup->GetShardPeers().at(shard);
            LOG_GENERAL(INFO, "The Tx Belongs to " << shard << " Shard");

            vector<unsigned char> tx_message
                = {MessageType::NODE,
                   NodeInstructionType::CREATETRANSACTIONFROMLOOKUP};
            curr_offset += MessageOffset::BODY;

            tx.Serialize(tx_message, curr_offset);

            LOG_GENERAL(INFO, "Tx Serialized");

            vector<Peer> toSend;

            auto it = shardMembers.begin();
            for (unsigned int i = 0; i < 1 && it != shardMembers.end();
                 i++, it++)
            {
                toSend.push_back(it->second);
            }

            P2PComm::GetInstance().SendMessage(toSend, tx_message);
        }
        else
        {
            LOG_GENERAL(INFO, "No shards yet");

            return "Could Not Create Transaction";
        }

        return tx.GetTranID().hex();
    }
    catch (exception& e)
    {

        LOG_GENERAL(INFO,
                    "[Error]" << e.what()
                              << " Input: " << _json.toStyledString());

        return "Unable to process";
    }
}

Json::Value Server::GetTransaction(const string& transactionHash)
{
    LOG_MARKER();
    try
    {
        TxBodySharedPtr tx;
        TxnHash tranHash(transactionHash);
        if (transactionHash.size() != TRAN_HASH_SIZE * 2)
        {
            Json::Value _json;
            _json["error"] = "Size not appropriate";

            return _json;
        }
        bool isPresent
            = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tx);
        if (!isPresent)
        {
            Json::Value _json;
            _json["error"] = "Txn Hash not Present";
            return _json;
        }
        Transaction txn(*tx);
        return JSONConversion::convertTxtoJson(txn);
    }
    catch (exception& e)
    {
        Json::Value _json;
        LOG_GENERAL(INFO,
                    "[Error]" << e.what() << " Input: " << transactionHash);
        _json["Error"] = "Unable to Process";
        return _json;
    }
}

Json::Value Server::GetDsBlock(const string& blockNum)
{

    try
    {
        boost::multiprecision::uint256_t BlockNum(blockNum);
        return JSONConversion::convertDSblocktoJson(
            m_mediator.m_dsBlockChain.GetBlock(BlockNum));
    }
    catch (const char* msg)
    {
        Json::Value _json;
        _json["Error"] = msg;
        return _json;
    }
    catch (runtime_error& e)
    {
        Json::Value _json;
        LOG_GENERAL(INFO, "Error " << e.what());
        _json["Error"] = "String not numeric";
        return _json;
    }
    catch (exception& e)
    {
        Json::Value _json;
        LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
        _json["Error"] = "Unable to Process";
        return _json;
    }
}

Json::Value Server::GetTxBlock(const string& blockNum)
{

    try
    {
        boost::multiprecision::uint256_t BlockNum(blockNum);
        return JSONConversion::convertTxBlocktoJson(
            m_mediator.m_txBlockChain.GetBlock(BlockNum));
    }
    catch (const char* msg)
    {
        Json::Value _json;
        _json["Error"] = msg;
        return _json;
    }
    catch (runtime_error& e)
    {
        Json::Value _json;
        LOG_GENERAL(INFO, "Error " << e.what());
        _json["Error"] = "String not numeric";
        return _json;
    }
    catch (exception& e)
    {
        Json::Value _json;
        LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
        _json["Error"] = "Unable to Process";
        return _json;
    }
}

string Server::GetGasPrice() { return "Hello"; }

Json::Value Server::GetLatestDsBlock()
{
    LOG_MARKER();
    DSBlock Latest = m_mediator.m_dsBlockChain.GetLastBlock();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "BlockNum " << Latest.GetHeader().GetBlockNum().str()
                          << "  Timestamp:        "
                          << Latest.GetHeader().GetTimestamp().str());

    return JSONConversion::convertDSblocktoJson(Latest);
}

Json::Value Server::GetLatestTxBlock()
{
    LOG_MARKER();
    TxBlock Latest = m_mediator.m_txBlockChain.GetLastBlock();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "BlockNum " << Latest.GetHeader().GetBlockNum().str()
                          << "  Timestamp:        "
                          << Latest.GetHeader().GetTimestamp().str());

    return JSONConversion::convertTxBlocktoJson(Latest);
}

Json::Value Server::GetBalance(const string& address)
{
    LOG_MARKER();

    try
    {

        if (address.size() != ACC_ADDR_SIZE * 2)
        {
            Json::Value _json;
            _json["Error"] = "Address size not appropriate";
            return _json;
        }
        vector<unsigned char> tmpaddr
            = DataConversion::HexStrToUint8Vec(address);
        Address addr(tmpaddr);
        const Account* account = AccountStore::GetInstance().GetAccount(addr);

        Json::Value ret;
        if (account != nullptr)
        {
            boost::multiprecision::uint256_t balance = account->GetBalance();
            boost::multiprecision::uint256_t nonce = account->GetNonce();

            ret["balance"] = balance.str();
            //FIXME: a workaround, 256-bit unsigned int being truncated
            ret["nonce"] = nonce.convert_to<unsigned int>();
            LOG_GENERAL(INFO,
                        "balance " << balance.str() << " nonce: "
                                   << nonce.convert_to<unsigned int>());
        }
        else if (account == nullptr)
        {
            ret["balance"] = 0;
            ret["nonce"] = 0;
        }

        return ret;
    }
    catch (exception& e)
    {
        LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
        Json::Value _json;
        _json["Error"] = "Unable To Process";

        return _json;
    }
}

string Server::GetStorageAt(const string& address, const string& position)
{
    return "Hello";
}

Json::Value Server::GetTransactionHistory(const string& transactionHash)
{
    return "Hello";
}

string Server::GetBlockTransactionCount(const string& blockHash)
{
    return "Hello";
}

string Server::GetCode(const string& address) { return "Hello"; }

string Server::CreateMessage(const Json::Value& _json) { return "Hello"; }

string Server::GetGasEstimate(const Json::Value& _json) { return "Hello"; }

Json::Value Server::GetTransactionReceipt(const string& transactionHash)
{
    return "Hello";
}

bool Server::isNodeSyncing() { return "Hello"; }

bool Server::isNodeMining() { return "Hello"; }

string Server::GetHashrate() { return "Hello"; }

unsigned int Server::GetNumPeers()
{
    LOG_MARKER();
    unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();
    lock_guard<mutex> g(m_mediator.m_mutexDSCommitteeNetworkInfo);
    return numPeers + m_mediator.m_DSCommitteeNetworkInfo.size();
}

string Server::GetNumTxBlocks()
{
    LOG_MARKER();

    return m_mediator.m_txBlockChain.GetBlockCount().str();
}

string Server::GetNumDSBlocks()
{
    LOG_MARKER();

    return m_mediator.m_dsBlockChain.GetBlockCount().str();
}

string Server::GetNumTransactions()
{
    LOG_MARKER();

    boost::multiprecision::uint256_t currBlock
        = m_mediator.m_txBlockChain.GetBlockCount() - 1;
    if (m_BlockTxPair.first < currBlock)
    {
        for (boost::multiprecision::uint256_t i = m_BlockTxPair.first + 1;
             i <= currBlock; i++)
        {
            m_BlockTxPair.second += m_mediator.m_txBlockChain.GetBlock(i)
                                        .GetHeader()
                                        .GetNumTxs();
        }
    }
    m_BlockTxPair.first = currBlock;

    return m_BlockTxPair.second.str();
}

boost::multiprecision::uint256_t
Server::GetNumTransactions(boost::multiprecision::uint256_t blockNum)
{
    boost::multiprecision::uint256_t currBlockNum
        = m_mediator.m_txBlockChain.GetBlockCount() - 1;

    if (blockNum >= currBlockNum)
    {
        return 0;
    }

    boost::multiprecision::uint256_t i, res = 0;

    for (i = blockNum + 1; i <= currBlockNum; i++)
    {

        res += m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
    }

    return res;
}
double Server::GetTransactionRate()
{
    LOG_MARKER();

    boost::multiprecision::uint256_t refBlockNum
        = m_mediator.m_txBlockChain.GetBlockCount() - 1,
        refTimeTx = 0;

    if (refBlockNum <= REF_BLOCK_DIFF)
    {
        if (refBlockNum <= 1)
        {
            LOG_GENERAL(INFO, "Not enough blocks for information");
            return 0;
        }
        else
        {
            refBlockNum = 1;
            //In case there are less than REF_DIFF_BLOCKS blocks in blockchain, blocknum 1 can be ref block;
        }
    }
    else
    {
        refBlockNum = refBlockNum - REF_BLOCK_DIFF;
    }

    boost::multiprecision::cpp_dec_float_50 numTxns(
        Server::GetNumTransactions(refBlockNum));
    LOG_GENERAL(INFO, "Num Txns: " << numTxns);

    try
    {

        TxBlock tx = m_mediator.m_txBlockChain.GetBlock(refBlockNum);
        refTimeTx = tx.GetHeader().GetTimestamp();
    }
    catch (const char* msg)
    {
        if (string(msg) == "Blocknumber Absent")
        {
            LOG_GENERAL(INFO, "Error in fetching ref block");
        }
        return 0;
    }

    boost::multiprecision::uint256_t TimeDiff
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetTimestamp()
        - refTimeTx;

    if (TimeDiff == 0 || refTimeTx == 0)
    {
        //something went wrong
        LOG_GENERAL(INFO,
                    "TimeDiff or refTimeTx = 0 \n TimeDiff:"
                        << TimeDiff.str() << " refTimeTx:" << refTimeTx.str());
        return 0;
    }
    numTxns = numTxns * 1000000; // conversion from microseconds to seconds
    boost::multiprecision::cpp_dec_float_50 TimeDiffFloat
        = static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
    boost::multiprecision::cpp_dec_float_50 ans = numTxns / TimeDiffFloat;

    return ans.convert_to<double>();
}

double Server::GetDSBlockRate()
{
    LOG_MARKER();

    string numDSblockStr = m_mediator.m_dsBlockChain.GetBlockCount().str();
    boost::multiprecision::cpp_dec_float_50 numDs(numDSblockStr);

    if (m_StartTimeDs == 0) //case when m_StartTime has not been set
    {
        try
        {
            //Refernce time chosen to be the first block's timestamp
            DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
            m_StartTimeDs = dsb.GetHeader().GetTimestamp();
        }
        catch (const char* msg)
        {
            if (string(msg) == "Blocknumber Absent")
            {
                LOG_GENERAL(INFO, "No DSBlock has been mined yet");
            }
            return 0;
        }
    }
    boost::multiprecision::uint256_t TimeDiff
        = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetTimestamp()
        - m_StartTimeDs;

    if (TimeDiff == 0)
    {
        LOG_GENERAL(INFO, "Wait till the second block");
        return 0;
    }
    //To convert from microSeconds to seconds
    numDs = numDs * 1000000;
    boost::multiprecision::cpp_dec_float_50 TimeDiffFloat
        = static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
    boost::multiprecision::cpp_dec_float_50 ans = numDs / TimeDiffFloat;
    return ans.convert_to<double>();
}

double Server::GetTxBlockRate()
{
    LOG_MARKER();

    string numTxblockStr = m_mediator.m_txBlockChain.GetBlockCount().str();
    boost::multiprecision::cpp_dec_float_50 numTx(numTxblockStr);

    if (m_StartTimeTx == 0)
    {
        try
        {
            //Reference Time chosen to be first block's timestamp
            TxBlock txb = m_mediator.m_txBlockChain.GetBlock(1);
            m_StartTimeTx = txb.GetHeader().GetTimestamp();
        }
        catch (const char* msg)
        {
            if (string(msg) == "Blocknumber Absent")
            {
                LOG_GENERAL(INFO, "No TxBlock has been mined yet");
            }
            return 0;
        }
    }
    boost::multiprecision::uint256_t TimeDiff
        = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetTimestamp()
        - m_StartTimeTx;

    if (TimeDiff == 0)
    {
        LOG_GENERAL(INFO, "Wait till the second block");
        return 0;
    }
    //To convert from microSeconds to seconds
    numTx = numTx * 1000000;
    boost::multiprecision::cpp_dec_float_50 TimeDiffFloat(TimeDiff.str());
    boost::multiprecision::cpp_dec_float_50 ans = numTx / TimeDiffFloat;
    return ans.convert_to<double>();
}

string Server::GetCurrentMiniEpoch()
{
    LOG_MARKER();

    return to_string(m_mediator.m_currentEpochNum);
}

string Server::GetCurrentDSEpoch()
{
    LOG_MARKER();

    return m_mediator.m_dsBlockChain.GetLastBlock()
        .GetHeader()
        .GetBlockNum()
        .str();
}

Json::Value Server::DSBlockListing(unsigned int page)
{

    LOG_MARKER();

    boost::multiprecision::uint256_t currBlockNum
        = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    Json::Value _json;

    auto maxPages = (currBlockNum / PAGE_SIZE) + 1;

    _json["maxPages"] = int(maxPages);

    if (m_DSBlockCache.second.size() == 0)
    {
        try
        {
            //add the hash of genesis block
            DSBlockHeader dshead
                = m_mediator.m_dsBlockChain.GetBlock(0).GetHeader();
            SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
            vector<unsigned char> vec;
            dshead.Serialize(vec, 0);
            sha2.Update(vec);
            const vector<unsigned char>& resVec = sha2.Finalize();
            m_DSBlockCache.second.push_back(
                DataConversion::Uint8VecToHexStr(resVec));
        }
        catch (const char* msg)
        {
            _json["Error"] = msg;
            return _json;
        }
    }

    if (page > maxPages || page < 1)
    {
        _json["Error"] = "Pages out of limit";
        return _json;
    }

    if (currBlockNum > m_DSBlockCache.first)
    {
        for (boost::multiprecision::uint256_t i = m_DSBlockCache.first + 1;
             i < currBlockNum; i++)
        {
            m_DSBlockCache.second.push_back(
                m_mediator.m_dsBlockChain.GetBlock(i + 1)
                    .GetHeader()
                    .GetPrevHash()
                    .hex());
        }
        //for the latest block
        DSBlockHeader dshead
            = m_mediator.m_dsBlockChain.GetBlock(currBlockNum).GetHeader();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        dshead.Serialize(vec, 0);
        sha2.Update(vec);
        const vector<unsigned char>& resVec = sha2.Finalize();

        m_DSBlockCache.second.push_back(
            DataConversion::Uint8VecToHexStr(resVec));
        m_DSBlockCache.first = currBlockNum;
    }

    unsigned int offset = PAGE_SIZE * (page - 1);
    Json::Value tmpJson;
    if (page <= NUM_PAGES_CACHE) //can use cache
    {

        boost::multiprecision::uint256_t cacheSize(
            m_DSBlockCache.second.capacity());
        if (cacheSize > m_DSBlockCache.second.size())
        {
            cacheSize = m_DSBlockCache.second.size();
        }

        boost::multiprecision::uint256_t size = m_DSBlockCache.second.size();

        for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
             i++)
        {
            tmpJson.clear();
            tmpJson["Hash"] = m_DSBlockCache.second[size - i - 1];
            tmpJson["BlockNum"] = int(currBlockNum - i);
            _json["data"].append(tmpJson);
        }
    }
    else
    {
        for (boost::multiprecision::uint256_t i = offset;
             i < PAGE_SIZE + offset && i <= currBlockNum; i++)
        {
            tmpJson.clear();
            tmpJson["Hash"]
                = m_mediator.m_dsBlockChain.GetBlock(currBlockNum - i + 1)
                      .GetHeader()
                      .GetPrevHash()
                      .hex();
            tmpJson["BlockNum"] = int(currBlockNum - i);
            _json["data"].append(tmpJson);
        }
    }

    return _json;
}

Json::Value Server::TxBlockListing(unsigned int page)
{
    LOG_MARKER();

    boost::multiprecision::uint256_t currBlockNum
        = m_mediator.m_txBlockChain.GetBlockCount() - 1;
    Json::Value _json;

    auto maxPages = (currBlockNum / PAGE_SIZE) + 1;

    _json["maxPages"] = int(maxPages);

    if (m_TxBlockCache.second.size() == 0)
    {
        try
        {

            //add the hash of genesis block
            TxBlockHeader txhead
                = m_mediator.m_txBlockChain.GetBlock(0).GetHeader();
            SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
            vector<unsigned char> vec;
            txhead.Serialize(vec, 0);
            sha2.Update(vec);
            const vector<unsigned char>& resVec = sha2.Finalize();
            m_TxBlockCache.second.push_back(
                DataConversion::Uint8VecToHexStr(resVec));
        }
        catch (const char* msg)
        {
            _json["Error"] = msg;
            return _json;
        }
    }

    if (page > maxPages || page < 1)
    {
        _json["Error"] = "Pages out of limit";
        return _json;
    }

    if (currBlockNum > m_TxBlockCache.first)
    {
        for (boost::multiprecision::uint256_t i = m_TxBlockCache.first + 1;
             i < currBlockNum; i++)
        {
            m_TxBlockCache.second.push_back(
                m_mediator.m_txBlockChain.GetBlock(i + 1)
                    .GetHeader()
                    .GetPrevHash()
                    .hex());
        }
        //for the latest block
        TxBlockHeader txhead
            = m_mediator.m_txBlockChain.GetBlock(currBlockNum).GetHeader();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        txhead.Serialize(vec, 0);
        sha2.Update(vec);
        const vector<unsigned char>& resVec = sha2.Finalize();

        m_TxBlockCache.second.push_back(
            DataConversion::Uint8VecToHexStr(resVec));
        m_TxBlockCache.first = currBlockNum;
    }

    unsigned int offset = PAGE_SIZE * (page - 1);
    Json::Value tmpJson;
    if (page <= NUM_PAGES_CACHE) //can use cache
    {

        boost::multiprecision::uint256_t cacheSize(
            m_TxBlockCache.second.capacity());

        if (cacheSize > m_TxBlockCache.second.size())
        {
            cacheSize = m_TxBlockCache.second.size();
        }

        boost::multiprecision::uint256_t size = m_TxBlockCache.second.size();

        for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
             i++)
        {
            tmpJson.clear();
            tmpJson["Hash"] = m_TxBlockCache.second[size - i - 1];
            tmpJson["BlockNum"] = int(currBlockNum - i);
            _json["data"].append(tmpJson);
        }
    }
    else
    {

        for (boost::multiprecision::uint256_t i = offset;
             i < PAGE_SIZE + offset && i <= currBlockNum; i++)
        {
            tmpJson.clear();
            tmpJson["Hash"]
                = m_mediator.m_txBlockChain.GetBlock(currBlockNum - i + 1)
                      .GetHeader()
                      .GetPrevHash()
                      .hex();
            tmpJson["BlockNum"] = int(currBlockNum - i);
            _json["data"].append(tmpJson);
        }
    }

    return _json;
}

Json::Value Server::GetBlockchainInfo()
{
    Json::Value _json;

    _json["NumPeers"] = Server::GetNumPeers();
    _json["NumTxBlocks"] = Server::GetNumTxBlocks();
    _json["NumDSBlocks"] = Server::GetNumDSBlocks();
    _json["NumTransactions"] = Server::GetNumTransactions();
    _json["TransactionRate"] = Server::GetTransactionRate();
    _json["TxBlockRate"] = Server::GetTxBlockRate();
    _json["DSBlockRate"] = Server::GetDSBlockRate();
    _json["CurrentMiniEpoch"] = Server::GetCurrentMiniEpoch();
    _json["CurrentDSEpoch"] = Server::GetCurrentDSEpoch();

    return _json;
}

Json::Value Server::GetRecentTransactions()
{
    LOG_MARKER();

    lock_guard<mutex> g(m_mutexRecentTxns);
    Json::Value _json;
    boost::multiprecision::uint256_t actualSize(
        m_RecentTransactions.capacity());
    if (actualSize > m_RecentTransactions.size())
    {
        actualSize = m_RecentTransactions.size();
    }
    boost::multiprecision::uint256_t size = m_RecentTransactions.size();
    _json["number"] = int(actualSize);
    _json["TxnHashes"] = Json::Value(Json::arrayValue);
    for (boost::multiprecision::uint256_t i = 0; i < actualSize; i++)
    {
        _json["TxnHashes"].append(m_RecentTransactions[size - i - 1]);
    }

    return _json;
}

void Server::AddToRecentTransactions(const TxnHash& txhash)
{
    lock_guard<mutex> g(m_mutexRecentTxns);
    m_RecentTransactions.push_back(txhash.hex());
}
Json::Value Server::GetShardingStructure()
{
    LOG_MARKER();

    try
    {
        Json::Value _json;
        vector<map<PubKey, Peer>> shards = m_mediator.m_lookup->GetShardPeers();
        unsigned int num_shards = shards.size();

        if (num_shards == 0)
        {
            _json["Error"] = "No shards yet";
            return _json;
        }
        else
        {
            for (unsigned int i = 0; i < num_shards; i++)
            {
                _json["NumPeers"].append(
                    static_cast<unsigned int>(shards[i].size()));
            }
        }
        return _json;
    }
    catch (exception& e)
    {
        Json::Value _json;
        _json["Error"] = "Unable to process ";
        LOG_GENERAL(WARNING, e.what());
        return _json;
    }
}

uint32_t Server::GetNumTxnsTxEpoch()
{
    LOG_MARKER();

    try
    {
        return m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetNumTxs();
    }
    catch (exception& e)
    {
        LOG_GENERAL(WARNING, e.what());
        return 0;
    }
}

string Server::GetNumTxnsDSEpoch()
{
    LOG_MARKER();

    try
    {

        auto latestTxBlock
            = m_mediator.m_txBlockChain.GetLastBlock().GetHeader();
        auto latestTxBlockNum = latestTxBlock.GetBlockNum();
        auto latestDSBlockNum = latestTxBlock.GetDSBlockNum();

        if (latestTxBlockNum > m_TxBlockCountSumPair.first)
        {

            //Case where the DS Epoch is same
            if (m_mediator.m_txBlockChain.GetBlock(m_TxBlockCountSumPair.first)
                    .GetHeader()
                    .GetDSBlockNum()
                == latestDSBlockNum)
            {
                for (auto i = latestTxBlockNum; i > m_TxBlockCountSumPair.first;
                     i--)
                {
                    m_TxBlockCountSumPair.second
                        += m_mediator.m_txBlockChain.GetBlock(i)
                               .GetHeader()
                               .GetNumTxs();
                }
            }
            //Case if DS Epoch Changed
            else
            {
                m_TxBlockCountSumPair.second = 0;

                for (auto i = latestTxBlockNum; i > m_TxBlockCountSumPair.first;
                     i--)
                {
                    if (m_mediator.m_txBlockChain.GetBlock(i)
                            .GetHeader()
                            .GetDSBlockNum()
                        < latestDSBlockNum)
                    {
                        break;
                    }
                    m_TxBlockCountSumPair.second
                        += m_mediator.m_txBlockChain.GetBlock(i)
                               .GetHeader()
                               .GetNumTxs();
                }
            }

            m_TxBlockCountSumPair.first = latestTxBlockNum;
        }

        return m_TxBlockCountSumPair.second.str();
    }

    catch (exception& e)
    {
        LOG_GENERAL(WARNING, e.what());
        return "0";
    }
}

#endif //IS_LOOKUP_NODE
