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
    response = this->GetTxBlock(request[0u].asString());
  }

  inline virtual void GetTxBlockVerboseI(const Json::Value& request,
                                         Json::Value& response) {
    response = this->GetTxBlock(request[0u].asString(), true);
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
    response = this->GetBalance(request[0u].asString());
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

  inline virtual void GetEthCallI(const Json::Value& request,
                                  Json::Value& response) {
    response = this->GetEthCall(request[0u]);
  }

  // Eth style functions here
  inline virtual void GetEthBlockNumberI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    static uint64_t block_number = 2675001;
    block_number++;

    std::stringstream stream;
    stream << "0x" << std::hex << block_number;

    response = stream.str();
  }

  inline virtual void GetEthBlockByNumberI(const Json::Value& request,
                                           Json::Value& response) {
    response =
        this->GetEthBlockByNumber(request[0u].asString(), request[1u].asBool());
  }

  inline virtual void GetEthGasPriceI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = "0xd9e63a68c";
  }

  inline virtual void GetCodeI(const Json::Value& request,
                               Json::Value& response) {
    (void)request;
    response = "0x";
  }

  inline virtual void GetEthEstimateGasI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = "0x5208";
  }

  inline virtual void GetEthTransactionCountI(const Json::Value& request,
                                              Json::Value& response) {
    (void)request;

    std::string address = request[0u].asString();
    DataConversion::NormalizeHexString(address);
    int resp = 0;

    resp = this->GetBalance(address)["nonce"].asUInt() + 1;

    response = DataConversion::IntToHexString(resp);
  }

  inline virtual void GetTransactionReceiptI(const Json::Value& request,
                                             Json::Value& response) {
    (void)request;

    response = this->GetTransactionReceipt(request[0u].asString());
  }

  inline virtual void GetEthSendRawTransactionI(const Json::Value& request,
                                                Json::Value& response) {
    (void)request;
    auto rawTx = request[0u].asString();

    // Erase '0x' at the beginning if it exists
    if (rawTx[1] == 'x') {
      rawTx.erase(0, 2);
    }

    auto pubKey = RecoverECDSAPubSig(rawTx, stoi(ETH_CHAINID));

    if (pubKey.empty()) {
      return;
    }

    auto fields = parseRawTxFields(rawTx);

    auto shards = m_mediator.m_lookup->GetShardPeers().size();
    auto currentGasPrice =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice();

    auto resp = CreateTransactionEth(fields, pubKey, shards, currentGasPrice,
                                     m_createTransactionTarget);

    response = resp["TranID"];
  }

  inline virtual void GetEthBalanceI(const Json::Value& request,
                                     Json::Value& response) {
    (void)request;
    std::string address = request[0u].asString();
    DataConversion::NormalizeHexString(address);

    auto resp = this->GetBalance(address, true)["balance"];

    auto balanceStr = resp.asString();

    resp = balanceStr;

    response = resp;
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
    response = this->GetWeb3Sha3(request[0u]);
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
   * chain_id of zilliqa.
   * @param request : params none
   * @param response : string with the zilliqa chain_id
   */
  virtual void GetNetVersionI(const Json::Value& /*request*/,
                              Json::Value& response) {
    response = this->GetNetVersion();
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
    response = this->GetEthAccounts();
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
  Json::Value GetTxBlock(const std::string& blockNum, bool verbose = false);
  Json::Value GetLatestDsBlock();
  Json::Value GetLatestTxBlock();
  Json::Value GetBalance(const std::string& address);
  Json::Value GetBalance(const std::string& address, bool noThrow);
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
  std::string GetEthCall(const Json::Value& _json);
  std::string GetWeb3ClientVersion();
  std::string GetWeb3Sha3(const Json::Value& _json);
  Json::Value GetEthUncleCount();
  Json::Value GetEthUncleBlock();
  Json::Value GetEthMining();
  std::string GetEthCoinbase();
  std::string GetNetVersion();
  Json::Value GetNetListening();
  std::string GetNetPeerCount();
  std::string GetProtocolVersion();
  std::string GetEthChainId();
  Json::Value GetEthSyncing();
  Json::Value GetEthAccounts();

  static Json::Value GetRecentTransactions();
  Json::Value GetShardingStructure();
  std::string GetNumTxnsDSEpoch();
  std::string GetNumTxnsTxEpoch();

  // Eth calls
  Json::Value GetTransactionReceipt(const std::string& txnhash);
  Json::Value GetEthBlockByNumber(const std::string& blockNumberStr,
                                  bool includeTransactionHashes);
  Json::Value CreateTransactionEth(
      EthFields const& fields, bytes const& pubKey,
      const unsigned int num_shards, const uint128_t& gasPrice,
      const CreateTransactionTargetFunc& targetFunc);

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
