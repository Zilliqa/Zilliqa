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

using namespace std;
using namespace boost::multiprecision;

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

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    cout << "[USAGE] " << argv[0]
         << " <local node listen_port> <command> [command args]" << endl;
    cout << "Available commands: addpeers broadcast cmd reportto" << endl;
    return -1;
  }

  const char* instruction = argv[2];

  const message_handler message_handlers[] = {{"addpeers", &process_addpeers},
                                              {"broadcast", &process_broadcast},
                                              {"cmd", &process_cmd}};

  const message_handler_2 message_handlers_2[] = {
      {"remotecmd", &process_remote_cmd}};

  bool processed = false;
  for (auto message_handler : message_handlers) {
    if (std::string(instruction) == std::string(message_handler.ins)) {
      (*message_handler.func)(argc - 3, argv[0], argv[2],
                              static_cast<unsigned int>(atoi(argv[1])),
                              argv + 3);
      processed = true;
      break;
    }
  }

  if (!processed) {
    instruction = argv[3];
    for (auto i : message_handlers_2) {
      if (std::string(instruction) == std::string(i.ins)) {
        (*i.func)(argc - 4, argv[0], argv[3], argv[1],
                  static_cast<unsigned int>(atoi(argv[2])), argv + 4);
        processed = true;
        break;
      }
    }
  }

  if (!processed) {
    cout << "Unknown command parameter supplied" << endl;
  }

  return 0;
}
