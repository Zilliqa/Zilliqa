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

void EvmClient::Init() {
  LOG_MARKER();
  LOG_GENERAL(INFO, "Intending to use " << EVM_SERVER_SOCKET_PATH
                                        << " for communication");
  if (LAUNCH_EVM_DAEMON){
    CleanupPreviousInstances();
  } else {
    LOG_GENERAL(INFO, "EVM Client not launching daemon - debug mode ");
  }
}

EvmClient::~EvmClient() { LOG_MARKER(); }

bool EvmClient::Terminate() {
  LOG_MARKER();
  Json::Value _json;
  LOG_GENERAL(DEBUG, "Call evm with die request:" << _json);
  // call evm
  try {
    const auto oldJson = m_client->CallMethod("die", _json);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Caught an exception calling die " << e.what());
    try {
      if (m_child.running()) {
        m_child.terminate();
      }
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Exception caught terminating child " << e.what());
    }
  } catch (...) {
    LOG_GENERAL(WARNING, "Unknown error encountered");
  }
  return true;
}

bool EvmClient::OpenServer() {
  LOG_MARKER();
  boost::filesystem::path p(EVM_SERVER_BINARY);

  LOG_GENERAL(INFO, "OpenServer for EVM ");

  if (not boost::filesystem::exists(p)) {
    LOG_GENERAL(INFO, "Cannot create a subprocess that does not exist " +
                          EVM_SERVER_BINARY);
    return false;
  }

  const std::vector<std::string> args = {"--socket",
                                         EVM_SERVER_SOCKET_PATH,
                                         "--tracing",
                                         "--zil-scaling-factor",
                                         std::to_string(EVM_ZIL_SCALING_FACTOR),
                                         "--log4rs",
                                         EVM_LOG_CONFIG};

  try {
    if (LAUNCH_EVM_DAEMON) {
      boost::process::child c(p, boost::process::args(args));
      LOG_GENERAL(INFO, "child created ");
      m_child = std::move(c);
      pid_t thread_id = m_child.id();
      if (thread_id > 0 && m_child.valid()) {
        LOG_GENERAL(WARNING, "Valid child created at " << thread_id);
      } else {
        LOG_GENERAL(WARNING, "Valid child is not valid " << thread_id);
      }
    }
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught creating child " << e.what());
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING, "Unhandled Exception caught creating child ");
    return false;
  }
  // child should be running
  // but we will give it a couple of seconds ...
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  try {
    m_connector = std::make_unique<evmdsrpc::EvmDsDomainSocketClient>(
        EVM_SERVER_SOCKET_PATH);
    m_client = std::make_unique<jsonrpc::Client>(*m_connector,
                                                 jsonrpc::JSONRPC_CLIENT_V2);
  } catch (...) {
    // these methods do not throw ...
    LOG_GENERAL(WARNING, "Unhandled Exception creating connector");
    return false;
  }
  return true;
}

bool EvmClient::CleanupPreviousInstances() {
  std::string s = "pkill -9 -f " + EVM_SERVER_BINARY;
  int sysRep = std::system(s.c_str());
  if (sysRep != 0) {
    LOG_GENERAL(INFO, "system call return value " << sysRep);
  }
  return true;
}

bool EvmClient::CallRunner(uint32_t version, const Json::Value& _json,
                           evmproj::CallResponse& result,
                           const uint32_t counter) {
  LOG_MARKER();
#ifdef USE_LOCKING_EVM
  std::lock_guard<std::mutex> g(m_mutexMain);
#endif
  if (counter == 0 && LOG_SC) {
    LOG_GENERAL(INFO, "tried to resend three times and failed");
    return false;
  }

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
      result.SetSuccess(false);
      return false;
    }
  } catch (jsonrpc::JsonRpcException& e) {
    while (true) {
      LOG_GENERAL(WARNING,
                  "RPC Exception calling EVM-DS : attempting server "
                  "resend in 2 seconds "
                      << e.what());
      std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      return CallRunner(version, _json, result, counter - 1);
    }
    LOG_GENERAL(WARNING, "Too many retry attempts not sending " << e.what());
    result.SetSuccess(false);
    return false;
  } catch (...) {
    LOG_GENERAL(WARNING, "Exception caught executing run ");
    return false;
  }
  result.SetSuccess(true);
  return true;
}
