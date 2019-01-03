/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <execinfo.h>  // for backtrace
#include <signal.h>

#include <arpa/inet.h>
#include <algorithm>
#include <iostream>
#include "libUtils/Logger.h"

#include "depends/NAT/nat.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"

using namespace std;
using namespace boost::multiprecision;

int main(int argc, const char* argv[]) {
  const int num_args_required = 1 + 7;  // first 1 = program name
  struct in_addr ip_addr;
  Peer my_network_info;

  INIT_FILE_LOGGER("zilliqa");
  INIT_STATE_LOGGER("state");
  INIT_EPOCHINFO_LOGGER("epochinfo");

  if (argc != num_args_required) {
    cout << "Copyright (C) Zilliqa. Version 3.0 (Durian - Mao Shan Wang). "
            "<https://www.zilliqa.com/> "
         << endl;
    cout << "For bug reporting, please create an issue at "
            "<https://github.com/Zilliqa/Zilliqa> \n"
         << endl;
    cout << "[USAGE] " << argv[0]
         << " <32-byte private_key> <33-byte public_key> "
            "<listen_ip_address or \"NAT\"> <listen_port> <1 if "
            "loadConfig, 0 "
            "otherwise> <SyncType, 0 for no, 1 for new,"
            " 2 for normal, 3 for ds, 4 for lookup, 5 for node recovery, 6 for "
            "new lookup and 7 for ds guard node sync> "
            "<1 if recovery, 0 otherwise>"
         << endl;
    return 0;
  }

  unsigned int localPort = static_cast<unsigned int>(atoi(argv[4]));
  unique_ptr<NAT> nt;

  if (string(argv[3]) == "NAT") {
    nt = make_unique<NAT>();
    nt->init();

    int mappedPort = nt->addRedirect(localPort);

    if (mappedPort <= 0) {
      LOG_GENERAL(WARNING, "NAT ERROR");
      return -1;
    } else {
      LOG_GENERAL(INFO, "My external IP is " << nt->externalIP().c_str()
                                             << " and my mapped port is "
                                             << mappedPort);
    }

    inet_pton(AF_INET, nt->externalIP().c_str(), &ip_addr);
    my_network_info = Peer((uint128_t)ip_addr.s_addr, mappedPort);
  } else {
    inet_pton(AF_INET, argv[3], &ip_addr);
    my_network_info = Peer((uint128_t)ip_addr.s_addr, localPort);
  }

  bytes tmPrivkey = DataConversion::HexStrToUint8Vec(argv[1]);
  bytes tmpPubkey = DataConversion::HexStrToUint8Vec(argv[2]);

  PrivKey privkey;
  if (privkey.Deserialize(tmPrivkey, 0) != 0) {
    LOG_GENERAL(WARNING, "We failed to deserialize PrivKey.");
    return -1;
  }

  PubKey pubkey;
  if (pubkey.Deserialize(tmpPubkey, 0) != 0) {
    LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
    return -1;
  }

  Zilliqa zilliqa(make_pair(privkey, pubkey), my_network_info,
                  atoi(argv[5]) == 1, atoi(argv[6]), atoi(argv[7]) == 1);

  auto dispatcher = [&zilliqa](pair<bytes, Peer>* message) mutable -> void {
    zilliqa.Dispatch(message);
  };
  auto broadcast_list_retriever =
      [&zilliqa](unsigned char msg_type, unsigned char ins_type,
                 const Peer& from) mutable -> vector<Peer> {
    return zilliqa.RetrieveBroadcastList(msg_type, ins_type, from);
  };

  P2PComm::GetInstance().StartMessagePump(my_network_info.m_listenPortHost,
                                          dispatcher, broadcast_list_retriever);

  return 0;
}
