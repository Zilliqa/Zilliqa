/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBETH_ETH_H_
#define ZILLIQA_SRC_LIBETH_ETH_H_

#include <json/value.h>
#include <cstdint>
#include "common/BaseType.h"

struct EthFields {
  uint32_t version{};
  uint64_t nonce{};
  bytes toAddr;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit{};
  bytes code;
  bytes data;
  bytes signature;
};

Json::Value populateReceiptHelper(std::string const &txnhash, bool success,
                                  const std::string &from,
                                  const std::string &to,
                                  const std::string &gasUsed,
                                  const std::string &blockHash,
                                  const std::string &blockNumber);

EthFields parseRawTxFields(std::string const &message);

#endif  // ZILLIQA_SRC_LIBETH_ETH_H_
