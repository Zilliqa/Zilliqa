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
#include "boost/program_options.hpp"
#include "libUtils/Logger.h"

#include "depends/NAT/nat.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/DataConversion.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"

using namespace std;
using namespace boost::multiprecision;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

namespace po = boost::program_options;

int main(int argc, const char* argv[]) {
  try {
    INIT_FILE_LOGGER("zilliqa");
    INIT_STATE_LOGGER("state");
    INIT_EPOCHINFO_LOGGER("epochinfo");

    Peer my_network_info;
    string privK;
    string pubK;
    string address;
    int port = 0;
    uint8_t synctype = 0;
    const char* synctype_descr =
        "0(default) for no, 1 for new, 2 for normal, 3 for ds, 4 for lookup";

    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "privk,i", po::value<string>(&privK)->required(),
        "32-byte private key")("pubk,u", po::value<string>(&pubK)->required(),
                               "32-byte public key")(
        "address,a", po::value<string>(&address)->required(),
        "Listen IPv4/6 address in standard \"dotted decimal\" format, "
        "otherwise \"NAT\"")("port,p", po::value<int>(&port)->required(),
                             "Specifies port to bind to")(
        "loadconfig,l", "Loads configuration if set")(
        "synctype,s", po::value<uint8_t>(&synctype), synctype_descr)(
        "recovery,r", "Runs in recovery mode if set");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      /** --help option
       */
      if (vm.count("help")) {
        LogInfo::LogBrandBugReport();
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);

      if ((port < 0) || (port > 65535)) {
        LogInfo::LogBrandBugReport();
        std::cerr << "Invalid port number." << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      if (privK.length() != 32) {
        LogInfo::LogBrandBugReport();
        std::cerr << "Invalid length of private key." << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      if (pubK.length() != 32) {
        LogInfo::LogBrandBugReport();
        std::cerr << "Invalid length of public key." << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      if (synctype > 4) {
        LogInfo::LogBrandBugReport();
        std::cerr << "Invalid synctype, please select: " << synctype_descr
                  << "." << endl;
      }
    } catch (boost::program_options::required_option& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    unique_ptr<NAT> nt;
    uint128_t ip;

    if (address == "NAT") {
      nt = make_unique<NAT>();
      nt->init();

      int mappedPort = nt->addRedirect(port);

      if (mappedPort <= 0) {
        LOG_GENERAL(WARNING, "NAT ERROR");
        return -1;
      } else {
        LOG_GENERAL(INFO, "My external IP is " << nt->externalIP().c_str()
                                               << " and my mapped port is "
                                               << mappedPort);
      }

      if (IPConverter::ToNumericalIPFromStr(nt->externalIP().c_str(), ip) !=
          0) {
        return ERROR_IN_COMMAND_LINE;
      }
      my_network_info = Peer(ip, mappedPort);
    } else {
      if (IPConverter::ToNumericalIPFromStr(address, ip) != 0) {
        return ERROR_IN_COMMAND_LINE;
      }
      my_network_info = Peer(ip, port);
    }

    vector<unsigned char> tmPrivkey =
        DataConversion::HexStrToUint8Vec(privK.c_str());
    vector<unsigned char> tmpPubkey =
        DataConversion::HexStrToUint8Vec(pubK.c_str());

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
                    vm.count("loadconfig"), synctype, vm.count("recovery"));
    auto dispatcher =
        [&zilliqa](pair<vector<unsigned char>, Peer>* message) mutable -> void {
      zilliqa.Dispatch(message);
    };
    auto broadcast_list_retriever =
        [&zilliqa](unsigned char msg_type, unsigned char ins_type,
                   const Peer& from) mutable -> vector<Peer> {
      return zilliqa.RetrieveBroadcastList(msg_type, ins_type, from);
    };

    P2PComm::GetInstance().StartMessagePump(
        my_network_info.m_listenPortHost, dispatcher, broadcast_list_retriever);

  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }

  return SUCCESS;
}
