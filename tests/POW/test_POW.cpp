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

/*
 * Test cases obtained from https://github.com/ethereum/ethash
 */

#include <libDirectoryService/DirectoryService.h>
#include <libPOW/pow.h>
#include <iomanip>

#ifdef _WIN32
#include <Shlobj.h>
#include <windows.h>
#endif

#define BOOST_TEST_MODULE powtest
#define BOOST_TEST_DYN_LINK

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include "libTestUtils/TestUtils.h"

using namespace std;
using byte = uint8_t;
using bytes = std::vector<byte>;

static constexpr uint64_t ETHASH_DATASET_BYTES_INIT = 1073741824U;  // 2**30
static constexpr uint64_t ETHASH_MIX_BYTES = 128;

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(powtest)

// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings in
// windows consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))
#define UNUSED(x) (void)x
// some functions taken from eth::dev for convenience.
std::string bytesToHexString(const uint8_t* str, const uint64_t s) {
  std::ostringstream ret;

  for (size_t i = 0; i < s; ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase
        << (int)str[i];

  return ret.str();
}

int fromHex(char _i) {
  if (_i >= '0' && _i <= '9') return _i - '0';
  if (_i >= 'a' && _i <= 'f') return _i - 'a' + 10;
  if (_i >= 'A' && _i <= 'F') return _i - 'A' + 10;

  BOOST_REQUIRE_MESSAGE(false, "should never get here");
  return -1;
}

bytes hexStringToBytes(std::string const& _s) {
  unsigned s = (_s[0] == '0' && _s[1] == 'x') ? 2 : 0;
  std::vector<uint8_t> ret;
  ret.reserve((_s.size() - s + 1) / 2);

  if (_s.size() % 2) try {
      ret.emplace_back(fromHex(_s[s++]));
    } catch (...) {
      ret.emplace_back(0);
    }
  for (unsigned i = s; i < _s.size(); i += 2) try {
      ret.emplace_back((byte)(fromHex(_s[i]) * 16 + fromHex(_s[i + 1])));
    } catch (...) {
      ret.emplace_back(0);
    }
  return ret;
}

BOOST_AUTO_TEST_CASE(test_stringToBlockhash) {
  INIT_FILE_LOGGER("zilliqa", "./");
  string original =
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b34";
  ethash_hash256 testhash = POW::StringToBlockhash(original);
  string result = POW::BlockhashToHexString(testhash);
  BOOST_REQUIRE_MESSAGE(result == original,
                        "Expected: "
                        "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539ee"
                        "dad4b92b34 Obtained: "
                            << result);
}

BOOST_AUTO_TEST_CASE(test_stringToBlockhash_smaller_than_expect_message) {
  string original = "badf00d";
  ethash_hash256 testhash = POW::StringToBlockhash(original);
  string result = POW::BlockhashToHexString(testhash);
  BOOST_REQUIRE_MESSAGE(result != original, "Obtained: " << result);
}

BOOST_AUTO_TEST_CASE(test_stringToBlockhash_overflow) {
  string original =
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356e"
      "e3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356ee3441623"
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356e"
      "e3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356ee3441623"
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356e"
      "e3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356ee3441623"
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356e"
      "e3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b347e44356ee3441623"
      "bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b34";
  ethash_hash256 testhash = POW::StringToBlockhash(original);
  string result = POW::BlockhashToHexString(testhash);
  BOOST_REQUIRE_MESSAGE(result != original, "Obtained: " << result);
}

BOOST_AUTO_TEST_CASE(ethash_params_init_genesis_check) {
  uint64_t blockNumber = 0;
  auto epochNumber = ethash::get_epoch_number(blockNumber);
  auto epochContextLight = ethash::create_epoch_context(epochNumber);

  uint64_t full_size = ethash::get_full_dataset_size(
      ethash::calculate_full_dataset_num_items(epochNumber));
  uint64_t cache_size = ethash::get_light_cache_size(
      ethash::calculate_light_cache_num_items(epochNumber));
  BOOST_REQUIRE_MESSAGE(full_size < ETHASH_DATASET_BYTES_INIT,
                        "\nfull size: " << full_size << "\n"
                                        << "should be less than or equal to: "
                                        << ETHASH_DATASET_BYTES_INIT << "\n");
  BOOST_REQUIRE_MESSAGE(
      full_size + 20 * ETHASH_MIX_BYTES >= ETHASH_DATASET_BYTES_INIT,
      "\nfull size + 20*MIX_BYTES: "
          << full_size + 20 * ETHASH_MIX_BYTES << "\n"
          << "should be greater than or equal to: " << ETHASH_DATASET_BYTES_INIT
          << "\n");
  BOOST_REQUIRE_MESSAGE(cache_size < ETHASH_DATASET_BYTES_INIT / 32,
                        "\ncache size: " << cache_size << "\n"
                                         << "should be less than or equal to: "
                                         << ETHASH_DATASET_BYTES_INIT / 32
                                         << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_params_init_genesis_calcifide_check) {
  uint64_t blockNumber = 22;
  auto epochNumber = ethash::get_epoch_number(blockNumber);
  auto epochContextLight = ethash::create_epoch_context(epochNumber);

  uint64_t full_size = ethash::get_full_dataset_size(
      ethash::calculate_full_dataset_num_items(epochNumber));
  uint64_t cache_size = ethash::get_light_cache_size(
      ethash::calculate_light_cache_num_items(epochNumber));
  const uint32_t expected_full_size = 1073739904;
  const uint32_t expected_cache_size = 16776896;
  BOOST_REQUIRE_MESSAGE(full_size == expected_full_size,
                        "\nexpected: " << expected_full_size << "\n"
                                       << "actual: " << full_size << "\n");
  BOOST_REQUIRE_MESSAGE(cache_size == expected_cache_size,
                        "\nexpected: " << expected_cache_size << "\n"
                                       << "actual: " << cache_size << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_params_calcifide_check_30000) {
  uint64_t blockNumber = 30000;
  auto epochNumber = ethash::get_epoch_number(blockNumber);
  auto epochContextLight = ethash::create_epoch_context(epochNumber);

  uint64_t full_size = ethash::get_full_dataset_size(
      ethash::calculate_full_dataset_num_items(epochNumber));
  uint64_t cache_size = ethash::get_light_cache_size(
      ethash::calculate_light_cache_num_items(epochNumber));

  const uint32_t expected_full_size = 1082130304;
  const uint32_t expected_cache_size = 16907456;
  BOOST_REQUIRE_MESSAGE(full_size == expected_full_size,
                        "\nexpected: " << expected_full_size << "\n"
                                       << "actual: " << full_size << "\n");
  BOOST_REQUIRE_MESSAGE(cache_size == expected_cache_size,
                        "\nexpected: " << expected_cache_size << "\n"
                                       << "actual: " << cache_size << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_check_difficulty_check) {
  ethash_hash256 hash{};
  ethash_hash256 target{};
  memcpy(hash.bytes, "11111111111111111111111111111111", 32);
  memcpy(target.bytes, "22222222222222222222222222222222", 32);
  BOOST_REQUIRE_MESSAGE(
      POW::CheckDifficulty(hash, target),
      "\nexpected \"" << std::string((char*)&hash, 32).c_str()
                      << "\" to have the same or less difficulty than \""
                      << std::string((char*)&target, 32).c_str() << "\"\n");
  BOOST_REQUIRE_MESSAGE(POW::CheckDifficulty(hash, hash), "");
  // "\nexpected \"" << hash << "\" to have the same or less difficulty than \""
  // << hash << "\"\n");
  memcpy(target.bytes, "11111111111111111111111111111112", 32);
  BOOST_REQUIRE_MESSAGE(POW::CheckDifficulty(hash, target), "");
  // "\nexpected \"" << hash << "\" to have the same or less difficulty than \""
  // << target << "\"\n");
  memcpy(target.bytes, "11111111111111111111111111111110", 32);
  BOOST_REQUIRE_MESSAGE(!POW::CheckDifficulty(hash, target), "");
  // "\nexpected \"" << hash << "\" to have more difficulty than \"" << target
  // << "\"\n");
}

BOOST_AUTO_TEST_CASE(test_block22_verification) {
  // from POC-9 testnet, epoch 0
  auto epochContextLight =
      ethash::create_epoch_context(ethash::get_epoch_number(22));
  // ethash_light_t light = ethash_light_new(22);
  ethash_hash256 seedhash = POW::StringToBlockhash(
      "372eca2454ead349c3df0ab5d00b0b706b23e49d469387db91811cee0358fc6d");
  BOOST_ASSERT(epochContextLight);
  ethash::result ret =
      ethash::hash(*epochContextLight, seedhash, 0x495732e0ed7a801cU);
  BOOST_REQUIRE_EQUAL(
      POW::BlockhashToHexString(ret.final_hash),
      "00000b184f1fdd88bfd94c86c39e65db0c36144d5e43f745f722196e730cb614");
  ethash_hash256 difficulty{};
  const auto&& initList = {0x2, 0x5, 0x40};
  move(initList.begin(), initList.end(), difficulty.bytes);
  BOOST_REQUIRE(POW::CheckDifficulty(ret.final_hash, difficulty));
}

BOOST_AUTO_TEST_CASE(test_block30001_verification) {
  // from POC-9 testnet, epoch 1
  // ethash_light_t light = ethash_light_new(30001);
  auto epochContextLight =
      ethash::create_epoch_context(ethash::get_epoch_number(30001));
  ethash_hash256 seedhash = POW::StringToBlockhash(
      "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b34");
  BOOST_ASSERT(epochContextLight);
  ethash::result ret =
      ethash::hash(*epochContextLight, seedhash, 0x318df1c8adef7e5eU);
  ethash_hash256 difficulty{};
  const auto&& initList = {0x17, 0x62, 0xff};
  move(initList.begin(), initList.end(), difficulty.bytes);
  BOOST_REQUIRE(POW::CheckDifficulty(ret.final_hash, difficulty));
}

BOOST_AUTO_TEST_CASE(test_block60000_verification) {
  // from POC-9 testnet, epoch 2
  // ethash_light_t light = ethash_light_new(60000);
  auto epochContextLight =
      ethash::create_epoch_context(ethash::get_epoch_number(60000));
  ethash_hash256 seedhash = POW::StringToBlockhash(
      "5fc898f16035bf5ac9c6d9077ae1e3d5fc1ecc3c9fd5bee8bb00e810fdacbaa0");
  BOOST_ASSERT(epochContextLight);
  ethash::result ret =
      ethash::hash(*epochContextLight, seedhash, 0x50377003e5d830caU);
  ethash_hash256 difficulty{};
  const auto&& initList = {0x25, 0xa6, 0x1e};
  move(initList.begin(), initList.end(), difficulty.bytes);
  BOOST_REQUIRE(POW::CheckDifficulty(ret.final_hash, difficulty));
}

BOOST_AUTO_TEST_CASE(mining_and_verification) {
  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 5;
  uint64_t blockToUse = 0;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, false,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(verifyLight);

  rand1 = {{'0', '3'}};
  auto wrongHeaderHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  bool verifyRand =
      POWClient.PoWVerify(blockToUse, difficultyToUse, wrongHeaderHash,
                          winning_result.winning_nonce, winning_result.result,
                          winning_result.mix_hash);
  BOOST_REQUIRE(!verifyRand);

  // Now let's adjust the difficulty expectation during verification
  difficultyToUse = 30;
  bool verifyDifficulty = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyDifficulty);

  difficultyToUse = 10;
  uint64_t winning_nonce = 0;
  bool verifyWinningNonce = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyWinningNonce);
}

BOOST_AUTO_TEST_CASE(mining_and_verification_big_block_number) {
  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 3;
  uint64_t blockToUse = 34567;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, false,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(verifyLight);

  rand1 = {{'0', '3'}};
  auto wrongHeaderHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  bool verifyRand =
      POWClient.PoWVerify(blockToUse, difficultyToUse, wrongHeaderHash,
                          winning_result.winning_nonce, winning_result.result,
                          winning_result.mix_hash);
  BOOST_REQUIRE(!verifyRand);

  // Now let's adjust the difficulty expectation during verification
  difficultyToUse = 30;
  bool verifyDifficulty = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyDifficulty);

  difficultyToUse = 3;
  uint64_t winning_nonce = 0;
  bool verifyWinningNonce = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyWinningNonce);
}

