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

#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

int main() {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Reset();
  bytes message;
  string s;
  cin >> s;
  // TODO: Handle Exceptions
  PubKey key(DataConversion::HexStrToUint8Vec(s), 0);
  key.Serialize(message, 0);
  sha2.Update(message, 0, PUB_KEY_SIZE);
  const bytes& tmp2 = sha2.Finalize();
  Address toAddr;
  copy(tmp2.end() - ACC_ADDR_SIZE, tmp2.end(), toAddr.asArray().begin());
  cout << toAddr << endl;
}
