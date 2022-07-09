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
#include "libUtils/Logger.h"
#include "Server.h"
#include "depends/common/RLP.h"
#include "libCrypto/EthCrypto.h"

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
  inline virtual void GetChainIdI(const Json::Value&, Json::Value& response) {
    //(void)request;
    std::cout << "REQ" << std::endl;
    response = "0x666";  // 1638 decimal - mainnet is reserved for chainId 1
  }

  inline virtual void GetBlocknumEthI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    // response = this->GetBlocknum();
    static uint64_t block_number = 2675001;
    block_number++;

    std::stringstream stream;
    stream << "0x" << std::hex << block_number;
    std::string result(stream.str());
    std::cout << stream.str() << std::endl;

    response = stream.str();
  }

  //"result": {
  //  "difficulty": "0x3ff800000",
  //      "extraData": "0x476574682f76312e302e302f6c696e75782f676f312e342e32",
  //      "gasLimit": "0x1388",
  //      "gasUsed": "0x0",
  //      "hash":
  //      "0x88e96d4537bea4d9c05d12549907b32561d3bf31f45aae734cdc119f13406cb6",
  //      "logsBloom":
  //      "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
  //      "miner": "0x05a56e2d52c817161883f50c441c3228cfe54d9f",
  //      "mixHash":
  //      "0x969b900de27b6ac6a67742365dd65f55a0526c41fd18e1b16f1a1215c2e66f59",
  //      "nonce": "0x539bd4979fef1ec4",
  //      "number": "0x1",
  //      "parentHash":
  //      "0xd4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3",
  //      "receiptsRoot":
  //      "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
  //      "sha3Uncles":
  //      "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
  //      "size": "0x219",
  //      "stateRoot":
  //      "0xd67e4d450343046425ae4271474353857ab860dbc0a1dde64b41b5cd3a532bf3",
  //      "timestamp": "0x55ba4224",
  //      "totalDifficulty": "0x7ff800000",
  //      "transactions": [],
  //      "transactionsRoot":
  //      "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
  //      "uncles": []
  //},

  inline virtual void GetBlockByNumber(const Json::Value& request,
                                       Json::Value& response) {
    (void)request;
    std::cout << "GBBN " << request[0u].asString() << std::endl;
    std::cout << "GBBN2 " << request[1u].asString() << std::endl;

    Json::Value ret;

    ret["difficulty"] = "0x3ff800000";
    ret["extraData"] = "0x476574682f76312e302e302f6c696e75782f676f312e342e32";
    ret["gasLimit"] = "0x1388";
    ret["gasUsed"] = "0x0";
    ret["hash"] =
        "0x88e96d4537bea4d9c05d12549907b32561d3bf31f45aae734cdc119f13406cb6";
    ret["logsBloom"] =
        "0x00000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000";
    ret["miner"] = "0x05a56e2d52c817161883f50c441c3228cfe54d9f";
    ret["mixHash"] =
        "0x969b900de27b6ac6a67742365dd65f55a0526c41fd18e1b16f1a1215c2e66f59";
    ret["nonce"] = "0x539bd4979fef1ec4";
    ret["number"] = "0x1";
    ret["parentHash"] =
        "0xd4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3";
    ret["receiptsRoot"] =
        "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
    ret["sha3Uncles"] =
        "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347";
    ret["size"] = "0x219";
    ret["stateRoot"] =
        "0xd67e4d450343046425ae4271474353857ab860dbc0a1dde64b41b5cd3a532bf3";
    ret["timestamp"] = "0x55ba4224";
    ret["totalDifficulty"] = "0x7ff800000";
    ret["transactions"] = Json::arrayValue;
    ret["transactionsRoot"] =
        "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
    ret["uncles"] = Json::arrayValue;

    response = ret;
  }

  inline virtual void GetNetVersionI(const Json::Value& request,
                                     Json::Value& response) {
    (void)request;
    response = "0x666";  // 1638 decimal - mainnet is reserved for chainId 1
  }

  inline virtual void GetGasPriceI(const Json::Value& request,
                                     Json::Value& response) {
    (void)request;
    response = "0xd9e63a68c";
  }

  inline virtual void GetCodeI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = "0x";
  }

  inline virtual void EstimateGasI(const Json::Value& request,
                               Json::Value& response) {
    (void)request;
    response = "0x5208";
  }

  inline virtual void GetTransactionCountI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = "0x0";
  }

  inline virtual void SendRawTransactionI(const Json::Value& request,
                                           Json::Value& response) {
    (void)request;
    std::cout << "Got raw TX!!!" << std::endl;
    auto aa = request[0u].asString();

    // Erase '0x' at the beginning if it exists
    if (aa[1] == 'x') {
      aa.erase(0, 2);
    }

    std::cout << aa << std::endl;

    bytes out;
    DataConversion::HexStrToUint8Vec(aa, out);

    //std::string twoItemListString = "\xc5\x0f\x83""dog";
    dev::RLP rlpStream1(out);
    dev::RLPStream rlpStream2(9);
    dev::RLPStream rlpStream3(9);

    std::cout << "Parsed rlp stream is: " << rlpStream1  << std::endl;

    std::vector<std::string> fieldsHex{};
    std::vector<bytes> fieldsHexBytes{};
    bytes message;
    bytes rsv;
    int i = 0;

    for (const auto& item : rlpStream1) {
      std::cout << "parsing"  << std::endl;
      auto zz = item.operator bytes();


      if (i <= 5) {
        message.insert(message.end(), zz.begin(), zz.end());
        rlpStream3 << zz;
      } else {
        rsv.insert(rsv.end(), zz.begin(), zz.end());
      }

      if (i == 6) {
        //rlpStream3 << DataConversion::HexStrToUint8VecRet("01");
        rlpStream3 << zz;
      }

      if (i == 7 || i == 8) {
        rlpStream3 << bytes{};
      }

      i++;

      fieldsHexBytes.push_back(zz);
      std::string conv;
      DataConversion::Uint8VecToHexStr(zz, conv);
      std::cout << conv << std::endl;
      //std::cout << item.toString()  << std::endl;
      fieldsHex.push_back(conv);
    }

    // Create a TX from what we have got here
    if (fieldsHex.size() < 9) {
      // todo(NH): return a proper error code to the caller
      LOG_GENERAL(WARNING, "Badly formatted raw transaction");
      response = "0x0";
      return;
    }

    //auto rsv = fieldsHexBytes[6] + fieldsHexBytes[7] + fieldsHexBytes[8];
    //auto message = fieldsHexBytes[6] + fieldsHexBytes[7] + fieldsHexBytes[8];

    // Test the message signature thingie
    // From EIP-155 Consider a transaction with nonce = 9,
    // gasprice = 20 * 10**9, startgas = 21000,
    // to = 0x3535353535353535353535353535353535353535,
    // value = 10**18, data='' (empty).

    rlpStream2 << DataConversion::HexStrToUint8VecRet("09")
               << DataConversion::HexStrToUint8VecRet("04A817C800")
               << DataConversion::HexStrToUint8VecRet("5208")
               << DataConversion::HexStrToUint8VecRet("3535353535353535353535353535353535353535")
               << DataConversion::HexStrToUint8VecRet("0de0b6b3a7640000")
               << DataConversion::HexStrToUint8VecRet("")
               << DataConversion::HexStrToUint8VecRet("01")
               << DataConversion::HexStrToUint8VecRet("")
               << DataConversion::HexStrToUint8VecRet("");

    std::cout << "RLP stream is: " << std::endl;
    auto outBytes = rlpStream2.out();
    std::string retme;
    DataConversion::Uint8VecToHexStr(outBytes, retme);
    std::cout << retme << std::endl;

    std::cout << "second rlp stream is: " << std::endl;
    outBytes = rlpStream3.out();
    DataConversion::Uint8VecToHexStr(outBytes, retme);

    DataConversion::NormalizeHexString(retme);
    std::cout << retme << std::endl;

    auto publicKeyRecovered = recoverECDSAPubSig(rsv, retme);

    response = CreateTransactionEth(fieldsHex[0], fieldsHex[1], fieldsHex[2],
                                    fieldsHex[3], fieldsHex[4], fieldsHex[5],
                                    fieldsHex[6], fieldsHex[7], fieldsHex[8]);

    response = "0x0";
  }

  inline virtual void GetBalanceEth(const Json::Value& request,
                                    Json::Value& response) {
    std::cout << "GETBALETH " << request[0u].asString() << std::endl;
    (void)request;
    //response = "0x1010000000000000000000000"; // for a fake response
    std::string address = request[0u].asString();
    DataConversion::NormalizeHexString(address);

    response = this->GetBalance(address)["balance"];
  }

  inline virtual void GetBlockchainInfoXI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    std::cout << "GET BC INFO " << std::endl;
    std::cerr << "GET BC INFO " << std::endl;
    LOG_GENERAL(WARNING, "killme");
    std::exit(1);
    response = this->GetBlockchainInfo();
  }

  std::string GetNetworkId();
  Json::Value CreateTransaction(const Json::Value& _json,
                                const unsigned int num_shards,
                                const uint128_t& gasPrice,
                                const CreateTransactionTargetFunc& targetFunc);
  Json::Value CreateTransactionEth(const std::string& nonce,
                                   const std::string& gasPrice,
                                   const std::string& gasLimit,
                                   const std::string& toAddr,
                                   const std::string& amount,
                                   const std::string& data,
                                   const std::string& R,
                                   const std::string& S,
                                   const std::string& V);
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
};

#endif  // ZILLIQA_SRC_LIBSERVER_LOOKUPSERVER_H_
