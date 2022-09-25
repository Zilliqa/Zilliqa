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

#include "Server.h"
#include "common/Constants.h"
#include "libCrypto/EthCrypto.h"
#include "libEth/Eth.h"
#include "libUtils/GasConv.h"
#include "libUtils/Logger.h"

class Mediator;

typedef std::function<bool(const Transaction& tx, uint32_t shardId)>
    CreateTransactionTargetFunc;

class LookupServer : public Server,
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

  CreateTransactionTargetFunc m_createTransactionTarget =
      [this](const Transaction& tx, uint32_t shardId) -> bool {
    return m_mediator.m_lookup->AddToTxnShardMap(tx, shardId);
  };

  Json::Value GetTransactionsForTxBlock(const std::string& txBlockNum,
                                        const std::string& pageNumber);

  std::pair<std::string, unsigned int> CheckContractTxnShards(
      bool priority, unsigned int shard, const Transaction& tx,
      unsigned int num_shards, bool toAccountExist, bool toAccountIsContract);

 public:
  LookupServer(Mediator& mediator, jsonrpc::AbstractServerConnector& server);
  ~LookupServer() = default;

  inline virtual void GetNetworkIdI(const Json::Value& request,
                                    Json::Value& response) {
    (void)request;
    response = this->GetNetworkId();
  }

  inline virtual void CreateTransactionI(const Json::Value& request,
                                         Json::Value& response) {
    response = CreateTransaction(
        request[0u], m_mediator.m_lookup->GetShardPeers().size(),
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice(),
        m_createTransactionTarget);
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

  inline virtual void GetEthCallEthI(const Json::Value& request,
                                     Json::Value& response) {
    response = this->GetEthCallEth(request[0u], request[1u].asString());
  }

  // TODO: remove once we fully move to Eth compatible APIs.
  inline virtual void GetEthCallZilI(const Json::Value& request,
                                     Json::Value& response) {
    response = this->GetEthCallZil(request[0u]);
  }

  // Eth style functions here
  inline virtual void GetEthBlockNumberI(const Json::Value& /*request*/,
                                         Json::Value& response) {
    response = this->GetEthBlockNumber();
  }

  inline virtual void GetEthBlockByNumberI(const Json::Value& request,
                                           Json::Value& response) {
    response =
        this->GetEthBlockByNumber(request[0u].asString(), request[1u].asBool());
  }

  inline virtual void GetEthBlockByHashI(const Json::Value& request,
                                         Json::Value& response) {
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
  inline virtual void GetEthEstimateGasI(const Json::Value& /*request*/,
                                         Json::Value& response) {
    // TODO: implement eth_estimateGas for real.
    // At the moment, the default value of 300,000 gas will allow to proceed
    // with the internal/external testnet testing before it is implemented.
    response = "0x493e0";
  }

  inline virtual void GetEthTransactionCountI(const Json::Value& request,
                                              Json::Value& response) {
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
    response = this->GetEthTransactionReceipt(request[0u].asString());
  }

  inline virtual void GetEthSendRawTransactionI(const Json::Value& request,
                                                Json::Value& response) {
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

    auto shards = m_mediator.m_lookup->GetShardPeers().size();

    // For Eth transactions, pass gas Price in Wei
    const auto gasPrice =
        (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice() *
         EVM_ZIL_SCALING_FACTOR) /
        GasConv::GetScalingFactor();

    response = CreateTransactionEth(fields, pubKey, shards, gasPrice,
                                    m_createTransactionTarget);
  }

  inline virtual void GetEthBalanceI(const Json::Value& request,
                                     Json::Value& response) {
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
    response = this->GetEthTransactionByHash(request[0u].asString());
  }

  /**
   * @brief Handles json rpc 2.0 request on method: web3_clientVersion
   * @param request : Params none
   * @param response : string with the client version
   */
  inline virtual void GetWeb3ClientVersionI(const Json::Value& /*request*/,
                                            Json::Value& response) {
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
    std::cout << "UjRGH" << std::endl;
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
    response = this->GetEthMining();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: eth_coinbase.
   * Returns the client coinbase address. The coinbase address is the
   * account to pay mining rewards to.
   * @param request : params none
   * @param response : string, 20 bytes with the current coinbase address. e.g.
   * 0x407d73d8a49eeb85d32cf465507dd71d507100c1
   */
  virtual void GetEthCoinbaseI(const Json::Value& /*request*/,
                               Json::Value& response) {
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
    response = this->GetEthChainId();
  }

  /**
   * @brief Handles json rpc 2.0 request on method: net_listening.
   * @param request : params none
   * @param response : Boolean - true when listening, otherwise false.
   */
  virtual void GetNetListeningI(const Json::Value& /*request*/,
                                Json::Value& response) {
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
    response = this->GetEthStorageAt(
        request[0u].asString(), request[1u].asString(), request[2u].asString());
  }

  virtual void GetEthCodeI(const Json::Value& request, Json::Value& response) {
    response = this->GetEthCode(request[0u].asString(), request[1u].asString());
  }

  inline virtual void GetEthFeeHistoryI(const Json::Value& request,
                                        Json::Value& response) {
    response = this->GetEthFeeHistory(
        request[0u].asString(), request[1u].asString(), request[1u].asString());
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
    response = this->GetEthTransactionByBlockNumberAndIndex(
        request[0u].asString(), request[1u].asString());
  }

  virtual void EthNewFilterI(const Json::Value& request,
                             Json::Value& response) {
    response = this->EthNewFilter(request[0u]);
  }

  virtual void EthNewBlockFilterI(const Json::Value& /*request*/,
                                  Json::Value& response) {
    response = this->EthNewBlockFilter();
  }

  virtual void EthNewPendingTransactionFilterI(const Json::Value& /*request*/,
                                               Json::Value& response) {
    response = this->EthNewPendingTransactionFilter();
  }

  virtual void EthGetFilterChangesI(const Json::Value& request,
                                    Json::Value& response) {
    response = this->EthGetFilterChanges(request[0u].asString());
  }

  virtual void EthUninstallFilterI(const Json::Value& request,
                                   Json::Value& response) {
    response = this->EthUninstallFilter(request[0u].asString());
  }

  std::string GetNetworkId();

  Json::Value CreateTransaction(const Json::Value& _json,
                                const unsigned int num_shards,
                                const uint128_t& gasPrice,
                                const CreateTransactionTargetFunc& targetFunc);
  Json::Value GetStateProof(const std::string& address,
                            const Json::Value& request,
                            const uint64_t& blockNum);
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
  Json::Value GetCurrentDSComm();
  Json::Value GetShardMembers(unsigned int shardID);
  Json::Value DSBlockListing(unsigned int page);
  Json::Value TxBlockListing(unsigned int page);
  Json::Value GetBlockchainInfo();
  struct ApiKeys;
  std::string GetEthCallZil(const Json::Value& _json);
  std::string GetEthCallEth(const Json::Value& _json,
                            const std::string& block_or_tag);
  std::string GetEthCallImpl(const Json::Value& _json, const ApiKeys& apiKeys);
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
  Json::Value GetEthSyncing();
  Json::Value GetEthTransactionByHash(const std::string& hash);
  Json::Value GetEmptyResponse();
  Json::Value GetEthStorageAt(std::string const& address,
                              std::string const& position,
                              std::string const& blockNum);
  Json::Value GetEthCode(std::string const& address,
                         std::string const& blockNum);
  Json::Value GetEthFeeHistory(std::string const& range,
                               std::string const& startBlock,
                               std::string const& rewardPercentiles);

  static Json::Value GetRecentTransactions();
  Json::Value GetShardingStructure();
  std::string GetNumTxnsDSEpoch();
  std::string GetNumTxnsTxEpoch();

  TxBlock GetBlockFromTransaction(
      const TransactionWithReceipt& transaction) const;
  uint64_t GetTransactionIndexFromBlock(const TxBlock& txBlock,
                                        const std::string& txnhash) const;

  // Eth calls
  Json::Value GetEthTransactionReceipt(const std::string& txnhash);
  Json::Value GetEthBlockByNumber(const std::string& blockNumberStr,
                                  bool includeFullTransactions);
  Json::Value GetEthBlockNumber();
  Json::Value GetEthBlockByHash(const std::string& blockHash,
                                bool includeFullTransactions);
  Json::Value GetEthBlockCommon(const TxBlock& txBlock,
                                bool includeFullTransactions);
  Json::Value GetEthBalance(const std::string& address, const std::string& tag);

  Json::Value GetEthGasPrice() const;

  std::string CreateTransactionEth(
      Eth::EthFields const& fields, bytes const& pubKey,
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
};

#endif  // ZILLIQA_SRC_LIBSERVER_LOOKUPSERVER_H_
