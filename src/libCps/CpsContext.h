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

#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libUtils/Evm.pb.h"

namespace libCps {
struct CpsContext {
  using Address = dev::h160;
  Address origSender;
  int64_t gasLeftCore = 0;
  bool isStatic = false;
  bool estimate = false;
  evm::EvmEvalExtras evmExtras;
  ScillaProcessContext scillaExtras;
};

struct GasTrack {
 public:
  void decreaseByCore(uint64_t coreVal) {

  }
  void decreaseByEth(uint64_t coreVal) {

  }
  void increaseByCore(uint64_t coreVal) {

  }
  void increaseByEth(uint64_t ethVal) {

  }
private:
  uint64_t gasCore = 0;
  uint64_t ethRemainder = 0;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_