BOOST_AUTO_TEST_CASE(mining_and_verification_full) {
  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 5;
  uint64_t blockToUse = 0;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, true,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(verifyLight);

  rand1 = {{'0', '3'}};
  auto wrongHeaderHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  bool verifyRand =
      POWClient.PoWVerify(blockToUse, difficultyToUse, wrongHeaderHash,
                          winning_result.winning_nonce, winning_result.result,
                          winning_result.mix_hash);
  BOOST_REQUIRE(!verifyRand);

  // Now let's adjust the difficulty expectation during verification
  difficultyToUse = 30;
  bool verifyDifficulty = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyDifficulty);

  difficultyToUse = 10;
  uint64_t winning_nonce = 0;
  bool verifyWinningNonce = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyWinningNonce);
}

BOOST_AUTO_TEST_CASE(mining_high_diffculty_time_out) {
  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 50;
  uint64_t blockToUse = 0;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, true,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  BOOST_REQUIRE(!winning_result.success);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyLight);
}

// Please enable the OPENCL_GPU_MINE option in constants.xml to run this test
// case
BOOST_AUTO_TEST_CASE(gpu_mining_and_verification_1) {
  if (!OPENCL_GPU_MINE && !CUDA_GPU_MINE) {
    std::cout << "OPENCL_GPU_MINE and CUDA_GPU_MINE option are not "
                 "enabled, skip test case "
                 "gpu_mining_and_verification_1"
              << std::endl;
    return;
  }

  if (OPENCL_GPU_MINE) {
    std::cout << "OPENCL_GPU_MINE enabled, test with OpenCL GPU" << std::endl;
  } else if (CUDA_GPU_MINE) {
    std::cout << "CUDA_GPU_MINE enabled, test with CUDA GPU" << std::endl;
  }

  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 10;
  uint64_t blockToUse = 0;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, true,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(verifyLight);

  rand1 = {{'0', '3'}};
  auto wrongHeaderHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  bool verifyRand =
      POWClient.PoWVerify(blockToUse, difficultyToUse, wrongHeaderHash,
                          winning_result.winning_nonce, winning_result.result,
                          winning_result.mix_hash);
  BOOST_REQUIRE(!verifyRand);

  // Now let's adjust the difficulty expectation during verification
  difficultyToUse = 30;
  bool verifyDifficulty = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyDifficulty);

  difficultyToUse = 10;
  uint64_t winning_nonce = 0;
  bool verifyWinningNonce = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyWinningNonce);
}

