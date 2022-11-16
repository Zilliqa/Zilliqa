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
#include "Transaction.h"
#include "common/TxnStatus.h"
#include "libCrypto/EthCrypto.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmCallParameters.h"
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
#include "TransactionReceipt.h"
#include "common/TxnStatus.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"
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
    : m_contractType(Transaction::GetTransactionType(txn)),
      m_direct(false),
      m_commit(commit),
      m_extras(extras),
      m_gasPrice(txn.GetGasPriceQa()),
      m_versionIdentifier(txn.GetVersionIdentifier()) {
  m_ethTransaction = txn.IsEth();
  m_innerData.m_caller = txn.GetSenderAddr();
  m_innerData.m_contract = txn.GetToAddr();
  m_innerData.m_code = txn.GetCode();
  m_innerData.m_data = txn.GetData();
  m_innerData.m_gas = txn.GetGasLimitRaw();
  m_innerData.m_amount = txn.GetAmountRaw();
  m_innerData.m_tranID = txn.GetTranID();
  m_innerData.m_blkNum = blkNum;

  Validate();

  m_onlyEstimate = false;
  m_status = true;
}

/*
 * Validate that the inputTransaction is in good Shape.
 */

bool EvmProcessContext::Validate() {

  if (m_contractType == Transaction::ERROR) {
    m_errorCode = TxnStatus::ERROR;
    m_status = false;
  }
  m_baseFee = GetBaseFee();
  // Calculate how much we need to take as a deposit for transaction.

  if (m_contractType == Transaction::CONTRACT_CREATION) {
    if (GetCode().empty()) {
      m_errorCode = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
      m_status = false;
    }

    // Check if limit is sufficient for creation fee
    if (m_innerData.m_gas < m_baseFee) {
      m_errorCode = TxnStatus::INSUFFICIENT_GAS_LIMIT;
      m_status = false;
    }
  }
  if (!SafeMath<uint256_t>::mul(GetGasLimitZil(), GetGasPriceWei(),
                                m_gasDepositWei)) {
    m_errorCode = TxnStatus::MATH_ERROR;
    m_status = false;
  }

  return m_status;
}
/*
 *   EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
 *                       const TxnExtras& extras, bool commit = true)
 *   This is the DirectCall format as used by 8.3 and beyond series.
 *
 */
EvmProcessContext::EvmProcessContext(const DirectCall& params,
                                     const TxnExtras& extras, bool estimate,
                                     bool commit)
    : m_innerData(params),
      m_direct(true),
      m_commit(commit),
      m_extras(extras),
      m_versionIdentifier(TRANSACTION_VERSION_ETH),
      m_onlyEstimate(estimate) {
  m_ethTransaction = true;
  m_direct = true;
  m_contractType =
      this->GetInternalType(params.m_contract, params.m_code, params.m_data);

  Validate();
}

bool EvmProcessContext::GetCommit() const { return m_commit; }

/*
 * GetEstimateGas()
 *
 * Flag that registers in EstimateMode is set.
 */

bool EvmProcessContext::GetEstimateOnly() const { return m_onlyEstimate; }

/* GetContractType()
 *
 * return the contract type from the transaction
 * This is deduced from looking at code and data fields.
 * */

Transaction::ContractType EvmProcessContext::GetContractType() const {
  return this->GetInternalType(m_innerData.m_contract, m_innerData.m_code,
                               m_innerData.m_data);
}

/*
 * SetCode(const zbytes& code)
 *
 * In the case of a contract_call or non_contract then the contract already
 * exists in the account and the official version from the storage will
 * always be used regardless of what the use has passed to us.
 */

void EvmProcessContext::SetCode(const zbytes& code) {
  m_innerData.m_code = code;
}

/*
 * const zbyte& GetCode()
 *
 * get a const ref to the binary code that represents the EVM contract
 */

const zbytes& EvmProcessContext::GetCode() const { return m_innerData.m_code; }

/*
 * const zbyte& GetData()
 *
 * get a const ref to the binary data that usual represents the parameters to
 * the EVM contract
 */

const zbytes& EvmProcessContext::GetData() const { return m_innerData.m_data; }

/*  SetContractAddress()
 *
 *  Used within a create contract and must be set by the user when they
 *  actually create a new contract.
 *
 */

void EvmProcessContext::SetContractAddress(const Address& addr) {
  m_innerData.m_contract = addr;
}

/*
 * GetContractAddress()
 */

const Address& EvmProcessContext::GetContractAddress() const {
  return m_innerData.m_contract;
}

/* GetTranID()
 *
 * GetTransactionId() supplied by transaction
 * Probably useful for debugging
 * */

dev::h256 EvmProcessContext::GetTranID() const { return m_innerData.m_tranID; }

/* GetStatus()
 * returns true when all is good, otherwise Journal
 * contains the log of operations performed.
 * */

const bool& EvmProcessContext::GetStatus() const { return m_status; }

/*
 * GetJournal()
 * returns a journal of operations performed and final error if a failure
 * caused a bad status
 */

const std::vector<std::string>& EvmProcessContext::GetJournal() const {
  return m_journal;
}

/*
 * GetGasDeposit()
 * returns the GasDeposit calculated from the input parameters
 * for transactions :
 * txn.GetGasLimitZil() * txn.GetGasPriceWei();
 *
 * for direct :
 */

const uint256_t& EvmProcessContext::GetGasDeposit() const {
  return m_gasDepositWei;
}

