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

#include <array>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <google/protobuf/text_format.h>
#include <json/value.h>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/endian.hpp>
#include <boost/filesystem.hpp>
#include <websocketpp/base64/base64.hpp>
#include "EvmUtils.h"

#include "JsonUtils.h"
#include "Logger.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/GasConv.h"
#include "libUtils/TxnExtras.h"

using namespace std;
using namespace boost::multiprecision;

Json::Value EvmUtils::GetEvmCallJson(const evm::EvmArgs& args) {
  Json::Value arr_ret(Json::arrayValue);

  std::string output;
  args.SerializeToString(&output);
  // Output can contain non-UTF8, so must be wrapped in base64.
  arr_ret.append(websocketpp::base64_encode(output));
  return arr_ret;
}

evm::EvmResult& EvmUtils::GetEvmResultFromJson(const Json::Value& json,
                                               evm::EvmResult& result) {
  std::string data = websocketpp::base64_decode(json.asString());
  if (!result.ParseFromString(data)) {
    throw std::runtime_error("Cannot parse EVM result protobuf");
  }
  return result;
}

bool EvmUtils::isEvm(const zbytes& code) {
  if (not ENABLE_EVM) {
    return false;
  }

  if (code.empty()) {
    // returning false which means it will behave as if it was a scilla only
    // Scilla handles scilla smartContracts and non contracts
    return false;
  }

  if (code.size() < 4) {
    return false;
  }

  auto const hasEvm = (code[0] == 'E' && code[1] == 'V' && code[2] == 'M');

  return hasEvm;
}

static std::string ExitErrorString(const evm::ExitReason::Error& error) {
  switch (error.kind()) {
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_STACK_OVERFLOW:
      return "Error: stack overflow";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_STACK_UNDERFLOW:
      return "stack underflow";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_INVALID_JUMP:
      return "invalid jump";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_INVALID_RANGE:
      return "invalid range";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_DESIGNATED_INVALID:
      return "designated invalid";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_CALL_TOO_DEEP:
      return "call too deep";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_CREATE_COLLISION:
      return "create collision";
    case evm::ExitReason_Error::Kind::
        ExitReason_Error_Kind_CREATE_CONTRACT_LIMIT:
      return "create contract limit";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_INVALID_CODE:
      return "invalid code";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_OUT_OF_OFFSET:
      return "out of offset";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_OUT_OF_GAS:
      return "out of gas";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_OUT_OF_FUND:
      return "out of fund";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_PC_UNDERFLOW:
      return "pc underflow";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_CREATE_EMPTY:
      return "pc underflow";
    case evm::ExitReason_Error::Kind::ExitReason_Error_Kind_OTHER:
      return error.error_string();
    default:;
  }
  return "unknown error";
}

std::string EvmUtils::ExitReasonString(const evm::ExitReason& exit_reason) {
  switch (exit_reason.exit_reason_case()) {
    case evm::ExitReason::ExitReasonCase::kSucceed:
      return "Succeed";
    case evm::ExitReason::ExitReasonCase::kRevert:
      return "Reverted";
    case evm::ExitReason::ExitReasonCase::kFatal:
      switch (exit_reason.fatal().kind()) {
        case evm::ExitReason_Fatal::Kind::
            ExitReason_Fatal_Kind_UNHANDLED_INTERRUPT:
          return "Fatal: unhandled interrupt";
        case evm::ExitReason_Fatal::Kind::ExitReason_Fatal_Kind_NOT_SUPPORTED:
          return "Fatal: not supported";
        case evm::ExitReason_Fatal::Kind::
            ExitReason_Fatal_Kind_CALL_ERROR_AS_FATAL:
          return "Fatal: " + ExitErrorString(exit_reason.fatal().error());
        case evm::ExitReason_Fatal::Kind::ExitReason_Fatal_Kind_OTHER:
          return "Fatal: " + exit_reason.fatal().error_string();
        default:;
      }
      return "Fatal: unknown error";
      break;
    case evm::ExitReason::ExitReasonCase::kError:
      return "Error: " + ExitErrorString(exit_reason.error());
    default:;
  }
  return "Unknown failure";
}

