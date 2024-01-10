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

#ifndef ZILLIQA_SRC_LIBSERVER_LOOKUPSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_LOOKUPSERVER_H_

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "EthRpcMethods.h"
#include "Server.h"
#include "libMetrics/Api.h"

namespace mp = boost::multiprecision;

class Mediator;
namespace rpc {
class APIServer;
}

typedef std::function<bool(const Transaction& tx, uint32_t shardId)>
    CreateTransactionTargetFunc;

class LookupServer : public Server,
                     public EthRpcMethods,
                     public jsonrpc::AbstractServer<LookupServer> {
  std::mutex m_mutexBlockTxPair;
  std::pair<uint64_t, uint128_t> m_BlockTxPair;
  std::mutex m_mutexTxBlockCountSumPair;
  std::pair<uint64_t, uint128_t> m_TxBlockCountSumPair;
  uint64_t m_StartTimeTx;
  uint64_t m_StartTimeDs;
  std::mutex m_mutexDSBlockCache;
  std::pair<uint64_t, CircularArray<std::string>> m_DSBlockCache;
  std::mutex m_mutexTxBlockCache;
  std::pair<uint64_t, CircularArray<std::string>> m_TxBlockCache;
  static CircularArray<std::string> m_RecentTransactions;
  static std::mutex m_mutexRecentTxns;
  std::mt19937 m_eng;
  std::shared_ptr<rpc::APIServer> m_apiServer;

  Z_I64METRIC m_callCount{Z_FL::API_SERVER, "lookup_invocation_count",
                          "Calls to Lookup Server", "Calls"};

  Json::Value GetTransactionsForTxBlock(const std::string& txBlockNum,
                                        const std::string& pageNumber);

  std::string CheckContractTxn(const Transaction& tx, bool toAccountExist,
                               bool toAccountIsContract);
  mp::cpp_dec_float_50 CalculateTotalSupply();

 public:
  LookupServer(Mediator& mediator, std::shared_ptr<rpc::APIServer> apiServer);
  ~LookupServer() = default;

  inline bool bindAndAddExternalMethod(const jsonrpc::Procedure& proc,
                                       methodPointer_t pointer) {
    return bindAndAddMethod(proc, pointer);
  }

  inline virtual void GetNetworkIdI(const Json::Value& request,
                                    Json::Value& response) {
    (void)request;
    response = this->GetNetworkId();
  }

  inline virtual void CreateTransactionI(const Json::Value& request,
                                         Json::Value& response) {
    response = CreateTransaction(
        request[0u],
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice());
  }

  inline virtual void GetTransactionI(const Json::Value& request,
                                      Json::Value& response) {
    response = this->GetTransaction(request[0u].asString());
  }

  inline virtual void GetSoftConfirmedTransactionI(const Json::Value& request,
                                                   Json::Value& response) {
    response = this->GetSoftConfirmedTransaction(request[0u].asString());
  }

  inline virtual void GetDsBlockI(const Json::Value& request,
                                  Json::Value& response) {
    response = this->GetDsBlock(request[0u].asString());
  }

  inline virtual void GetDsBlockVerboseI(const Json::Value& request,
                                         Json::Value& response) {
    response = this->GetDsBlock(request[0u].asString(), true);
  }

  inline virtual void GetTxBlockI(const Json::Value& request,
                                  Json::Value& response) {
    response = this->GetTxBlockByNum(request[0u].asString());
  }

  inline virtual void GetTxBlockVerboseI(const Json::Value& request,
                                         Json::Value& response) {
    response = this->GetTxBlockByNum(request[0u].asString(), true);
  }

  inline virtual void GetLatestDsBlockI(const Json::Value& request,
                                        Json::Value& response) {
    (void)request;
    response = this->GetLatestDsBlock();
  }

  inline virtual void GetLatestTxBlockI(const Json::Value& request,
                                        Json::Value& response) {
    (void)request;
    response = this->GetLatestTxBlock();
  }

  inline virtual void GetBalanceI(const Json::Value& request,
                                  Json::Value& response) {
    response = this->GetBalanceAndNonce(request[0u].asString());
  }

  inline virtual void GetMinimumGasPriceI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetMinimumGasPrice();
  }

  inline virtual void GetSmartContractsI(const Json::Value& request,
                                         Json::Value& response) {
    response = this->GetSmartContracts(request[0u].asString());
  }

  inline virtual void GetContractAddressFromTransactionIDI(
      const Json::Value& request, Json::Value& response) {
    response =
        this->GetContractAddressFromTransactionID(request[0u].asString());
  }

  inline virtual void GetNumPeersI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = this->GetNumPeers();
  }

  inline virtual void GetNumTxBlocksI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetNumTxBlocks();
  }

  inline virtual void GetNumDSBlocksI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetNumDSBlocks();
  }

  inline virtual void GetNumTransactionsI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetNumTransactions();
  }

  inline virtual void GetTransactionRateI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetTransactionRate();
  }

  inline virtual void GetTxBlockRateI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetTxBlockRate();
  }

  inline virtual void GetDSBlockRateI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetDSBlockRate();
  }

  inline virtual void DSBlockListingI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->DSBlockListing(request[0u].asUInt());
  }

  inline virtual void TxBlockListingI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->TxBlockListing(request[0u].asUInt());
  }

  inline virtual void GetBlockchainInfoI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->GetBlockchainInfo();
  }

  inline virtual void GetRecentTransactionsI(const Json::Value& request,
                                             Json::Value& response) {
    (void)request;
    response = this->GetRecentTransactions();
  }

  inline virtual void GetShardingStructureI(const Json::Value& request,
                                            Json::Value& response) {
    (void)request;
    response = this->GetShardingStructure();
  }

  inline virtual void GetNumTxnsTxEpochI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->GetNumTxnsTxEpoch();
  }

  inline virtual void GetNumTxnsDSEpochI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->GetNumTxnsDSEpoch();
  }

  inline virtual void GetSmartContractSubStateI(const Json::Value& request,
                                                Json::Value& response) {
    response = this->GetSmartContractState(request[0u].asString(),
                                           request[1u].asString(), request[2u]);
  }

  inline virtual void GetSmartContractStateI(const Json::Value& request,
                                             Json::Value& response) {
    response = this->GetSmartContractState(request[0u].asString());
  }

  inline virtual void GetSmartContractCodeI(const Json::Value& request,
                                            Json::Value& response) {
    response = this->GetSmartContractCode(request[0u].asString());
  }

  inline virtual void GetSmartContractInitI(const Json::Value& request,
                                            Json::Value& response) {
    response = this->GetSmartContractInit(request[0u].asString());
  }

  inline virtual void GetTransactionsForTxBlockI(const Json::Value& request,
                                                 Json::Value& response) {
    response = this->GetTransactionsForTxBlock(request[0u].asString(), "");
  }

  inline virtual void GetTransactionsForTxBlockExI(const Json::Value& request,
                                                   Json::Value& response) {
    response = this->GetTransactionsForTxBlock(request[0u].asString(),
                                               request[1u].asString());
  }

  inline virtual void GetTxnBodiesForTxBlockI(const Json::Value& request,
                                              Json::Value& response) {
    response = this->GetTxnBodiesForTxBlock(request[0u].asString(), "");
  }

  inline virtual void GetTxnBodiesForTxBlockExI(const Json::Value& request,
                                                Json::Value& response) {
    response = this->GetTxnBodiesForTxBlock(request[0u].asString(),
                                            request[1u].asString());
  }

  inline virtual void GetShardMembersI(const Json::Value& request,
                                       Json::Value& response) {
    response = this->GetShardMembers(request[0u].asUInt());
  }

  inline virtual void GetCurrentDSCommI(const Json::Value& request,
                                        Json::Value& response) {
    (void)request;
    response = this->GetCurrentDSComm();
  }

  inline virtual void GetTotalCoinSupplyI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetTotalCoinSupply();
  }

  inline virtual void GetTotalCoinSupplyAsIntI(const Json::Value& request,
                                               Json::Value& response) {
    (void)request;
    static_assert(sizeof(unsigned long) <= sizeof(Json::UInt64));
    response = static_cast<Json::UInt64>(this->GetTotalCoinSupplyAsInt());
  }

  inline virtual void GetPendingTxnsI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetPendingTxns();
  }

  inline virtual void GetMinerInfoI(const Json::Value& request,
                                    Json::Value& response) {
    response = this->GetMinerInfo(request[0u].asString());
  }

  inline virtual void GetTransactionStatusI(const Json::Value& request,
                                            Json::Value& response) {
    response = this->GetTransactionStatus(request[0u].asString());
  }

  inline virtual void GetStateProofI(const Json::Value& request,
                                     Json::Value& response) {
    response = this->GetStateProof(
        request[0u].asString(), request[1u].asString(), request[2u].asString());
  }
  std::string GetNetworkId();
  Json::Value CreateTransaction(const Json::Value& _json,
                                const uint128_t& gasPrice);

  Json::Value GetTransaction(const std::string& transactionHash);
  Json::Value GetSoftConfirmedTransaction(const std::string& txnHash);
  Json::Value GetDsBlock(const std::string& blockNum, bool verbose = false);
  Json::Value GetTxBlockByNum(const std::string& blockNum,
                              bool verbose = false);
  Json::Value GetLatestDsBlock();
  Json::Value GetLatestTxBlock();
  Json::Value GetBalanceAndNonce(const std::string& address);
  std::string GetMinimumGasPrice();
  Json::Value GetSmartContracts(const std::string& address);
  std::string GetContractAddressFromTransactionID(const std::string& tranID);
  unsigned int GetNumPeers();
  std::string GetNumTxBlocks();
  std::string GetNumDSBlocks();
  std::string GetNumTransactions();
  double GetTransactionRate();
  double GetTxBlockRate();
  double GetDSBlockRate();
  std::string GetTotalCoinSupply();
  unsigned long GetTotalCoinSupplyAsInt();
  Json::Value GetCurrentDSComm();
  Json::Value GetShardMembers(unsigned int shardID);
  Json::Value DSBlockListing(unsigned int page);
  Json::Value TxBlockListing(unsigned int page);
  Json::Value GetBlockchainInfo();
  static Json::Value GetRecentTransactions();
  Json::Value GetShardingStructure();
  std::string GetNumTxnsDSEpoch();
  std::string GetNumTxnsTxEpoch();

  size_t GetNumTransactions(uint64_t blockNum);
  bool StartCollectorThread();
  std::string GetNodeState();

  static void AddToRecentTransactions(const dev::h256& txhash);

  // gets the number of transaction starting from block blockNum to most recent
  // block
  Json::Value GetPendingTxns();
  Json::Value GetSmartContractState(
      const std::string& address, const std::string& vname = "",
      const Json::Value& indices = Json::arrayValue);
  Json::Value GetSmartContractInit(const std::string& address);
  Json::Value GetSmartContractCode(const std::string& address);

  static Json::Value GetTransactionsForTxBlock(
      const TxBlock& txBlock,
      const uint32_t pageNumber = std::numeric_limits<uint32_t>::max());

  Json::Value GetMinerInfo(const std::string& blockNum);
  Json::Value GetTxnBodiesForTxBlock(const std::string& txBlockNum,
                                     const std::string& pageNumber);
  Json::Value GetTransactionStatus(const std::string& txnhash);
  Json::Value GetStateProof(const std::string& address, const std::string& key,
                            const std::string& txBlockNumOrTag = "latest");

  auto GetApiServer() const { return m_apiServer; }
};

#endif  // ZILLIQA_SRC_LIBSERVER_LOOKUPSERVER_H_
