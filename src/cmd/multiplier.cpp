/*
 * Copyright (C) 2023 Zilliqa
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
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <boost/asio/signal_set.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "libMetrics/Tracing.h"
#include "common/Multiplier.h"
#include "libCrypto/Sha2.h"
#include "libNetwork/P2P.h"
#include "libUtils/HardwareSpecification.h"
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"
#include "libUtils/DetachedFunction.h"


using namespace zil::p2p;
std::chrono::high_resolution_clock::time_point startTime;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_HARDWARE_SPEC_MISMATCH_EXCEPTION -2
#define ERROR_UNHANDLED_EXCEPTION -3
#define ERROR_IN_CONSTANTS -4


void process_message(std::shared_ptr<zil::p2p::Message> message) {
  LOG_MARKER();

  if (message->msg.size() < 10) {
    LOG_GENERAL(INFO, "Received message '"
                          << (char*)&message->msg.at(0) << "' at port "
                          << message->from.m_listenPortHost << " from address "
                          << message->from.m_ipAddress);
  } else {
    std::chrono::duration<double, std::milli> time_span =
        std::chrono::high_resolution_clock::now() - startTime;
    LOG_GENERAL(INFO, "Received " << message->msg.size() / (1024 * 1024)
                                  << " MB message in " << time_span.count()
                                  << " ms");
    LOG_GENERAL(INFO, "Benchmark: " << (1000 * message->msg.size()) /
                                           (time_span.count() * 1024 * 1024)
                                    << " MBps");
  }
}

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  Peer my_network_info;
  std::string privK;
  std::string pubK;
  PrivKey privkey;
  PubKey pubkey;
  std::string address;
  std::string logpath(std::filesystem::absolute("./").string());
  std::string identity;
  int port = 4002;
  uint128_t ip;

  po::options_description desc("Options");
  desc.add_options()("help,h", "Print help messages")(
      "privk,i", po::value<std::string>(&privK)->default_value("ABCD"),
      "32-byte private key")("pubk,u",
                             po::value<std::string>(&pubK)->default_value("XYZ"),
                             "33-byte public key")(
      "address,a", po::value<std::string>(&address)->default_value("127.0.0.1"),
      "Listen IPv4/6 address formated as \"dotted decimal\" or optionally "
      "\"dotted decimal:portnumber\" format, otherwise \"NAT\"")(
      "port,p", po::value<int>(&port)->default_value(4009),
      "Specifies port to bind to, if not specified in address")(
      "stdoutlog,o", "Send application logs to stdout instead of file")(
      "logpath,g", po::value<std::string>(&logpath),
      "customized log path, could be relative path (e.g., \"./logs/\"), or "
      "absolute path (e.g., \"/usr/local/test/logs/\")")(
      "version,v", "Displays the Zilliqa Multiplier version information");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      SWInfo::LogBrandBugReport();
      std::cout << desc << std::endl;
      return SUCCESS;
    }

    if (vm.count("version")) {
      std::cout << VERSION_TAG << std::endl;
      return SUCCESS;
    }

    po::notify(vm);

    try {
      privkey = PrivKey::GetPrivKeyFromString(privK);
    } catch (std::invalid_argument& e) {
      std::cerr << e.what() << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    try {
      pubkey = PubKey::GetPubKeyFromString(pubK);
    } catch (std::invalid_argument& e) {
      std::cerr << e.what() << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (address != "NAT") {
      if (!IPConverter::ToNumericalIPFromStr(address, ip)) {
        return ERROR_IN_COMMAND_LINE;
      }

      std::string address_;
      if (IPConverter::GetIPPortFromSocket(address, address_, port)) {
        address = address_;
      }
    }

    if ((port < 0) || (port > 65535)) {
      SWInfo::LogBrandBugReport();
      std::cerr << "Invalid or missing port number" << std::endl;
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

  INIT_FILE_LOGGER("multiplier", std::filesystem::current_path());
  LOG_DISPLAY_LEVEL_ABOVE(INFO);

  auto func = [port]() mutable -> void {
    boost::asio::io_context ctx(1);
    boost::asio::signal_set sig(ctx, SIGINT, SIGTERM);
    sig.async_wait([&](const boost::system::error_code&, int) { ctx.stop(); });

    auto dispatcher = [](std::shared_ptr<zil::p2p::Message> message) {
      process_message(std::move(message));
    };

    zil::p2p::GetInstance().StartServer(ctx, port, 0, std::move(dispatcher));

    ctx.run();
  };

  DetachedFunction(1, func);

  while (1) {
    std::cout << "Waiting for activity ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}

