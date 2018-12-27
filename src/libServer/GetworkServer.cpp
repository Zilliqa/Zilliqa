/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <chrono>

#include <jsonrpccpp/server/connectors/httpserver.h>
#include "depends/libethash/include/ethash/ethash.hpp"

#include "GetworkServer.h"
#include "common/Constants.h"
#include "libPOW/pow.h"

using namespace std;
using namespace jsonrpc;

//////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////

static ethash_mining_result_t FAIL_RESULT = {"", "", 0, false};

bool HexStringToUint64(const std::string& s, uint64_t* res) {
  if (s.size() > 18) {
    return false;
  }
  try {
    *res = std::stoul(s, nullptr, 16);
  } catch (const std::invalid_argument& e) {
    LOG_GENERAL(WARNING, "Convert failed, invalid input: " << s);
    return false;
  } catch (const std::out_of_range& e) {
    LOG_GENERAL(WARNING, "Convert failed, out of range: " << s);
    return false;
  }
  return true;
}

bool NormalizeHexString(std::string& s) {
  if (s.size() < 2) {
    return false;
  }

  unsigned pos = 0;
  unsigned prefix_size = 0;

  for (char& c : s) {
    pos++;
    c = tolower(c);

    if (std::isdigit(c) || (('a' <= c) && (c <= 'f'))) {
      continue;
    }
    if ((c == 'x') && (pos == 2)) {
      prefix_size = 2;
      continue;
    }
    return false;
  }
  // remove prefix "0x"
  s.erase(0, prefix_size);

  return s.size() > 0;
}

// GetInstance returns the singleton instance
GetWorkServer& GetWorkServer::GetInstance() {
  static HttpServer httpserver(GETWORK_SERVER_PORT);
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
// if wait_ms < 0: wait until first accept result
// if wait_ms = 0: return current result immediately
// if wait_ms > 0: wait until timeout, return the last result
ethash_mining_result_t GetWorkServer::GetResult(const int& wait_ms) {
  if (wait_ms >= 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    return m_curResult;
  }

  // wait_ms < 0
  // wait until the first accept result
  std::unique_lock<std::mutex> lk(m_mutexResult);
  while (m_isMining && !m_curResult.success) {
    m_cvGotResult.wait(lk);

    // wait for a while miner is working
    lk.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    lk.lock();
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
  if (HexStringToUint64(nonce, &winning_nonce)) {
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

    auto header_hash = POW::StringToBlockhash(header);

    auto ethash_result = POW::GetInstance().LightHash(
        m_curWork.blocknum, header_hash, winning_nonce);

    auto final_result = POW::BlockhashToHexString(ethash_result.final_hash);

    auto success = POW::GetInstance().PoWVerify(
        m_curWork.blocknum, m_curWork.difficulty, header_hash, winning_nonce,
        final_result, mixdigest);
    if (!success) {
      LOG_GENERAL(WARNING, "PoW Verify Failed!");
      return FAIL_RESULT;
    }

    return ethash_mining_result_t{final_result, mixdigest, winning_nonce, true};
  }
  LOG_GENERAL(WARNING, "Invalid nonce: " << nonce);
  return FAIL_RESULT;
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
                               const string& _miner_wallet,
                               const string& _worker) {
  (void)_miner_wallet;
  (void)_worker;

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

  if (!NormalizeHexString(nonce) || !NormalizeHexString(header) ||
      !NormalizeHexString(mixdigest) || !NormalizeHexString(boundary)) {
    LOG_GENERAL(WARNING, "Invalid input parameters");
    return false;
  }

  auto result = VerifySubmit(nonce, header, mixdigest, boundary);

  bool accepted = UpdateCurrentResult(result);

  return accepted;
}

bool GetWorkServer::submitHashrate(const string& hashrate,
                                   const string& miner_wallet,
                                   const string& worker) {
  (void)hashrate;
  (void)miner_wallet;
  (void)worker;

  return true;
}
