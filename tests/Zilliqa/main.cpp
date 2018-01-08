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

#include <iostream>
#include <arpa/inet.h>
#include <algorithm>
#include "libUtils/Logger.h"

#include "libNetwork/PeerStore.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/Logger.h"
#include "libUtils/DataConversion.h"
#include "libZilliqa/Zilliqa.h"

using namespace std;
using namespace boost::multiprecision;

/* Obtain a backtrace and print it to stdout. */
void print_trace (void)
{
    void *array[10];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace (array, 10);
    strings = backtrace_symbols (array, size);

    LOG_MESSAGE ("Obtained " << size << " stack frames.\n");

    for (i = 0; i < size; i++)
    {
        LOG_MESSAGE(strings[i])
    }
     //printf ("%s\n", strings[i]);
    free (strings);
}

void got_terminated()
{
    LOG_MESSAGE("Error: Abort was triggered.")
    print_trace ();
    raise (SIGABRT); // generate core dump thru abort
}

void got_unxpected()
{
    LOG_MESSAGE("Error: Unexpected was triggered.")
    print_trace ();
    raise (SIGABRT); // generate core dump thru abort
}

int main(int argc, const char * argv[])
{
    const int num_args_required = 1 + 5; // first 1 = program name

    if (argc != num_args_required)
    {
        cout << "[USAGE] " << argv[0] << " <32-byte private_key> <33-byte public_key> <listen_ip_address> <listen_port> <1 if loadConfig, 0 otherwise>" << endl;
    }
    else
    {
        INIT_FILE_LOGGER("zilliqa");
        INIT_STATE_LOGGER("state");

        //std::set_terminate( &got_terminated );
        //std::set_unexpected( &got_unxpected );

        vector<unsigned char> tmpprivkey = DataConversion::HexStrToUint8Vec(argv[1]);
        vector<unsigned char> tmppubkey = DataConversion::HexStrToUint8Vec(argv[2]);
        PrivKey privkey(tmpprivkey, 0);
        PubKey pubkey(tmppubkey, 0);

        struct in_addr ip_addr;
        inet_pton(AF_INET, argv[3], &ip_addr);
        Peer my_port((uint128_t)ip_addr.s_addr, static_cast<unsigned int>(atoi(argv[4])));

        Zilliqa zilliqa(make_pair(privkey, pubkey), my_port, atoi(argv[5]) == 1);

        auto dispatcher = [&zilliqa](const vector<unsigned char> & message, const Peer & from) mutable -> void { zilliqa.Dispatch(message, from); };
        auto broadcast_list_retriever = [&zilliqa](unsigned char msg_type, unsigned char ins_type, const Peer & from) mutable -> vector<Peer> { return zilliqa.RetrieveBroadcastList(msg_type, ins_type, from); };

        P2PComm::GetInstance().StartMessagePump(my_port.m_listenPortHost, dispatcher, broadcast_list_retriever);
    }

    return 0;
}