// Please enable the OPENCL_GPU_MINE or CUDA_GPU_MINE option in constants.xml to
// run this test case
BOOST_AUTO_TEST_CASE(gpu_mining_and_verification_2) {
  if (!OPENCL_GPU_MINE && !CUDA_GPU_MINE) {
    std::cout << "OPENCL_GPU_MINE and CUDA_GPU_MINE option are not "
                 "enabled, skip test case "
                 "gpu_mining_and_verification_2"
              << std::endl;
    return;
  }

  if (OPENCL_GPU_MINE) {
    std::cout << "OPENCL_GPU_MINE enabled, test with OpenCL GPU" << std::endl;
  } else if (CUDA_GPU_MINE) {
    std::cout << "CUDA_GPU_MINE enabled, test with CUDA GPU" << std::endl;
  }

  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = {{'0', '1'}};
  std::array<unsigned char, 32> rand2 = {{'0', '2'}};
  auto peer = TestUtils::GenerateRandomPeer();
  auto keyPair = Schnorr::GenKeyPair();
  auto pubKey = keyPair.second;

  // Light client mine and verify
  uint8_t difficultyToUse = 20;
  uint64_t blockToUse = 1234567;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  ethash_mining_result_t winning_result =
      POWClient.PoWMine(blockToUse, difficultyToUse, keyPair, headerHash, true,
                        std::time(0), POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(verifyLight);

  rand1 = {{'0', '3'}};
  auto wrongHeaderHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  bool verifyRand =
      POWClient.PoWVerify(blockToUse, difficultyToUse, wrongHeaderHash,
                          winning_result.winning_nonce, winning_result.result,
                          winning_result.mix_hash);
  BOOST_REQUIRE(!verifyRand);

  // Now let's adjust the difficulty expectation during verification
  difficultyToUse = 30;
  bool verifyDifficulty = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyDifficulty);

  difficultyToUse = 10;
  uint64_t winning_nonce = 0;
  bool verifyWinningNonce = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_nonce,
      winning_result.result, winning_result.mix_hash);
  BOOST_REQUIRE(!verifyWinningNonce);
}

