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

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/iterator_range.hpp>
#include <thread>

#include "EvmClient.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SysCommand.h"

/* EvmClient Init */
void EvmClient::Init() {
  LOG_MARKER();

  CheckClient(0, false);

  m_initialised = true;
}

bool EvmClient::OpenServer(bool force) {
  LOG_MARKER();

  std::string programName =
      boost::filesystem::path(EVM_SERVER_BINARY).filename().string();
  std::string cmdStr = "pkill " + programName + " ; " + EVM_SERVER_BINARY +
                       " --socket " + EVM_SERVER_SOCKET_PATH +
                       " --tracing >/dev/null &";

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

  if (force) {
    m_initialised = false;
    CheckClient(0, false);
    m_initialised = true;
  }

  return true;
}

bool EvmClient::CheckClient(uint32_t version, bool enforce) {
  std::lock_guard<std::mutex> g(m_mutexMain);

  if (m_initialised) return true;

  if (!OpenServer(enforce)) {
    LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
    return false;
  }

  std::shared_ptr<jsonrpc::UnixDomainSocketClient> conn =
      std::make_shared<jsonrpc::UnixDomainSocketClient>(EVM_SERVER_SOCKET_PATH);

  m_connectors[version] = conn;

  std::shared_ptr<jsonrpc::Client> c = std::make_shared<jsonrpc::Client>(
      *m_connectors.at(version), jsonrpc::JSONRPC_CLIENT_V2);
  m_clients[version] = c;

  return true;
}

bool EvmClient::CallRunner(uint32_t version, const Json::Value& _json,
                           evmproj::CallRespose& result, uint32_t counter) {
  if (counter == 0) {
    return false;
  }

  version = 0;

  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    Json::Value oldJson;
    evmproj::CallRespose reply;
    oldJson = m_clients.at(version)->CallMethod("run", _json);
    // Populate the C++ struct with the return values
    reply = evmproj::GetReturn(oldJson, result);
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallRunner failed: " << e.what());
    m_initialised = false;
    if (!CheckClient(version, true)) {
      LOG_GENERAL(WARNING,
                  "Restart OpenServer for version " << version << "failed");
    }
    result.m_ok = false;
    return false;
  }

  return true;
}
