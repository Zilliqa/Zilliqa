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
#include <cassert>

#define BOOST_TEST_MODULE remotemine
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(remotemine)

using zbytes = std::vector<uint8_t>;

// static std::array<uint8_t, 32> generateRandomArray() {
//   std::array<uint8_t, 32> randResult{};
//   std::srand(
//       std::time(nullptr));  // use current time as seed for random generator
//   for (int i = 0; i < 32; ++i) {
//     randResult[i] = std::rand() % std::numeric_limits<uint8_t>::max();
//   }
//   return randResult;
// }

int FromHex(char _i) {
  if (_i >= '0' && _i <= '9') return _i - '0';
  if (_i >= 'a' && _i <= 'f') return _i - 'a' + 10;
  if (_i >= 'A' && _i <= 'F') return _i - 'A' + 10;
  return -1;
}

zbytes HexStringToBytes(std::string const& _s) {
  unsigned s = (_s[0] == '0' && _s[1] == 'x') ? 2 : 0;
  zbytes ret;
  ret.reserve((_s.size() - s + 1) / 2);

  if (_s.size() % 2) try {
      ret.push_back(FromHex(_s[s++]));
    } catch (...) {
      ret.push_back(0);
    }
  for (unsigned i = s; i < _s.size(); i += 2) try {
      ret.push_back((uint8_t)(FromHex(_s[i]) * 16 + FromHex(_s[i + 1])));
    } catch (...) {
      ret.push_back(0);
    }
  return ret;
}

// BOOST_AUTO_TEST_CASE(test_remoteMineCase1) {
//   POW& POWClient = POW::GetInstance();
//   std::array<unsigned char, 32> rand1 = generateRandomArray();
//   std::array<unsigned char, 32> rand2 = generateRandomArray();
//   auto peer = TestUtils::GenerateRandomPeer();
//   PrivKey privKey(
//       DataConversion::StringToCharArray(
//           "80AA3FB5F4A60E87F1387E758CAA9EB34FCE7BAC62E1BDE4FEFE92FEA5281223"),
//       0);
//   // PubKey pubKey(
//   //    DataConversion::StringToCharArray(
//   //        "02025DB9FCB3FCF98BB8220F9249DBCB3CB8A2998260DDA50D9C19F0C1D38B8C16"),
//   //    0);
//   PubKey pubKey(privKey);
//   std::cout << "Test with pubkey: " << pubKey << std::endl;
//   PairOfKey keyPair(privKey, pubKey);
//   zbytes extraData = TestUtils::GenerateRandomCharVector(32);

//   // Light client mine and verify
//   uint8_t difficultyToUse = POW_DIFFICULTY;
//   uint64_t blockToUse = 1000;
//   auto headerHash =
//       POW::GenHeaderHash(rand1, rand2, peer, pubKey, 0, 0, extraData);
//   auto boundary = POW::DifficultyLevelInIntDevided(difficultyToUse);

//   HeaderHashParams headerParams{rand1, rand2, peer, pubKey, 0, 0};
//   ethash_mining_result_t winning_result =
//       POWClient.RemoteMine(keyPair, blockToUse, headerHash, boundary,
//                            POW_WINDOW_IN_SECONDS, headerParams);
//   bool verifyLight = POWClient.PoWVerify(
//       blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
//       winning_result.result, winning_result.mix_hash);
//   std::cout << "Verify difficulty " << std::to_string(difficultyToUse)
//             << " result " << verifyLight << std::endl;

//   difficultyToUse = DS_POW_DIFFICULTY;
//   boundary = POW::DifficultyLevelInIntDevided(difficultyToUse);

//   winning_result =
//       POWClient.RemoteMine(keyPair, blockToUse, headerHash, boundary,
//                            POW_WINDOW_IN_SECONDS, headerParams);
//   verifyLight = POWClient.PoWVerify(
//       blockToUse, difficultyToUse, headerHash, winning_result.winning_nonce,
//       winning_result.result, winning_result.mix_hash);
//   std::cout << "Verify difficulty " << std::to_string(difficultyToUse)
//             << " result " << verifyLight << std::endl;
// }

BOOST_AUTO_TEST_CASE(test_remoteMineHeaderHashGenerate) {
  std::string expectedHeaderhash =
      "406cb087b1123a00dfad0791836a46c2b33c86fb6dbc77dab7846375104beed9";
  std::array<unsigned char, 32> rand1{};
  std::array<unsigned char, 32> rand2{};
  const std::string pubkeystr =
      "0x02bcaf228edea3829a0bb64c7e842ca1d3344c019fcb1d5a3af81162ceb0d0a1c2";
  const std::string rand1str =
      "0x89F3C9C4CE7D091F8BF1F4780C375BBAFAA0D7E8234C73AD24FFE406737028C9";
  const std::string rand2str =
      "0xBC8E23704290908A380CF71EFAB15161B3CD3914C8AE07521D5EC9FB2396BD7F";
  const std::string ip_port_str = "0x000000000000000000000000326e5b230000816d";
  const std::string lookupIdstr = "0x00000000";
  const std::string gasPricestr = "0x00000000000000000000000000000000";
  const std::string extraDatastr = "de40c1d34d4141c4bf56c806ffd5c00f";
  zbytes bytes_pub_key = HexStringToBytes(pubkeystr);
  PubKey pubkey(bytes_pub_key, 0);
  zbytes bytes_ip_port = HexStringToBytes(ip_port_str);
  Peer peer;
  peer.Deserialize(bytes_ip_port, 0);
  std::cout << "Peer = " << peer << std::endl;
  std::cout << "PubKey = " << pubkey << std::endl;
  DataConversion::HexStrToStdArray(rand1str, rand1);
  DataConversion::HexStrToStdArray(rand2str, rand2);
  std::vector<uint8_t> zbytesextraData{};
  zbytesextraData = toZbytes(extraDatastr);
  zbytes extraDataBytes = DataConversion::HexStrToUint8VecRet(extraDatastr);
  auto computedHeaderHash =
      POW::GenHeaderHash(rand1, rand2, peer, pubkey, 0, 0, zbytesextraData);
  std::cout << "computedHeaderHash = "
            << POW::BlockhashToHexString(computedHeaderHash) << std::endl;
  BOOST_REQUIRE_MESSAGE(POW::BlockhashToHexString(computedHeaderHash) == expectedHeaderhash,
                        "Obtained: " << computedHeaderHash);
}

BOOST_AUTO_TEST_SUITE_END()
