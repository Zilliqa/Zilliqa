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
#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <thread>
#include "libUtils/EvmJsonResponse.h"
#include "libUtils/EvmUtils.h"

namespace {

bool LaunchEvmDaemon(boost::process::child& child,
                     const std::string& binaryPath,
                     const std::string& socketPath) {
  LOG_MARKER();

  const std::vector<std::string> args = {"--socket",
                                         EVM_SERVER_SOCKET_PATH,
                                         "--tracing",
                                         "--zil-scaling-factor",
                                         std::to_string(EVM_ZIL_SCALING_FACTOR),
                                         "--log4rs",
                                         EVM_LOG_CONFIG};

  boost::filesystem::path bin_path(binaryPath);
  boost::filesystem::path socket_path(socketPath);
  boost::system::error_code ec;

  if (boost::filesystem::exists(socket_path)) {
    boost::filesystem::remove(socket_path, ec);
    if (ec.failed()) {
      LOG_GENERAL(WARNING, "Problem removing filesystem entry for socket ");
    }
  }
  if (not boost::filesystem::exists(bin_path)) {
    LOG_GENERAL(WARNING, "Cannot create a subprocess that does not exist " +
                             EVM_SERVER_BINARY);
    return false;
  }
  boost::process::child c =
      boost::process::child(bin_path, boost::process::args(args));
  child = std::move(c);
  pid_t thread_id = child.id();
  if (thread_id > 0 && child.valid()) {
    if (LOG_SC) {
      LOG_GENERAL(INFO, "Valid child created at " << thread_id);
    }
  } else {
    LOG_GENERAL(WARNING, "child is not valid " << thread_id);
    return false;
  }
  int counter{0};
  while (not boost::filesystem::exists(socket_path)) {
    if ((counter++ % 10) == 0)
      LOG_GENERAL(WARNING, "Awaiting Launch of the evm-ds daemon ");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return true;
}

bool CleanupPreviousInstances() {
  std::string s = "pkill -9 -f " + EVM_SERVER_BINARY;
  int sysRep = std::system(s.c_str());
  if (sysRep != -1) {
    LOG_GENERAL(INFO, "system call return value " << sysRep);
  }
  return true;
}

bool Terminate(boost::process::child& child,
               const std::unique_ptr<jsonrpc::Client>& client) {
  LOG_MARKER();
  Json::Value _json;
  LOG_GENERAL(DEBUG, "Call evm with die request:" << _json);
  // call evm
  try {
    const auto oldJson = client->CallMethod("die", _json);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Caught an exception calling die " << e.what());
    try {
      if (child.running()) {
        child.terminate();
      }
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Exception caught terminating child " << e.what());
    }
  } catch (...) {
    LOG_GENERAL(WARNING, "Unknown error encountered");
  }
  return true;
}

}  // namespace

void EvmClient::Init() {
  LOG_MARKER();
  LOG_GENERAL(INFO, "Intending to use " << EVM_SERVER_SOCKET_PATH
                                        << " for communication");
  if (LAUNCH_EVM_DAEMON) {
    CleanupPreviousInstances();
  } else {
    LOG_GENERAL(INFO, "Not killing previous instances dut to config " );
  }
}

void EvmClient::Reset() {
  Terminate(m_child, m_client);
  CleanupPreviousInstances();
}

EvmClient::~EvmClient() { LOG_MARKER(); }

bool EvmClient::OpenServer() {
  bool status{true};
  LOG_MARKER();
  LOG_GENERAL(INFO, "OpenServer for EVM ");

  try {
    if (LAUNCH_EVM_DAEMON) {
      status =
          LaunchEvmDaemon(m_child, EVM_SERVER_BINARY, EVM_SERVER_SOCKET_PATH);
    }
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught creating child " << e.what());
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING, "Unhandled Exception caught creating child ");
    return false;
  }
  try {
    m_connector = std::make_unique<evmdsrpc::EvmDsDomainSocketClient>(
        EVM_SERVER_SOCKET_PATH);
    m_client = std::make_unique<jsonrpc::Client>(*m_connector,
                                                 jsonrpc::JSONRPC_CLIENT_V2);
  } catch (...) {
    LOG_GENERAL(WARNING, "Unhandled Exception initialising client");
    return false;
  }
  return status;
}

bool EvmClient::CallRunner(const Json::Value& _json,
                           evmproj::CallResponse& result) {
  LOG_MARKER();
#ifdef USE_LOCKING_EVM
  std::lock_guard<std::mutex> g(m_mutexMain);
#endif
  if (not m_child.running()) {
    if (not EvmClient::OpenServer()) {
      LOG_GENERAL(INFO, "Failed to establish connection to evmd-ds");
      return false;
    }
  }
  try {
    const auto oldJson = m_client->CallMethod("run", _json);
    try {
      const auto reply = evmproj::GetReturn(oldJson, result);
      return true;
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING,
                  "Exception out of parsing json response " << e.what());
      return false;
    }
  } catch (jsonrpc::JsonRpcException& e) {
    throw e;
  } catch (...) {
    LOG_GENERAL(WARNING, "Exception caught executing run ");
    return false;
  }
}
