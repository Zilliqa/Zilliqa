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

#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/Logger.h"
#include "libValidator/Validator.h"
#include <array>
#include <fstream>
#include <string>
#include <vector>

void GenTxn(unsigned int k, const Address& fromAddr, unsigned int iteration)
{

    vector<unsigned char> txns;
    unsigned j = 0;
    for (auto& privKeyHexStr : GENESIS_KEYS)
    {
        fstream file;

        auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
        auto privKey = PrivKey{privKeyBytes, 0};
        auto pubKey = PubKey{privKey};
        auto address = Account::GetAddressFromPublicKey(pubKey);
        auto nonce = iteration * NUM_TXN;

        file.open(TXN_PATH + "/" + address.hex() + "_" + to_string(nonce) + ".zil",
                  ios ::app | ios::binary);

        if (!file.is_open())
        {
            cout << "Unable to open file" << endl;
            continue;
        }

        size_t n = k;
        txns.clear();

        Address receiverAddr = fromAddr;
        //unsigned int curr_offset = 0;
        txns.reserve(n);
        for (auto i = 0u; i != n; i++)
        {

            Transaction txn(0, nonce + i + 1, receiverAddr,
                            make_pair(privKey, pubKey), 10 * i + 2, 1, 1, {},
                            {});
            /*txns.emplace_back(txn);*/
            txn.Serialize(txns, 0);
            for (auto& i : txns)
            {
                file << i;
            }
        }

        file.close();
        cout << "Iteration " << j << endl;
        j++;
    }
}

int main()
{

    Address toAddr;

    for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }
    for (unsigned int i = 0; i < 10000; i++)
    {
        GenTxn(NUM_TXN, toAddr, i);
    }
}
