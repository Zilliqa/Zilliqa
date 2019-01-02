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

#include <iostream>
#include "libCrypto/Schnorr.h"

using namespace std;

int main(int argc, const char* argv[]) {
  if (4 != argc) {
    cout << "Input format: ./sign message privateKeyFileName "
            "publicKeyFileName";
    return -1;
  }

  const bytes message = DataConversion::HexStrToUint8Vec(string(argv[1]));

  string line;
  vector<PrivKey> privKeys;
  {
    fstream privFile(argv[2], ios::in);

    while (getline(privFile, line)) {
      privKeys.emplace_back(DataConversion::HexStrToUint8Vec(line), 0);
    }
  }

  vector<PubKey> pubKeys;
  {
    fstream pubFile(argv[3], ios::in);

    while (getline(pubFile, line)) {
      pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(line), 0);
    }
  }

  if (privKeys.size() != pubKeys.size()) {
    cout << "Private key number must equal to public key number!";
    return -1;
  }

  for (unsigned int i = 0; i < privKeys.size(); ++i) {
    Signature sig;
    Schnorr::GetInstance().Sign(message, privKeys.at(i), pubKeys.at(i), sig);
    bytes result;
    sig.Serialize(result, 0);
    cout << DataConversion::Uint8VecToHexStr(result);
  }

  return 0;
}
