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

#include <chrono>

#include "depends/libethash/include/ethash/ethash.hpp"
#include "depends/safeserver/safehttpserver.h"

#include "GetWorkServer.h"
#include "common/Constants.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"

using namespace std;
using namespace jsonrpc;

//////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////

static ethash_mining_result_t FAIL_RESULT = {"", "", 0, false};

// GetInstance returns the singleton instance
GetWorkServer& GetWorkServer::GetInstance() {
  static SafeHttpServer httpserver(GETWORK_SERVER_PORT);
  static GetWorkServer powserver(httpserver);
  return powserver;
}

//////////////////////////////////////////////////
// Server Methods
//////////////////////////////////////////////////

// StartServer starts RPC server
bool GetWorkServer::StartServer() {
  if (!GETWORK_SERVER_MINE) {
    LOG_GENERAL(WARNING, "GETWORK_SERVER_MINE is not enabled");
    return false;
  }
  if (FULL_DATASET_MINE) {
    LOG_GENERAL(WARNING, "FULL_DATASET_MINE will be disabled");
  }
  if (OPENCL_GPU_MINE || CUDA_GPU_MINE) {
    LOG_GENERAL(WARNING, "OPENCL_GPU_MINE and CUDA_GPU_MINE will be disabled");
  }
  return StartListening();
}

//////////////////////////////////////////////////
// Mining methods
//////////////////////////////////////////////////

// StartMining starts mining
bool GetWorkServer::StartMining(const PoWWorkPackage& wp) {
  // clear the last result
  {
    lock_guard<mutex> g(m_mutexResult);
    m_curResult.success = false;
  }
  // set work package
  {
    lock_guard<mutex> g(m_mutexWork);
    m_startTime = std::chrono::system_clock::now();
    m_curWork = wp;
    m_isMining = true;
  }

  LOG_GENERAL(INFO, "Got PoW Work : "
                        << "header [" << wp.header << "], block ["
                        << wp.blocknum << "], difficulty ["
                        << int(wp.difficulty) << "]");

  return m_isMining;
}

// StopMining stops mining and clear result
void GetWorkServer::StopMining() {
  m_isMining = false;

  lock_guard<mutex> g(m_mutexResult);
  m_curResult.success = false;
}

// SetNextPoWTime sets the time of next PoW
void GetWorkServer::SetNextPoWTime(
    const std::chrono::system_clock::time_point& tp) {
  lock_guard<mutex> g(m_mutexPoWTime);
  m_nextPoWTime = tp;
}

// GetSecondsToNextPoW returns how many seconds to next PoW
int GetWorkServer::GetSecondsToNextPoW() {
  lock_guard<mutex> g(m_mutexPoWTime);
  auto now = std::chrono::system_clock::now();
  auto delta =
      std::chrono::duration_cast<std::chrono::seconds>(m_nextPoWTime - now);
  auto seconds = delta.count();
  return seconds > 0 ? seconds : 0;
}

// GetResult returns the Pow Result
ethash_mining_result_t GetWorkServer::GetResult(int waitTime) {
  {
    lock_guard<mutex> g(m_mutexResult);
    if (!m_isMining || m_curResult.success) {
      return m_curResult;
    }
  }

  std::unique_lock<std::mutex> lk(m_mutexResult);
  if (m_cvGotResult.wait_for(lk, chrono::seconds(waitTime)) ==
      std::cv_status::timeout) {
    LOG_GENERAL(WARNING, "GetResult Timeout, time window " << waitTime);
  }

  return m_curResult;
}

