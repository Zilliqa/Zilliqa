//
// Created by steve on 04/05/22.
//

#ifndef ZILLIQA_EVMCLIENT_H
#define ZILLIQA_EVMCLIENT_H

#include <map>
#include <memory>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/unixdomainsocketclient.h>

#include "common/Constants.h"

class EvmClient {
  std::shared_ptr<jsonrpc::Client>                      m_client;
  std::shared_ptr<jsonrpc::UnixDomainSocketClient>      m_connector;
  bool                                                  m_initialised = {false};

  std::mutex m_mutexMain;

  EvmClient(){}

 public:
  virtual ~EvmClient(){}

 private:

 public:
  static EvmClient& GetInstance() {
    static EvmClient evmClient;
    return evmClient;
  }

  void Init();

  bool CallRunner(uint32_t version, const Json::Value& _json,
                  std::string& result,
                  __attribute__((unused)) uint32_t counter = MAXRETRYCONN);
};

#endif  // ZILLIQA_EVMCLIENT_H
