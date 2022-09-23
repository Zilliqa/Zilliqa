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
 *
 * Test cases obtained from https://www.di-mgt.com.au/sha_testvectors.html
 */

#include "libCrypto/EthCrypto.h"
#include "libEth/Eth.h"
#include "libUtils/DataConversion.h"

#define BOOST_TEST_MODULE ethCryptotest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

/// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings
/// in windows consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))

BOOST_AUTO_TEST_SUITE(ethCryptotest)

/**
 * \brief Test parsing of raw TX fields
 *
 * \details Test the fields of a raw TX can be parsed correctly
 */
BOOST_AUTO_TEST_CASE(TestEthTXParse) {
  // Parse a RLP Tx to extract the fields
  std::string const rlp =
      "f86e01850d9e63a68c82520894673e5ef1ae0a2ef7d0714a96a734ffcd1d8a381f872386"
      "f26fc1000080830102bda0ef23fef2ffa3538b2c8204278ad0427491b5359c346c50a923"
      "6b9b554c45749ea02da3eba55c891dde91e73a312fd3748936fb7af8fb34c2f0fed8a987"
      "7f227e1d";

  auto const result = Eth::parseRawTxFields(rlp);

  BOOST_CHECK_EQUAL(DataConversion::Uint8VecToHexStrRet(result.toAddr),
                    "673E5EF1AE0A2EF7D0714A96A734FFCD1D8A381F");
  BOOST_CHECK_EQUAL(result.amount, uint128_t{"10000000000000000"});
  BOOST_CHECK_EQUAL(result.gasPrice, uint128_t{"58491905676"});
  BOOST_CHECK_EQUAL(result.code.size(), 0);
  BOOST_CHECK_EQUAL(result.data.size(), 0);
  BOOST_CHECK_EQUAL(
      DataConversion::Uint8VecToHexStrRet(result.signature),
      "EF23FEF2FFA3538B2C8204278AD0427491B5359C346C50A9236B9B554C45749E2DA3EBA5"
      "5C891DDE91E73A312FD3748936FB7AF8FB34C2F0FED8A9877F227E1D");
}

/**
 * \brief Test recovery of ECDSA pub key given only message and signature
 *
 * \details As above
 */
BOOST_AUTO_TEST_CASE(TestRecoverECDSASig) {
  // Example RLP raw transaction, sent from metamask with chain ID 33101
  // Note: must use that chain id for this RLP example
  // private key:
  // a8b68f4800bc7513fca14a752324e41b2fa0a7c06e80603aac9e5961e757d906 eth addr:
  // 0x6cCAa29b6cD36C8238E8Fa137311de6153b0b4e7 seed phrase: art rubber roof off
  // fetch bulb board foot payment engage pyramid tiger

  std::string const rlp =
      "f86e01850d9e63a68c82520894673e5ef1ae0a2ef7d0714a96a734ffcd1d8a381f872386"
      "f26fc1000080830102bda0ef23fef2ffa3538b2c8204278ad0427491b5359c346c50a923"
      "6b9b554c45749ea02da3eba55c891dde91e73a312fd3748936fb7af8fb34c2f0fed8a987"
      "7f227e1d";
  std::string const pubKey =
      "041419977507436A81DD0AC7BEB6C7C0DECCBF1A1A1A5E595F647892628A0F65BC9D19CB"
      "F0712F881B529D39E7F75D543DC3E646880A0957F6E6DF5C1B5D0EB278";

  auto const result = RecoverECDSAPubKey(rlp, 33101);
  auto const restultStr = DataConversion::Uint8VecToHexStrRet(result);

  // If this fails, check the pubkey starts with '04' (is uncompressed)
  BOOST_CHECK_EQUAL(restultStr.compare(pubKey), 0);
}

/**
 * \brief Test contract address generation works correctly
 *
 * \details Test contract address generation works - should be a keccak of the
 * RLP : https://ethereum.stackexchange.com/questions/760
 */
BOOST_AUTO_TEST_CASE(TestEthContractAddrGenerate) {
  // Parse a RLP Tx to extract the fields
  std::string const rlp =
      "f86e01850d9e63a68c82520894673e5ef1ae0a2ef7d0714a96a734ffcd1d8a381f872386"
      "f26fc1000080830102bda0ef23fef2ffa3538b2c8204278ad0427491b5359c346c50a923"
      "6b9b554c45749ea02da3eba55c891dde91e73a312fd3748936fb7af8fb34c2f0fed8a987"
      "7f227e1d";

  // We will just compare against known contract address hash outputs
  auto const sender = DataConversion::HexStrToUint8VecRet(
      "0x6ac7ea33f8831ea9dcc53393aaa88b25a785dbf0");

  std::string addresses[] = {"CD234A471B72BA2F1CCF0A70FCABA648A5EECD8D",
                             "343C43A37D37DFF08AE8C4A11544C718ABB4FCF8",
                             "F778B86FA74E846C4F0A1FBD1335FE81C00A0C91",
                             "FFFD933A0BC612844EAF0C6FE3E5B8E9B6C1D19C"};

  for (int i = 0; i < 4; i++) {
    auto const result = CreateContractAddr(sender, i);

    BOOST_CHECK_EQUAL(DataConversion::Uint8VecToHexStrRet(result),
                      addresses[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
