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
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmCallParameters.h"
#include "libUtils/GasConv.h"
#include "libUtils/TxnExtras.h"
#include "libUtils/SafeMath.h"
#include "libCrypto/EthCrypto.h"

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
  uint64_t m_available_gas = {0};
  boost::multiprecision::uint256_t m_apparent_value = {0};
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

  ProcessingParameters(const uint64_t& blkNum,const Transaction& txn, const TxnExtras& extras, bool commit=true)
   :m_contractType(Transaction::GetTransactionType(txn)),m_blkNum(blkNum) {
    m_direct = false;
    m_caller = txn.GetSenderAddr();
    m_contract = txn.GetToAddr();
    m_code = txn.GetCode();
    m_data = txn.GetData();
    m_available_gas = txn.GetGasLimitEth();
    m_apparent_value = txn.GetAmountWei().convert_to<uint256_t>();
    m_tranID = txn.GetTranID();
    std::ostringstream stringStream;

    // We charge for creating a contract, this is included in our base fee.

    if (m_contractType == Transaction::CONTRACT_CREATION) {
      if (txn.GetCode().empty()) {
        m_errorCode = TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION;
        m_status = false;
        stringStream.clear();
        stringStream << "Jrn:" << "Cannot create a contract with empty code";
        m_journal.push_back(stringStream.str());
        return;
      }

      m_baseFee =
          Eth::getGasUnitsForContractDeployment(txn.GetCode(), txn.GetData());

      stringStream.clear();
      stringStream << "Base Fee " << m_baseFee << " : gwei";
      m_journal.push_back(stringStream.str());
      
      
      
      // Check if limit is sufficient for creation fee
      if (m_available_gas < m_baseFee) {
        m_errorCode = TxnStatus::INSUFFICIENT_GAS_LIMIT;
        m_status = false;
        stringStream.clear();
        stringStream << "Err:" << "Gas " << txn.GetGasLimitEth() << " less than Base Fee " << m_baseFee;
        m_journal.push_back(stringStream.str());
        return;
      }
    }
    
    // Calculate how much we need to take as a deposit for transaction.
    
    if (!SafeMath<uint256_t>::mul(txn.GetGasLimitZil(),
                                  txn.GetGasPriceWei(), m_gasDepositWei)) {
      m_errorCode = TxnStatus::MATH_ERROR;
      m_status = false;
      return ;
    }
    
    stringStream.clear();
    stringStream << "Jrn:" << "Gas Deposit Fee " << m_baseFee << " : gwei";
    m_journal.push_back(stringStream.str());
    

    // setters required.
    m_onlyEstimateGas = false;
    //
    // TxnExtras is an Evn Soecific entity is therefore supplied in
    // gwei
    //
    m_extras = std::move(extras);

    if (this->GenerateEvmArgs()){
      stringStream.clear();
      stringStream << "Generated Evm Args";
      m_journal.push_back(stringStream.str());
    } else {
      stringStream.clear();
      stringStream << "Failed Generating Evm Args";
      m_journal.push_back(stringStream.str());
      m_status = false;
      return ;
    }

    m_status = true;
    if (commit) {}
  }

  ProcessingParameters(const EvmCallParameters& params,const TxnExtras& extras,bool commit=true)
  : m_contractType(Transaction::NON_CONTRACT){
    m_direct = true;
    m_extras = std::move(extras);
    if (commit) {}
    if (params.m_onlyEstimateGas) {}
  }

  bool GetCommit(){
    return m_commit;
  }

  /* GetContractType()
   *
   * return the contract type from the transaction
   * This is deduced from looking at code and data fields.
   * */

  Transaction::ContractType
      GetContractType(){
    return m_contractType;
  }

  /*  SetContractAddress()
   *
   *  Used within a create contract and must be set by the user when they
   *  actually create a new contract.
   *
   */

  void SetContractAddress(const Address& addr){
    m_contract = addr;
  }

  /* GetTranID()
   *
   * GetTransactionId() supplied by transaction
   * Probably useful for debugging
   * */

  dev::h256
      GetTranID(){
    return m_tranID;
  }

 /* GetStatus()
  * returns true when all is good, otherwise Journal
  * contains the log of operations performed.
  * */

  const bool& GetStatus(){
    return m_status;
  }

  /*
   * GetJournal()
   * returns a journal of operations performed and final error if a failure
   * caused a bad status
   */

  const std::vector<std::string>&
      GetJournal(){
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
  
  const  uint256_t& GetGasDeposit(){
    return m_gasDepositWei;
  }

  /*
   * GetEvmArgs()
   *
   * Get the arguments in the format ready for passing to evm
   * must have called generateArgs()
   *
   */

  const evm::EvmArgs&
  GetEvmArgs() {
    std::ostringstream stringStream;
    if (this->GenerateEvmArgs()){
      stringStream.clear();
      stringStream << "Generated Evm Args";
      m_journal.push_back(stringStream.str());
    } else {
      stringStream.clear();
      stringStream << "Failed Generating Evm Args";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    return m_evmArgs;
  }

  /*
   * Diagnostic routines used in development and verification process
   */

  bool
      CompareEvmArgs(const evm::EvmArgs& actual,const evm::EvmArgs& expected){
    std::ostringstream stringStream;
    m_status = true;
    if (actual.code() != expected.code()){
      stringStream.clear();
      stringStream << "code different " << actual.code().data() << " expected " << expected.code().data();
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if (actual.data() != expected.data()){
      stringStream.clear();
      stringStream << "data different";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if(actual.address().SerializeAsString() != expected.address().SerializeAsString()){
      stringStream.clear();
      stringStream << "address different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if(actual.origin().SerializeAsString() != expected.origin().SerializeAsString()){
      stringStream.clear();
      stringStream << "origin different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if(actual.apparent_value().SerializeAsString() != expected.apparent_value().SerializeAsString()){
      stringStream.clear();
      stringStream << "aparaent value different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    if(actual.gas_limit() != expected.gas_limit()){
      stringStream.clear();
      stringStream << "gas value different ";
      m_journal.push_back(stringStream.str());
      m_status = false;
    }
    return m_status;
  }


 private:

  bool GenerateEvmArgs(){
    *m_evmArgs.mutable_address() = AddressToProto(m_contract);
    *m_evmArgs.mutable_origin() = AddressToProto(m_caller);
    *m_evmArgs.mutable_code() = DataConversion::CharArrayToString(StripEVM(m_code));
    *m_evmArgs.mutable_data() =DataConversion::CharArrayToString(m_data);
    m_evmArgs.set_gas_limit(this->m_available_gas);
    *m_evmArgs.mutable_apparent_value() = UIntToProto(m_apparent_value);
    if (!GetEvmEvalExtras(m_blkNum, m_extras, *m_evmArgs.mutable_extras())) {
      std::ostringstream stringStream;
      stringStream.clear();
      stringStream << "Call to GetEvmExtraValues has failed";
      m_journal.push_back(stringStream.str());
      m_status = false;
      return false;
    }
    return true;
  }



  TxnExtras                 m_extras;
  Transaction::ContractType m_contractType;

  bool  m_direct{false};
  bool  m_commit{false};
  Address       m_caller;
  Address       m_contract;
  zbytes        m_code;
  zbytes        m_data;
  uint64_t m_available_gas = {0};
  boost::multiprecision::uint256_t m_apparent_value = {0};
  // for tracing purposes
  dev::h256      m_tranID;
  uint64_t       m_baseFee{0};
  int            m_errorCode;
  bool           m_status{true};
  std::vector<std::string>  m_journal;
  uint256_t      m_gasDepositWei;
  evm::EvmArgs   m_evmArgs;
  uint64_t       m_blkNum{0};
  bool           m_onlyEstimateGas{false};
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSING_H_
