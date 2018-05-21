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

void Print(const vector<unsigned char>& payload)
{
    static const char* hex_table = "0123456789ABCDEF";

    size_t payload_string_len = (payload.size() * 2) + 1;
    unique_ptr<char[]> payload_string = make_unique<char[]>(payload_string_len);
    for (unsigned int payload_idx = 0, payload_string_idx = 0;
         (payload_idx < payload.size())
         && ((payload_string_idx + 2) < payload_string_len);
         payload_idx++)
    {
        payload_string.get()[payload_string_idx++]
            = hex_table[(payload.at(payload_idx) >> 4) & 0xF];
        payload_string.get()[payload_string_idx++]
            = hex_table[payload.at(payload_idx) & 0xF];
    }
    payload_string.get()[payload_string_len - 1] = '\0';
    cout << payload_string.get();
}

int main(int argc, const char* argv[])
{
    pair<PrivKey, PubKey> keypair = Schnorr::GetInstance().GenKeyPair();

    vector<unsigned char> privkey, pubkey;
    keypair.first.Serialize(privkey, 0);
    keypair.second.Serialize(pubkey, 0);

    Print(pubkey);
    cout << " ";
    Print(privkey);
    //FIXME: add '\n' back
    // cout << '\n';

    return 0;
}
