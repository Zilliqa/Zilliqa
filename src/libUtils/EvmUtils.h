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

#ifndef ZILLIQA_SRC_LIBUTILS_EVMUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_EVMUTILS_H_

#include <json/json.h>

#include <boost/multiprecision/cpp_int.hpp>
#include "libData/AccountData/InvokeType.h"
#include "libUtils/EvmCallParameters.h"

// fwd decls

class TransactionReceipt;
class ScillaIPCServer;
class Account;

namespace evmproj {
struct ApplyInstructions;
}

class EvmUtils {
 public:
  /// get the command for invoking the evm_runner while calling
  static Json::Value GetEvmCallJson(const EvmCallParameters& params);

  static uint64_t UpdateGasRemaining(TransactionReceipt& receipt,
                                     INVOKE_TYPE invoke_type,
                                     uint64_t& oldValue, uint64_t newValue);

  static bool EvmUpdateContractStateAndAccount(
      const std::shared_ptr<ScillaIPCServer>& ipcServer,
      Account* contractAccount, evmproj::ApplyInstructions& op);

  using bytes = std::vector<uint8_t>;

  static bool isEvm(const bytes& code);
};

#endif  // ZILLIQA_SRC_LIBUTILS_EVMUTILS_H_
