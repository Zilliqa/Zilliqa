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

#ifndef __TXNPOOL_H__
#define __TXNPOOL_H__

#include <functional>
#include <map>
#include <unordered_map>

#include "Account.h"
#include "Transaction.h"

struct TxnPool {
  struct PubKeyNonceHash {
    std::size_t operator()(const std::pair<PubKey, uint128_t>& p) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, std::string(p.first));
      boost::hash_combine(seed, p.second.convert_to<std::string>());

      return seed;
    }
  };

  std::unordered_map<TxnHash, Transaction> HashIndex;
  std::map<uint128_t, std::map<TxnHash, Transaction>, std::greater<uint128_t>>
      GasIndex;
  std::unordered_map<std::pair<PubKey, uint64_t>, Transaction, PubKeyNonceHash>
      NonceIndex;

  void clear() {
    HashIndex.clear();
    GasIndex.clear();
    NonceIndex.clear();
  }

  unsigned int size() { return HashIndex.size(); }

  bool exist(const TxnHash& th) {
    return HashIndex.find(th) != HashIndex.end();
  }

  bool get(const TxnHash& th, Transaction& t) {
    if (!exist(th)) {
      return false;
    }
    t = HashIndex.at(th);

    return true;
  }

  bool insert(const Transaction& t) {
    if (exist(t.GetTranID())) {
      return false;
    }

    auto searchNonce = NonceIndex.find({t.GetSenderPubKey(), t.GetNonce()});
    if (searchNonce != NonceIndex.end()) {
      if ((t.GetGasPrice() > searchNonce->second.GetGasPrice()) ||
          (t.GetGasPrice() == searchNonce->second.GetGasPrice() &&
           t.GetTranID() < searchNonce->second.GetTranID())) {
        // erase from HashIdxTxns
        auto searchHash = HashIndex.find(searchNonce->second.GetTranID());
        if (searchHash != HashIndex.end()) {
          HashIndex.erase(searchHash);
        }
        // erase from GasIdxTxns
        auto searchGas = GasIndex.find(searchNonce->second.GetGasPrice());
        if (searchGas != GasIndex.end()) {
          auto searchGasHash =
              searchGas->second.find(searchNonce->second.GetTranID());
          if (searchGasHash != searchGas->second.end()) {
            searchGas->second.erase(searchGasHash);
          }
        }
        HashIndex[t.GetTranID()] = t;
        GasIndex[t.GetGasPrice()][t.GetTranID()] = t;
        searchNonce->second = t;
      }
    } else {
      HashIndex[t.GetTranID()] = t;
      GasIndex[t.GetGasPrice()][t.GetTranID()] = t;
      NonceIndex[{t.GetSenderPubKey(), t.GetNonce()}] = t;
    }
    return true;
  }

  void findSameNonceButHigherGas(Transaction& t) {
    auto searchNonce = NonceIndex.find({t.GetSenderPubKey(), t.GetNonce()});
    if (searchNonce != NonceIndex.end()) {
      if (searchNonce->second.GetGasPrice() > t.GetGasPrice()) {
        t = std::move(searchNonce->second);

        // erase tx nonce map
        NonceIndex.erase(searchNonce);
        // erase tx gas map
        GasIndex[t.GetGasPrice()].erase(t.GetTranID());
        if (GasIndex[t.GetGasPrice()].empty()) {
          GasIndex.erase(t.GetGasPrice());
        }
        // erase tx hash map
        HashIndex.erase(t.GetTranID());
      }
    }
  }

  bool findOne(Transaction& t) {
    if (GasIndex.empty()) {
      return false;
    }

    auto firstGas = GasIndex.begin();
    auto firstHash = firstGas->second.begin();

    if (firstHash != firstGas->second.end()) {
      t = std::move(firstHash->second);

      // erase tx gas map
      firstGas->second.erase(firstHash);
      if (firstGas->second.empty()) {
        GasIndex.erase(firstGas);
      }
      // erase tx nonce map
      NonceIndex.erase({t.GetSenderPubKey(), t.GetNonce()});
      // erase tx hash m ap
      HashIndex.erase(t.GetTranID());
      return true;
    }
    return false;
  }
};

inline std::ostream& operator<<(std::ostream& os, const TxnPool& t) {
  os << "Txn in txnPool: " << std::endl;
  for (const auto& entry : t.HashIndex) {
    os << "TranID: " << entry.first.hex() << " Sender:"
       << Account::GetAddressFromPublicKey(entry.second.GetSenderPubKey())
       << " Nonce: " << entry.second.GetNonce() << std::endl;
  }
  return os;
}

#endif  // __TXNPOOL_H__
