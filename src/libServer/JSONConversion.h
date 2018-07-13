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

#ifndef __JSONCONVERSION_H__
#define __JSONCONVERSION_H__

#include <array>
#include <json/json.h>
#include <vector>

#include "libData/BlockData/Block.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"

class JSONConversion
{
public:
    //converts an bool array to JSON array containing 1 and 0
    static const Json::Value convertBoolArraytoJson(const std::vector<bool>& v);
    //converts a TxnHash array to JSON array containing TxnHash strings
    static const Json::Value
    convertTxnHashArraytoJson(const std::vector<TxnHash>& v);
    static const Json::Value
    convertTxnHashArraytoJson(const std::vector<MicroBlockHashSet>& v);
    //converts a MicroBlockHashSet to JSON array containing TxnHash and StateDeltaHash strings
    //do consider replacing convertTxnHashArraytoJson with this
    static const Json::Value
    convertMicroBlockHashSettoJson(const std::vector<MicroBlockHashSet>& v);
    //converts a TxBlock to JSON object
    static const Json::Value convertTxBlocktoJson(const TxBlock& txblock);
    //converts a DSBlocck to JSON object
    static const Json::Value convertDSblocktoJson(const DSBlock& dsblock);
    //converts a JSON to Tx
    static const Transaction convertJsontoTx(const Json::Value& _json);
    //check if a Json is a valid Tx
    static const bool checkJsonTx(const Json::Value& _json);
    //Convert a Tx to JSON object
    static const Json::Value convertTxtoJson(const Transaction& tx);
};

#endif // __JSONCONVERSION_H__
