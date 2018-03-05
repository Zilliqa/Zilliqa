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


#include <iostream>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "Server.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libUtils/Logger.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "common/Serializable.h"
#include "common/Messages.h"

using namespace jsonrpc;
using namespace std;

#ifdef IS_LOOKUP_NODE 

Server::Server(Mediator & mediator) : AbstractZServer(*(new HttpServer(4201))), m_mediator(mediator)
{
	// constructor
}

Server::~Server() 
{
	// destructor
}

string Server::getClientVersion()
{
	return "Hello";
}

string Server::getNetworkId()
{
	return "TestNet";
}

string Server::getProtocolVersion()
{
	return "Hello";
}

string Server::createTransaction(const Json::Value& _json)
{
	LOG_MARKER();

	//dummy transaction
	uint32_t version = 1;
	boost::multiprecision::uint256_t nonce = 0;
	boost::multiprecision::uint256_t amount = 289;
	Address toAddr;
	for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }
    std::array<unsigned char, TRAN_SIG_SIZE> signature;

    for (unsigned int i = 0; i < signature.size(); i++)
    {
        signature.at(i) = 2;
    }
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    Transaction tx(version, nonce, toAddr, pubKey, amount, signature);

    LOG_MESSAGE("Created Dummy Tx");


    //[TODO] Function to convert Json to Transaction

    unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

    
    const PubKey & senderPubKey = tx.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);
    unsigned int curr_offset = 0;
    
    if(num_shards>0)
    {
    	map <PubKey, Peer> shardMembers = m_mediator.m_lookup->GetShardPeers()[shard];
    	LOG_MESSAGE("The Tx Belongs to "<<shard<<" Shard");

    	vector<unsigned char> tx_message = {MessageType::NODE, NodeInstructionType::CREATETRANSACTIONFROMLOOKUP};
    	curr_offset += MessageOffset::BODY;

    	tx.Serialize(tx_message, curr_offset);

    	LOG_MESSAGE("Tx Serialized");


    	P2PComm::GetInstance().SendMessage(shardMembers.begin()->second, tx_message);

    	LOG_MESSAGE("Message sent to "<<shardMembers.begin()->second<<" ");
    }
    else
    {
    	LOG_MESSAGE("No shards yet");

    	return "Could Not Create Transaction";
    }

    
   	return tx.GetTranID().hex(); 
}

Json::Value Server::getTransaction(const string & transactionHash)
{
	return "Hello";
}

Json::Value Server::getDsBlock(const string & blockHash)
{
	return "Hello";
}

Json::Value Server::getTxBlock(const string & blockHash)
{
	return "Hello";
}

string Server::getGasPrice()
{
	return "Hello";
}

Json::Value Server::getLatestDsBlock()
{
	return "1";
}

Json::Value Server::getLatestTxBlock()
{
	return "Hello";
}

Json::Value Server::getBalance(const string & address)
{
	vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
	Address addr(tmpaddr);
	const Account* account = AccountStore::GetInstance().GetAccount(addr);

	Json::Value ret;
	if(account != nullptr)
	{
		boost::multiprecision::uint256_t balance = account->GetBalance();
		boost::multiprecision::uint256_t nonce = account->GetNonce();

		ret["balance"] = balance.str();
		ret["nonce"] = nonce.str();
	}
	else if(account == nullptr)
	{
		ret["balance"] = 0;
		ret["nonce"] = 0;
	}

	return ret;
}

string Server::getStorageAt(const string & address, const string & position)
{
	return "Hello";
}

Json::Value Server::getTransactionHistory(const string & transactionHash)
{
	return "Hello";
}

string Server::getBlockTransactionCount(const string & blockHash)
{
	return "Hello";
}

string Server::getCode(const string & address)
{
	return "Hello";
}

string Server::createMessage(const Json::Value &_json)
{
	return "Hello";
}

string Server::getGasEstimate(const Json::Value &_json)
{
	return "Hello";
}

Json::Value Server::getTransactionReceipt(const string & transactionHash)
{
	return "Hello";
}

bool Server::isNodeSyncing()
{
	return "Hello";
}

bool Server::isNodeMining()
{
	return "Hello";
}

string Server::getHashrate()
{
	return "Hello";
}


#endif //IS_LOOKUP_NODE