// VerifySubmit verify and create ethash result
ethash_mining_result_t GetWorkServer::VerifySubmit(const string& nonce,
                                                   const string& header,
                                                   const string& mixdigest,
                                                   const string& boundary) {
  uint64_t winning_nonce = {0};

  // convert
  if (!DataConversion::HexStringToUint64(nonce, &winning_nonce)) {
    LOG_GENERAL(WARNING, "Invalid nonce: " << nonce);
    return FAIL_RESULT;
  }

  lock_guard<mutex> g(m_mutexWork);

  // check the header and boundary is same with current work
  if (header != m_curWork.header) {
    LOG_GENERAL(WARNING, "Submit header diff with current work");
    LOG_GENERAL(WARNING, "Current header: " << m_curWork.header);
    LOG_GENERAL(WARNING, "Submit header: " << header);
    return FAIL_RESULT;
  }
  if (boundary != m_curWork.boundary) {
    LOG_GENERAL(WARNING, "Submit boundary diff with current work");
    LOG_GENERAL(WARNING, "Current boundary: " << m_curWork.boundary);
    LOG_GENERAL(WARNING, "Submit boundary: " << boundary);
    return FAIL_RESULT;
  }

  ethash_hash256 final_result;
  if (!POW::GetInstance().VerifyRemoteSoln(
          m_curWork.blocknum, POW::StringToBlockhash(boundary), winning_nonce,
          POW::StringToBlockhash(header), POW::StringToBlockhash(mixdigest),
          final_result)) {
    LOG_GENERAL(WARNING, "Failed to verify PoW result from miner.");
    return FAIL_RESULT;
  }

  return ethash_mining_result_t{POW::BlockhashToHexString(final_result),
                                mixdigest, winning_nonce, true};
}

// UpdateCurrentResult check and update new result
bool GetWorkServer::UpdateCurrentResult(
    const ethash_mining_result_t& newResult) {
  if (!newResult.success) {
    LOG_GENERAL(WARNING, "newResult is not success");
    return false;
  }

  bool accept = false;

  lock_guard<mutex> g(m_mutexResult);

  if (!m_curResult.success) {
    // accept the new result directly if current result is false
    accept = true;
  } else {
    // accept the new result if it less or equal than current one
    auto new_hash = POW::StringToBlockhash(newResult.result);
    auto cur_hash = POW::StringToBlockhash(m_curResult.result);
    accept = ethash::is_less_or_equal(new_hash, cur_hash);
  }

  if (accept) {
    // save new result, and notify other threads to get new result
    m_curResult = newResult;
    m_cvGotResult.notify_all();
    LOG_GENERAL(INFO, "newResult accepted!");
  } else {
    LOG_GENERAL(INFO, "newResult is not accepted!");
  }

  return accept;
}

//////////////////////////////////////////////////
// RPC Methods
//////////////////////////////////////////////////

// ETH getWork Server
Json::Value GetWorkServer::getWork() {
  LOG_MARKER();
  Json::Value result;

  lock_guard<mutex> g(m_mutexWork);

  result.append(m_isMining ? m_curWork.header : "");
  result.append(m_isMining ? m_curWork.seed : "");
  result.append(m_isMining ? m_curWork.boundary : "");
  result.append(m_isMining.load());
  result.append(GetSecondsToNextPoW());

  return result;
}

bool GetWorkServer::submitWork(const string& _nonce, const string& _header,
                               const string& _mixdigest,
                               const string& _boundary,
                               [[gnu::unused]] const string& _miner_wallet,
                               [[gnu::unused]] const string& _worker) {
  LOG_MARKER();

  if (!m_isMining) {
    LOG_GENERAL(WARNING, "PoW is not running, ignore submit");
    return false;
  }

  string nonce = _nonce;
  string header = _header;
  string mixdigest = _mixdigest;
  string boundary = _boundary;

  LOG_GENERAL(INFO, "Got PoW Result: ");
  LOG_GENERAL(INFO, "    nonce: " << nonce);
  LOG_GENERAL(INFO, "    header: " << header);
  LOG_GENERAL(INFO, "    mixdigest: " << mixdigest);
  LOG_GENERAL(INFO, "    boundary: " << boundary);

  if (!DataConversion::NormalizeHexString(nonce) ||
      !DataConversion::NormalizeHexString(header) ||
      !DataConversion::NormalizeHexString(mixdigest) ||
      !DataConversion::NormalizeHexString(boundary)) {
    LOG_GENERAL(WARNING, "Invalid input parameters");
    return false;
  }

  auto result = VerifySubmit(nonce, header, mixdigest, boundary);

  return UpdateCurrentResult(result);
  ;
}

bool GetWorkServer::submitHashrate([[gnu::unused]] const string& hashrate,
                                   [[gnu::unused]] const string& miner_wallet,
                                   [[gnu::unused]] const string& worker) {
  return true;
}
