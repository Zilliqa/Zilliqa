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
#include "JSONConversion.h"



using namespace std;
using namespace boost::multiprecision;


const Json::Value JSONConversion::convertBoolArraytoJson(vector<bool> v)
{
	Json::Value jsonBool;
	for(auto i:v)
	{
		jsonBool.append(i?1:0);
	}
	return jsonBool;
}


const Json::Value JSONConversion::convertTxnHashArraytoJson(vector<TxnHash> v)
{
	Json::Value jsonTxnHash;

	for(auto i:v)
	{
		jsonTxnHash.append(i.hex());
	}
	return jsonTxnHash;
}


const Json::Value JSONConversion::convertTxBlocktoJson(TxBlock txblock)
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
	
	string str(txblock.GetHeaderSig().begin(),txblock.GetHeaderSig().end());
	ret_body["HeaderSign"] = str; 

	
	ret_body["MicroBlockEmpty"] = convertBoolArraytoJson(txblock.GetIsMicroBlockEmpty());

	ret_body["MicroBlockHashes"] = convertTxnHashArraytoJson(txblock.GetMicroBlockHashes());

	ret["header"] = ret_head;
	ret["body"] = ret_body;

	return ret;


}

const Json::Value JSONConversion::convertDSblocktoJson(DSBlock dsblock)
{

	Json::Value ret;
	Json::Value ret_header;
	Json::Value ret_sign;

	DSBlockHeader dshead = dsblock.GetHeader();

	string sign(dsblock.GetSignature().begin(),dsblock.GetSignature().end());

	ret_sign = sign;

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