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

#ifndef ZILLIQA_SRC_COMMON_TXNSTATUS_H_
#define ZILLIQA_SRC_COMMON_TXNSTATUS_H_

#include "depends/common/FixedHash.h"

using TxnHash = dev::h256;

enum TxnStatus : uint8_t {

  NOT_PRESENT = 0,
  DISPATCHED = 1,
  SOFT_CONFIRMED = 2,
  CONFIRMED = 3,
  // Pending
  PRESENT_NONCE_HIGH = 4,
  PRESENT_GAS_EXCEEDED = 5,
  PRESENT_VALID_CONSENSUS_NOT_REACHED = 6,
  // RareDropped
  MATH_ERROR = 10,
  FAIL_SCILLA_LIB = 11,
  FAIL_CONTRACT_INIT = 12,
  INVALID_FROM_ACCOUNT = 13,
  HIGH_GAS_LIMIT = 14,
  INCORRECT_TXN_TYPE = 15,
  INCORRECT_SHARD = 16,
  CONTRACT_CALL_WRONG_SHARD = 17,
  HIGH_BYTE_SIZE_CODE = 18,
  VERIF_ERROR = 19,
  //
  INSUFFICIENT_GAS_LIMIT = 20,
  INSUFFICIENT_BALANCE = 21,
  INSUFFICIENT_GAS = 22,
  MEMPOOL_ALREADY_PRESENT = 23,
  MEMPOOL_SAME_NONCE_LOWER_GAS = 24,
  //
  INVALID_TO_ACCOUNT = 25,
  FAIL_CONTRACT_ACCOUNT_CREATION = 26,
  ERROR = 255  // MISC_ERROR
};

inline bool IsTxnDropped(TxnStatus code) {
  return (static_cast<uint8_t>(code) >= 10);
}

using HashCodeMap = std::unordered_map<TxnHash, TxnStatus>;

class TTLTxns {
 private:
  std::unordered_map<uint64_t, std::unordered_set<TxnHash>> m_txnHashExpiration;
  HashCodeMap m_txnCode;

 public:
  bool insert(const TxnHash& txhash, const TxnStatus status,
              const uint64_t& epochNum);
  void clear(const uint64_t& epochNum, const unsigned int& TTL);
  const HashCodeMap& GetHashCodeMap() const;
  void clearAll();
};

enum PendingData { HASH_CODE_MAP, PUBKEY, SHARD_ID };

#endif  // ZILLIQA_SRC_COMMON_TXNSTATUS_H_
