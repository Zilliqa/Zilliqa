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

#include "ScillaClient.h"

#include "libScilla/ScillaUtils.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/SysCommand.h"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/iterator_range.hpp>

using namespace std::filesystem;

ScillaClient::~ScillaClient() {
  std::string cmdStr = "pkill " + SCILLA_SERVER_BINARY + " >/dev/null &";
  LOG_GENERAL(INFO, "cmdStr: " << cmdStr);

  try {
    if (!SysCommand::ExecuteCmd(SysCommand::WITHOUT_OUTPUT, cmdStr)) {
      LOG_GENERAL(WARNING, "ExecuteCmd failed: " << cmdStr);
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception caught in SysCommand::ExecuteCmd: " << e.what());
  } catch (...) {
    LOG_GENERAL(WARNING, "Unknown error encountered");
  }
}

void ScillaClient::Init() {
  LOG_MARKER();

  if (!ENABLE_SCILLA) {
    return;
  }

  if (ENABLE_SCILLA_MULTI_VERSION) {
    path scilla_root_path(SCILLA_ROOT);
    // scan existing versions
    LOG_GENERAL(INFO, "looking in directory " << scilla_root_path << " ...  ");
    for (auto& entry :
         boost::make_iterator_range(directory_iterator(scilla_root_path), {})) {
      std::string folder_name = entry.path().string();
      folder_name.erase(0, SCILLA_ROOT.size() + 1);
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

bool ScillaClient::isScillaRuning() {
  std::string cmdStr = "ps aux | awk '{print $2\"\\t\"$11}' | grep \"" +
                       SCILLA_SERVER_BINARY + "\" | awk '{print $1}'";
  std::string result;
  try {
    if (!SysCommand::ExecuteCmd(SysCommand::WITH_OUTPUT, cmdStr, result)) {
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
  if (result.empty()) {
    return false;
  }
  return true;
}

bool ScillaClient::OpenServer(uint32_t version) {
  LOG_MARKER();

  std::string cmdStr;
  std::string root_w_version;
  if (!ScillaUtils::PrepareRootPathWVersion(version, root_w_version)) {
    LOG_GENERAL(WARNING, "ScillaUtils::PrepareRootPathWVersion failed");
    return false;
  }

  std::string server_path = root_w_version + "/bin/" + SCILLA_SERVER_BINARY;

  std::string killStr, executeStr;

  if (ENABLE_SCILLA_MULTI_VERSION) {
    cmdStr = "ps aux | awk '{print $2\"\\t\"$11}' | grep \"" + server_path +
             "\" | awk '{print $1}' | xargs kill -SIGTERM ; " + server_path +
             " -socket " + SCILLA_SERVER_SOCKET_PATH + "." +
             std::to_string(version) + " >/dev/null &";
  } else {
    cmdStr = "pkill " + SCILLA_SERVER_BINARY + " ; " + server_path +
             " -socket " + SCILLA_SERVER_SOCKET_PATH + " >/dev/null &";
  }

  LOG_GENERAL(INFO, "cmdStr: " << cmdStr);

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

  LOG_GENERAL(WARNING, "terminated: " << cmdStr);

  std::this_thread::sleep_for(
      std::chrono::milliseconds(SCILLA_SERVER_PENDING_IN_MS));

  return true;
}

void ScillaClient::RestartScillaClient() {
  LOG_MARKER();
  if (ENABLE_SCILLA_MULTI_VERSION == true) {
    for (const auto& entry : m_clients) {
      CheckClient(entry.first, true);
    }
  } else {
    CheckClient(0, true);
  }
}

bool ScillaClient::CheckClient(uint32_t version, bool enforce) {
  std::lock_guard<std::mutex> g(m_mutexMain);

  if (m_clients.find(version) != m_clients.end() && !enforce) {
    return true;
  }

  if (!OpenServer(version)) {
    LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
    return false;
  }

  m_connectors[version] = std::make_unique<rpc::UnixDomainSocketClient>(
      SCILLA_SERVER_SOCKET_PATH +
      (ENABLE_SCILLA_MULTI_VERSION ? ("." + std::to_string(version)) : ""));

  m_clients[version] = std::make_unique<jsonrpc::Client>(
      *m_connectors.at(version), jsonrpc::JSONRPC_CLIENT_V2);

  return true;
}

bool ScillaClient::CallChecker(uint32_t version, const Json::Value& _json,
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
      if (e.GetCode() == jsonrpc::Errors::ERROR_RPC_JSON_PARSE_ERROR ||
          e.GetCode() == jsonrpc::Errors::ERROR_CLIENT_CONNECTOR){
        LOG_GENERAL(WARNING, "Looks like connection problem");
        if (!isScillaRuning()) {
          LOG_GENERAL(WARNING, "Scilla is not running");
          CheckClient(version,true);
        }
        return CallChecker(version, _json, result, counter - 1);
      }
      result = e.what();
    }

    return false;
  }

  return true;
}

bool ScillaClient::CallRunner(uint32_t version, const Json::Value& _json,
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
    result = m_clients.at(version)->CallMethod("run", _json).asString();
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallRunner failed: " << e.what());
    if (std::string(e.what()).find(SCILLA_SERVER_SOCKET_PATH) !=
        std::string::npos) {
      if (!CheckClient(version, true)) {
        LOG_GENERAL(WARNING, "CheckClient for version " << version << "failed");
        return CallRunner(version, _json, result, counter - 1);
      }
    } else {
      if (e.GetCode() == jsonrpc::Errors::ERROR_RPC_JSON_PARSE_ERROR ||
          e.GetCode() == jsonrpc::Errors::ERROR_CLIENT_CONNECTOR){
        LOG_GENERAL(WARNING, "Looks like connection problem");
        if (!isScillaRuning()) {
          LOG_GENERAL(WARNING, "Scilla is not running");
          CheckClient(version,true);
        }
        return CallChecker(version, _json, result, counter - 1);
      }
      result = e.what();
    }

    return false;
  }

  return true;
}

bool ScillaClient::CallDisambiguate(uint32_t version, const Json::Value& _json,
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
      if (e.GetCode() == jsonrpc::Errors::ERROR_RPC_JSON_PARSE_ERROR ||
          e.GetCode() == jsonrpc::Errors::ERROR_CLIENT_CONNECTOR){
        LOG_GENERAL(WARNING, "Looks like connection problem");
        if (!isScillaRuning()) {
          LOG_GENERAL(WARNING, "Scilla is not running");
          CheckClient(version,true);
        }
        return CallChecker(version, _json, result, counter - 1);
      }
      result = e.what();
    }

    return false;
  }

  return true;
}
