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
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <thread>
#include "libUtils/DetachedFunction.h"
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/SysCommand.h"

/* EvmClient Init */
void EvmClient::Init() {
  LOG_MARKER();

  CheckClient(0, false);
}

EvmClient::~EvmClient() {


  std::lock_guard<std::mutex> g(m_mutexMain);
  Json::Value _json;
  LOG_GENERAL(DEBUG, "Call evm with die request:" << _json);
  // call evm
  try {
    const auto oldJson = m_clients.at(0)->CallMethod("die", _json);
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING,
                "Caught an exception calling die " << e.what());
    std::string cmdStr = "pkill " + EVM_SERVER_BINARY + " >/dev/null &";
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
}

bool EvmClient::OpenServer(uint32_t version) {
  LOG_MARKER();

  const std::string programName =
      boost::filesystem::path(EVM_SERVER_BINARY).filename().string();
  const std::string cmdStr =
      "pkill " + programName + " ; " + EVM_SERVER_BINARY +                 //
      " --socket " + EVM_SERVER_SOCKET_PATH +                              //
      " --tracing " +                                                      //
      " --zil-scaling-factor " + std::to_string(EVM_ZIL_SCALING_FACTOR) +  //
      " --log4rs '" + EVM_LOG_CONFIG +                                     //
      "'>/dev/null &";

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

  LOG_GENERAL(WARNING, "Executed: " << cmdStr << "on " << version);

  // Sleep an extra 5x because of very slow networks on Devnet

  std::this_thread::sleep_for(
      std::chrono::milliseconds(SCILLA_SERVER_PENDING_IN_MS * 5));

  return true;
}

bool EvmClient::CheckClient(uint32_t version, bool enforce) {
  std::lock_guard<std::mutex> g(m_mutexMain);

  if (m_clients.find(version) != m_clients.end() && !enforce) {
    return true;
  }

  if (!OpenServer(enforce)) {
    LOG_GENERAL(WARNING, "OpenServer for version " << version << "failed");
    return false;
  }

  m_connectors[version] =
      std::make_shared<jsonrpc::UnixDomainSocketClient>(EVM_SERVER_SOCKET_PATH);

  m_clients[version] = std::make_shared<jsonrpc::Client>(
      *m_connectors.at(version), jsonrpc::JSONRPC_CLIENT_V2);

  return true;
}

bool EvmClient::CallRunner(uint32_t version, const Json::Value& _json,
                           evmproj::CallResponse& result,
                           const uint32_t counter) {
  //
  // Fail the call if counter is zero
  //
  if (counter == 0 && LOG_SC) {
    LOG_GENERAL(INFO, "Call counter was zero returning");
    return false;
  }

  version = 0;

  if (!CheckClient(version)) {
    LOG_GENERAL(WARNING, "CheckClient failed");
    return false;
  }

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    LOG_GENERAL(DEBUG, "Call evm with request:" << _json);
    // call evm
    const auto oldJson = m_clients.at(version)->CallMethod("run", _json);

    // Populate the C++ struct with the return values
    try {
      const auto reply = evmproj::GetReturn(oldJson, result);
      if (reply.Success() && LOG_SC) {
        LOG_GENERAL(INFO, "Parsed Json response correctly");
      }
      return true;
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING,
                  "detected an Error in decoding json response " << e.what());
      result.SetSuccess(false);
    }
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING,
                "RPC Exception calling EVM-DS : attempting server "
                "restart "
                    << e.what());

    if (!CheckClient(version, true)) {
      LOG_GENERAL(WARNING, "CheckClient for version " << version << "failed");
      return CallRunner(version, _json, result, counter - 1);
    } else {
      result.SetSuccess(false);
    }
    return false;
  }
  return true;
}
