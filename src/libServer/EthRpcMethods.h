/*
 * Copyright (C) 2022 Zilliqa
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
#ifndef ZILLIQA_SRC_LIBSERVER_ETHRPCMETHODS_H_
#define ZILLIQA_SRC_LIBSERVER_ETHRPCMETHODS_H_

#include "Server.h"
#include <jsonrpccpp/common/exception.h>
#include "common/Constants.h"
#include "libCrypto/EthCrypto.h"
#include "libEth/Eth.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libMetrics/Api.h"
#include "libUtils/GasConv.h"

class LookupServer;

typedef std::function<bool(const Transaction& tx, uint32_t shardId)>
    CreateTransactionTargetFunc;

class EthRpcMethods {
 public:
  EthRpcMethods(Mediator& mediator)
      : m_sharedMediator(mediator), m_lookupServer(nullptr) {}

  Z_I64METRIC m_apiCallCount{Z_FL::EVM_RPC, "ethrpc_invocation_count",
                             "Calls to ethereum API", "Calls"};

  std::pair<std::string, unsigned int> CheckContractTxnShards(
      bool priority, unsigned int shard, const Transaction& tx,
      unsigned int num_shards, bool toAccountExist, bool toAccountIsContract);

  CreateTransactionTargetFunc m_createTransactionTarget =
      [this](const Transaction& tx, uint32_t shardId) -> bool {
    return m_sharedMediator.m_lookup->AddToTxnShardMap(tx, shardId);
  };

  void Init(LookupServer* lookupServer);

  virtual void GetEthCallEthI(const Json::Value& request,
                              Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->GetEthCallEth(request[0u], request[1u].asString());
    LOG_GENERAL(DEBUG, "EthCall response:" << response);
  }

  virtual void DebugTraceCallI(const Json::Value& request,
                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->DebugTraceCallEth(request[0u], request[1u].asString(),
                                       request[2u]);
    LOG_GENERAL(DEBUG, "DebugTraceCall response:" << response);
  }

  // TODO: remove once we fully move to Eth compatible APIs.
  inline virtual void GetEthCallZilI(const Json::Value& request,
                                     Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthCallZil(request[0u]);
  }

  // Eth style functions here
  virtual void GetEthBlockNumberI(const Json::Value& /*request*/,
                                  Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthBlockNumber();
  }

  inline virtual void GetEthBlockByNumberI(const Json::Value& request,
                                           Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();

    // Because we bypassed the library to validate parameters, we should do it manually.
    if (request[0].empty() || !request[1].isBool()){
      throw jsonrpc::JsonRpcException(ServerBase::RPC_INVALID_PARAMS);
    }

    response =
        this->GetEthBlockByNumber(request[0u].asString(), request[1u].asBool());
  }

  inline virtual void GetEthBlockByHashI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response =
        this->GetEthBlockByHash(request[0u].asString(), request[1u].asBool());
  }

  /**
   * @brief Get the Eth Gas Price. Returns the gas price in Wei.
   * @param request none
   * @param response Hex string of the current gas price in wei
   */
  inline virtual void GetEthGasPriceI(const Json::Value& /*request*/,
                                      Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthGasPrice();
  }

  /**
   * @brief Generates and returns an estimate of how much gas is necessary to
   * allow the transaction to complete. The transaction will not be added to the
   * blockchain. Note that the estimate may be significantly more than the
   * amount of gas actually used by the transaction, for a variety of reasons
   * including EVM mechanics and node performance.
   *
   * @param request none
   * @param response Hex string with the estimated gasprice
   */
  inline virtual void GetEthEstimateGasI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    std::string block_or_tag;
    if (request.size()> 1) {
      block_or_tag = request[1u].asString();
    }

    response = this->GetEthEstimateGas(request[0u],
                                       (request.size() > 1 ? &block_or_tag : nullptr));
  }

  inline virtual void GetEthTransactionCountI(const Json::Value& request,
                                              Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    try {
      std::string address = request[0u].asString();
      DataConversion::NormalizeHexString(address);
      const auto resp = this->GetBalanceAndNonce(address)["nonce"].asUInt();
      response = DataConversion::IntToHexString(resp);
    } catch (...) {
      response = "0x0";
    }
  }

  inline virtual void GetEthTransactionReceiptI(const Json::Value& request,
                                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->GetEthTransactionReceipt(request[0u].asString());
  }

  inline virtual void GetEthSendRawTransactionI(const Json::Value& request,
                                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    auto rawTx = request[0u].asString();

    // Erase '0x' at the beginning if it exists
    if (rawTx.size() >= 2 && rawTx[1] == 'x') {
      rawTx.erase(0, 2);
    }

    auto pubKey = RecoverECDSAPubKey(rawTx, ETH_CHAINID);

    if (pubKey.empty()) {
      return;
    }

    auto fields = Eth::parseRawTxFields(rawTx);

    auto shards = m_sharedMediator.m_lookup->GetShardPeers().size();

    // For Eth transactions, pass gas Price in Wei
    const auto gasPrice = (m_sharedMediator.m_dsBlockChain.GetLastBlock()
                               .GetHeader()
                               .GetGasPrice() *
                           EVM_ZIL_SCALING_FACTOR) /
                          GasConv::GetScalingFactor();

    response = CreateTransactionEth(fields, pubKey, shards, gasPrice,
                                    m_createTransactionTarget);
  }

  inline virtual void GetEthBalanceI(const Json::Value& request,
                                     Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    auto address{request[0u].asString()};
    DataConversion::NormalizeHexString(address);

    const std::string tag{request[1u].asString()};

    response = this->GetEthBalance(address, tag);
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_getTransactionByHash
   * @param request : transaction hash
   * @param response : string with the client version
   */
  inline virtual void GetEthTransactionByHashI(const Json::Value& request,
                                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthTransactionByHash(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method: web3_clientVersion
   * @param request : Params none
   * @param response : string with the client version
   */
  inline virtual void GetWeb3ClientVersionI(const Json::Value& /*request*/,
                                            Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetWeb3ClientVersion();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: web3_sha3
   * Returns Keccak-256 (not the standardized SHA3-256) of the given data.
   * @param request : params[] with data that will be converted into sha3
   * @param response : The SHA3 result of the given data string.
   */
  inline virtual void GetWeb3Sha3I(const Json::Value& request,
                                   Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = std::string{"0x"} + this->GetWeb3Sha3(request[0u]);
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getUncleCountByBlock[Hash|Number]. Returns number of uncles.
   * @param request : params[] with hash/number of a block and uncle's index
   * position (both ignored).
   * @param response : Integer: Number of uncles
   */
  inline virtual void GetEthUncleCountI(const Json::Value& /*request*/,
                                        Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthUncleCount();
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getUncleByBlock[Hash|Number]AndIndex. Returns uncle block object.
   * @param request : params[] with hash/number of a block and uncle's index
   * position (both ignored)
   * @param response : Object - returns compound type representing Block
   */
  inline virtual void GetEthUncleBlockI(const Json::Value& /*request*/,
                                        Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthUncleBlock();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_mining.
   * Returns true if client is actively mining new blocks.
   * @param request : params none
   * @param response : Boolean - returns true of the client is mining, otherwise
   * false
   */
  inline virtual void GetEthMiningI(const Json::Value& /*request*/,
                                    Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthMining();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_coinbase.
   * Returns the client coinbase address. The coinbase address is the
   * account to pay mining rewards to.
   * @param request : params none
   * @param response : string, 20 zbytes with the current coinbase address. e.g.
   * 0x407d73d8a49eeb85d32cf465507dd71d507100c1
   */
  virtual void GetEthCoinbaseI(const Json::Value& /*request*/,
                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthCoinbase();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: net_version. Returns the
   * current network id.
   * @param request : params none
   * @param response : String - The zilliqa network id.
   */
  virtual void GetNetVersionI(const Json::Value& /*request*/,
                              Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetNetVersion();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: net_listening.
   * @param request : params none
   * @param response : Boolean - true when listening, otherwise false.
   */
  virtual void GetNetListeningI(const Json::Value& /*request*/,
                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetNetListening();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: net_peerCount.
   * Returns number of peers currently connected to the client.
   * @param request : params none
   * @param response : QUANTITY - hex string of the number of connected peers.
   */
  virtual void GetNetPeerCountI(const Json::Value& /*request*/,
                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetNetPeerCount();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_protocolVersion.
   * Returns the current Ethereum protocol version.
   * @param request : params none
   * @param response : String - The current Ethereum protocol version
   */
  virtual void GetProtocolVersionI(const Json::Value& /*request*/,
                                   Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetProtocolVersion();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_chainId.
   * Zilliqa's chainId on ethereum
   * @param request : params none
   * @param response : QUANTITY - hex string of the chain id
   */
  virtual void GetEthChainIdI(const Json::Value& /*request*/,
                              Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthChainId();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_syncing
   * Returns an object with data about the sync status or false.
   * @param request : params none
   * @param response : Object|Boolean, An object with sync status data or FALSE,
   * when not syncing:
   *
   * startingBlock: QUANTITY - The block at which the import started (will only
   * be reset, after the sync reached his head)
   *
   * currentBlock: QUANTITY - The current block, same as eth_blockNumber
   *
   * highestBlock: QUANTITY - The estimated highest block
   */
  virtual void GetEthSyncingI(const Json::Value& /*request*/,
                              Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthSyncing();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_accounts
   * Returns a list of addresses owned by client.
   * @param request : params none
   * @param response : Array of DATA, 20 Bytes - addresses owned by the client.
   */
  virtual void GetEthAccountsI(const Json::Value& /*request*/,
                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEmptyResponse();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_accounts
   * Returns a list of addresses owned by client.
   * @param request : params none
   * @param response : Array of DATA, 20 Bytes - addresses owned by the client.
   */
  virtual void GetEthStorageAtI(const Json::Value& request,
                                Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthStorageAt(
        request[0u].asString(), request[1u].asString(), request[2u].asString());
  }

  virtual void GetEthCodeI(const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthCode(request[0u].asString(), request[1u].asString());
  }

  inline virtual void GetEthFeeHistoryI(const Json::Value& /*request*/,
                                        Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEmptyResponse();
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getBlockTransactionCountByHash Returns transactions count for given
   * block.
   * @param request : params: block hash
   * @param response : number of transactions.
   */

  inline virtual void GetEthBlockTransactionCountByHashI(
      const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthBlockTransactionCountByHash(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getBlockTransactionCountByNumber Returns transactions count for given
   * block.
   * @param request : params: block hash
   * @param response : number of transactions.
   */

  inline virtual void GetEthBlockTransactionCountByNumberI(
      const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response =
        this->GetEthBlockTransactionCountByNumber(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getTransactionByBlockHashAndIndex Returns transaction for given block
   * and index
   * @param request : params: block hash and index
   * @param response : transaction object or null if not found.
   */

  inline virtual void GetEthTransactionByBlockHashAndIndexI(
      const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthTransactionByBlockHashAndIndex(
        request[0u].asString(), request[1u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getTransactionByBlockHashAndIndex Returns transaction for given block
   * and index
   * @param request : params: block number (or tag) and index
   * @param response : transaction object or null if not found.
   */

  inline virtual void GetEthTransactionByBlockNumberAndIndexI(
      const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthTransactionByBlockNumberAndIndex(
        request[0u].asString(), request[1u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_newFilter
   * @param request : params: Json object
   * @param response : Filter ID (string) on success
   */
  virtual void EthNewFilterI(const Json::Value& request,
                             Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthNewFilter(request[0u]);
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_newBlockFilter
   * @param request : params: none
   * @param response : Filter ID (string) on success
   */
  virtual void EthNewBlockFilterI(const Json::Value& /*request*/,
                                  Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthNewBlockFilter();
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_newPendingTransactionFilter
   * @param request : params: none
   * @param response : Filter ID (string) on success
   */
  virtual void EthNewPendingTransactionFilterI(const Json::Value& /*request*/,
                                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthNewPendingTransactionFilter();
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getFilterChanges
   * @param request : params: Filter ID (string)
   * @param response : Json array of filter changes since last seen state
   */
  virtual void EthGetFilterChangesI(const Json::Value& request,
                                    Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthGetFilterChanges(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_uninstallFilter
   * @param request : params: Filter ID (string)
   * @param response : boolean (if the filter was uninstalled)
   */
  virtual void EthUninstallFilterI(const Json::Value& request,
                                   Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthUninstallFilter(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getFilterLogs
   * @param request : params: Filter ID (string)
   * @param response : Json array of items applicable to the filter
   */
  virtual void EthGetFilterLogsI(const Json::Value& request,
                                 Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthGetFilterLogs(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getFilterLogs
   * @param request : params: event filter params json object
   * @param response : Json array of items applicable to the filter
   */
  virtual void EthGetLogsI(const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthGetLogs(request[0u]);
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getFilterLogs
   * @param request : params: Transaction rlp as string
   * @param response : Address of the sender of the RLP
   */
  virtual void EthRecoverTransactionI(const Json::Value& request,
                                      Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    EnsureEvmAndLookupEnabled();
    response = this->EthRecoverTransaction(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method:
   * eth_getFilterLogs
   * @param request : params: Bloc number, hash or identifier as string
   * @param response : Json array of transaction receipts from block
   */
  inline virtual void GetEthBlockReceiptsI(const Json::Value& request,
                                           Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetEthBlockReceipts(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method: GetDSLeaderTxnPool
   * @param request : no parameters
   * @param response : Json array of transactions from the DSLeader txn pool.
   */
  inline virtual void GetDSLeaderTxnPoolI(const Json::Value& /*request*/,
                                          Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetDSLeaderTxnPool();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: debug_traceTransaction
   * @param request : transaction hash
   * @param response : transaction trace
   */
  inline virtual void DebugTraceTransactionI(const Json::Value& request,
                                             Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->DebugTraceTransaction(request[0u].asString(), request[1u]);
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_enable
   * @param request : none
   * @param response : none
   */
  inline virtual void OtterscanEnableI(const Json::Value& request,
                                                      Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    ARCHIVAL_LOOKUP_WITH_TX_TRACES = request[0u].asBool();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_getInternalOperations
   * @param request : transaction hash
   * @param response : transaction internal operations
   */
  inline virtual void OtterscanGetInternalOperationsI(const Json::Value& request,
                                             Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanGetInternalOperations(request[0u].asString(), "otter_internal_tracer");
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_getInternalOperations
   * @param request : transaction hash
   * @param response : transaction internal operations
   */
  inline virtual void OtterscanGetTransactionErrorI(const Json::Value& request,
                                                      Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanGetInternalOperations(request[0u].asString(), "otter_transaction_error");
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_traceTransaction
   * @param request : transaction hash
   * @param response : transaction trace (abridged)
   */
  inline virtual void OtterscanTraceTransactionI(const Json::Value& request,
                                                      Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanGetInternalOperations(request[0u].asString(), "otter_call_tracer");
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_traceTransaction
   * @param request : transaction hash
   * @param response : transaction trace (abridged)
   */
  inline virtual void OtterscanSearchTransactionsBeforeI(const Json::Value& request,
                                                 Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanSearchTransactions(request[0u].asString(), request[1u].asInt64(), request[2u].asInt64(), true);
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_traceTransaction
   * @param request : transaction hash
   * @param response : transaction trace (abridged)
   */
  inline virtual void OtterscanSearchTransactionsAfterI(const Json::Value& request,
                                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanSearchTransactions(request[0u].asString(), request[1u].asInt64(), request[2u].asInt64(), false);
  }

  /**
   * @brief Handles json rpc 2.0 request on method: ots_traceTransaction
   * @param request : transaction hash
   * @param response : transaction trace (abridged)
   */
  inline virtual void OtterscanGetTransactionBySenderAndNonceI(const Json::Value& request,
                                                 Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->OtterscanGetTransactionBySenderAndNonce(request[0u].asString(), request[1u].asInt64());
  }

  /**
   * @brief Handles json rpc 2.0 request on method: debug_traceBlockByNumber
   * @param request : block number, trace type
   * @param response : transaction trace
   */
  inline virtual void DebugTraceBlockByNumberI(const Json::Value& request,
                                               Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response =
        this->DebugTraceBlockByNumber(request[0u].asString(), request[1u]);
  }

  inline virtual void GetHeaderByNumberI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetHeaderByNumber(request[0u].asUInt64());
  }

  inline virtual void GetOtterscanApiLevelI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = 8;
  }

  inline virtual void HasCodeI(const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->HasCode(request[0u].asString(), request[1u].asString());
  }

  inline virtual void GetBlockDetailsI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetBlockDetails(request[0u].asUInt64());
  }

  inline virtual void GetBlockTransactionsI(const Json::Value& request,
                                         Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetBlockTransactions(request[0u].asUInt64(), boost::numeric_cast<uint32_t>(request[1u].asUInt64()), boost::numeric_cast<uint32_t>(request[2u].asUInt64()));
  }

  inline virtual void GetContractCreatorI(const Json::Value& request, Json::Value& response) {
    LOG_MARKER_CONTITIONAL(LOG_SC);
    response = this->GetContractCreator(request[0u].asString());
  }

  struct ApiKeys;
  std::string GetEthCallZil(const Json::Value& _json);
  std::string GetEthCallEth(const Json::Value& _json,
                            const std::string& block_or_tag);
  std::string DebugTraceCallEth(const Json::Value& _json,
                                const std::string& block_or_tag,
                                const Json::Value& tracer);
  std::string GetEthEstimateGas(const Json::Value& _json, const std::string *block_or_tag);
  std::string GetEthCallImpl(const Json::Value& _json, const ApiKeys& apiKeys,
                             std::string const& tracer = "");
  Json::Value GetBalanceAndNonce(const std::string& address);
  std::string GetWeb3ClientVersion();
  std::string GetWeb3Sha3(const Json::Value& _json);
  Json::Value GetEthUncleCount();
  Json::Value GetEthUncleBlock();
  Json::Value GetEthMining();
  std::string GetEthCoinbase();
  Json::Value GetNetListening();
  std::string GetNetPeerCount();
  std::string GetProtocolVersion();
  std::string GetEthChainId();
  std::string GetNetVersion();
  Json::Value GetEthSyncing();
  Json::Value GetEthTransactionByHash(const std::string& hash);
  Json::Value GetEmptyResponse();
  Json::Value GetEthStorageAt(std::string const& address,
                              std::string const& position,
                              std::string const& blockNum);
  Json::Value GetEthCode(std::string const& address,
                         std::string const& blockNum);
  TxBlock GetBlockFromTransaction(
      const TransactionWithReceipt& transaction) const;
  uint64_t GetTransactionIndexFromBlock(const TxBlock& txBlock,
                                        const std::string& txnhash) const;

  // Eth calls
  Json::Value GetEthTransactionReceipt(const std::string& txnhash);
  Json::Value GetEthBlockByNumber(const std::string& blockNumberStr,
                                  const bool includeFullTransactions);
  Json::Value GetEthBlockNumber();
  Json::Value GetEthBlockByHash(const std::string& blockHash,
                                const bool includeFullTransactions);
  Json::Value GetEthBlockCommon(const TxBlock& txBlock,
                                const bool includeFullTransactions);
  Json::Value GetEthBalance(const std::string& address, const std::string& tag);

  Json::Value GetEthGasPrice() const;
  uint256_t GetEthGasPriceNum() const;

  std::string CreateTransactionEth(
      Eth::EthFields const& fields, zbytes const& pubKey,
      const unsigned int num_shards, const uint128_t& gasPriceWei,
      const CreateTransactionTargetFunc& targetFunc);

  Json::Value GetEthBlockTransactionCountByHash(const std::string& blockHash);
  Json::Value GetEthBlockTransactionCountByNumber(
      const std::string& blockNumber);

  Json::Value GetEthTransactionByBlockHashAndIndex(
      const std::string& blockHash, const std::string& index) const;
  Json::Value GetEthTransactionByBlockNumberAndIndex(
      const std::string& blockNumber, const std::string& index) const;
  Json::Value GetEthTransactionFromBlockByIndex(const TxBlock& txBlock,
                                                const uint64_t index) const;

  std::string EthNewFilter(const Json::Value& param);
  std::string EthNewBlockFilter();
  std::string EthNewPendingTransactionFilter();
  Json::Value EthGetFilterChanges(const std::string& filter_id);
  bool EthUninstallFilter(const std::string& filter_id);
  Json::Value EthGetFilterLogs(const std::string& filter_id);
  Json::Value EthGetLogs(const Json::Value& param);

  std::string EthRecoverTransaction(const std::string& txnRpc) const;

  Json::Value GetEthBlockReceipts(const std::string& blockId);
  Json::Value DebugTraceTransaction(const std::string& txHash,
                                    const Json::Value& json);
  Json::Value OtterscanGetInternalOperations(const std::string& txHash, const std::string &tracer);
  Json::Value OtterscanSearchTransactions(const std::string& address, unsigned long blockNumber, unsigned long pageSize, bool before);
  Json::Value OtterscanGetTransactionBySenderAndNonce(const std::string& address, uint64_t nonce);
  Json::Value DebugTraceBlockByNumber(const std::string& blockNum,
                                      const Json::Value& json);

  Json::Value GetHeaderByNumber(const uint64_t blockNumber);
  bool HasCode(const std::string& address, const std::string& block);
  Json::Value GetBlockDetails(const uint64_t blockNumber);
  Json::Value GetBlockTransactions(const uint64_t blockNumber, const uint32_t pageNumber, const uint32_t pageSize);
  Json::Value GetContractCreator(const std::string& address);

  Json::Value GetDSLeaderTxnPool();
  void EnsureEvmAndLookupEnabled();
  static bool UnpackRevert(const std::string &data_in, std::string &message);

 public:
  Mediator& m_sharedMediator;

 private:
  LookupServer* m_lookupServer;
};

#endif  // ZILLIQA_SRC_LIBSERVER_ETHRPCMETHODS_H_
