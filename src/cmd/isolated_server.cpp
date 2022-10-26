/*
 * Copyright (C) 2020 Zilliqa
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

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include "libEth/Filters.h"
#include "libServer/APIServer.h"
#include "libServer/IsolatedServer.h"
#include "libServer/LookupServer.h"
#include "libServer/WebsocketServer.h"

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

using namespace std;
namespace po = boost::program_options;

namespace {

int readAccountJsonFromFile(const string& path) {
  ifstream in(path.c_str());

  if (!in) {
    cerr << "Cannot open file \n" << path << endl;
    return -1;
  }
  Json::Value _json;
  in >> _json;

  try {
    for (const auto& i : _json.getMemberNames()) {
      Address addr(i);
      uint128_t balance(_json[i]["amount"].asString());
      if (AccountStore::GetInstance().AddAccount(
              addr, {balance, _json[i]["nonce"].asUInt()})) {
        LOG_GENERAL(INFO, "Added " << addr << " with balance " << balance);
      }
    }
  } catch (exception& e) {
    cout << "Unable to load data " << e.what() << endl;
    return -1;
  }
  AccountStore::GetInstance().UpdateStateTrieAll();
  return 0;
}

void createConfigFile() {
  ofstream configFile;
  configFile.open("config.xml");
  configFile << "<nodes></nodes>" << endl;
  configFile.close();
}

void help(const char* argv[]) {
  cout << "Usage" << endl;
  cout << argv[0]
       << " --file [Path to Json Account File] --port [Port to run "
          "RPC] --blocknum [Initial blocknum]"
       << endl;
}

Json::Value BlockByHash(IsolatedServer& server, const std::string& hash) {
  Json::Value value;
  try {
    server.GetEthBlockByHash(hash, false);
  } catch (...) {
    LOG_GENERAL(WARNING, "BlockByHash failed with hash=" << hash);
  }
  return value;
}

}  // namespace

int main(int argc, const char* argv[]) {
  using namespace evmproj;

  string accountJsonFilePath;
  uint port{5555};
  string blocknum_str{"1"};
  uint timeDelta{0};
  bool loadPersistence{false};
  bool nonisoload{false};
  string uuid;

  LOOKUP_NODE_MODE = true;

  try {
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help message")(
        "file,f", po::value<string>(&accountJsonFilePath),
        "Json file containing bootstrap accounts")(
        "port,p", po::value<uint>(&port),
        "Port to run server on {default: 5555")(
        "blocknum,b", po::value<string>(&blocknum_str),
        "Initial blocknumber {default : 1 }")(
        "time,t", po::value<uint>(&timeDelta),
        "the automatic blocktime for incrementing block number (in ms)  "
        "(Disabled by default)")(
        "load,l", po::bool_switch()->default_value(false),
        "Load from persistence folder (False by default)")(
        "nonisoload,n", po::bool_switch()->default_value(false),
        "Load is either of testnet or mainnet having state-deltas (False by "
        "default)")(
        "uuid,u", po::value<string>(&uuid),
        "unique id to be provided upon startup (can be any string)");

    po::variables_map vm;

    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help")) {
        help(argv);
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
      loadPersistence = vm["load"].as<bool>();
      if (loadPersistence) {
        nonisoload = vm["nonisoload"].as<bool>();
      }
    } catch (boost::program_options::required_option& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    ISOLATED_SERVER = true;

    createConfigFile();

    PairOfKey key;
    Peer peer;

    Mediator mediator(key, peer);
    Node node(mediator, 0, false);
    Lookup lk(mediator, NO_SYNC);
    auto vd = make_shared<Validator>(mediator);

    if (!BlockStorage::GetBlockStorage().RefreshAll()) {
      LOG_GENERAL(WARNING, "BlockStorage::RefreshAll failed");
    }
    if (!AccountStore::GetInstance().RefreshDB()) {
      LOG_GENERAL(WARNING, "AccountStore::RefreshDB failed");
    }

    mediator.RegisterColleagues(nullptr, &node, &lk, vd.get());

    AccountStore::GetInstance().InitSoft();

    uint64_t blocknum;

    if (uuid.empty()) {
      LOG_GENERAL(WARNING, "Please set a valid uuid using -u flag");
      return ERROR_IN_COMMAND_LINE;
    }

    if (!loadPersistence && accountJsonFilePath.empty()) {
      LOG_GENERAL(
          WARNING,
          "Either set the accounts files using -f option or use persistence to "
          "load using -l option. Neither option specified");
      return ERROR_IN_COMMAND_LINE;
    }
    if (!loadPersistence) {
      try {
        blocknum = stoull(blocknum_str);
      } catch (exception& e) {
        cerr << "Error: "
             << "blocknum not numeric" << endl;
        return ERROR_IN_COMMAND_LINE;
      }

      if (readAccountJsonFromFile(accountJsonFilePath)) {
        cerr << "ERROR: "
             << "Unable to parse account json file" << endl;
        return ERROR_IN_COMMAND_LINE;
      }
    }

    auto ctx = std::make_shared<boost::asio::io_context>(1);

    APIServer::Options options;
    options.port = static_cast<uint16_t>(port);

    auto apiServer = APIServer::CreateAndStart(ctx, std::move(options));
    if (!apiServer) {
      cerr << "Server failed to listen" << endl;
      return ERROR_UNHANDLED_EXCEPTION;
    } else {
      cout << "Server listening on " << port << endl;
    }

    auto isolatedServer = make_shared<IsolatedServer>(
        mediator, apiServer->GetRPCServerBackend(), blocknum, timeDelta);

    if (ENABLE_EVM) {
      mediator.m_filtersAPICache->EnableWebsocketAPI(
          apiServer->GetWebsocketServer(),
          [&](const std::string& blockHash) -> Json::Value {
            return BlockByHash(*isolatedServer, blockHash);
          });
    }

    isolatedServer->m_uuid = std::move(uuid);

    if (loadPersistence) {
      LOG_GENERAL(INFO, "Trying to load persistence.. ");
      if (!isolatedServer->RetrieveHistory(nonisoload)) {
        LOG_GENERAL(WARNING, "RetrieveHistory Failed");
        return ERROR_UNHANDLED_EXCEPTION;
      }
    }

    if (ENABLE_WEBSOCKET) {
      if (timeDelta > 0) {
        LOG_GENERAL(INFO, "Starting websocket on port " << WEBSOCKET_PORT);
      } else {
        LOG_GENERAL(WARNING,
                    "Websocket can only be enabled in time-trigger mode")
      }
    }

    pthread_setname_np(pthread_self(), "main");

    boost::asio::signal_set sig(*ctx, SIGINT, SIGTERM);
    sig.async_wait(
        [&](const boost::system::error_code&, int) { apiServer->Close(); });

    ctx->run();
    LOG_GENERAL(INFO, "Event loop stopped");

  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }

  return SUCCESS;
}
