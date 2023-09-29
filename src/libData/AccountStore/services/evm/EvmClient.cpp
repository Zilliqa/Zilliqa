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
#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <filesystem>
#include <sstream>
#include <thread>
#include "libMetrics/Api.h"
#include "libScilla/ScillaUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"

namespace {

Z_I64METRIC& GetCallsCounter() {
  static Z_I64METRIC evmClientCount{Z_FL::EVM_CLIENT, "jsonrpc",
                                    "Calls to EVM-DS over jsonrpc", "Calls"};
  return evmClientCount;
}

const std::vector<std::string>& GetEvmDaemonArgs() {
  static const std::vector<std::string> args = {"--socket",
    EVM_SERVER_SOCKET_PATH,
    "--zil-scaling-factor",
    std::to_string(EVM_ZIL_SCALING_FACTOR),
    "--log4rs",
    EVM_LOG_CONFIG};
  return args;
}

bool LaunchEvmDaemon(boost::process::child& child,
                     const std::string& binaryPath,
                     const std::string& socketPath) {
  TRACE(zil::trace::FilterClass::DEMO);
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  const std::vector<std::string>& args = GetEvmDaemonArgs();
  std::filesystem::path bin_path(binaryPath);
  std::filesystem::path socket_path(socketPath);
  boost::system::error_code ec;

  if (std::filesystem::exists(socket_path)) {
    std::filesystem::remove(socket_path, ec);
    if (ec.failed()) {
      TRACE_ERROR("Problem removing filesystem entry for socket ");
    }
  }
  if (not std::filesystem::exists(bin_path)) {
    std::stringstream ss;
    TRACE_ERROR("Cannot create a subprocess that does not exist " +
                EVM_SERVER_BINARY);
    return false;
  }
  boost::process::child c =
      boost::process::child(bin_path.native(), boost::process::args(args));
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
  while (not std::filesystem::exists(socket_path)) {
    if ((counter++ % 10) == 0)
      LOG_GENERAL(WARNING, "Awaiting Launch of the evm-ds daemon ");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return true;
}

bool CleanupPreviousInstances() {
  INC_CALLS(GetCallsCounter());

  std::string s = "pkill -9 -f " + EVM_SERVER_BINARY;
  int sysRep = std::system(s.c_str());
  if (sysRep != -1) {
    LOG_GENERAL(INFO, "system call return value " << sysRep);
  }
  return true;
}

bool Terminate(boost::process::child& child,
               const std::unique_ptr<jsonrpc::Client>& client) {
  INC_CALLS(GetCallsCounter());

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
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();
  LOG_GENERAL(INFO, "Intending to use " << EVM_SERVER_SOCKET_PATH
                                        << " for communication");
  if (LAUNCH_EVM_DAEMON) {
    CleanupPreviousInstances();
  } else {
    // There is a lot of junk on stackoverflow about how to do this, but for us, this will do..
    const std::vector<std::string>& args(GetEvmDaemonArgs());
    std::ostringstream cmdLine;
    cmdLine << EVM_SERVER_BINARY;
    for (auto &arg : args) {
      cmdLine << " " << arg;
    }
    LOG_GENERAL(INFO, "Not launching evm due to config flag");
    LOG_GENERAL(INFO, "To launch it yourself, from " << std::filesystem::current_path() << " :");
    LOG_GENERAL(INFO, cmdLine.str());
  }
}

void EvmClient::Reset() {
  INC_CALLS(GetCallsCounter());

  Terminate(m_child, m_client);
  CleanupPreviousInstances();
}

EvmClient::~EvmClient() { LOG_MARKER(); }

bool EvmClient::OpenServer() {
  INC_CALLS(GetCallsCounter());

  bool status{true};
  TRACE_EVENT("OpenClient", "status", "OpenServer for EVM ");

  try {
    if (LAUNCH_EVM_DAEMON) {
      status =
          LaunchEvmDaemon(m_child, EVM_SERVER_BINARY, EVM_SERVER_SOCKET_PATH);
    }
  } catch (std::exception& e) {
    TRACE_ERROR("Exception caught creating child ");
    GetCallsCounter().IncrementAttr(
        {{"Error", "Serious"}, {"Exception#1", "OpenServer"}});
    return false;
  } catch (...) {
    TRACE_ERROR("Unhandled Exception caught creating child ");
    GetCallsCounter().IncrementAttr(
        {{"Error", "Serious"}, {"Exception#1", "OpenServer"}});
    return false;
  }
  try {
    m_connector =
        std::make_unique<rpc::UnixDomainSocketClient>(EVM_SERVER_SOCKET_PATH);
    m_client = std::make_unique<jsonrpc::Client>(*m_connector,
                                                 jsonrpc::JSONRPC_CLIENT_V2);
  } catch (...) {
    TRACE_ERROR("Unhandled Exception initialising client");
    GetCallsCounter().IncrementAttr(
        {{"Error", "Serious"}, {"Exception#3", "OpenServer"}});
    return false;
  }
  return status;
}

bool EvmClient::CallRunner(const Json::Value& _json, evm::EvmResult& result) {
  LOG_MARKER();
  TRACE(zil::trace::FilterClass::DEMO);

  std::lock_guard<std::mutex> g(m_mutexMain);

  if (not m_child.running()) {
    if (not EvmClient::OpenServer()) {
      TRACE_ERROR("Failed to establish connection to evmd-ds");
      return false;
    }
  }
  try {
    const auto replyJson = m_client->CallMethod("run", _json);

    try {
      EvmUtils::GetEvmResultFromJson(replyJson, result);

      if (LOG_SC) {
        LOG_GENERAL(INFO, "<============ Call EVM result: ");
        EvmUtils::PrintDebugEvmResult(result);
      }

      return true;
    } catch (std::exception& e) {
      TRACE_ERROR("Exception parsing json response");
      return false;
    }
  } catch (jsonrpc::JsonRpcException& e) {
    throw e;
  } catch (...) {
    TRACE_ERROR("Exception caught executing run ");
    return false;
  }
}
