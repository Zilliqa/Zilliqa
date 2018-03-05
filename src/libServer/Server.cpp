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

#include "ServerFunc.h"

#include <iostream>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "common/Serializable.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libUtils/Logger.h"
#include "Server.h"



using namespace jsonrpc;
using namespace std;


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
	return "Hello";
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
	LOG_MARKER();
	DSBlock Latest = m_mediator.m_dsBlockChain.GetLastBlock();
	
	LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Call to getLatestDsBlock, BlockNum "<<Latest.GetHeader().GetBlockNum().str()<<"  Timestamp: 		"<<Latest.GetHeader().GetTimestamp().str());
	
	return ServerFunc::convertDSblocktoJson(Latest);
}

Json::Value Server::getLatestTxBlock()
{
	LOG_MARKER();
	TxBlock Latest = m_mediator.m_txBlockChain.GetLastBlock();

	LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Call to getLatestTxBlock, BlockNum "<<Latest.GetHeader().GetBlockNum().str()<<"  Timestamp: 		"<<Latest.GetHeader().GetTimestamp().str());
	
	return ServerFunc::convertTxBlocktoJson(Latest);
}

Json::Value Server::getBalance(const string & address)
{
	LOG_MARKER();
	vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
	Address addr(tmpaddr);
	boost::multiprecision::uint256_t balance = AccountStore::GetInstance().GetBalance(addr);

	Json::Value ret;
	ret["balance"] = balance.str();
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


// int main()
// {
// 	HttpServer httpserver(4201);
// 	Server s(httpserver);
// 	s.StartListening();
// 	getchar();
// 	s.StopListening();
// 	return 0;
// }
