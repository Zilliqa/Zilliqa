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

#ifndef __POW_GETWORK_SERVER_H__
#define __POW_GETWORK_SERVER_H__

#include <chrono>

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

#include "common/Constants.h"
#include "libPOW/pow.h"

// Generate from zil_spec.json
class AbstractStubServer : public jsonrpc::AbstractServer<AbstractStubServer> {
 public:
  AbstractStubServer(
      jsonrpc::AbstractServerConnector &conn,
      jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V1V2)
      : jsonrpc::AbstractServer<AbstractStubServer>(conn, type) {
    // ETH getWork
    // https://github.com/ethereum/wiki/wiki/JSON-RPC
    this->bindAndAddMethod(
        jsonrpc::Procedure("eth_getWork", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_ARRAY, NULL),
        &AbstractStubServer::getWorkI);

    this->bindAndAddMethod(
        jsonrpc::Procedure("eth_submitHashrate", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_BOOLEAN, "Hashrate",
                           jsonrpc::JSON_STRING, "miner_wallet",
                           jsonrpc::JSON_STRING, "worker", jsonrpc::JSON_STRING,
                           NULL),
        &AbstractStubServer::submitHashrateI);

    this->bindAndAddMethod(
        jsonrpc::Procedure(
            "eth_submitWork", jsonrpc::PARAMS_BY_POSITION,
            jsonrpc::JSON_BOOLEAN, "nonce", jsonrpc::JSON_STRING, "header",
            jsonrpc::JSON_STRING, "mixdigest", jsonrpc::JSON_STRING, "boundary",
            jsonrpc::JSON_STRING, "miner_wallet", jsonrpc::JSON_STRING,
            "worker", jsonrpc::JSON_STRING, NULL),
        &AbstractStubServer::submitWorkI);
  }

  inline virtual void getWorkI(const Json::Value &request,
                               Json::Value &response) {
    (void)request;
    response = this->getWork();
  }
  inline virtual void submitHashrateI(const Json::Value &request,
                                      Json::Value &response) {
    response = this->submitHashrate(
        request[0u].asString(), request[1u].asString(), request[2u].asString());
  }
  inline virtual void submitWorkI(const Json::Value &request,
                                  Json::Value &response) {
    response = this->submitWork(request[0u].asString(), request[1u].asString(),
                                request[2u].asString(), request[3u].asString(),
                                request[4u].asString(), request[5u].asString());
  }

  virtual Json::Value getWork() = 0;
  virtual bool submitHashrate(const std::string &hashrate,
                              const std::string &miner_wallet,
                              const std::string &worker) = 0;
  virtual bool submitWork(const std::string &nonce, const std::string &header,
                          const std::string &mixdigest,
                          const std::string &boundary,
                          const std::string &miner_wallet,
                          const std::string &worker) = 0;
};

struct PoWWorkPackage {
  PoWWorkPackage() = default;

  std::string header;
  std::string seed;
  std::string boundary;

  uint64_t blocknum;
  uint8_t difficulty;
};

// helper functions
extern bool HexStringToUint64(const std::string &s, uint64_t *res);
extern bool NormalizeHexString(std::string &s);  // change in-place

// Implement AbstractStubServer
class GetWorkServer : public AbstractStubServer {
  // Constructor
  GetWorkServer(jsonrpc::AbstractServerConnector &conn)
      : AbstractStubServer(conn) {}

  ~GetWorkServer() {}

  GetWorkServer(GetWorkServer const &) = delete;
  void operator=(GetWorkServer const &) = delete;

  // Mining
  std::atomic<bool> m_isMining{false};
  std::chrono::system_clock::time_point m_startTime;
  std::chrono::system_clock::time_point m_nextPoWTime;
  std::mutex m_mutexPoWTime;

  PoWWorkPackage m_curWork;
  std::mutex m_mutexWork;

  ethash_mining_result_t m_curResult;
  std::mutex m_mutexResult;
  std::condition_variable m_cvGotResult;

 public:
  // Returns the singleton instance.
  static GetWorkServer &GetInstance();

  // Server methods
  bool StartServer();
  bool StopServer() {
    StopMining();
    return StopListening();
  };

  // Mining methods
  void SetNextPoWTime(const std::chrono::system_clock::time_point &tp);
  int GetSecondsToNextPoW();

  bool StartMining(const PoWWorkPackage &wp);
  void StopMining();
  ethash_mining_result_t VerifySubmit(const std::string &nonce,
                                      const std::string &header,
                                      const std::string &mixdigest,
                                      const std::string &boundary);

  // Protocol for GetResult
  // if wait_ms < 0: wait until the first accept result
  // if wait_ms = 0: return current result immediately
  // if wait_ms > 0: wait until timeout, return the last result
  ethash_mining_result_t GetResult(const int &wait_ms = -1);

  bool UpdateCurrentResult(const ethash_mining_result_t &newResult);

  // RPC methods
  virtual Json::Value getWork();
  virtual bool submitHashrate(const std::string &hashrate,
                              const std::string &miner_wallet,
                              const std::string &worker);
  virtual bool submitWork(const std::string &nonce, const std::string &header,
                          const std::string &mixdigest,
                          const std::string &boundary,
                          const std::string &miner_wallet,
                          const std::string &worker);
};

#endif  // __POW_GETWORK_SERVER_H__
