#include "libData/BlockData/Block.h"
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"
#include <array>
#include <string>
#include <thread>
#include <vector>

#include "common/Messages.h"
#include "common/Serializable.h"
#include "common/Constants.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Address.h"
#include "libData/BlockData/Block.h"
#include "libData/AccountData/Transaction.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <jsoncpp/json/json.h>
#include "libUtils/TimeUtils.h"


using namespace std;
using namespace boost::multiprecision;


DSBlock returnDummyBlock()
{
	
	BlockHash prevHash1;

    for (unsigned int i = 0; i < prevHash1.asArray().size(); i++)
    {
        prevHash1.asArray().at(i) = i + 1;
    }

    std::array<unsigned char, BLOCK_SIG_SIZE> signature1;

    for (unsigned int i = 0; i < signature1.size(); i++)
    {
        signature1.at(i) = i + 8;
    }

    std::pair<PrivKey, PubKey> pubKey1 = Schnorr::GetInstance().GenKeyPair();
 
    DSBlockHeader header1(20, prevHash1, 12344, pubKey1.first, pubKey1.second, 0, 789);

    DSBlock dsblock(header1, signature1);

    return dsblock;
}



TxBlock createBlock()
{

	std::pair<PrivKey, PubKey> pubKey1 = Schnorr::GetInstance().GenKeyPair();

    TxBlockHeader header(TXBLOCKTYPE::FINAL, BLOCKVERSION::VERSION1, 1, 1, BlockHash(), 0, 
                            get_time_as_int(), TxnHash(), StateHash(), 0, 5, pubKey1.second, 0, BlockHash());
    
    array<unsigned char, BLOCK_SIG_SIZE> emptySig = { 0 };

    std::vector<TxnHash> tranHashes;

    for(int i=0; i<5; i++)
    {
        tranHashes.push_back(TxnHash());
    }

    TxBlock txblock(header, emptySig, vector<bool>(), tranHashes);


    return txblock;
}

Json::Value convertBoolArraytoJson(vector<bool> v)
{
	Json::Value jsonBool;
	for(auto i:v)
	{
		jsonBool.append(i?1:0);
	}
	return jsonBool;
}


Json::Value convertTxnHashArraytoJson(vector<TxnHash> v)
{
	Json::Value jsonTxnHash;

	for(auto i:v)
	{
		jsonTxnHash.append(i.hex());
	}
	return jsonTxnHash;
}


Json::Value convertTxBlocktoJson(TxBlock txblock)
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

Json::Value convertDSblocktoJson(DSBlock dsblock)
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

// Json::Value test_ServerFunc()
// {
// 	DSBlock A = returnDummyBlock();
// 	Json::Value B = convertDSblocktoJSON(A);


// 	return B;
// }