BOOST_AUTO_TEST_CASE(difficulty_adjustment_small_network) {
  uint8_t currentDifficulty = 3;
  uint8_t minDifficulty = 3;
  int64_t powSubmissions = 25;
  int64_t expectedNodes = 20;
  uint32_t adjustThreshold = 5;

  int newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 4);

  currentDifficulty = 6;
  powSubmissions =
      15;  // Node number is droping and number of pow submissions is less than
           // expected node, so expect difficulty will drop.
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 5);

  currentDifficulty = 14;
  expectedNodes = 200;
  powSubmissions = 201;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 14);
}

BOOST_AUTO_TEST_CASE(difficulty_adjustment_large_network) {
  uint8_t currentDifficulty = 3;
  uint8_t minDifficulty = 3;
  int64_t powSubmissions = 5100;
  int64_t expectedNodes = 5000;
  uint32_t adjustThreshold = 99;

  int newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 4);

  currentDifficulty = 4;
  expectedNodes = 10001;  // The current nodes exceed expected node
  powSubmissions =
      10002;  // Pow submission still increase, need to increase difficulty
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 4);

  currentDifficulty = 10;
  expectedNodes = 8000;
  powSubmissions =
      7900;  // Node number is droping and number of pow submissions is less
             // than expected node, so expect difficulty will drop.
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 9);

  currentDifficulty = 5;
  expectedNodes = 8000;
  powSubmissions = 8000;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty ==
                5);  // nothing changes, expect keep the same difficulty

  currentDifficulty = 14;
  expectedNodes = 10002;
  powSubmissions = 10005;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 14);
}

