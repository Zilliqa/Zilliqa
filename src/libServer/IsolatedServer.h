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
#include "common/Constants.h"

class Mediator;

namespace rcp {
class APIServer;
}

class IsolatedServer : public LookupServer,
                       public jsonrpc::AbstractServer<IsolatedServer> {
  uint64_t m_blocknum;
  bool m_pause{false};
  bool m_intervalMiningInitialized{false};
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
  std::string m_uuid;
  IsolatedServer(Mediator& mediator, std::shared_ptr<rpc::APIServer> apiServer,
                 const uint64_t& blocknum, const uint32_t& timeDelta);
  ~IsolatedServer() = default;

  inline virtual void CreateTransactionI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->CreateTransaction(request[0u]);
  }

  void BindAllEvmMethods();

  inline virtual void GetEvmMineI(const Json::Value&, Json::Value&) {
    PostTxBlock();
  }

  inline virtual void GetEvmSetIntervalMiningI(const Json::Value& request,
                                               Json::Value&) {
    m_timeDelta = request[0u].asUInt();

    // If this the first time we're going to use interval mining, initialize the
    // block num thread.
    if (!m_intervalMiningInitialized && m_timeDelta > 0) {
      StartBlocknumIncrement();
    }

    // If new interval is 0, stop interval mining.
    m_pause = m_timeDelta == 0;
  }

  inline virtual void GetEthSendRawTransactionI(const Json::Value& request,
                                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    auto rawTx = request[0u].asString();

    if (LOG_SC) {
      LOG_GENERAL(INFO, "rawTx: " << rawTx);
    }

    // Erase '0x' at the beginning if it exists
    if (rawTx.size() >= 2 && rawTx[1] == 'x') {
      rawTx.erase(0, 2);
    }
    auto const pubKey = RecoverECDSAPubKey(rawTx, ETH_CHAINID);

    if (pubKey.empty()) {
      response = "0x0";
      return;
    }

    auto const fields = Eth::parseRawTxFields(rawTx);

    response = CreateTransactionEth(fields, pubKey);
  }

  inline virtual void GetEthBlockNumberI(const Json::Value& /*request*/,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthBlockNumber();
  }

  inline virtual void IncreaseBlocknumI(const Json::Value& request,
                                        Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->IncreaseBlocknum(request[0u].asUInt());
  }
  inline virtual void GetMinimumGasPriceI(const Json::Value& request,
                                          Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    (void)request;
    response = this->GetMinimumGasPrice();
  }
  inline virtual void SetMinimumGasPriceI(const Json::Value& request,
                                          Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->SetMinimumGasPrice(request[0u].asString());
  }
  inline virtual void GetBlocknumI(const Json::Value& request,
                                   Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    (void)request;
    response = this->GetBlocknum();
  }

  inline virtual void GetTransactionsForTxBlockI(const Json::Value& request,
                                                 Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetTransactionsForTxBlock(request[0u].asString());
  }

  inline virtual void CheckPauseI(const Json::Value& request,
                                  Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->CheckPause(request[0u].asString());
  }

  inline virtual void TogglePauseI(const Json::Value& request,
                                   Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->TogglePause(request[0u].asString());
  }

  bool CheckPause(const std::string& uuid);
  bool TogglePause(const std::string& uuid);

  std::string GetMinimumGasPrice();
  std::string SetMinimumGasPrice(const std::string& gasPrice);
  Json::Value CreateTransaction(const Json::Value& _json);
  std::string CreateTransactionEth(Eth::EthFields const& fields,
                                   zbytes const& pubKey);
  Json::Value GetEthStorageAt(std::string const& address,
                              std::string const& position,
                              std::string const& blockNum);
  std::string IncreaseBlocknum(const uint32_t& delta);
  std::string GetBlocknum();
  Json::Value GetEthBlockNumber();
  Json::Value GetTransactionsForTxBlock(const std::string& txBlockNum);
  bool ValidateTxn(const Transaction& tx, const Address& fromAddr,
                   const Account* sender, const uint128_t& gasPrice);
  bool RetrieveHistory(const bool& nonisoload);

  void ExtractTxnHashes(const TxBlock& txBlock, std::vector<std::string>& out);
  bool ExtractTxnReceipt(const std::string& txHash, Json::Value& receipt);
};

#endif  // ZILLIQA_SRC_LIBSERVER_ISOLATEDSERVER_H_
