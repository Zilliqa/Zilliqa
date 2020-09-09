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

#include "libUtils/DetachedFunction.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/ScillaUtils.h"
#include "libUtils/SysCommand.h"

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
    killStr = "ps --no-headers axk comm o pid,args | awk '$2 ~ \"" +
              server_path + "\"{print $1}' | xargs kill -9";
    executeStr = server_path + " -socket " + SCILLA_SERVER_SOCKET_PATH + "." +
                 std::to_string(version);
  } else {
    killStr = "pkill " + SCILLA_SERVER_BINARY;
    executeStr = server_path + " -socket " + SCILLA_SERVER_SOCKET_PATH;
  }

  cmdStr = killStr + "; " + executeStr;

  auto func = [&cmdStr]() mutable -> void {
    LOG_GENERAL(INFO, "cmdStr: " << cmdStr);

    try {
      if (!SysCommand::ExecuteCmd(SysCommand::WITHOUT_OUTPUT, cmdStr)) {
        LOG_GENERAL(WARNING, "ExecuteCmd failed: " << cmdStr);
      }
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING,
                  "Exception caught in SysCommand::ExecuteCmd: " << e.what());
    }

    LOG_GENERAL(WARNING, "terminated: " << cmdStr);
  };

  DetachedFunction(1, func);

  usleep(200 * 1000);

  return true;
}

bool ScillaClient::CheckClient(uint32_t version) {
  std::lock_guard<std::mutex> g(m_mutexMain);

  if (m_clients.find(version) == m_clients.end()) {
    if (!OpenServer(version)) {
      LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
      return false;
    }

    std::shared_ptr<jsonrpc::UnixDomainSocketClient> conn =
        std::make_shared<jsonrpc::UnixDomainSocketClient>(
            SCILLA_SERVER_SOCKET_PATH + (ENABLE_SCILLA_MULTI_VERSION
                                             ? ("." + std::to_string(version))
                                             : ""));

    m_connectors.insert({version, conn});

    std::shared_ptr<jsonrpc::Client> c = std::make_shared<jsonrpc::Client>(
        *m_connectors.at(version), jsonrpc::JSONRPC_CLIENT_V2);
    m_clients.insert({version, c});
  }

  return true;
}

bool ScillaClient::CallChecker(uint32_t version, const Json::Value& _json,
                               std::string& result, uint32_t counter) {
  if (counter == 0) {
    return false;
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
      if (!OpenServer(version)) {
        LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
        return CallChecker(version, _json, result, counter - 1);
      }
    } else {
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
      if (!OpenServer(version)) {
        LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
        return CallChecker(version, _json, result, counter - 1);
      }
    } else {
      result = e.what();
    }

    return false;
  }

  return true;
}