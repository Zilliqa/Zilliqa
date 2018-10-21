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

#include <boost/multiprecision/cpp_int.hpp>
#include <mutex>
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"

#include "ServerRequest.pb.h"
#include "ServerResponse.pb.h"
#include "ServerMessages.pb.h"

using namespace ZilliqaMessage;

class Mediator;

class Server {
  Mediator& m_mediator;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_BlockTxPair;
  std::pair<uint64_t, boost::multiprecision::uint256_t> m_TxBlockCountSumPair;
  boost::multiprecision::uint256_t m_StartTimeTx;
  boost::multiprecision::uint256_t m_StartTimeDs;
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

  // Interfaces returning default values.
  DefaultResponse GetClientVersion();
  DefaultResponse GetNetworkId();
  DefaultResponse GetProtocolVersion();
  DefaultResponse GetGasPrice();
  DefaultResponse GetStorageAt(GetStorageAtRequest &request);
  DefaultResponse GetBlockTransactionCount(GetBlockTransactionCountRequest& request);
  DefaultResponse CreateMessage();
  DefaultResponse GetGasEstimate();
  DefaultResponse GetTransactionReceipt(GetTransactionRequest& request);
  DefaultResponse isNodeSyncing();
  DefaultResponse isNodeMining();
  DefaultResponse GetHashrate();

  CreateTransactionResponse CreateTransaction(CreateTransactionRequest& request);

  GetTransactionResponse GetTransaction(GetTransactionRequest& request);

  GetDSBlockResponse GetDsBlock(ProtoBlockNum& protoBlockNum);

  GetTxBlockResponse GetTxBlock(ProtoBlockNum& protoBlockNum);

  GetDSBlockResponse GetLatestDsBlock();

  GetTxBlockResponse GetLatestTxBlock();

  GetBalanceResponse GetBalance(ProtoAddress& protoAddress);

  GetSmartContractStateResponse GetSmartContractState(ProtoAddress& protoAddress);

  GetSmartContractInitResponse GetSmartContractInit(ProtoAddress& protoAddress);

  GetSmartContractCodeResponse GetSmartContractCode(ProtoAddress& protoAddress);

  GetSmartContractResponse GetSmartContracts(ProtoAddress& protoAddress);

  StringResponse GetContractAddressFromTransactionID(ProtoTxId& protoTxId);

  UIntResponse GetNumPeers();

  StringResponse GetNumTxBlocks();

  StringResponse GetNumDSBlocks();

  StringResponse GetNumTransactions();

  DoubleResponse GetTransactionRate();

  DoubleResponse GetDSBlockRate();

  DoubleResponse GetTxBlockRate();

  StringResponse GetCurrentMiniEpoch();

  StringResponse GetCurrentDSEpoch();

  ProtoBlockListing DSBlockListing(ProtoPage& protoPage);

  ProtoBlockListing TxBlockListing(ProtoPage& protoPage);

  ProtoBlockChainInfo GetBlockchainInfo();

  ProtoTxHashes GetRecentTransactions();

  ProtoShardingStruct GetShardingStructure();

  UIntResponse GetNumTxnsTxEpoch();

  StringResponse GetNumTxnsDSEpoch();

};

