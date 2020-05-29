/*
 * Copyright (C) 2020 Zilliqa
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

#include <boost/algorithm/hex.hpp>
#include <boost/program_options.hpp>
#include <cstring>

#include <Schnorr.h>
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/CryptoUtils.h"

namespace po = boost::program_options;

using namespace std;

std::ostream& operator<<(std::ostream& os, const PrivKey& p) {
  string output;
  bytes tmp;
  p.Serialize(tmp, 0);
  boost::algorithm::hex(tmp.begin(), tmp.end(), back_inserter(output));
  os << output;
  return os;
}

void description() {
  cout << endl << "Description:\n";
  cout << "Generate wallet_address for a target shard\n";
}

int main(int argc, char** argv) {
  unsigned int numShards = 3;
  unsigned int numPerShard = 1;

  po::options_description desc("Options");

  desc.add_options()("help, h", "Print help message")(
      "numshards, s", po::value<unsigned int>(&numShards),
      "Total number of shards (default=3)")(
      "numpershard, p", po::value<unsigned int>(&numPerShard),
      "Number of accounts per shard (default=1)");

  po::variables_map vm;

  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return 1;
  }

  cout << "numshards=" << numShards << " numpershard=" << numPerShard << endl;

  for (unsigned int targetShard = 0; targetShard < numShards; targetShard++) {
    for (unsigned int count = 0; count < numPerShard; count++) {
      while (true) {
        PairOfKey keypair = Schnorr::GenKeyPair();
        Address addr = CryptoUtils::GetAddressFromPubKey(keypair.second);
        if (Transaction::GetShardIndex(addr, numShards) != targetShard) {
          continue;
        }
        cout << "\t\t<account>" << endl;
        cout << "\t\t\t<private_key>" << keypair.first << "</private_key>"
             << endl;
        cout << "\t\t\t<wallet_address>" << addr << "</wallet_address>" << endl;
        cout << "\t\t</account>" << endl;
        break;
      }
    }
  }

  return 0;
}
