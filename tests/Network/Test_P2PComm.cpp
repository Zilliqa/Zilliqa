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

#include "libNetwork/P2PComm.h"
#include "libUtils/DetachedFunction.h"
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <vector>

using namespace std;
chrono::high_resolution_clock::time_point startTime;

void process_message(const vector<unsigned char>& message, const Peer& from)
{
    LOG_MARKER();

    if (message.size() < 10)
    {
        LOG_GENERAL(INFO,
                    "Received message '"
                        << (char*)&message.at(0) << "' at port "
                        << from.m_listenPortHost << " from address "
                        << from.m_ipAddress);
    }
    else
    {
        chrono::duration<double, std::milli> time_span
            = chrono::high_resolution_clock::now() - startTime;
        LOG_GENERAL(INFO,
                    "Received " << message.size() / (1024 * 1024)
                                << " MB message in " << time_span.count()
                                << " ms");
        LOG_GENERAL(INFO,
                    "Benchmark: " << (1000 * message.size())
                            / (time_span.count() * 1024 * 1024)
                                  << " MBps");
    }
}

int main()
{
    INIT_STDOUT_LOGGER();

    auto func = []() mutable -> void {
        P2PComm::GetInstance().StartMessagePump(30303, process_message,
                                                nullptr);
    };

    DetachedFunction(1, func);

    this_thread::sleep_for(chrono::seconds(1)); // short delay to prepare socket

    struct in_addr ip_addr;
    inet_aton("127.0.0.1", &ip_addr);
    Peer peer = {ip_addr.s_addr, 30303};
    vector<unsigned char> message1
        = {'H', 'e', 'l', 'l', 'o', '\0'}; // Send Hello once

    P2PComm::GetInstance().SendMessage(peer, message1);

    vector<Peer> peers = {peer, peer, peer};
    vector<unsigned char> message2
        = {'W', 'o', 'r', 'l', 'd', '\0'}; // Send World 3x

    P2PComm::GetInstance().SendMessage(peers, message2);

    vector<unsigned char> longMsg(1024 * 1024 * 1024, 'z');
    longMsg.push_back('\0');

    startTime = chrono::high_resolution_clock::now();
    P2PComm::GetInstance().SendMessage(peer, longMsg);

    return 0;
}
