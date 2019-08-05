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

#include <libPOW/pow.h>
#include <iomanip>

#ifdef _WIN32
#include <Shlobj.h>
#include <windows.h>
#endif

#include <iostream>
#include <vector>
#include "libTestUtils/TestUtils.h"

static std::array<uint8_t, 32> generateRandomArray() {
  std::array<uint8_t, 32> randResult{};
  std::srand(
      std::time(nullptr));  // use current time as seed for random generator
  for (int i = 0; i < 32; ++i) {
    randResult[i] = std::rand() % std::numeric_limits<uint8_t>::max();
  }
  return randResult;
}

void TestRemoteMineCase_1() {
  POW& POWClient = POW::GetInstance();
  std::array<unsigned char, 32> rand1 = generateRandomArray();
  std::array<unsigned char, 32> rand2 = generateRandomArray();
  auto peer = TestUtils::GenerateRandomPeer();
  PrivKey privKey(
      DataConversion::StringToCharArray(
          "80AA3FB5F4A60E87F1387E758CAA9EB34FCE7BAC62E1BDE4FEFE92FEA5281223"),
      0);
  // PubKey pubKey(
  //    DataConversion::StringToCharArray(
  //        "02025DB9FCB3FCF98BB8220F9249DBCB3CB8A2998260DDA50D9C19F0C1D38B8C16"),
  //    0);
  PubKey pubKey(privKey);
  std::cout << "Test with pubkey: " << pubKey << std::endl;
  PairOfKey keyPair(privKey, pubKey);

  // Light client mine and verify
  uint8_t difficultyToUse = POW_DIFFICULTY;
  uint64_t blockToUse = 1000;
  auto headerHash = POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0);
  auto boundary = POW::DifficultyLevelInIntDevided(difficultyToUse);

  ethash_mining_result_t winning_result = POWClient.RemoteMine(
      keyPair, blockToUse, headerHash, boundary, POW_WINDOW_IN_SECONDS);
  bool verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  std::cout << "Verify difficulty " << std::to_string(difficultyToUse)
            << " result " << verifyLight << std::endl;

  difficultyToUse = DS_POW_DIFFICULTY;
  boundary = POW::DifficultyLevelInIntDevided(difficultyToUse);

  winning_result = POWClient.RemoteMine(keyPair, blockToUse, headerHash,
                                        boundary, POW_WINDOW_IN_SECONDS);
  verifyLight = POWClient.PoWVerify(
      blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
      winning_result.result, winning_result.mix_hash);
  std::cout << "Verify difficulty " << std::to_string(difficultyToUse)
            << " result " << verifyLight << std::endl;
}

int main([[gnu::unused]] int argc, [[gnu::unused]] const char* argv[]) {
  INIT_STDOUT_LOGGER();

  TestRemoteMineCase_1();
}