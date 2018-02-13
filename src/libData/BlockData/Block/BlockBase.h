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

#ifndef __BLOCKBASE_H__
#define __BLOCKBASE_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>

#include "libData/AccountData/Transaction.h"
#include "common/Serializable.h"
#include "common/Constants.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"

/// [TODO] Base class for all supported block data types
class BlockBase : public Serializable
{
	// TODO: pull out all common code from ds, micro and tx block

public:

    /// Default constructor.
    BlockBase();
};

#endif // __BLOCKBASE_H__