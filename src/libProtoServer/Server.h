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

#include <grpc/grpc.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <mutex>

#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"

#include "Server.grpc.pb.h"

class Mediator;

class ProtoServer final : public ZilliqaMessage::Server::Service {
  Mediator& m_mediator;
  const unsigned int m_serverPort;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_BlockTxPair;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_TxBlockCountSumPair;
  boost::multiprecision::uint256_t m_StartTimeTx;
  boost::multiprecision::uint256_t m_StartTimeDs;
  std::pair<uint64_t, CircularArray<std::string>> m_DSBlockCache;
  std::pair<uint64_t, CircularArray<std::string>> m_TxBlockCache;
  CircularArray<std::string> m_RecentTransactions;
  std::mutex m_mutexRecentTxns;

  // Auxillary functions.
  boost::multiprecision::uint256_t GetNumTransactions(uint64_t blockNum);
  void AddToRecentTransactions(const dev::h256& txhash);

 public:
  explicit ProtoServer(Mediator& mediator, const unsigned int serverPort);

  void StartServer();
  void StopServer();

  // Interfaces returning default values.
  grpc::Status GetClientVersion(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetNetworkId(grpc::ServerContext* context,
                            const ZilliqaMessage::Empty* request,
                            ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetProtocolVersion(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetGasPrice(grpc::ServerContext* context,
                           const ZilliqaMessage::Empty* request,
                           ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetStorageAt(grpc::ServerContext* context,
                            const ZilliqaMessage::GetStorageAtRequest* request,
                            ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetBlockTransactionCount(
      grpc::ServerContext* context,
      const ZilliqaMessage::GetBlockTransactionCountRequest* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status CreateMessage(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetGasEstimate(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetTransactionReceipt(
      grpc::ServerContext* context,
      const ZilliqaMessage::GetTransactionRequest* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status isNodeSyncing(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status isNodeMining(grpc::ServerContext* context,
                            const ZilliqaMessage::Empty* request,
                            ZilliqaMessage::DefaultResponse* response) override;
  grpc::Status GetHashrate(grpc::ServerContext* context,
                           const ZilliqaMessage::Empty* request,
                           ZilliqaMessage::DefaultResponse* response) override;

  grpc::Status CreateTransaction(
      grpc::ServerContext* context,
      const ZilliqaMessage::CreateTransactionRequest* request,
      ZilliqaMessage::CreateTransactionResponse* ret) override;

  grpc::Status GetTransaction(
      grpc::ServerContext* context,
      const ZilliqaMessage::GetTransactionRequest* request,
      ZilliqaMessage::GetTransactionResponse* ret) override;

  grpc::Status GetDSBlock(grpc::ServerContext* context,
                          const ZilliqaMessage::ProtoBlockNum* protoBlockNum,
                          ZilliqaMessage::GetDSBlockResponse* ret) override;

  grpc::Status GetTxBlock(grpc::ServerContext* context,
                          const ZilliqaMessage::ProtoBlockNum* protoBlockNum,
                          ZilliqaMessage::GetTxBlockResponse* ret) override;

  grpc::Status GetLatestDsBlock(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::GetDSBlockResponse* ret) override;

  grpc::Status GetLatestTxBlock(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::GetTxBlockResponse* ret) override;

  grpc::Status GetBalance(grpc::ServerContext* context,
                          const ZilliqaMessage::ProtoAddress* protoAddress,
                          ZilliqaMessage::GetBalanceResponse* ret) override;

  grpc::Status GetSmartContractState(
      grpc::ServerContext* context,
      const ZilliqaMessage::ProtoAddress* protoAddress,
      ZilliqaMessage::GetSmartContractStateResponse* ret) override;

  grpc::Status GetSmartContractInit(
      grpc::ServerContext* context,
      const ZilliqaMessage::ProtoAddress* protoAddress,
      ZilliqaMessage::GetSmartContractInitResponse* ret) override;

  grpc::Status GetSmartContractCode(
      grpc::ServerContext* context,
      const ZilliqaMessage::ProtoAddress* protoAddress,
      ZilliqaMessage::GetSmartContractCodeResponse* ret) override;

  grpc::Status GetSmartContracts(
      grpc::ServerContext* context,
      const ZilliqaMessage::ProtoAddress* protoAddress,
      ZilliqaMessage::GetSmartContractResponse* ret) override;

  grpc::Status GetContractAddressFromTransactionID(
      grpc::ServerContext* context, const ZilliqaMessage::ProtoTxId* protoTxId,
      ZilliqaMessage::StringResponse* ret) override;

  grpc::Status GetNumPeers(grpc::ServerContext* context,
                           const ZilliqaMessage::Empty* request,
                           ZilliqaMessage::UIntResponse* ret) override;

  grpc::Status GetNumTxBlocks(grpc::ServerContext* context,
                              const ZilliqaMessage::Empty* request,
                              ZilliqaMessage::StringResponse* ret) override;

  grpc::Status GetNumDSBlocks(grpc::ServerContext* context,
                              const ZilliqaMessage::Empty* request,
                              ZilliqaMessage::StringResponse* ret) override;

  grpc::Status GetNumTransactions(grpc::ServerContext* context,
                                  const ZilliqaMessage::Empty* request,
                                  ZilliqaMessage::StringResponse* ret) override;

  grpc::Status GetTransactionRate(grpc::ServerContext* context,
                                  const ZilliqaMessage::Empty* request,
                                  ZilliqaMessage::DoubleResponse* ret) override;

  grpc::Status GetDSBlockRate(grpc::ServerContext* context,
                              const ZilliqaMessage::Empty* request,
                              ZilliqaMessage::DoubleResponse* ret) override;

  grpc::Status GetTxBlockRate(grpc::ServerContext* context,
                              const ZilliqaMessage::Empty* request,
                              ZilliqaMessage::DoubleResponse* ret) override;

  grpc::Status GetCurrentMiniEpoch(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::UInt64Response* ret) override;

  grpc::Status GetCurrentDSEpoch(grpc::ServerContext* context,
                                 const ZilliqaMessage::Empty* request,
                                 ZilliqaMessage::UInt64Response* ret) override;

  grpc::Status DSBlockListing(grpc::ServerContext* context,
                              const ZilliqaMessage::ProtoPage* protoPage,
                              ZilliqaMessage::ProtoBlockListing* ret) override;

  grpc::Status TxBlockListing(grpc::ServerContext* context,
                              const ZilliqaMessage::ProtoPage* protoPage,
                              ZilliqaMessage::ProtoBlockListing* ret) override;

  grpc::Status GetBlockchainInfo(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::ProtoBlockChainInfo* ret) override;

  grpc::Status GetRecentTransactions(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::ProtoTxHashes* ret) override;

  grpc::Status GetShardingStructure(
      grpc::ServerContext* context, const ZilliqaMessage::Empty* request,
      ZilliqaMessage::ProtoShardingStruct* ret) override;

  grpc::Status GetNumTxnsTxEpoch(grpc::ServerContext* context,
                                 const ZilliqaMessage::Empty* request,
                                 ZilliqaMessage::UIntResponse* ret) override;

  grpc::Status GetNumTxnsDSEpoch(grpc::ServerContext* context,
                                 const ZilliqaMessage::Empty* request,
                                 ZilliqaMessage::StringResponse* ret) override;
};