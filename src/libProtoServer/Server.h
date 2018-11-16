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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <mutex>
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"

#include "ServerMessages.pb.h"
#include "ServerRequest.pb.h"
#include "ServerResponse.pb.h"

class Mediator;

class Server {
  Mediator& m_mediator;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_BlockTxPair;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_TxBlockCountSumPair;
  uint64_t m_StartTimeTx;
  uint64_t m_StartTimeDs;
  std::pair<uint64_t, CircularArray<std::string>> m_DSBlockCache;
  std::pair<uint64_t, CircularArray<std::string>> m_TxBlockCache;
  static CircularArray<std::string> m_RecentTransactions;
  static std::mutex m_mutexRecentTxns;

 public:
  Server(Mediator& mediator);
  ~Server();

  // Auxillary functions.
  boost::multiprecision::uint256_t GetNumTransactions(uint64_t blockNum);
  void AddToRecentTransactions(const dev::h256& txhash);
  bool ValidateProtoTransaction(
      const ZilliqaMessage::ProtoTransaction& protoTransaction);

  // Interfaces returning default values.
  ZilliqaMessage::DefaultResponse GetClientVersion();
  ZilliqaMessage::DefaultResponse GetNetworkId();
  ZilliqaMessage::DefaultResponse GetProtocolVersion();
  ZilliqaMessage::DefaultResponse GetGasPrice();
  ZilliqaMessage::DefaultResponse GetStorageAt(
      ZilliqaMessage::GetStorageAtRequest& request);
  ZilliqaMessage::DefaultResponse GetBlockTransactionCount(
      ZilliqaMessage::GetBlockTransactionCountRequest& request);
  ZilliqaMessage::DefaultResponse CreateMessage();
  ZilliqaMessage::DefaultResponse GetGasEstimate();
  ZilliqaMessage::DefaultResponse GetTransactionReceipt(
      ZilliqaMessage::GetTransactionRequest& request);
  ZilliqaMessage::DefaultResponse isNodeSyncing();
  ZilliqaMessage::DefaultResponse isNodeMining();
  ZilliqaMessage::DefaultResponse GetHashrate();

  ZilliqaMessage::CreateTransactionResponse CreateTransaction(
      ZilliqaMessage::CreateTransactionRequest& request);

  ZilliqaMessage::GetTransactionResponse GetTransaction(
      ZilliqaMessage::GetTransactionRequest& request);

  ZilliqaMessage::GetDSBlockResponse GetDsBlock(
      ZilliqaMessage::ProtoBlockNum& protoBlockNum);

  ZilliqaMessage::GetTxBlockResponse GetTxBlock(
      ZilliqaMessage::ProtoBlockNum& protoBlockNum);

  ZilliqaMessage::GetDSBlockResponse GetLatestDsBlock();

  ZilliqaMessage::GetTxBlockResponse GetLatestTxBlock();

  ZilliqaMessage::GetBalanceResponse GetBalance(
      ZilliqaMessage::ProtoAddress& protoAddress);

  ZilliqaMessage::GetSmartContractStateResponse GetSmartContractState(
      ZilliqaMessage::ProtoAddress& protoAddress);

  ZilliqaMessage::GetSmartContractInitResponse GetSmartContractInit(
      ZilliqaMessage::ProtoAddress& protoAddress);

  ZilliqaMessage::GetSmartContractCodeResponse GetSmartContractCode(
      ZilliqaMessage::ProtoAddress& protoAddress);

  ZilliqaMessage::GetSmartContractResponse GetSmartContracts(
      ZilliqaMessage::ProtoAddress& protoAddress);

  ZilliqaMessage::StringResponse GetContractAddressFromTransactionID(
      ZilliqaMessage::ProtoTxId& protoTxId);

  ZilliqaMessage::UIntResponse GetNumPeers();

  ZilliqaMessage::StringResponse GetNumTxBlocks();

  ZilliqaMessage::StringResponse GetNumDSBlocks();

  ZilliqaMessage::StringResponse GetNumTransactions();

  ZilliqaMessage::DoubleResponse GetTransactionRate();

  ZilliqaMessage::DoubleResponse GetDSBlockRate();

  ZilliqaMessage::DoubleResponse GetTxBlockRate();

  ZilliqaMessage::UInt64Response GetCurrentMiniEpoch();

  ZilliqaMessage::UInt64Response GetCurrentDSEpoch();

  ZilliqaMessage::ProtoBlockListing DSBlockListing(
      ZilliqaMessage::ProtoPage& protoPage);

  ZilliqaMessage::ProtoBlockListing TxBlockListing(
      ZilliqaMessage::ProtoPage& protoPage);

  ZilliqaMessage::ProtoBlockChainInfo GetBlockchainInfo();

  ZilliqaMessage::ProtoTxHashes GetRecentTransactions();

  ZilliqaMessage::ProtoShardingStruct GetShardingStructure();

  ZilliqaMessage::UIntResponse GetNumTxnsTxEpoch();

  ZilliqaMessage::StringResponse GetNumTxnsDSEpoch();
};
