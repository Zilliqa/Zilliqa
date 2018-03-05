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
#include "libData/BlockChainData/DSBlockChain.h"
#include "libData/BlockChainData/TxBlockChain.h"

using namespace std;
using namespace boost::multiprecision;

class JSONConversion
{
public:
	//converts an bool array to JSON array containing 1 and 0
	static const Json::Value convertBoolArraytoJson(const vector<bool> &v);
	//converts a TxnHash array to JSON array containing TxnHash strings
	static const Json::Value convertTxnHashArraytoJson(const vector<TxnHash> &v);
	//converts a TxBlock to JSON object
	static const Json::Value convertTxBlocktoJson(TxBlock txblock);
	//converts a DSBlocck to JSON object
	static const Json::Value convertDSblocktoJson(DSBlock dsblock);

};

#endif // __JSONCONVERSION_H__


