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

#ifndef __POW_H__
#define __POW_H__

#include <stdint.h>
#include <array>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#include "jsonrpccpp/client.h"
#include "jsonrpccpp/client/connectors/httpclient.h"
#pragma GCC diagnostic pop

#include "common/Constants.h"
#include "depends/common/Miner.h"
#include "depends/libethash/include/ethash/ethash.hpp"
//#include "ethash/ethash.hpp"
#include "libCrypto/Schnorr.h"
#include "libUtils/Logger.h"

/// Stores the result of PoW mining.
typedef struct ethash_mining_result {
  std::string result;
  std::string mix_hash;
  uint64_t winning_nonce;
  bool success;
} ethash_mining_result_t;

/// Implements the proof-of-work functionality.
class POW {
  static std::string BytesToHexString(const uint8_t* str, const uint64_t s);
  static int FromHex(char _i);
  static bytes HexStringToBytes(std::string const& _s);
  std::mutex m_mutexLightClientConfigure;
  std::mutex m_mutexPoWMine;

  POW();
  ~POW();

  POW(POW const&) = delete;
  void operator=(POW const&) = delete;

 public:
  static ethash_hash256 StringToBlockhash(std::string const& _s);
  static std::string BlockhashToHexString(const ethash_hash256& _hash);
  static bool CheckDifficulty(const ethash_hash256& result,
                              const ethash_hash256& boundary);
  static size_t CountLeadingZeros(const ethash_hash256& boundary);

  /// Returns the singleton POW instance.
  static POW& GetInstance();

  /// Initializes the POW hash function for the specified block number.
  bool EthashConfigureClient(uint64_t block_number, bool fullDataset = false);

  static ethash_hash256 GenHeaderHash(
      const std::array<unsigned char, UINT256_SIZE>& rand1,
      const std::array<unsigned char, UINT256_SIZE>& rand2,
      const uint128_t& ipAddr, const PubKey& pubKey, uint32_t lookupId,
      const uint128_t& gasPrice);

  /// Triggers the proof-of-work mining.
  ethash_mining_result_t PoWMine(uint64_t blockNum, uint8_t difficulty,
                                 const PairOfKey& pairOfKey,
                                 const ethash_hash256& headerHash,
                                 bool fullDataset, uint64_t startNonce,
                                 int timeWindow);

  /// Terminates proof-of-work mining.
  void StopMining();

  /// Verifies a proof-of-work submission.
  bool PoWVerify(uint64_t blockNum, uint8_t difficulty,
                 const ethash_hash256& headerHash, uint64_t winning_nonce,
                 const std::string& winning_result,
                 const std::string& winning_mixhash);
  static bytes ConcatAndhash(
      const std::array<unsigned char, UINT256_SIZE>& rand1,
      const std::array<unsigned char, UINT256_SIZE>& rand2,
      const uint128_t& ipAddr, const PubKey& pubKey, uint32_t lookupId,
      const uint128_t& gasPrice);
  static ethash_hash256 DifficultyLevelInInt(uint8_t difficulty);
  static ethash_hash256 DifficultyLevelInIntDevided(uint8_t difficulty);
  static uint8_t DevidedBoundaryToDifficulty(ethash_hash256 boundary);
  ethash::result LightHash(uint64_t blockNum, ethash_hash256 const& headerHash,
                           uint64_t nonce);
  bool CheckSolnAgainstsTargetedDifficulty(const ethash_hash256& result,
                                           uint8_t difficulty);
  bool CheckSolnAgainstsTargetedDifficulty(const std::string& result,
                                           uint8_t difficulty);
  static std::set<unsigned int> GetGpuToUse();

  // Put it to public function so can directly test with it
  ethash_mining_result_t RemoteMine(const PairOfKey& pairOfKey,
                                    uint64_t blockNum,
                                    ethash_hash256 const& headerHash,
                                    ethash_hash256 const& boundary,
                                    int timeWindow);

  bool SendWorkToProxy(const PairOfKey& pairOfKey, uint64_t blockNum,
                       ethash_hash256 const& headerHash,
                       ethash_hash256 const& boundary, int timeWindow);
  bool CheckMiningResult(const PairOfKey& pairOfKey,
                         ethash_hash256 const& headerHash,
                         ethash_hash256 const& boundary, uint64_t& nonce,
                         ethash_hash256& mixHash, int timeWindow);
  bool VerifyRemoteSoln(uint64_t blockNum, ethash_hash256 const& boundary,
                        uint64_t nonce, const ethash_hash256& headerHash,
                        const ethash_hash256& mixHash,
                        ethash_hash256& hashResult);
  bool SendVerifyResult(const PairOfKey& pairOfKey,
                        const ethash_hash256& headerHash,
                        ethash_hash256 const& boundary, bool verifyResult);

 private:
  std::shared_ptr<ethash::epoch_context> m_epochContextLight = nullptr;
  std::shared_ptr<ethash::epoch_context_full> m_epochContextFull = nullptr;
  uint64_t m_currentBlockNum;
  std::atomic<bool> m_shouldMine;
  std::vector<dev::eth::MinerPtr> m_miners;
  std::vector<ethash_mining_result_t> m_vecMiningResult;
  std::atomic<int> m_minerIndex;
  std::condition_variable m_cvMiningResult;
  std::mutex m_mutexMiningResult;
  std::unique_ptr<jsonrpc::HttpClient> m_httpClient;

  ethash_mining_result_t MineLight(ethash_hash256 const& headerHash,
                                   ethash_hash256 const& boundary,
                                   uint64_t startNonce, int timeWindow);
  ethash_mining_result_t MineFull(ethash_hash256 const& headerHash,
                                  ethash_hash256 const& boundary,
                                  uint64_t startNonce, int timeWindow);
  ethash_mining_result_t MineGetWork(uint64_t blockNum,
                                     ethash_hash256 const& headerHash,
                                     uint8_t difficulty, int timeWindow);
  ethash_mining_result_t MineFullGPU(uint64_t blockNum,
                                     ethash_hash256 const& headerHash,
                                     uint8_t difficulty, uint64_t startNonce,
                                     int timeWindow);
  void MineFullGPUThread(uint64_t blockNum, ethash_hash256 const& headerHash,
                         uint8_t difficulty, uint64_t nonce, int timeWindow);
  void InitOpenCL();
  void InitCUDA();
};
#endif  // __POW_H__
