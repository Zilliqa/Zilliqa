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

 public:
   static EvmClient& GetInstance() {
     static EvmClient evmClient;
     return evmClient;
   }

   void Init();

   bool CheckClient(uint32_t version,
                    __attribute__((unused)) bool enforce = false);

   bool CallChecker(uint32_t version, const Json::Value& _json,
                    std::string& result, uint32_t counter = MAXRETRYCONN);
   bool CallRunner(uint32_t version, const Json::Value& _json,
                   std::string& result, uint32_t counter = MAXRETRYCONN);
   bool CallDisambiguate(uint32_t version, const Json::Value& _json,
                         std::string& result, uint32_t counter = MAXRETRYCONN);
 private:

  EvmClient(){}
  virtual ~EvmClient(){}
  bool OpenServer(uint32_t version);

  std::map<uint32_t, std::shared_ptr<jsonrpc::Client>> m_clients;
  std::map<uint32_t, std::shared_ptr<jsonrpc::UnixDomainSocketClient>> m_connectors;

  std::mutex m_mutexMain;

};

#endif  // ZILLIQA_EVMCLIENT_H
