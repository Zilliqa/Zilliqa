/*
* Copyright (C) 2020 Zilliqa
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
