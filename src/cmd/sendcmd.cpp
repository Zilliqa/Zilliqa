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

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>

#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/IPConverter.h"
#include "libUtils/SWInfo.h"

using namespace std;
using namespace boost::multiprecision;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

typedef void (*handler_func)(const char*, const char*, vector<string>,
                             const uint32_t);

typedef void (*handler_func_remote)(const char*, const char*, vector<string>,
                                    const uint128_t&, const uint32_t);

struct MessageHandler {
  const char* ins;
  handler_func func;
};

struct MessageHandler2 {
  const char* ins;
  handler_func_remote func;
};

void process_cmd(const char* progname, const char* cmdname, vector<string> args,
                 const uint32_t listen_port) {
  const int num_args_required = 1;
  int numargs = args.size();
  if (numargs != num_args_required) {
    cout << "[USAGE] " << progname << " <local node listen_port> " << cmdname
         << " <hex string message>" << endl;
  } else {
    struct in_addr ip_addr {};
    inet_pton(AF_INET, "127.0.0.1", &ip_addr);
    Peer my_port((uint128_t)ip_addr.s_addr, listen_port);

    // Send the generic message to the local node
    bytes tmp;
    DataConversion::HexStrToUint8Vec(args[0].c_str(), tmp);
    P2PComm::GetInstance().SendMessageNoQueue(my_port, tmp);
  }
}

void process_remote_cmd(const char* progname, const char* cmdname,
                        vector<string> args, const uint128_t& remote_ip,
                        const uint32_t listen_port) {
  const int num_args_required = 1;
  int numargs = args.size();
  if (numargs != num_args_required) {
    cout << "[USAGE] " << progname
         << " <remote node ip_address> <remote node listen_port> " << cmdname
         << " <hex string message>" << endl;
  } else {
    Peer my_port(remote_ip, listen_port);
    bytes tmp;
    DataConversion::HexStrToUint8Vec(args[0].c_str(), tmp);
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
    po::options_description desc("Options");
    handler_func cmd_f = NULL;
    handler_func_remote cmd_f_remote = NULL;
    vector<string> cmd_v;
    const MessageHandler message_handlers[] = {{"cmd", &process_cmd}};

    const MessageHandler2 message_handlers_2[] = {
        {"remotecmd", &process_remote_cmd}};

    for (auto message_handler : message_handlers) {
      cmd_v.push_back(string(message_handler.ins));
    }
    for (auto message_handler : message_handlers_2) {
      cmd_v.push_back(string(message_handler.ins));
    }

    desc.add_options()("help,h", "Print help messages")(
        "port,p", po::value<int>(&port), "Local node listen port")(
        "address,a", po::value<string>(&ip),
        "Remote node IPv4/6 address formated as \"dotted decimal\" or "
        "optionally \"dotted decimal:portnumber\"")(
        "cmd,c", po::value<string>(&cmd)->required(),
        "Command; see commands listed below")(
        "cmdarg,g", po::value<std::vector<string>>(&cmdarg)->required(),
        "Command arguments");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      /** --help option
       */
      string cmd_help = string("Commands supported:\n") + "\t" +
                        string(boost::algorithm::join(cmd_v, "\n\t"));
      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        cout << desc << endl;
        cout << cmd_help << endl;
        return SUCCESS;
      }
      po::notify(vm);

      // cmd lookup
      bool found = false;
      for (auto message_handler : message_handlers) {
        if (string(message_handler.ins) == cmd) {
          cmd_f = message_handler.func;
          found = true;
          break;
        }
      }
      for (auto message_handler : message_handlers_2) {
        if (string(message_handler.ins) == cmd) {
          cmd_f_remote = message_handler.func;
          found = true;
          break;
        }
      }
      if (!found) {
        SWInfo::LogBrandBugReport();
        std::cerr << "Unknown command" << endl;
        return ERROR_IN_COMMAND_LINE;
      }
    } catch (boost::program_options::required_option& e) {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (cmd != "remotecmd") {
      if ((port < 0) || (port > 65535)) {
        SWInfo::LogBrandBugReport();
        std::cerr << "Invalid or missing port number" << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      (*cmd_f)(progname.c_str(), cmd.c_str(), cmdarg, port);

      return SUCCESS;
    } else {
      string ip_;
      if (IPConverter::GetIPPortFromSocket(ip, ip_, port)) {
        ip = ip_;
      }
      uint128_t remote_ip;
      if (!IPConverter::ToNumericalIPFromStr(ip, remote_ip)) {
        return ERROR_IN_COMMAND_LINE;
      }
      if ((port < 0) || (port > 65535)) {
        SWInfo::LogBrandBugReport();
        std::cerr << "Invalid or missing port number" << endl;
        return ERROR_IN_COMMAND_LINE;
      }
      (*cmd_f_remote)(progname.c_str(), cmd.c_str(), cmdarg, remote_ip, port);
      return SUCCESS;
    }

  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return ERROR_UNHANDLED_EXCEPTION;
}
