/*
 * Copyright (C) 2022 Zilliqa
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

#include "EvmClient.h"

#include "libUtils/DetachedFunction.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SysCommand.h"

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/iterator_range.hpp>
#include <thread>

/* EvmClient Init */
void EvmClient::Init() {
  LOG_MARKER();
  if (ENABLE_EVM_MULTI_VERSION) {
    boost::filesystem::path scilla_root_path(SCILLA_ROOT);
    // scan existing versions
    for (auto& entry : boost::make_iterator_range(
             boost::filesystem::directory_iterator(scilla_root_path), {})) {
      LOG_GENERAL(INFO, "scilla-server path: " << entry.path().string());
      std::string folder_name = entry.path().string();
      folder_name.erase(0, EVM_ROOT.size() + 1);
      LOG_GENERAL(INFO, "folder_name: " << folder_name);
      uint32_t version = 0;
      try {
        version = boost::lexical_cast<uint32_t>(folder_name);
        if (!CheckClient(version)) {
          LOG_GENERAL(WARNING,
                      "OpenServer for version " << version << "failed");
          continue;
        }
      } catch (...) {
        LOG_GENERAL(WARNING, "Not valid folder name");
        continue;
      }
    }
  } else {
    CheckClient(0, false);
  }
}

bool EvmClient::OpenServer(uint32_t version) {
  LOG_MARKER();

  std::string cmdStr;
  std::string root_w_version;

  if (!EvmUtils::PrepareRootPathWVersion(version, root_w_version)) {
    LOG_GENERAL(WARNING, "ScillaUtils::PrepareRootPathWVersion failed");
    return false;
  }

  std::string server_path =
      root_w_version + EVM_SERVER_PATH + EVM_SERVER_BINARY;
  std::string killStr, executeStr;

  if (ENABLE_EVM_MULTI_VERSION) {
    cmdStr = "ps aux | awk '{print $2\"\\t\"$11}' | grep \"" + server_path +
             "\" | awk '{print $1}' | xargs kill -SIGTERM ; " + server_path +
             " --socket " + EVM_SERVER_SOCKET_PATH + "." +
             std::to_string(version) + " >/dev/null &";
  } else {
    cmdStr = "pkill " + EVM_SERVER_BINARY + " ; " + server_path + " --socket " +
             EVM_SERVER_SOCKET_PATH + " --tracing >/dev/null &";
  }

  LOG_GENERAL(INFO, "running cmdStr: " << cmdStr);

  try {
    if (!SysCommand::ExecuteCmd(SysCommand::WITHOUT_OUTPUT, cmdStr)) {
      LOG_GENERAL(WARNING, "ExecuteCmd failed: " << cmdStr);
      return false;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception caught in SysCommand::ExecuteCmd: " << e.what());
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING, "Unknown error encountered");
    return false;
  }

  LOG_GENERAL(WARNING, "Executed: " << cmdStr);

  std::this_thread::sleep_for(
      std::chrono::milliseconds(SCILLA_SERVER_PENDING_IN_MS));

  return true;
}

bool EvmClient::CheckClient(uint32_t version,
                            __attribute__((unused)) bool enforce) {
  std::lock_guard<std::mutex> g(m_mutexMain);

  if (!OpenServer(version)) {
    LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
    return false;
  }

  std::shared_ptr<jsonrpc::UnixDomainSocketClient> conn =
      std::make_shared<jsonrpc::UnixDomainSocketClient>(
          EVM_SERVER_SOCKET_PATH +
          (ENABLE_EVM_MULTI_VERSION ? ("." + std::to_string(version)) : ""));

  m_connectors[version] = conn;

  std::shared_ptr<jsonrpc::Client> c = std::make_shared<jsonrpc::Client>(
      *m_connectors.at(version), jsonrpc::JSONRPC_CLIENT_V2);
  m_clients[version] = c;

  return true;
}

bool EvmClient::CallChecker(uint32_t version, const Json::Value& _json,
                            std::string& result, uint32_t counter) {
  if (counter == 0) {
    return false;
  }

  if (!ENABLE_SCILLA_MULTI_VERSION) {
    version = 0;
  }

  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    result = m_clients.at(version)->CallMethod("check", _json).asString();
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallChecker failed: " << e.what());
    if (std::string(e.what()).find(SCILLA_SERVER_SOCKET_PATH) !=
        std::string::npos) {
      if (!CheckClient(version, true)) {
        LOG_GENERAL(WARNING, "CheckClient for version " << version << "failed");
        return CallChecker(version, _json, result, counter - 1);
      }
    } else {
      result = e.what();
    }

    return false;
  }

  return true;
}

bool EvmClient::CallRunner(uint32_t version, const Json::Value& _json,
                           Json::Value& result, uint32_t counter) {
  if (counter == 0) {
    return false;
  }

  if (!ENABLE_SCILLA_MULTI_VERSION) {
    version = 0;
  }

  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    std::cout << "Sending|" << _json << "| to EVM" << std::endl;

    result = m_clients.at(version)->CallMethod("run", _json);

    //
    // The result should contain the code that we then need to execute, and also
    // store back into the contract and probably the chain.
    //
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallRunner failed: " << e.what());
    return false;
  }

  return true;
}

bool EvmClient::CallDisambiguate(uint32_t version, const Json::Value& _json,
                                 std::string& result, uint32_t counter) {
  if (counter == 0) {
    return false;
  }

  if (!ENABLE_SCILLA_MULTI_VERSION) {
    version = 0;
  }

  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    result =
        m_clients.at(version)->CallMethod("disambiguate", _json).asString();
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallDisambiguate failed: " << e.what());
    if (std::string(e.what()).find(SCILLA_SERVER_SOCKET_PATH) !=
        std::string::npos) {
      if (!CheckClient(version, true)) {
        LOG_GENERAL(WARNING, "CheckClient for version " << version << "failed");
        return CallDisambiguate(version, _json, result, counter - 1);
      }
    } else {
      result = e.what();
    }

    return false;
  }

  return true;
}
