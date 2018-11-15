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

#ifndef __POW_H__
#define __POW_H__

#include <stdint.h>
#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/Constants.h"
#include "depends/common/Miner.h"
#include "depends/libethash/ethash.h"
#include "depends/libethash/internal.h"
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
  static std::vector<uint8_t> HexStringToBytes(std::string const& _s);
  static ethash_h256_t DifficultyLevelInInt(uint8_t difficulty);
  std::mutex m_mutexLightClientConfigure;
  std::mutex m_mutexPoWMine;

  POW();
  ~POW();

  POW(POW const&) = delete;
  void operator=(POW const&) = delete;

 public:
  static ethash_h256_t StringToBlockhash(std::string const& _s);
  static std::string BlockhashToHexString(ethash_h256_t* _hash);

  /// Returns the singleton POW instance.
  static POW& GetInstance();

  /// Initializes the POW hash function for the specified block number.
  bool EthashConfigureLightClient(uint64_t block_number);

  /// Triggers the proof-of-work mining.
  ethash_mining_result_t PoWMine(
      uint64_t blockNum, uint8_t difficulty,
      const std::array<unsigned char, UINT256_SIZE>& rand1,
      const std::array<unsigned char, UINT256_SIZE>& rand2,
      const boost::multiprecision::uint128_t& ipAddr, const PubKey& pubKey,
      uint32_t lookupId, const boost::multiprecision::uint256_t& gasPrice,
      bool fullDataset);

  /// Terminates proof-of-work mining.
  void StopMining();

  /// Verifies a proof-of-work submission.
  bool PoWVerify(uint64_t blockNum, uint8_t difficulty,
                 const std::array<unsigned char, UINT256_SIZE>& rand1,
                 const std::array<unsigned char, UINT256_SIZE>& rand2,
                 const boost::multiprecision::uint128_t& ipAddr,
                 const PubKey& pubKey, uint32_t lookupId,
                 const boost::multiprecision::uint256_t& gasPrice,
                 bool fullDataset, uint64_t winning_nonce,
                 const std::string& winning_result,
                 const std::string& winning_mixhash);
  std::vector<unsigned char> ConcatAndhash(
      const std::array<unsigned char, UINT256_SIZE>& rand1,
      const std::array<unsigned char, UINT256_SIZE>& rand2,
      const boost::multiprecision::uint128_t& ipAddr, const PubKey& pubKey,
      uint32_t lookupId, const boost::multiprecision::uint256_t& gasPrice);
  ethash_return_value_t LightHash(uint64_t blockNum,
                                  ethash_h256_t const& header_hash,
                                  uint64_t nonce);
  bool CheckSolnAgainstsTargetedDifficulty(const ethash_h256_t& result,
                                           uint8_t difficulty);
  bool CheckSolnAgainstsTargetedDifficulty(const std::string& result,
                                           uint8_t difficulty);
  static std::set<unsigned int> GetGpuToUse();

 private:
  ethash_light_t ethash_light_client;
  uint64_t currentBlockNum;
  std::atomic<bool> m_shouldMine;
  std::vector<dev::eth::MinerPtr> m_miners;
  std::vector<ethash_mining_result_t> m_vecMiningResult;
  std::atomic<int> m_minerIndex;
  std::condition_variable m_cvMiningResult;
  std::mutex m_mutexMiningResult;

  ethash_light_t EthashLightNew(uint64_t block_number);
  ethash_light_t EthashLightReuse(ethash_light_t ethashLight,
                                  uint64_t block_number);
  void EthashLightDelete(ethash_light_t light);
  ethash_return_value_t EthashLightCompute(ethash_light_t& light,
                                           ethash_h256_t const& header_hash,
                                           uint64_t nonce);
  ethash_full_t EthashFullNew(ethash_light_t& light,
                              ethash_callback_t& CallBack);
  void EthashFullDelete(ethash_full_t& full);
  ethash_return_value_t EthashFullCompute(ethash_full_t& full,
                                          ethash_h256_t const& header_hash,
                                          uint64_t nonce);
  ethash_mining_result_t MineLight(ethash_light_t& light,
                                   ethash_h256_t const& header_hash,
                                   ethash_h256_t& difficulty);
  ethash_mining_result_t MineFull(ethash_full_t& full,
                                  ethash_h256_t const& header_hash,
                                  ethash_h256_t& difficulty);
  ethash_mining_result_t MineFullGPU(uint64_t blockNum,
                                     ethash_h256_t const& header_hash,
                                     uint8_t difficulty);
  void MineFullGPUThread(uint64_t blockNum, ethash_h256_t const& header_hash,
                         uint8_t difficulty, uint64_t nonce);
  bool VerifyLight(ethash_light_t& light, ethash_h256_t const& header_hash,
                   uint64_t winning_nonce, ethash_h256_t& difficulty,
                   ethash_h256_t& result, ethash_h256_t& mixhash);
  bool VerifyFull(ethash_full_t& full, ethash_h256_t const& header_hash,
                  uint64_t winning_nonce, ethash_h256_t& difficulty,
                  ethash_h256_t& result, ethash_h256_t& mixhash);
  void InitOpenCL();
  void InitCUDA();
};
#endif  // __POW_H__