/*
 * GetBlockNumber()
 *
 * returns the Block number as passed in by the EvmMessage
 */

const uint64_t& EvmProcessContext::GetBlockNumber() const {
  return m_innerData.m_blkNum;
}

/*
 * GetSenderAddress()
 *
 * returns the Address of the Sender of The message passed in by the
 * EvmMessage
 */

const Address& EvmProcessContext::GetSenderAddress() const {
  return m_innerData.m_caller;
}

/*
 * GetGasLimit()
 *
 * return GasLimit in Eth
 */

uint64_t EvmProcessContext::GetGasLimitEth() const {
  if (m_ethTransaction) {
    return m_innerData.m_gas;
  }
  return GasConv::GasUnitsFromCoreToEth(m_innerData.m_gas);
}

uint64_t EvmProcessContext::GetGasLimitRaw() const {
  return this->m_innerData.m_gas;
}

/*
 * GetGasLimitZil()
 *
 * limit in zil as the name suggests
 */

uint64_t EvmProcessContext::GetGasLimitZil() const {
  if (m_ethTransaction) {
    return GasConv::GasUnitsFromEthToCore(m_innerData.m_gas);
  }
  return m_innerData.m_gas;
}

/*
 * GetAmountWei()
 *
 * as name implies
 */

const uint256_t EvmProcessContext::GetAmountWei() const {
  if (m_ethTransaction) {
    return m_innerData.m_amount;
  } else {
    // We know the amounts in transactions are capped, so it won't overlow.
    return m_innerData.m_amount * EVM_ZIL_SCALING_FACTOR;
  }
}

const uint128_t& EvmProcessContext::GetGasPriceRaw() const {
  return m_gasPrice;
}

/*
 * GetGasPriceWei()
 */

const uint128_t EvmProcessContext::GetGasPriceWei() const {
  if (m_ethTransaction) {
    return m_gasPrice;
  } else {
    return m_gasPrice * EVM_ZIL_SCALING_FACTOR / GasConv::GetScalingFactor();
  }
}

/*
 * GetAmountQa()
 */

const uint128_t EvmProcessContext::GetAmountQa() const {
  if (m_ethTransaction) {
    return m_innerData.m_amount.convert_to<uint128_t>() /
           EVM_ZIL_SCALING_FACTOR;
  } else {
    return m_innerData.m_amount.convert_to<uint128_t>();
  }
}

/*
 * GetVersionIdentifier()
 */

const uint32_t& EvmProcessContext::GetVersionIdentifier() const {
  return m_versionIdentifier;
}

/*
 * GetBaseFee()
 */

const uint64_t& EvmProcessContext::GetBaseFee() {
  m_baseFee = Eth::getGasUnitsForContractDeployment(GetCode(), GetData());
  return m_baseFee;
}

/*
 * GetEvmArgs()
 *
 * Get the arguments in the format ready for passing to evm
 * must have called generateArgs()
 *
 */

evm::EvmArgs EvmProcessContext::GetEvmArgs() {
  evm::EvmArgs args;
  GenerateEvmArgs(args);
  return args;
}

/*
 * Return internal structure populated by call to evm
 */

const evm::EvmResult& EvmProcessContext::GetEvmResult() const {
  return m_evmResult;
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

bool EvmProcessContext::GenerateEvmArgs(evm::EvmArgs& arg) {
  *arg.mutable_address() = AddressToProto(m_innerData.m_contract);
  *arg.mutable_origin() = AddressToProto(m_innerData.m_caller);
  *arg.mutable_code() =
      DataConversion::CharArrayToString(StripEVM(m_innerData.m_code));
  *arg.mutable_data() = DataConversion::CharArrayToString(m_innerData.m_data);
  arg.set_gas_limit(GetGasLimitEth());
  *arg.mutable_apparent_value() = UIntToProto(GetAmountWei());
  arg.set_estimate(true);
  if (!GetEvmEvalExtras(m_innerData.m_blkNum, m_extras,
                        *arg.mutable_extras())) {
    std::ostringstream stringStream;
    stringStream.clear();
    stringStream << "Call to GetEvmExtraValues has failed";
    m_journal.push_back(stringStream.str());
    m_status = false;
    return false;
  }
  return true;
}
/*
 * Determine the type of call that is required by Evm Processing
 *
 * This is copied from the transaction class
 *
 * static ContractType GetTransactionType(const Transaction& tx) {
 * auto const nullAddr = IsNullAddress(tx.GetToAddr());
 *
 * if ((!tx.GetData().empty() && !nullAddr) && tx.GetCode().empty()) {
 *    return CONTRACT_CALL;
 * }
 *
 * if (!tx.GetCode().empty() && nullAddr) {
 *    return CONTRACT_CREATION;
 * }
 *
 * if ((tx.GetData().empty() && !nullAddr) && tx.GetCode().empty()) {
 *     return NON_CONTRACT;
 * }
 *
 * return ERROR;
 *}
 *
 */
Transaction::ContractType EvmProcessContext::GetInternalType(
    const Address& contractAddr, const zbytes& code, const zbytes& data) const {
  auto const nullAddr = IsNullAddress(contractAddr);

  if ((not data.empty() && not nullAddr) && code.empty()) {
    return Transaction::CONTRACT_CALL;
  }

  if (not code.empty() && nullAddr) {
    return Transaction::CONTRACT_CREATION;
  }

  if ((data.empty() && not nullAddr) && code.empty()) {
    return Transaction::NON_CONTRACT;
  }

  return Transaction::ERROR;
}
