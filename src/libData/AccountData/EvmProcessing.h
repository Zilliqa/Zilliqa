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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSING_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSING_H_

#include <memory>
#include "Transaction.h"
#include "libCrypto/EthCrypto.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TxnExtras.h"

#ifdef FOR_EXAMPLE_ONLY
struct tx {
  uint32_t version{};
  uint64_t nonce{};  // counter: the number of tx from m_fromAddr
  Address toAddr;
  PubKey senderPubKey;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit{};
  zbytes code;
  zbytes data;
};

struct ep {
  std::string m_contract;
  std::string m_caller;
  std::string m_code;
  std::string m_data;
  uint64_t m_gas = {0};
  boost::multiprecision::uint256_t m_amount = {0};
  ee m_extras;
  bool m_onlyEstimateGas = false;
};

struct ee {
  uint128_t block_timestamp{};
  uint64_t block_gas_limit{};
  uint64_t block_difficulty{};
  uint64_t block_number{};
  std::string gas_price{};
};
class GasConv {
 public:
  static uint64_t GasUnitsFromEthToCore(uint64_t gasLimit) {
    return gasLimit / GetScalingFactor();
  }

  static uint64_t GasUnitsFromCoreToEth(uint64_t gasLimit) {
    return gasLimit * GetScalingFactor();
  }

  static uint64_t GetScalingFactor() { return MIN_ETH_GAS / NORMAL_TRAN_GAS; }
};

#endif

