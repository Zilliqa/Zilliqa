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
#include <jsonrpccpp/common/exception.h>
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "common/Serializable.h"

#include "Server.h"

using namespace jsonrpc;
using namespace std;
using namespace boost::multiprecision;


Server::Server() : AbstractZServer(*(new HttpServer(4201)))
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
	return "1";
}

Json::Value Server::getLatestTxBlock()
{
	return "Hello";
}

Json::Value Server::getBalance(const string & address)
{
	return "Hello";
	// vector<unsigned char> input_address = DataConversion::HexStrToUint8Vec(address);
	// PrivKey privKey(tmpprivkey, 0);

	// std::vector<unsigned char> input_address(address.begin(), address.end());
	// const PrivKey priv(input_address, 0);
	// const PubKey pub(priv);
	// Address addr = Account::GetAddressFromPublicKey(pub);
	// Account* account = AccountStore::GetInstance().GetAccount(addr);

	// if(account != nullptr)
 //    {
 //    	Json::Value ret;
 //        ret["balance"] = 24;//account->GetBalance();
 //        ret["nonce"] = 0;//account->GetNonce();

 //        return ret;
 //    }

 //    return 0;
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
