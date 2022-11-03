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
#include "common/BaseType.h"
#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/InvokeType.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/TxnExtras.h"

// fwd decls

class TransactionReceipt;
class Account;

namespace evmproj {
struct ApplyInstructions;
}

class EvmUtils {
 public:
  /// get the command for invoking the evm_runner while calling
  static Json::Value GetEvmCallJson(const EvmCallParameters& params);

  /// get the command for invoking the evm_runner while calling
  static Json::Value GetEvmCallJson(const evm::EvmArgs& args);

  using zbytes = std::vector<uint8_t>;

  static bool isEvm(const zbytes& code);
};

bool GetEvmEvalExtras(const uint64_t& blockNum, const TxnExtras& extras_in,
                      evm::EvmEvalExtras& extras_out);

using H256 = dev::h256;

evm::H256 H256ToProto(const H256& hash);
H256 ProtoToH256(const evm::H256& hash);

evm::Address AddressToProto(const Address& address);
Address ProtoToAddress(const evm::Address& address);

uint128_t ProtoToUint(const evm::UInt128& numProto);
uint256_t ProtoToUint(const evm::UInt256& numProto);

evm::UInt128 UIntToProto(const uint128_t& num);
evm::UInt256 UIntToProto(const uint256_t& num);

#endif  // ZILLIQA_SRC_LIBUTILS_EVMUTILS_H_