BOOST_AUTO_TEST_CASE(difficulty_adjustment_for_ds_small) {
  uint8_t currentDifficulty = 9;
  uint8_t minDifficulty = 5;
  int64_t powSubmissions = 11;
  int64_t expectedNodes = 10;
  uint32_t adjustThreshold = 5;

  int newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 9);
}

BOOST_AUTO_TEST_CASE(difficulty_adjustment_for_ds_large) {
  uint8_t currentDifficulty = 5;
  uint8_t minDifficulty = 5;
  int64_t powSubmissions = 110;
  int64_t expectedNodes = 100;
  uint32_t adjustThreshold = 9;

  int newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 6);

  currentDifficulty = 6;
  powSubmissions = 120;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 8);

  currentDifficulty = 8;
  expectedNodes = 103;  // Current node number exceed expected number.
  powSubmissions =
      99;  // The PoW submissions drop not much, so keep difficulty.
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 8);

  currentDifficulty = 14;
  expectedNodes = 102;
  powSubmissions = 102;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 14);
}

BOOST_AUTO_TEST_CASE(devided_difficulty_adjustment_for_ds_large) {
  uint8_t currentDifficulty = 5;
  uint8_t minDifficulty = 5;
  int64_t powSubmissions = 110;
  int64_t expectedNodes = 100;
  uint32_t adjustThreshold = 9;

  int newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 6);

  // test difficulty increase
  currentDifficulty = 11;
  expectedNodes = 1800;
  powSubmissions = 1900;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 12);

  currentDifficulty = 26;
  expectedNodes = 1800;
  powSubmissions = 2500;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 28);

  currentDifficulty = 31;
  expectedNodes = 1800;
  powSubmissions = 1898;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 31);

  currentDifficulty = 31;
  expectedNodes = 1800;
  powSubmissions = 1899;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 32);

  currentDifficulty = 31;
  expectedNodes = 1800;
  powSubmissions = 2500;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 33);

  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 2500;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 39);

  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 3000;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 40);

  currentDifficulty = 51;
  expectedNodes = 1800;
  powSubmissions = 2400;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 57);

  currentDifficulty = 64;
  expectedNodes = 1800;
  powSubmissions = 1898;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 64);

  currentDifficulty = 64;
  expectedNodes = 1800;
  powSubmissions = 1899;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 65);

  currentDifficulty = 64;
  expectedNodes = 1800;
  powSubmissions = 1998;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 66);

  currentDifficulty = 79;
  expectedNodes = 1800;
  powSubmissions = 3000;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 87);

  currentDifficulty = 184;
  expectedNodes = 1800;
  powSubmissions = 2100;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 187);

  // test difficulty decrease
  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 1400;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 30);

  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 1702;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 32);

  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 1701;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 31);

  currentDifficulty = 32;
  expectedNodes = 1800;
  powSubmissions = 1700;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 31);

  currentDifficulty = 33;
  expectedNodes = 1800;
  powSubmissions = 500;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 30);

  currentDifficulty = 99;
  expectedNodes = 1800;
  powSubmissions = 500;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 91);

  currentDifficulty = 75;
  expectedNodes = 1800;
  powSubmissions = 1200;
  adjustThreshold = 99;
  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 69);
}

