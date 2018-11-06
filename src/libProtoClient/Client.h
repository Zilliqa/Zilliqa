/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <jsonrpccpp/server.h>
#include <string>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>

#include "Server.grpc.pb.h"

class ProtoClient {
 private:
  std::unique_ptr<ZilliqaMessage::Server::Stub> stub_;

 public:
  ProtoClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(ZilliqaMessage::Server::NewStub(channel)) {}

  std::string GetClientVersion();
  std::string GetNetworkId();
  std::string GetProtocolVersion();
  Json::Value CreateTransaction(const Json::Value& _json);
  Json::Value GetTransaction(const std::string& transactionHash);
  Json::Value GetDsBlock(const std::string& blockNum);
  Json::Value GetTxBlock(const std::string& blockNum);
  Json::Value GetLatestDsBlock();
  Json::Value GetLatestTxBlock();
  Json::Value GetBalance(const std::string& address);
  std::string GetGasPrice();
  std::string GetStorageAt(const std::string& address,
                           const std::string& position);
  Json::Value GetSmartContracts(const std::string& address);
  std::string GetBlockTransactionCount(const std::string& blockHash);
  std::string GetContractAddressFromTransactionID(const std::string& tranID);
  std::string CreateMessage(const Json::Value& _json);
  std::string GetGasEstimate(const Json::Value& _json);
  Json::Value GetTransactionReceipt(const std::string& transactionHash);
  bool isNodeSyncing();
  bool isNodeMining();
  std::string GetHashrate();
  unsigned int GetNumPeers();
  std::string GetNumTxBlocks();
  std::string GetNumDSBlocks();
  std::string GetNumTransactions();
  double GetTransactionRate();
  double GetTxBlockRate();
  double GetDSBlockRate();
  std::string GetCurrentMiniEpoch();
  std::string GetCurrentDSEpoch();
  Json::Value DSBlockListing(unsigned int page);
  Json::Value TxBlockListing(unsigned int page);
  Json::Value GetBlockchainInfo();
  Json::Value GetRecentTransactions();
  Json::Value GetShardingStructure();
  std::string GetNumTxnsDSEpoch();
  uint32_t GetNumTxnsTxEpoch();

  Json::Value GetSmartContractState(const std::string& address);
  Json::Value GetSmartContractInit(const std::string& address);
  Json::Value GetSmartContractCode(const std::string& address);
};