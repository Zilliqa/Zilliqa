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

#include <Schnorr.h>
#include <iostream>

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libUtils/DataConversion.h"

using namespace std;

void Print(const bytes& payload) {
  static const char* hex_table = "0123456789ABCDEF";

  size_t payload_string_len = (payload.size() * 2) + 1;
  unique_ptr<char[]> payload_string = make_unique<char[]>(payload_string_len);
  for (unsigned int payload_idx = 0, payload_string_idx = 0;
       (payload_idx < payload.size()) &&
       ((payload_string_idx + 2) < payload_string_len);
       payload_idx++) {
    payload_string.get()[payload_string_idx++] =
        hex_table[(payload.at(payload_idx) >> 4) & 0xF];
    payload_string.get()[payload_string_idx++] =
        hex_table[payload.at(payload_idx) & 0xF];
  }
  payload_string.get()[payload_string_len - 1] = '\0';
  cout << payload_string.get();
}

int main([[gnu::unused]] int argc, [[gnu::unused]] const char* argv[]) {
  PairOfKey keypair = Schnorr::GenKeyPair();

  bytes privkey, pubkey;
  keypair.first.Serialize(privkey, 0);
  keypair.second.Serialize(pubkey, 0);

  Print(pubkey);
  cout << " ";
  Print(privkey);
  cout << '\n';

  return 0;
}
