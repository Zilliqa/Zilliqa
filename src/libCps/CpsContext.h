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

#ifndef ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_
#define ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_

#include "libUtils/Evm.pb.h"

namespace libCps {
struct CpsContext {
  using Address = dev::h160;
  Address origSender;
  bool isStatic = false;
  bool estimate = false;
  evm::EvmEvalExtras evmExtras;
  uint64_t gasLimit = 0;
  uint32_t depth = 0;
  uint64_t SubAndGetGasLimit(uint64_t subtrahend) {
    gasLimit = gasLimit > subtrahend ? gasLimit - subtrahend : 0;
    return gasLimit;
  }
  void ReclaimGas(uint64_t reclaimed) { gasLimit += reclaimed; }
};
}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_