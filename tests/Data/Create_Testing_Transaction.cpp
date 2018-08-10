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
#include <string>
#include <vector>

#define BOOST_TEST_MODULE transactiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <fstream>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(GenTxnTest)

void GenTxn(unsigned int k, const Address& fromAddr)
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

        file.open(address.hex() + ".zil", ios ::app | ios::binary);

        if (!file.is_open())
        {
            LOG_GENERAL(WARNING, "Unable to open file");
            return;
        }

        auto nonce = 0;
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
        LOG_GENERAL(INFO, "Iteration " << j);
        j++;
    }
}

unsigned int num_txns = 1000000;

unsigned int TXN_SIZE = 317;

BOOST_AUTO_TEST_CASE(test1)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();
    //for Generating Offline
    /*fstream file;

    Address toAddr;

    for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }

    //TxnVec.clear();

    GenTxn(num_txns, toAddr);*/

    //for Checking the File
    /*vector<unsigned char> vec;
    auto& privKeyHexStr = GENESIS_KEYS[0];

    auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
    auto privKey = PrivKey{privKeyBytes, 0};
    auto pubKey = PubKey{privKey};
    auto address = Account::GetAddressFromPublicKey(pubKey);

    GetTxnFromFile::GetFromFile(address, 1, 10, vec);

    for (unsigned int i = 0; i < 10; i++)
    {
        Transaction tx(vec, TXN_SIZE * i);

        BOOST_CHECK_MESSAGE(tx.GetNonce() == i + 1, "Nonce Incorrect");
    }*/
}

BOOST_AUTO_TEST_SUITE_END()
