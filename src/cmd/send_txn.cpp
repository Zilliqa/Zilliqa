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

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerManager.h"
#include "libUtils/DataConversion.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

int main(int argc, const char* argv[])
{

    // To use ./sendtxn [port]
    if (argc < 2)
    {
        cout << "[USAGE] " << argv[0] << " <local node listen_port> <command>"
             << endl;
        cout << "Available commands: cmd " << endl;
    }

    uint32_t listen_port = static_cast<unsigned int>(atoi(argv[1]));
    struct in_addr ip_addr;
    inet_aton("127.0.0.1", &ip_addr);
    Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

    // Send the generic message to the local node
    std::string dummyTxn = "02030202AAB3EFF78CC0D5854AC5F3DCF2A7C372E9162340999"
                           "BB8032F7B7277D698A802A523F019D0BE0E008108C012716414"
                           "F6249DA59ECFF9597CC83AA4C0D825FD7500000000000000000"
                           "00000000000000000000000000000000000000000000064";
    vector<unsigned char> tmp = DataConversion::HexStrToUint8Vec(dummyTxn);

    P2PComm::GetInstance().SendMessage(my_port, tmp);
    this_thread::sleep_for(chrono::milliseconds(50));

    return 0;
}
