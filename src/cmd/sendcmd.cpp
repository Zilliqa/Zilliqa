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

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/PeerManager.h"
#include "libUtils/DataConversion.h"
#include "libUtils/SWInfo.h"

#include "boost/program_options.hpp"
#include <boost/algorithm/string/join.hpp>

using namespace std;
using namespace boost::multiprecision;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

typedef void (*handler_func)(int, const char*, const char*, uint32_t,
                             const char*[]);

typedef void (*handler_func_2)(int, const char*, const char*, const char*,
                               uint32_t, const char*[]);

struct message_handler {
  const char* ins;
  handler_func func;
};

struct message_handler_2 {
  const char* ins;
  handler_func_2 func;
};

void process_addpeers(int numargs, const char* progname, const char* cmdname,
                      uint32_t listen_port, const char* args[]) {
  const int min_args_required = 3;

  if (numargs < min_args_required) {
    cout << "[USAGE] " << progname << " <local node listen_port> " << cmdname
         << " <33-byte public_key> <ip_addr> <listen_port> ..." << endl;
  } else {
    struct in_addr ip_addr;
    inet_pton(AF_INET, "127.0.0.1", &ip_addr);
    Peer my_port(uint128_t(ip_addr.s_addr), listen_port);

    for (int i = 0; i < numargs;) {
      if (i + 2 >= numargs) {
        break;
      }

      // Assemble an ADDNODE message

      // Class and Inst bytes
      vector<unsigned char> addnode_message = {
          MessageType::PEER, PeerManager::InstructionType::ADDPEER};

      // Public key
      // Temporarily just accept the public key as an input (for use with the
      // peer store)
      vector<unsigned char> tmp = DataConversion::HexStrToUint8Vec(args[i++]);
      addnode_message.resize(MessageOffset::BODY + tmp.size());
      copy(tmp.begin(), tmp.end(),
           addnode_message.begin() + MessageOffset::BODY);

      // IP address
      inet_pton(AF_INET, args[i++], &ip_addr);
      uint128_t tmp2 = ip_addr.s_addr;
      Serializable::SetNumber<uint128_t>(addnode_message,
                                         MessageOffset::BODY + PUB_KEY_SIZE,
                                         tmp2, UINT128_SIZE);

      // Listen port
      Serializable::SetNumber<uint32_t>(
          addnode_message, MessageOffset::BODY + PUB_KEY_SIZE + UINT128_SIZE,
          static_cast<unsigned int>(atoi(args[i++])), sizeof(uint32_t));

      // Send the ADDNODE message to the local node
      P2PComm::GetInstance().SendMessageNoQueue(my_port, addnode_message);
    }
  }
}

void process_broadcast(int numargs, const char* progname, const char* cmdname,
                       uint32_t listen_port, const char* args[]) {
  const int num_args_required = 1;

  if (numargs != num_args_required) {
    cout << "[USAGE] " << progname << " <local node listen_port> " << cmdname
         << " <length of dummy message in bytes>" << endl;
  } else {
    struct in_addr ip_addr;
    inet_pton(AF_INET, "127.0.0.1", &ip_addr);
    Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

    unsigned int numbytes = static_cast<unsigned int>(atoi(args[0]));
    vector<unsigned char> broadcast_message(numbytes + MessageOffset::BODY,
                                            0xAA);
    broadcast_message.at(MessageOffset::TYPE) = MessageType::PEER;
    broadcast_message.at(MessageOffset::INST) =
        PeerManager::InstructionType::BROADCAST;
    broadcast_message.at(MessageOffset::BODY) = MessageType::PEER;

    // Send the BROADCAST message to the local node
    P2PComm::GetInstance().SendMessageNoQueue(my_port, broadcast_message);
  }
}

