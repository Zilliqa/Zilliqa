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

#include <execinfo.h> // for backtrace
#include <signal.h>

#include "libUtils/Logger.h"
#include <algorithm>
#include <arpa/inet.h>
#include <iostream>

#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"

using namespace std;
using namespace boost::multiprecision;

int main(int argc, const char* argv[])
{
    const int num_args_required = 1 + 7; // first 1 = program name
    if (argc != num_args_required)
    {
        cout << "Copyright (C) Zilliqa. Version 1.0 (Durian). "
                "<https://www.zilliqa.com/> "
             << endl;
        cout << "For bug reporting, please create an issue at "
                "<https://github.com/Zilliqa/Zilliqa> \n"
             << endl;
        cout << "[USAGE] " << argv[0]
             << " <32-byte private_key> <33-byte public_key> "
                "<listen_ip_address> <listen_port> <1 if loadConfig, 0 "
                "otherwise> <1 if sync, 0 otherwise> <1 if recovery, 0 "
                "otherwise>"
             << endl;
    }
    else
    {
        INIT_FILE_LOGGER("zilliqa");
        INIT_STATE_LOGGER("state");

        vector<unsigned char> tmpprivkey
            = DataConversion::HexStrToUint8Vec(argv[1]);
        vector<unsigned char> tmppubkey
            = DataConversion::HexStrToUint8Vec(argv[2]);
        // PrivKey privkey(tmpprivkey, 0);
        PrivKey privkey;
        if (privkey.Deserialize(tmpprivkey, 0) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize PrivKey.");
            return -1;
        }
        // PubKey pubkey(tmppubkey, 0);
        PubKey pubkey;
        if (pubkey.Deserialize(tmppubkey, 0) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
            return -1;
        }

        struct in_addr ip_addr;
        inet_aton(argv[3], &ip_addr);
        Peer my_port((uint128_t)ip_addr.s_addr,
                     static_cast<unsigned int>(atoi(argv[4])));

        Zilliqa zilliqa(make_pair(privkey, pubkey), my_port, atoi(argv[5]) == 1,
                        atoi(argv[6]) == 1, atoi(argv[7]) == 1);

        auto dispatcher = [&zilliqa](const vector<unsigned char>& message,
                                     const Peer& from) mutable -> void {
            zilliqa.Dispatch(message, from);
        };
        auto broadcast_list_retriever
            = [&zilliqa](unsigned char msg_type, unsigned char ins_type,
                         const Peer& from) mutable -> vector<Peer> {
            return zilliqa.RetrieveBroadcastList(msg_type, ins_type, from);
        };

        P2PComm::GetInstance().StartMessagePump(
            my_port.m_listenPortHost, dispatcher, broadcast_list_retriever);
    }

    return 0;
}