std::string EvmUtils::GetEvmResultJsonFromTextProto(
    const std::string& text_proto) {
  evm::EvmResult result;
  google::protobuf::TextFormat::ParseFromString(text_proto, &result);
  std::string output;
  result.SerializeToString(&output);
  return "\"" + websocketpp::base64_encode(output) + "\"";
}

bool GetEvmEvalExtras(const uint64_t& blockNum, const TxnExtras& extras_in,
                      evm::EvmEvalExtras& extras_out) {
  extras_out.set_block_timestamp(
      extras_in.block_timestamp.convert_to<uint64_t>());
  extras_out.set_block_gas_limit(DS_MICROBLOCK_GAS_LIMIT *
                                 GasConv::GetScalingFactor());
  extras_out.set_block_difficulty(extras_in.block_difficulty);
  extras_out.set_block_number(blockNum);
  uint256_t gasPrice = (extras_in.gas_price * evm::EVM_ZIL_SCALING_FACTOR) /
                       GasConv::GetScalingFactor();
  // The following ensures we get 'at least' that high price as it was before
  // dividing by GasScalingFactor
  gasPrice += evm::EVM_ZIL_SCALING_FACTOR;
  *extras_out.mutable_gas_price() = UIntToProto(gasPrice);
  return true;
}

evm::H256 H256ToProto(const H256& hash) {
  evm::H256 out;
  // H256 is assumed big-endian.
  auto hash_bytes = hash.asArray();
  out.set_x0(boost::endian::load_big_u64(&hash_bytes[0]));
  out.set_x1(boost::endian::load_big_u64(&hash_bytes[8]));
  out.set_x2(boost::endian::load_big_u64(&hash_bytes[16]));
  out.set_x3(boost::endian::load_big_u64(&hash_bytes[24]));
  return out;
}

H256 ProtoToH256(const evm::H256& hash) {
  zbytes buffer(32);
  boost::endian::store_big_u64(&buffer[0], hash.x0());
  boost::endian::store_big_u64(&buffer[8], hash.x1());
  boost::endian::store_big_u64(&buffer[16], hash.x2());
  boost::endian::store_big_u64(&buffer[24], hash.x3());
  return H256(buffer);
}

evm::Address AddressToProto(const Address& address) {
  evm::Address out;
  // Address is assumed big-endian.
  auto address_bytes = address.asArray();
  out.set_x0(boost::endian::load_big_u32(&address_bytes[0]));
  out.set_x1(boost::endian::load_big_u64(&address_bytes[4]));
  out.set_x2(boost::endian::load_big_u64(&address_bytes[12]));
  return out;
}

Address ProtoToAddress(const evm::Address& address) {
  zbytes buffer(20);
  boost::endian::store_big_u32(&buffer[0], address.x0());
  boost::endian::store_big_u64(&buffer[4], address.x1());
  boost::endian::store_big_u64(&buffer[12], address.x2());
  return Address(buffer);
}

uint128_t ProtoToUint(const evm::UInt128& numProto) {
  uint128_t result = numProto.x0();
  result <<= 64;
  result |= numProto.x1();
  return result;
}

uint256_t ProtoToUint(const evm::UInt256& numProto) {
  uint256_t result = numProto.x0();
  result <<= 64;
  result |= numProto.x1();
  result <<= 64;
  result |= numProto.x2();
  result <<= 64;
  result |= numProto.x3();
  return result;
}

evm::UInt128 UIntToProto(const uint128_t& num) {
  evm::UInt128 out;
  out.set_x1(num.convert_to<uint64_t>());
  out.set_x0((num >> 64).convert_to<uint64_t>());
  return out;
}

evm::UInt256 UIntToProto(const uint256_t& num) {
  evm::UInt256 out;
  out.set_x3(num.convert_to<uint64_t>());
  out.set_x2((num >> 64).convert_to<uint64_t>());
  out.set_x1((num >> 128).convert_to<uint64_t>());
  out.set_x0((num >> 192).convert_to<uint64_t>());
  return out;
}
