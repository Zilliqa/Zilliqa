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
#include <cstring>
#include <iostream>

#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerManager.h"
#include "libUtils/DataConversion.h"

using namespace std;
using namespace boost::multiprecision;

typedef void (*handler_func)(int, const char*, const char*, uint32_t,
                             const char* []);
struct message_handler
{
    const char* ins;
    handler_func func;
};

void process_addpeers(int numargs, const char* progname, const char* cmdname,
                      uint32_t listen_port, const char* args[])
{
    const int min_args_required = 3;

    if (numargs < min_args_required)
    {
        cout << "[USAGE] " << progname << " <local node listen_port> "
             << cmdname << " <33-byte public_key> <ip_addr> <listen_port> ..."
             << endl;
    }
    else
    {
        struct in_addr ip_addr;
        inet_aton("127.0.0.1", &ip_addr);
        Peer my_port(uint128_t(ip_addr.s_addr), listen_port);

        for (int i = 0; i < numargs;)
        {
            if (i + 2 >= numargs)
            {
                break;
            }

            // Assemble an ADDNODE message

            // Class and Inst bytes
            vector<unsigned char> addnode_message
                = {MessageType::PEER, PeerManager::InstructionType::ADDPEER};

            // Public key
            // Temporarily just accept the public key as an input (for use with the peer store)
            vector<unsigned char> tmp
                = DataConversion::HexStrToUint8Vec(args[i++]);
            addnode_message.resize(MessageOffset::BODY + tmp.size());
            copy(tmp.begin(), tmp.end(),
                 addnode_message.begin() + MessageOffset::BODY);

            // IP address
            inet_aton(args[i++], &ip_addr);
            uint128_t tmp2 = ip_addr.s_addr;
            Serializable::SetNumber<uint128_t>(
                addnode_message, MessageOffset::BODY + PUB_KEY_SIZE, tmp2,
                UINT128_SIZE);

            // Listen port
            Serializable::SetNumber<uint32_t>(
                addnode_message,
                MessageOffset::BODY + PUB_KEY_SIZE + UINT128_SIZE,
                static_cast<unsigned int>(atoi(args[i++])), sizeof(uint32_t));

            // Send the ADDNODE message to the local node
            P2PComm::GetInstance().SendMessage(my_port, addnode_message);
        }
    }
}

void process_broadcast(int numargs, const char* progname, const char* cmdname,
                       uint32_t listen_port, const char* args[])
{
    const int num_args_required = 1;

    if (numargs != num_args_required)
    {
        cout << "[USAGE] " << progname << " <local node listen_port> "
             << cmdname << " <length of dummy message in bytes>" << endl;
    }
    else
    {
        struct in_addr ip_addr;
        inet_aton("127.0.0.1", &ip_addr);
        Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

        unsigned int numbytes = static_cast<unsigned int>(atoi(args[0]));
        vector<unsigned char> broadcast_message(numbytes + MessageOffset::BODY,
                                                0xAA);
        broadcast_message.at(MessageOffset::TYPE) = MessageType::PEER;
        broadcast_message.at(MessageOffset::INST)
            = PeerManager::InstructionType::BROADCAST;
        broadcast_message.at(MessageOffset::BODY) = MessageType::PEER;

        // Send the BROADCAST message to the local node
        P2PComm::GetInstance().SendMessage(my_port, broadcast_message);
    }
}

void process_cmd(int numargs, const char* progname, const char* cmdname,
                 uint32_t listen_port, const char* args[])
{
    const int num_args_required = 1;

    if (numargs != num_args_required)
    {
        cout << "[USAGE] " << progname << " <local node listen_port> "
             << cmdname << " <hex string message>" << endl;
    }
    else
    {
        struct in_addr ip_addr;
        inet_aton("127.0.0.1", &ip_addr);
        Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

        // Send the generic message to the local node
        vector<unsigned char> tmp = DataConversion::HexStrToUint8Vec(args[0]);
        P2PComm::GetInstance().SendMessage(my_port, tmp);
    }
}

int main(int argc, const char* argv[])
{
    if (argc < 3)
    {
        cout << "[USAGE] " << argv[0]
             << " <local node listen_port> <command> [command args]" << endl;
        cout << "Available commands: addpeers broadcast cmd reportto" << endl;
        return -1;
    }

    const char* instruction = argv[2];

    const message_handler message_handlers[] = {
        {"addpeers", &process_addpeers},
        {"broadcast", &process_broadcast},
        {"cmd", &process_cmd},
    };

    const int num_handlers
        = sizeof(message_handlers) / sizeof(message_handlers[0]);

    bool processed = false;
    for (int i = 0; i < num_handlers; i++)
    {
        if (!strcmp(instruction, message_handlers[i].ins))
        {
            (*message_handlers[i].func)(
                argc - 3, argv[0], argv[2],
                static_cast<unsigned int>(atoi(argv[1])), argv + 3);
            processed = true;
            break;
        }
    }

    if (!processed)
    {
        cout << "Unknown command parameter supplied: " << instruction << endl;
    }

    return 0;
}
