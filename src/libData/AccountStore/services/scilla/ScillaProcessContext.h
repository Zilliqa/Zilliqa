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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_SERVICES_SCILLA_SCILLAPROCESSCONTEXT_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_SERVICES_SCILLA_SCILLAPROCESSCONTEXT_H_

#include "common/BaseType.h"
#include "common/FixedHash.h"

#include "libData/AccountData/Transaction.h"

struct ScillaProcessContext {
  using Address = dev::h160;
  Address origin;
  Address recipient;
  zbytes code;
  zbytes data;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit = 0;
  uint64_t blockNum = 0;
  uint64_t dsBlockNum = 0;
  Transaction::ContractType contractType;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_SERVICES_SCILLA_SCILLAPROCESSCONTEXT_H_