BOOST_AUTO_TEST_CASE(test_highest_difficulty) {
  std::cout << "Start test highest difficulty" << std::endl;
  uint8_t currentDifficulty = 255;
  uint8_t minDifficulty = 5;
  int64_t powSubmissions = 110;
  int64_t expectedNodes = 100;
  uint32_t adjustThreshold = 9;

  auto newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 255);

  currentDifficulty = 253;
  powSubmissions = 200;

  newDifficulty = DirectoryService::CalculateNewDifficultyCore(
      currentDifficulty, minDifficulty, powSubmissions, expectedNodes,
      adjustThreshold);
  BOOST_REQUIRE(newDifficulty == 255);
}

BOOST_AUTO_TEST_CASE(devided_boundary) {
  std::cout << "Start test devided_boundary" << std::endl;

  for (size_t diff = 1; diff < 256; diff++) {
    auto boundary = POW::DifficultyLevelInIntDevided(diff);
    std::string str_boundary = POW::BlockhashToHexString(boundary);

    std::cout << "Devided D->B: " << int(diff) << "->" << str_boundary
              << std::endl;

    auto difficulty = POW::DevidedBoundaryToDifficulty(boundary);

    std::cout << "Devided B->D: " << str_boundary << "->" << int(difficulty)
              << std::endl;

    BOOST_REQUIRE(difficulty == diff);
  }
}

BOOST_AUTO_TEST_SUITE_END()