void process_cmd(int numargs, const char* progname, const char* cmdname,
                 uint32_t listen_port, const char* args[]) {
  const int num_args_required = 1;

  if (numargs != num_args_required) {
    cout << "[USAGE] " << progname << " <local node listen_port> " << cmdname
         << " <hex string message>" << endl;
  } else {
    struct in_addr ip_addr;
    inet_pton(AF_INET, "127.0.0.1", &ip_addr);
    Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

    // Send the generic message to the local node
    vector<unsigned char> tmp = DataConversion::HexStrToUint8Vec(args[0]);
    P2PComm::GetInstance().SendMessageNoQueue(my_port, tmp);
  }
}

void process_remote_cmd(int numargs, const char* progname, const char* cmdname,
                        const char* remote_ip, uint32_t listen_port,
                        const char* args[]) {
  const int num_args_required = 1;

  if (numargs != num_args_required) {
    cout << "[USAGE] " << progname
         << " <remote node ip_address> <remote node listen_port> " << cmdname
         << " <hex string message>" << endl;
  } else {
    struct in_addr ip_addr;
    inet_pton(AF_INET, remote_ip, &ip_addr);
    Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

    vector<unsigned char> tmp = DataConversion::HexStrToUint8Vec(args[0]);
    P2PComm::GetInstance().SendMessageNoQueue(my_port, tmp);
  }
}

namespace po = boost::program_options;

int main(int argc, const char* argv[]) {

//  TODO:
//    - validate IP and port


  try {
    int port = -1;
    string progname(argv[0]);
    string cmd;
    vector<string> cmdarg;
    string ip;
    vector<std::pair<string, handler_func>> cmd_available_v;
    po::options_description desc("Options");
    handler_func cmd_f;

    const message_handler message_handlers[] = {{"addpeers", &process_addpeers},
                                                    {"broadcast", &process_broadcast},
                                                    {"cmd", &process_cmd}};

    const message_handler_2 message_handlers_2[] = {
        {"remotecmd", &process_remote_cmd}};
    vector<string> cmd_v;

    for (auto message_handler : message_handlers) {
      cmd_available_v.emplace_back(std::string(message_handler.ins), message_handler.func);
      cmd_v.push_back(string(message_handler.ins));
    }
    for (auto message_handler : message_handlers_2) {
      cmd_available_v.emplace_back(std::string(message_handler.ins), message_handler.func);
      cmd_v.push_back(string(message_handler.ins));
    }

    desc.add_options()
        ("help,h", "Print help messages")
        ("listen_port,p", po::value<int>(&port)->required(), "Local node listen_port")
        ("ip,i", po::value<string>(&ip)->required(), "Remote node ip_address")
        ("cmd,c", po::value<string>(&cmd)->required(), "Command")
        ("cmdarg,g", po::value<std::vector<string>>(&cmdarg)->required(), "Command arguments");

        po::variables_map vm;
    try
    {
      po::store(po::parse_command_line(argc, argv, desc),
          vm);

      /** --help option
       */
      string cmd_help = string("Commands supported:\n") + string(boost::algorithm::join(cmd_v, "\n"));
      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        cout << desc << endl;
        cout << cmd_help << endl;
        return SUCCESS;
      }

      // cmd lookup
      bool found = false;
      for (auto message_handler : cmd_available_v) {
        if (message_handler.first == cmd) {
          cmd_f = message_handler.second;
          found = true;
          break;
        }
      }
      if(!found){
        SWInfo::LogBrandBugReport();
        std::cerr << "Unknown command" << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      po::notify(vm);
    }
    catch(boost::program_options::required_option& e)
    {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    }
    catch(boost::program_options::error& e)
    {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }



    if (cmd != "remotecmd") {
//      (*cmd_f)(argc - 3, progname, cmd, ip, port
//                              static_cast<unsigned int>(atoi(argv[1])),
//                              argv + 3);
//      return SUCCESS;
  }
    else{
        (*cmd_f)(argc - 3, progname, cmd, ip, port, string(boost::algorithm::join(cmd_v, "\n")).c_str());
        return SUCCESS;
    }

  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return ERROR_UNHANDLED_EXCEPTION;
}
