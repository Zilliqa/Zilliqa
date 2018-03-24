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

#ifndef __RETRIEVER_H__
#define __RETRIEVER_H__

#include <list>
#include <map>
#include <unordered_map>

#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libMediator/Mediator.h"

class Retriever
{
public:
    Retriever(Mediator& mediator);

    void RetrieveDSBlocks(bool& result);

    void RetrieveTxBlocks(bool& result);
    bool RetrieveStates();
    bool ValidateStates();
    bool CleanExtraTxBodies();

private:
    Mediator& m_mediator;
};

#endif // __RETRIEVER_H__