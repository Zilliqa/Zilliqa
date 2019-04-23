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

#ifndef __GETWORK_SERVER_H__
#define __GETWORK_SERVER_H__

#include <chrono>

#include "jsonrpccpp/server.h"
#include "jsonrpccpp/server/abstractserverconnector.h"

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
  ethash_mining_result_t GetResult(int waitTime);

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

#endif  // __GETWORK_SERVER_H__
