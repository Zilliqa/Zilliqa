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

#ifndef __TXNPOOL_H__
#define __TXNPOOL_H__

#include <functional>
#include <map>
#include <unordered_map>

#include "libData/AccountData/Transaction.h"

struct TxnPool {
  struct PubKeyNonceHash {
    std::size_t operator()(
        const std::pair<PubKey, boost::multiprecision::uint128_t>& p) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, std::string(p.first));
      boost::hash_combine(seed, p.second.convert_to<std::string>());

      return seed;
    }
  };

  std::unordered_map<TxnHash, Transaction> HashIndex;
  std::map<boost::multiprecision::uint128_t, std::map<TxnHash, Transaction>,
           std::greater<boost::multiprecision::uint128_t>>
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

#endif  // __TXNPOOL_H__