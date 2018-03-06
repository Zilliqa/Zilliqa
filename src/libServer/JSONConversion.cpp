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
#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <vector>

#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"
#include "libData/BlockData/Block.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "JSONConversion.h"



using namespace std;
using namespace boost::multiprecision;


const Json::Value JSONConversion::convertBoolArraytoJson(const vector<bool> & v)
{
	Json::Value jsonBool;
	for(auto i:v)
	{
		jsonBool.append(i?1:0);
	}
	return jsonBool;
}


const Json::Value JSONConversion::convertTxnHashArraytoJson(const vector<TxnHash> & v)
{
	Json::Value jsonTxnHash;

	for(auto i:v)
	{
		jsonTxnHash.append(i.hex());
	}
	return jsonTxnHash;
}


const Json::Value JSONConversion::convertTxBlocktoJson(const TxBlock & txblock)
{
	Json::Value ret;
	Json::Value ret_head;
	Json::Value ret_body;

	TxBlockHeader txheader = txblock.GetHeader();

	ret_head["type"] = txheader.GetType();
	ret_head["version"] = txheader.GetVersion();
	ret_head["GasLimit"]= txheader.GetGasLimit().str();
	ret_head["GasUsed"] = txheader.GetGasUsed().str();

	ret_head["prevBlockHash"]= txheader.GetPrevHash().hex();
	ret_head["BlockNum"]=txheader.GetBlockNum().str();
	ret_head["Timestamp"]=txheader.GetTimestamp().str();

	ret_head["TxnHash"] = txheader.GetTxRootHash().hex();
	ret_head["StateHash"] = txheader.GetStateRootHash().hex();
	ret_head["NumTxns"] = txheader.GetNumTxs();
	ret_head["NumMicroBlocks"]=txheader.GetNumMicroBlockHashes();

	ret_head["MinerPubKey"] = static_cast<string>(txheader.GetMinerPubKey());
	ret_head["DSBlockNum"] = txheader.GetDSBlockNum().str();
	
	ret_body["HeaderSign"] = DataConversion::charArrToHexStr(txblock.GetHeaderSig()); 

	
	ret_body["MicroBlockEmpty"] = convertBoolArraytoJson(txblock.GetIsMicroBlockEmpty());

	ret_body["MicroBlockHashes"] = convertTxnHashArraytoJson(txblock.GetMicroBlockHashes());

	ret["header"] = ret_head;
	ret["body"] = ret_body;

	return ret;


}

const Json::Value JSONConversion::convertDSblocktoJson(const DSBlock & dsblock)
{

	Json::Value ret;
	Json::Value ret_header;
	Json::Value ret_sign;

	DSBlockHeader dshead = dsblock.GetHeader();


	ret_sign = DataConversion::charArrToHexStr(dsblock.GetSignature());

	ret_header["difficulty"] = dshead.GetDifficulty();
	ret_header["prevhash"]  = dshead.GetPrevHash().hex();
	ret_header["nonce"] = dshead.GetNonce().str();
	ret_header["minerPubKey"] = static_cast<string>(dshead.GetMinerPubKey());
	ret_header["leaderPubKey"] = static_cast<string>(dshead.GetLeaderPubKey());
	ret_header["blockNum"] = dshead.GetBlockNum().str();
	ret_header["timestamp"] = dshead.GetTimestamp().str();


	ret["header"] = ret_header;

	ret["signature"] = ret_sign;

	return ret;

}

const Transaction JSONConversion::convertJsontoTx(const Json::Value & _json)
{
	//LOG_MARKER();
	

	
	string nonce_str = _json["nonce"].asString();
	uint256_t nonce(nonce_str);

	string toAddr_str = _json["to"].asString();
	vector<unsigned char> toAddr_ser = DataConversion::HexStrToUint8Vec(toAddr_str);
	Address toAddr(toAddr_ser);
	//LOG_MESSAGE("toAddr size: "<<toAddr_ser.size());
		
	string amount_str = _json["amount"].asString();
	uint256_t amount(amount_str);

	string pubKey_str = _json["pubKey"].asString();
	vector <unsigned char> pubKey_ser = DataConversion::HexStrToUint8Vec(pubKey_str);
	PubKey pubKey(pubKey_ser,0);
	//LOG_MESSAGE("PubKey size: "<<pubKey_ser.size());
		
	string sign_str = _json["signature"].asString();
	array <unsigned char,TRAN_SIG_SIZE> sign = DataConversion::HexStrToStdArray64(sign_str);

	//LOG_MESSAGE("Sign size: "<<sign.size());

	Transaction tx1(1,nonce,toAddr,pubKey,amount,sign);
	LOG_MESSAGE("Tx converted");

	return tx1;
	
}

const bool JSONConversion::checkJsonTx(const Json::Value & _json)
{
	bool ret = true;

	ret &= _json.isObject();
	ret &= (_json.size() == JSON_TRAN_SIZE);
	ret &= _json.isMember("nonce");
	ret &= _json.isMember("to");
	ret &= _json.isMember("amount");
	ret &= _json.isMember("pubKey");
	ret &= _json.isMember("signature");
	
	if(ret)
	{
		if(!_json["nonce"].isIntegral())
		{
			LOG_MESSAGE("Fault in nonce");
			return false;
		}
		if(!_json["amount"].isIntegral())
		{
			LOG_MESSAGE("Fault in amount");
			return false;
		}
		if(_json["pubKey"].asString().size() != PUB_KEY_SIZE*2 )
		{
			LOG_MESSAGE("PubKey size wrong "<<_json["pubKey"].asString().size());
			return false;
		}
		if(_json["signature"].asString().size() != TRAN_SIG_SIZE*2 )
		{
			LOG_MESSAGE("signature size wrong "<<_json["signature"].asString().size());
			return false;
		}
		if(_json["to"].asString().size() != ACC_ADDR_SIZE*2)
		{
			LOG_MESSAGE("To Address size wrong "<<_json["signature"].asString().size());
			return false;
		}

	}
	else
	{
		LOG_MESSAGE("Json Data Object has missing components");
	}
	
	return ret;
}