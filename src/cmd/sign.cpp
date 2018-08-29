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
#include <iostream>

using namespace std;

int main(int argc, const char* argv[])
{
    if (4 != argc)
    {
        cout << "Input format: ./sign message privateKeyFileName "
                "publicKeyFileName";
        return -1;
    }

    const vector<unsigned char> message
        = DataConversion::HexStrToUint8Vec(string(argv[1]));

    char buf[100];
    vector<PrivKey> privKeys;
    FILE* privFile = fopen(argv[2], "r");

    while (fgets(buf, sizeof(buf), privFile))
    {
        string s = buf;

        if ('\n' == s.at(s.size() - 1))
        {
            s = s.substr(0, s.size() - 1);
        }

        if (s.empty())
        {
            continue;
        }

        privKeys.emplace_back(DataConversion::HexStrToUint8Vec(s), 0);
    }

    fclose(privFile);

    vector<PubKey> pubKeys;
    FILE* pubFile = fopen(argv[3], "r");

    while (fgets(buf, sizeof(buf), pubFile))
    {
        string s = buf;

        if ('\n' == s.at(s.size() - 1))
        {
            s = s.substr(0, s.size() - 1);
        }

        if (s.empty())
        {
            continue;
        }

        pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(s), 0);
    }

    fclose(pubFile);

    if (privKeys.size() != pubKeys.size())
    {
        cout << "Private key number must equal to public key number!";
        return -1;
    }

    for (unsigned int i = 0; i < privKeys.size(); ++i)
    {
        Signature sig;
        Schnorr::GetInstance().Sign(message, privKeys.at(i), pubKeys.at(i),
                                    sig);
        vector<unsigned char> result;
        sig.Serialize(result, 0);
        cout << DataConversion::Uint8VecToHexStr(result);
    }

    return 0;
}
