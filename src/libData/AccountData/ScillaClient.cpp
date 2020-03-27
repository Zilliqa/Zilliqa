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

#include "ScillaClient.h"
#include "common/Constants.h"
#include "libUtils/ScillaUtils.h"
#include "libUtils/SysCommand.h"

bool ScillaClient::OpenServer(uint32_t version) {
  std::string cmdStr;
  std::string root_w_version;
  if (!ScillaUtils::PrepareRootPathWVersion(version, root_w_version)) {
    LOG_GENERAL(WARNING, "ScillaUtils::PrepareRootPathWVersion failed");
    return false;
  }
  std::string server_path = root_w_version + '/' + SCILLA_SERVER_BINARY;
  std::string killStr, executeStr;

  if (ENABLE_SCILLA_MULTI_VERSION) {
    killStr = "ps aux | '{print $2\"\\t\"}'$11' | grep -E '^\\d+\\t'\"" +
              server_path + "\"'$' | awk '{print $1}' | xargs kill -SIGTERM";
    executeStr = server_path + "-socket" + SCILLA_SERVER_SOCKET_PATH + "." +
                 std::to_string(version);
  } else {
    killStr = "pkill " + SCILLA_SERVER_BINARY;
    executeStr = server_path + "-socket" + SCILLA_SERVER_SOCKET_PATH;
  }
  cmdStr = killStr + "; " + executeStr;

  LOG_GENERAL(INFO, "cmdStr: " << cmdStr);

  try {
    if (!SysCommand::ExecuteCmd(SysCommand::WITHOUT_OUTPUT, cmdStr)) {
      LOG_GENERAL(WARNING, "ExecuteCmd failed: " << cmdStr);
      return false;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception caught in SysCommand::ExecuteCmd (2): " << e.what());
    return false;
  }
  return true;
}

bool ScillaClient::CheckClient(uint32_t version) {
  if (m_clients.find(version) != m_clients.end()) {
    if (!OpenServer(version)) {
      LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
      return false;
    }
    jsonrpc::UnixDomainSocketClient conn(
        SCILLA_SERVER_SOCKET_PATH +
        (ENABLE_SCILLA_MULTI_VERSION ? "" : ("." + std::to_string(version))));
    jsonrpc::Client c(conn, jsonrpc::JSONRPC_CLIENT_V2);
    m_clients.emplace(version, c);
  }

  return true;
}

bool ScillaClient::CallChecker(uint32_t version, const Json::Value& _json,
                               std::string& result) {
  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  result = m_clients.at(version).CallMethod("check", _json).asString();
  return true;
}

bool ScillaClient::CallRunner(uint32_t version, const Json::Value& _json,
                              std::string& result) {
  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  result = m_clients.at(version).CallMethod("run", _json).asString();
  return true;
}