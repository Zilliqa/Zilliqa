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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TXNPOOL_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TXNPOOL_H_

#include <functional>
#include <map>
#include <unordered_map>

#include "Account.h"
#include "Transaction.h"
#include "common/TxnStatus.h"

using MempoolInsertionStatus = std::pair<TxnStatus, TxnHash>;

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

  bool insert(const Transaction& t, MempoolInsertionStatus& status) {
    if (exist(t.GetTranID())) {
      status = {TxnStatus::MEMPOOL_ALREADY_PRESENT, t.GetTranID()};
      return false;
    }

    auto searchNonce = NonceIndex.find({t.GetSenderPubKey(), t.GetNonce()});
    if (searchNonce != NonceIndex.end()) {
      if ((t.GetGasPrice() > searchNonce->second.GetGasPrice()) ||
          (t.GetGasPrice() == searchNonce->second.GetGasPrice() &&
           t.GetTranID() < searchNonce->second.GetTranID())) {
        // erase from HashIdxTxns
        TxnHash hashToBeRemoved = searchNonce->second.GetTranID();
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

        status = {TxnStatus::MEMPOOL_SAME_NONCE_LOWER_GAS, hashToBeRemoved};
        return true;
      } else {
        // GasPrice is higher but of same nonce
        // or same gas price and nonce but higher tranID
        status = {TxnStatus::MEMPOOL_SAME_NONCE_LOWER_GAS, t.GetTranID()};
        return false;
      }
    } else {
      HashIndex[t.GetTranID()] = t;
      GasIndex[t.GetGasPrice()][t.GetTranID()] = t;
      NonceIndex[{t.GetSenderPubKey(), t.GetNonce()}] = t;
    }
    status = {TxnStatus::NOT_PRESENT, t.GetTranID()};
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

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TXNPOOL_H_
