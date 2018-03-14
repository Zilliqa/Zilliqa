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

#include <iostream>
#include <boost/multiprecision/float128.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <deque>
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
#include "libUtils/TimeUtils.h"
#include "common/Serializable.h"
#include "common/Messages.h"

using namespace jsonrpc;
using namespace std;



Server::Server(Mediator & mediator, HttpServer & httpserver) : AbstractZServer(httpserver), m_mediator(mediator), m_BlockTxPair(0,0)
{
	m_StartTimeTx = 0;
	m_StartTimeDs = 0;
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

	if(!JSONConversion::checkJsonTx(_json))
	{
		return "Invalid Tx Json";
	}

	Transaction tx = JSONConversion::convertJsontoTx(_json);

	if(!Transaction::Verify(tx))
	{
		return "Signature incorrect";
	}
 
	//LOG_MESSAGE("Nonce: "<<tx.GetNonce().str()<<" toAddr: "<<tx.GetToAddr().hex()<<" senderPubKey: "<<static_cast<string>(tx.GetSenderPubKey());<<" amount: "<<tx.GetAmount().str());

    unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

    
    const PubKey & senderPubKey = tx.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int curr_offset = 0;
    
    if( num_shards > 0 )
    {
    	unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);
    	map <PubKey, Peer> shardMembers = m_mediator.m_lookup->GetShardPeers().at(shard);
    	LOG_MESSAGE("The Tx Belongs to "<<shard<<" Shard");

    	vector<unsigned char> tx_message = {MessageType::NODE, NodeInstructionType::CREATETRANSACTIONFROMLOOKUP};
    	curr_offset += MessageOffset::BODY;

    	tx.Serialize(tx_message, curr_offset);

    	LOG_MESSAGE("Tx Serialized");

    	vector<Peer> toSend;

    	auto it = shardMembers.begin();
    	for(unsigned int i = 0; i < NUM_PEERS_TO_SEND_IN_A_SHARD && it!=shardMembers.end() ; i++, it++ )
    	{
    		toSend.push_back(it->second);

    	}
	
	

    	P2PComm::GetInstance().SendMessage(toSend, tx_message);

    	
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
	LOG_MARKER();
	DSBlock Latest = m_mediator.m_dsBlockChain.GetLastBlock();
	
	LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "BlockNum "<<Latest.GetHeader().GetBlockNum().str()<<"  Timestamp: 		"<<Latest.GetHeader().GetTimestamp().str());
	
	return JSONConversion::convertDSblocktoJson(Latest);
}

Json::Value Server::getLatestTxBlock()
{
	LOG_MARKER();
	TxBlock Latest = m_mediator.m_txBlockChain.GetLastBlock();

	LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "BlockNum "<<Latest.GetHeader().GetBlockNum().str()<<"  Timestamp: 		"<<Latest.GetHeader().GetTimestamp().str());
	
	return JSONConversion::convertTxBlocktoJson(Latest);
}

Json::Value Server::getBalance(const string & address)
{
	LOG_MARKER();
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


unsigned int Server::getNumPeers()
{
	LOG_MARKER();
	unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();

	return numPeers;
}

string Server::getNumTxBlocks()
{
	LOG_MARKER();

	return m_mediator.m_txBlockChain.GetBlockCount().str();
}

string Server::getNumDSBlocks()
{
	LOG_MARKER();

	return m_mediator.m_dsBlockChain.GetBlockCount().str();
}

string Server::getNumTransactions()
{
	LOG_MARKER();

	
	boost::multiprecision::uint256_t currBlock = m_mediator.m_txBlockChain.GetBlockCount() - 1;
	LOG_MESSAGE("currBlock: "<<currBlock.str()<<" State: "<<m_BlockTxPair.first);
	if(m_BlockTxPair.first < currBlock)
	{
		for(boost::multiprecision::uint256_t i = m_BlockTxPair.first + 1 ; i<=currBlock ; i++)
		{
			m_BlockTxPair.second += m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();

		}
	}
	m_BlockTxPair.first = currBlock;

	return m_BlockTxPair.second.str();

}

double Server::getTransactionRate()
{
	LOG_MARKER();

	string numTxStr = Server::getNumTransactions();
	boost::multiprecision::float128 numTxns(numTxStr);
	LOG_MESSAGE("Num Txns: "<< numTxns);

	numTxns = numTxns*1000000; // conversion from microseconds to seconds

	//LOG_MESSAGE("TxBlockStart: "<<m_StartTimeTx<<" NumTxns: "<<numTxns);

	if(m_StartTimeTx == 0) //case when m_StartTime has not been set 
	{
		try
		{
			TxBlock tx  = m_mediator.m_txBlockChain.GetBlock(1);
			m_StartTimeTx = tx.GetHeader().GetTimestamp();
		}
		catch(const char* msg)
		{
			//cannot set
			if(strcmp(msg,"Blocknumber Absent") == 0)
			{
				LOG_MESSAGE("No Tx Block has been mined yet");
			}
			return 0;
		}
		
	}

	boost::multiprecision::uint256_t TimeDiff = m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetTimestamp()-m_StartTimeTx;

	

	if(TimeDiff == 0)
	{
		return 0;
	}

	boost::multiprecision::float128 TimeDiffFloat(TimeDiff.str());
	//Is there any loss in data?
	boost::multiprecision::float128 ans = numTxns/TimeDiffFloat;

	//LOG_MESSAGE("Rate: "<<ans);

	return ans.convert_to<double>();
	
	
}

double Server::getDSBlockRate()
{
	LOG_MARKER();

	string numDSblockStr = m_mediator.m_dsBlockChain.GetBlockCount().str();
	boost::multiprecision::float128 numDs(numDSblockStr);

	if(m_StartTimeDs == 0)
	{
		try
		{
			DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
			m_StartTimeDs = dsb.GetHeader().GetTimestamp();
		}
		catch(const char *msg)
		{
			if(strcmp(msg, "Blocknumber Absent") == 0)
			{
				LOG_MESSAGE("No DSBlock has been mined yet")
			}
			return 0;
		}
	}
	boost::multiprecision::uint256_t TimeDiff = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetTimestamp() - m_StartTimeDs;

	if(TimeDiff == 0)
	{
		LOG_MESSAGE("Wait till the second block");
		return 0;
	}

	boost::multiprecision::float128 TimeDiffFloat(TimeDiff.str());
	boost::multiprecision::float128 ans = numDs/TimeDiffFloat;

	return ans.convert_to<double>();


}

double Server::getTxBlockRate()
{
	LOG_MARKER();

	string numTxblockStr = m_mediator.m_txBlockChain.GetBlockCount().str();
	boost::multiprecision::float128 numTx(numTxblockStr);

	if(m_StartTimeTx == 0)
	{
		try
		{
			TxBlock txb = m_mediator.m_txBlockChain.GetBlock(1);
			m_StartTimeTx = txb.GetHeader().GetTimestamp();
		}
		catch(const char *msg)
		{
			if(strcmp(msg, "Blocknumber Absent") == 0)
			{
				LOG_MESSAGE("No TxBlock has been mined yet")
			}
			return 0;
		}
	}
	boost::multiprecision::uint256_t TimeDiff = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetTimestamp() - m_StartTimeTx;

	if(TimeDiff == 0)
	{
		LOG_MESSAGE("Wait till the second block");
		return 0;
	}

	boost::multiprecision::float128 TimeDiffFloat(TimeDiff.str());
	boost::multiprecision::float128 ans = numTx/TimeDiffFloat;

	return ans.convert_to<double>();


}

#endif //IS_LOOKUP_NODE