/* ProcessingParameters
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

#include "common/TxnStatus.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"

struct ProcessingParameters {
 public:
  //
  // DirectCall is the internal call format used by Zilliqa implementations
  // particularly in the eth library
  //
  struct DirectCall {
    Address m_caller;
    Address m_contract;
    zbytes m_code;
    zbytes m_data;
    uint64_t m_gas = {0};
    uint128_t m_amount = {0};
    // for tracing purposes
    dev::h256 m_tranID;
    uint64_t m_blkNum{0};
    bool m_onlyEstimateGas{false};
  };
  /*
   *   ProcessingParameters(const uint64_t& blkNum, const Transaction& txn,
   *                       const TxnExtras& extras, bool commit = true)
   *   This is the traditional form of the constructor as used by the existing
   *   Zilliqa platform pre-evm for the 8.3 and beyond series.
   *
   */
  ProcessingParameters(const uint64_t& blkNum, const Transaction& txn,
                       const TxnExtras& extras, bool commit = true)
      :
        m_contractType(Transaction::GetTransactionType(txn)),
        m_direct(false),
        m_extras(extras),
        m_gasPrice(extras.gas_price),
        m_versionIdentifier(txn.GetVersionIdentifier()){

    m_ethTransaction = txn.IsEth();
    _internal.m_caller = txn.GetSenderAddr();
    _internal.m_contract = txn.GetToAddr();
    _internal.m_code = txn.GetCode();
    _internal.m_data = txn.GetData();
    _internal.m_gas = txn.GetGasLimitRaw();
    _internal.m_amount = txn.GetAmountRaw();
    _internal.m_tranID = txn.GetTranID();
    _internal.m_blkNum = blkNum;

    std::ostringstream stringStream;

    // We charge for creating a contract, this is included in our base fee.

    if (m_contractType == Transaction::CONTRACT_CREATION) {


      if (txn.GetCode().empty()) {
        m_errorCode = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        m_status = false;
        stringStream.clear();
        stringStream << "Jrn:"
                     << "Cannot create a contract with empty code";
        m_journal.push_back(stringStream.str());
        return;
      }

      m_baseFee =
          Eth::getGasUnitsForContractDeployment(txn.GetCode(), txn.GetData());
      stringStream.clear();
      stringStream << "Base Fee " << m_baseFee << " : gwei" << std::endl;
      m_journal.push_back(stringStream.str());

      // Check if limit is sufficient for creation fee
      if (_internal.m_gas < m_baseFee) {
        m_errorCode = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        m_status = false;
        stringStream.clear();
        stringStream << "Err:"
                     << "Gas " << txn.GetGasLimitEth() << " less than Base Fee "
                     << m_baseFee;
        m_journal.push_back(stringStream.str());
        return;
      }
    }

    // Calculate how much we need to take as a deposit for transaction.

    if (!SafeMath<uint256_t>::mul(txn.GetGasLimitZil(), txn.GetGasPriceWei(),
                                  m_gasDepositWei)) {
      m_errorCode = TxnStatus::MATH_ERROR;
      m_status = false;
      return;
    }

    stringStream.clear();
    stringStream << "Jrn:"
                 << "Gas Deposit Fee " << m_baseFee << " : gwei";
    m_journal.push_back(stringStream.str());

    // setters required.
    _internal.m_onlyEstimateGas = not commit;
    m_status = true;
    m_commit = commit;
  }
  /*
   *   ProcessingParameters(const uint64_t& blkNum, const Transaction& txn,
   *                       const TxnExtras& extras, bool commit = true)
   *   This is the DirectCall format as used by 8.3 and beyond series.
   *
   */
  ProcessingParameters(const DirectCall& params, const TxnExtras& extras,
                       bool commit = true)
                               :_internal(params), m_extras(extras) {
    m_contractType =
        this->GetInternalType(params.m_contract, params.m_code, params.m_data);
    m_direct = true;
    // maybe able to std::move these after testing complete
    if (commit) {
    }
    if (params.m_onlyEstimateGas) {
    }
  }

  bool GetCommit() { return m_commit; }

  /* GetContractType()
   *
   * return the contract type from the transaction
   * This is deduced from looking at code and data fields.
   * */

  Transaction::ContractType GetContractType() { return m_contractType; }

  /*
   * SetCode(const zbytes& code)
   *
   * In the case of a contract_call or non_contract then the contract already
   * exists in the account and the official version from the storage will
   * always be used regardless of what the use has passed to us.
   */

  void SetCode(const zbytes& code) {
    // Todo, make sure that this is copyable and possibly std::move it for
    // efficiency.
    _internal.m_code = code;
  }

  /*
   * const zbyte& GetCode()
   *
   * get a const ref to the binary code that represents the EVM contract
   */

  const zbytes& GetCode() { return _internal.m_code; }

  /*
   * const zbyte& GetData()
   *
   * get a const ref to the binary data that usual represents the parameters to
   * the EVM contract
   */

  const zbytes& GetData() { return _internal.m_data; }

  /*  SetContractAddress()
   *
   *  Used within a create contract and must be set by the user when they
   *  actually create a new contract.
   *
   */

  void SetContractAddress(const Address& addr) { _internal.m_contract = addr; }

  /*
   * GetContractAddress()
   */

  const Address& GetContractAddress() {
    return _internal.m_contract;
  }

  /* GetTranID()
   *
   * GetTransactionId() supplied by transaction
   * Probably useful for debugging
   * */



  dev::h256 GetTranID() { return _internal.m_tranID; }

  /* GetStatus()
   * returns true when all is good, otherwise Journal
   * contains the log of operations performed.
   * */

  const bool& GetStatus() { return m_status; }

  /*
   * GetJournal()
   * returns a journal of operations performed and final error if a failure
   * caused a bad status
   */

  const std::vector<std::string>& GetJournal() { return m_journal; }

  /*
   * GetGasDeposit()
   * returns the GasDeposit calculated from the input parameters
   * for transactions :
   * txn.GetGasLimitZil() * txn.GetGasPriceWei();
   *
   * for direct :
   */

  const uint256_t& GetGasDeposit() { return m_gasDepositWei; }

  /*
   * GetBlockNumber()
   *
   * returns the Block number as passed in by the EvmMessage
   */

  const uint64_t& GetBlockNumber() { return _internal.m_blkNum; }

  /*
   * GetSenderAddress()
   *
   * returns the Address of the Sender of The message passed in by the
   * EvmMessage
   */

  const Address& GetSenderAddress() { return _internal.m_caller; }

  /*
   * GetGasLimit()
   *
   * return GasLimit in Eth
   */

  uint64_t GetGasLimitEth() const {
      if (m_ethTransaction) {
        return _internal.m_gas;
      }
      return GasConv::GasUnitsFromCoreToEth(_internal.m_gas);
  }

  uint64_t GetGasLimitRaw() const {
    return this->_internal.m_gas;
  }

  /*
   * GetGasLimitZil()
   *
   * limit in zil as the name suggests
   */

  uint64_t GetGasLimitZil() const {
    if (m_ethTransaction) {
      return GasConv::GasUnitsFromEthToCore(_internal.m_gas);
    }
    return _internal.m_gas;
  }

  /*
   * GetAmountWei()
   *
   * as name implies
   */

  const uint128_t GetAmountWei() const {
    if (m_ethTransaction) {
      return _internal.m_amount;
    } else {
      // We know the amounts in transactions are capped, so it won't overlow.
      return _internal.m_amount * EVM_ZIL_SCALING_FACTOR;
    }
  }

  const uint128_t& GetGasPriceRaw() const {
    return m_gasPrice;
  }

  /*
   * GetGasPriceWei()
   */

  const uint128_t GetGasPriceWei() const {
    if (m_ethTransaction) {
      return m_gasPrice;
    } else {
      // We know the amounts in transactions are capped, so it won't overlow.
      return m_gasPrice * EVM_ZIL_SCALING_FACTOR /
             GasConv::GetScalingFactor();
    }
  }

  /*
   * GetAmountQa()
   */

  const uint128_t GetAmountQa() const {
    if (m_ethTransaction) {
      return _internal.m_amount.convert_to<uint128_t>() / EVM_ZIL_SCALING_FACTOR;
    } else {
      return _internal.m_amount.convert_to<uint128_t>();
    }
  }

  /*
   * GetVersionIdentifier()
   */

  const uint32_t& GetVersionIdentifier(){
    return m_versionIdentifier;
  }

  /*
   * GetBaseFee()
   */

  const uint64_t& GetBaseFee(){
    return m_baseFee;
  }

  /*
   * GetEvmArgs()
   *
   * Get the arguments in the format ready for passing to evm
   * must have called generateArgs()
   *
   */

  const evm::EvmArgs GetEvmArgs() {
    evm::EvmArgs args;
    std::ostringstream stringStream;
    if (this->GenerateEvmArgs(args)) {
      stringStream.clear();
      stringStream << "Generated Evm Args";
      m_journal.push_back(stringStream.str());
    } else {
      stringStream.clear();
      stringStream << "Failed Generating Evm Args";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    return args;
  }

  /*
   * Diagnostic routines used in development and verification process
   * Do not delete these, they have proofed themselves many times.
   */

  bool CompareEvmArgs(const evm::EvmArgs& actual,
                      const evm::EvmArgs& expected) {
    std::ostringstream stringStream;
    m_status = true;
    if (actual.code() != expected.code()) {
      stringStream.clear();
      stringStream << "code different " << actual.code().data() << " expected "
                   << expected.code().data();
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.data() != expected.data()) {
      stringStream.clear();
      stringStream << "data different";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.address().SerializeAsString() !=
        expected.address().SerializeAsString()) {
      stringStream.clear();
      stringStream << "address different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.origin().SerializeAsString() !=
        expected.origin().SerializeAsString()) {
      stringStream.clear();
      stringStream << "origin different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.gas_limit() != expected.gas_limit()) {
      stringStream.clear();
      stringStream << "gas value different actual " << actual.gas_limit() << ":" << expected.gas_limit();
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.estimate() != expected.estimate()) {
      stringStream.clear();
      stringStream << "estimate different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    return m_status;
  }

  /*
   * Return internal structure populated by call to evm
   */

  const evm::EvmResult& GetEvmResult() { return m_evmResult; }

  /*
   * Return internal structure populated by call to evm
   */

  void SetEvmResult(const evm::EvmResult& result) {
    // TODO - once tested turn into std::move
    m_evmResult = result;
  }

 private:
  bool GenerateEvmArgs(evm::EvmArgs &arg) {
    *arg.mutable_address() = AddressToProto(_internal.m_contract);
    *arg.mutable_origin() = AddressToProto(_internal.m_caller);
    *arg.mutable_code() =
        DataConversion::CharArrayToString(StripEVM(_internal.m_code));
    *arg.mutable_data() =
        DataConversion::CharArrayToString(_internal.m_data);
    arg.set_gas_limit(GetGasLimitEth());
    *arg.mutable_apparent_value() =
        UIntToProto(GetAmountWei().convert_to<uint256_t>());

    if (!GetEvmEvalExtras(_internal.m_blkNum, m_extras,
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
   */
  Transaction::ContractType GetInternalType(const Address& contractAddr,
                                            const zbytes& code,
                                            const zbytes& data) {
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

 private:
  DirectCall _internal;
  Transaction::ContractType m_contractType;
  bool m_direct{false};
  bool m_commit{false};
  uint64_t m_baseFee{0};
  int m_errorCode;
  bool m_status{true};
  TxnExtras  m_extras;
  std::vector<std::string> m_journal;
  uint256_t m_gasDepositWei;
  uint128_t m_gasPrice;
  uint32_t  m_versionIdentifier;
  /*
   * For those folks that really need to know the internal business
   */
  evm::EvmResult m_evmResult;
  bool m_ethTransaction{false};
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSING_H_
