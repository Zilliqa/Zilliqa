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

#ifndef ZILLIQA_SRC_LIBSERVER_ISOLATEDSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_ISOLATEDSERVER_H_

#include "LookupServer.h"

class Mediator;

class IsolatedServer : public LookupServer,
                       public jsonrpc::AbstractServer<IsolatedServer> {
  uint64_t m_blocknum;
  uint128_t m_gasPrice{GAS_PRICE_MIN_VALUE};
  std::atomic<uint32_t> m_timeDelta;
  std::unordered_map<uint64_t, std::vector<TxnHash>> m_txnBlockNumMap;
  std::mutex mutable m_txnBlockNumMapMutex;
  std::mutex mutable m_blockMutex;
  const PairOfKey m_key;
  uint64_t m_currEpochGas{0};

  bool StartBlocknumIncrement();
  TxBlock GenerateTxBlock();
  void PostTxBlock();

 public:
  IsolatedServer(Mediator& mediator, jsonrpc::AbstractServerConnector& server,
                 const uint64_t& blocknum, const uint32_t& timeDelta);
  ~IsolatedServer() = default;

  inline virtual void CreateTransactionI(const Json::Value& request,
                                         Json::Value& response) {
    response = this->CreateTransaction(request[0u]);
  }

  inline virtual void IncreaseBlocknumI(const Json::Value& request,
                                        Json::Value& response) {
    response = this->IncreaseBlocknum(request[0u].asUInt());
  }
  inline virtual void GetMinimumGasPriceI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetMinimumGasPrice();
  }
  inline virtual void SetMinimumGasPriceI(const Json::Value& request,
                                          Json::Value& response) {
    response = this->SetMinimumGasPrice(request[0u].asString());
  }
  inline virtual void GetBlocknumI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = this->GetBlocknum();
  }

  inline virtual void GetTransactionsForTxBlockI(const Json::Value& request,
                                                 Json::Value& response) {
    response = this->GetTransactionsForTxBlock(request[0u].asString());
  }

  std::string GetMinimumGasPrice();
  std::string SetMinimumGasPrice(const std::string& gasPrice);
  Json::Value CreateTransaction(const Json::Value& _json);
  std::string IncreaseBlocknum(const uint32_t& delta);
  std::string GetBlocknum();
  Json::Value GetTransactionsForTxBlock(const std::string& txBlockNum);
  bool ValidateTxn(const Transaction& tx, const Address& fromAddr,
                   const Account* sender, const uint128_t& gasPrice);
  bool RetrieveHistory();
};

#endif  // ZILLIQA_SRC_LIBSERVER_ISOLATEDSERVER_H_
