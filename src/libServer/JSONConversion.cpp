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

#include "JSONConversion.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"
#include "libData/BlockData/Block.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int JSON_TRAN_OBJECT_SIZE = 8;

const Json::Value JSONConversion::convertBoolArraytoJson(const vector<bool>& v)
{
    Json::Value jsonBool;
    for (auto i : v)
    {
        jsonBool.append(i ? 1 : 0);
    }
    return jsonBool;
}

const Json::Value
JSONConversion::convertTxnHashArraytoJson(const vector<TxnHash>& v)
{
    Json::Value jsonTxnHash;

    for (auto i : v)
    {
        jsonTxnHash.append(i.hex());
    }
    return jsonTxnHash;
}

const Json::Value JSONConversion::convertTxBlocktoJson(const TxBlock& txblock)
{
    Json::Value ret;
    Json::Value ret_head;
    Json::Value ret_body;

    const TxBlockHeader& txheader = txblock.GetHeader();

    ret_head["type"] = txheader.GetType();
    ret_head["version"] = txheader.GetVersion();
    ret_head["GasLimit"] = txheader.GetGasLimit().str();
    ret_head["GasUsed"] = txheader.GetGasUsed().str();

    ret_head["prevBlockHash"] = txheader.GetPrevHash().hex();
    ret_head["BlockNum"] = txheader.GetBlockNum().str();
    ret_head["Timestamp"] = txheader.GetTimestamp().str();

    ret_head["TxnHash"] = txheader.GetTxRootHash().hex();
    ret_head["StateHash"] = txheader.GetStateRootHash().hex();
    ret_head["NumTxns"] = txheader.GetNumTxs();
    ret_head["NumMicroBlocks"] = txheader.GetNumMicroBlockHashes();

    ret_head["MinerPubKey"] = static_cast<string>(txheader.GetMinerPubKey());
    ret_head["DSBlockNum"] = txheader.GetDSBlockNum().str();

    ret_body["HeaderSign"]
        = DataConversion::SerializableToHexStr(txblock.GetCS2());

    ret_body["MicroBlockEmpty"]
        = convertBoolArraytoJson(txblock.GetIsMicroBlockEmpty());

    ret_body["MicroBlockHashes"]
        = convertTxnHashArraytoJson(txblock.GetMicroBlockHashes());

    ret["header"] = ret_head;
    ret["body"] = ret_body;

    return ret;
}

const Json::Value JSONConversion::convertDSblocktoJson(const DSBlock& dsblock)
{

    Json::Value ret;
    Json::Value ret_header;
    Json::Value ret_sign;

    const DSBlockHeader& dshead = dsblock.GetHeader();

    ret_sign = DataConversion::SerializableToHexStr(dsblock.GetCS2());

    ret_header["difficulty"] = dshead.GetDifficulty();
    ret_header["prevhash"] = dshead.GetPrevHash().hex();
    ret_header["nonce"] = dshead.GetNonce().str();
    ret_header["minerPubKey"] = static_cast<string>(dshead.GetMinerPubKey());
    ret_header["leaderPubKey"] = static_cast<string>(dshead.GetLeaderPubKey());
    ret_header["blockNum"] = dshead.GetBlockNum().str();
    ret_header["timestamp"] = dshead.GetTimestamp().str();

    ret["header"] = ret_header;

    ret["signature"] = ret_sign;

    return ret;
}

const Transaction JSONConversion::convertJsontoTx(const Json::Value& _json)
{

    uint32_t version = _json["version"].asUInt();

    string nonce_str = _json["nonce"].asString();
    uint256_t nonce(nonce_str);

    string toAddr_str = _json["to"].asString();
    vector<unsigned char> toAddr_ser
        = DataConversion::HexStrToUint8Vec(toAddr_str);
    Address toAddr(toAddr_ser);

    string amount_str = _json["amount"].asString();
    uint256_t amount(amount_str);

    string pubKey_str = _json["pubKey"].asString();
    vector<unsigned char> pubKey_ser
        = DataConversion::HexStrToUint8Vec(pubKey_str);
    // TODO: Handle exceptions
    PubKey pubKey(pubKey_ser, 0);

    string sign_str = _json["signature"].asString();
    array<unsigned char, TRAN_SIG_SIZE> sign
        = DataConversion::HexStrToStdArray64(sign_str);

    Transaction tx1(version, nonce, toAddr, pubKey, amount, sign);
    LOG_GENERAL(INFO, "Tx converted");

    return tx1;
}

const bool JSONConversion::checkJsonTx(const Json::Value& _json)
{
    bool ret = true;

    ret = ret && _json.isObject();
    ret = ret && (_json.size() == JSON_TRAN_OBJECT_SIZE);
    ret = ret && _json.isMember("nonce");
    ret = ret && _json.isMember("to");
    ret = ret && _json.isMember("amount");
    ret = ret && _json.isMember("pubKey");
    ret = ret && _json.isMember("signature");
    ret = ret && _json.isMember("version");

    if (ret)
    {
        if (!_json["nonce"].isIntegral())
        {
            LOG_GENERAL(INFO, "Fault in nonce");
            return false;
        }
        if (!_json["amount"].isIntegral())
        {
            LOG_GENERAL(INFO, "Fault in amount");
            return false;
        }
        if (!_json["version"].isIntegral())
        {
            LOG_GENERAL(INFO, "Fault in version");
            return false;
        }
        if (_json["pubKey"].asString().size() != PUB_KEY_SIZE * 2)
        {
            LOG_GENERAL(INFO,
                        "PubKey size wrong "
                            << _json["pubKey"].asString().size());
            return false;
        }
        if (_json["signature"].asString().size() != TRAN_SIG_SIZE * 2)
        {
            LOG_GENERAL(INFO,
                        "signature size wrong "
                            << _json["signature"].asString().size());
            return false;
        }
        if (_json["to"].asString().size() != ACC_ADDR_SIZE * 2)
        {
            LOG_GENERAL(INFO,
                        "To Address size wrong "
                            << _json["signature"].asString().size());
            return false;
        }
    }
    else
    {
        LOG_GENERAL(INFO, "Json Data Object has missing components");
    }

    return ret;
}

const Json::Value JSONConversion::convertTxtoJson(const Transaction& tx)
{
    Json::Value _json;

    _json["ID"] = tx.GetTranID().hex();
    _json["version"] = tx.GetVersion();
    _json["nonce"] = tx.GetNonce().str();
    _json["toAddr"] = tx.GetToAddr().hex();
    _json["senderPubKey"] = static_cast<string>(tx.GetSenderPubKey());
    _json["amount"] = tx.GetAmount().str();
    _json["signature"] = DataConversion::charArrToHexStr(tx.GetSignature());

    return _json;
}
