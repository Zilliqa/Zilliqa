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
