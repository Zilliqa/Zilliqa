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

#include <mutex>

#include "common/BaseType.h"
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"

#include "ServerMessages.pb.h"
#include "ServerRequest.pb.h"
#include "ServerResponse.pb.h"

class Mediator;

class Server {
  Mediator& m_mediator;
  std::pair<uint64_t, uint256_t> m_BlockTxPair;
  std::pair<uint64_t, uint256_t> m_TxBlockCountSumPair;
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
  uint256_t GetNumTransactions(uint64_t blockNum);
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
