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

#include <memory>
#include "common/TxnStatus.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/Transaction.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TxnExtras.h"
/* EvmProcessContext
 * *
 * This structure is the holding structure for data about
 * to be passed to the evm-ds processing engine.
 *
 * Balances within Zilliqa Blockchain are  :
 * measured in the smallest accounting unit Qa (or 10^-12 Zil).
 *
 * This Context is targeted at an ETH Evm based engine, therefore
 * storage for this context is in gwei (Ethereum units).
 * Gwei is a denomination of the cryptocurrency ether (ETH),
 * used on the Ethereum network to buy and sell goods and services.
 * · A gwei is one-billionth of one ETH.
 *
 * Incoming Zil/Qa will be converted to Eth/Gwei using the following methodology
 *
 * At the time of writing, MIN_ETH_GAS = 21000, NORMAL_TRAN_GAS = 50;
 * SCALING_FACTOR = MIN_ETH_GAS / NORMAL_TRAN_GAS;
 * Therefore this module uses a scaling factor of 21000/50 or 420
 *
 * This should not be confused with the EVM_ZIL_SCALING_FACTOR which is set at
 * 1000000 in the configuration.
 *
 *
 * */
#include "EvmProcessContext.h"
#include "common/TxnStatus.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"

namespace {

std::string txnIdToString(const TxnHash& txn) {
  std::ostringstream str;
  str << "0x" << txn;
  return str.str();
}
}  // namespace
/*
 *   EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
 *                       const TxnExtras& extras, bool commit = true)
 *   This is the traditional form of the constructor as used by the existing
 *   Zilliqa platform pre-evm for the 8.3 and beyond series.
 *
 */
EvmProcessContext::EvmProcessContext(const uint64_t& blkNum,
                                     const Transaction& txn,
                                     const TxnExtras& extras, bool commit)
    : m_txnCode(txn.GetCode()),
      m_txnData(txn.GetData()),
      m_legacyTxn(txn),
      m_direct(false),
      m_commit(commit),
      m_blockNumber(blkNum) {
  *m_protoData.mutable_address() = AddressToProto(txn.GetToAddr());

  *m_protoData.mutable_origin() = AddressToProto(txn.GetSenderAddr());
  *m_protoData.mutable_code() =
      DataConversion::CharArrayToString(StripEVM(txn.GetCode()));
  *m_protoData.mutable_data() =
      DataConversion::CharArrayToString(txn.GetData());
  m_protoData.set_gas_limit(txn.GetGasLimitEth());
  *m_protoData.mutable_apparent_value() =
      UIntToProto(txn.GetAmountWei().convert_to<uint256_t>());

  *m_protoData.mutable_context() = txnIdToString(txn.GetTranID());
  m_protoData.set_is_static_call(false);

  if (!GetEvmEvalExtras(blkNum, extras, *m_protoData.mutable_extras())) {
    m_status = false;
  }
  m_protoData.set_estimate(false);
  m_protoData.set_enable_cps(ENABLE_CPS);
  // Initialised OK
  m_status = true;
}

/*
 *   EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
 *                       const TxnExtras& extras, bool commit = true)
 *   This is the DirectCall format as used by 8.3 and beyond series.
 *
 */

EvmProcessContext::EvmProcessContext(
    const Address& caller, const Address& contract, const zbytes& code,
    const zbytes& data, const uint64_t& gas, const uint256_t& amount,
    const uint64_t& blkNum, const TxnExtras& extras, std::string_view context,
    bool estimate, bool direct)
    : m_txnCode(code),
      m_txnData(data),
      m_legacyTxn(m_dummyTransaction),
      m_direct(direct),
      m_blockNumber(blkNum) {
  *m_protoData.mutable_address() = AddressToProto(contract);
  *m_protoData.mutable_origin() = AddressToProto(caller);
  *m_protoData.mutable_code() =
      DataConversion::CharArrayToString(StripEVM(code));
  *m_protoData.mutable_data() = DataConversion::CharArrayToString(data);
  m_protoData.set_gas_limit(gas);
  *m_protoData.mutable_apparent_value() = UIntToProto(amount);
  m_protoData.set_estimate(estimate);
  *m_protoData.mutable_context() = context;
  m_protoData.set_is_static_call(false);
  if (!GetEvmEvalExtras(blkNum, extras, *m_protoData.mutable_extras())) {
    m_status = false;
  }
  m_protoData.set_enable_cps(ENABLE_CPS);
}

bool EvmProcessContext::GetCommit() const { return m_commit; }

/*
 * SetCode(const zbytes& code)
 *
 * In the case of a contract_call or non_contract then the contract already
 * exists in the account and the official version from the storage will
 * always be used regardless of what the use has passed to us.
 */

void EvmProcessContext::SetCode(const zbytes& code) {
  *m_protoData.mutable_code() =
      DataConversion::CharArrayToString(StripEVM(code));
}

/*
 * const zbyte& GetCode()
 *
 * get a const ref to the binary code that represents the EVM contract
 */

const zbytes& EvmProcessContext::GetCode() const { return m_txnCode; }

/*
 * const zbyte& GetData()
 *
 * get a const ref to the binary data that usual represents the parameters to
 * the EVM contract
 */

const zbytes& EvmProcessContext::GetData() const { return m_txnData; }

/*  SetContractAddress()
 *
 *  Used within a create contract and must be set by the user when they
 *  actually create a new contract.
 *
 */

void EvmProcessContext::SetContractAddress(const Address& addr) {
  *m_protoData.mutable_address() = AddressToProto(addr);
}

/* GetTranID()
 *
 * GetTransactionId() supplied by transaction
 * Probably useful for debugging
 * */

dev::h256 EvmProcessContext::GetTranID() const {
  return m_legacyTxn.GetTranID();
}

/* GetStatus()
 * returns true when all is good, otherwise Journal
 * contains the log of operations performed.
 * */

const bool& EvmProcessContext::GetStatus() const { return m_status; }

bool EvmProcessContext::GetEstimateOnly() const {
  return m_protoData.estimate();
}

void EvmProcessContext::SetGasLimit(uint64_t gasLimit) {
  m_protoData.set_gas_limit(gasLimit);
}

/*
 * GetEvmArgs()
 *
 * Get the arguments in the format ready for passing to evm
 * must have called generateArgs()
 *
 */

/*
 * Return internal structure populated by call to evm
 */

const evm::EvmResult& EvmProcessContext::GetEvmResult() const {
  return m_evmResult;
}

const evm::EvmArgs& EvmProcessContext::GetEvmArgs() const {
  return m_protoData;
}

/*
 * Return internal structure populated by call to evm
 */

void EvmProcessContext::SetEvmResult(const evm::EvmResult& result) {
  m_evmResult = result;
}

/*
 * SetEvmReceipt(const TransactionReceipt& tr)
 */

void EvmProcessContext::SetEvmReceipt(const TransactionReceipt& tr) {
  m_evmRcpt = tr;
}

/*
 * GetEvmReceipt()
 */

const TransactionReceipt& EvmProcessContext::GetEvmReceipt() const {
  return m_evmRcpt;
}

const uint64_t& EvmProcessContext::GetBlockNumber() { return m_blockNumber; }
