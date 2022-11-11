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
#include "libUtils/EvmCallParameters.h"
#include "libUtils/TxnExtras.h"
#include "libUtils/DataConversion.h"

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
#endif

struct ProcessingParameters {
 public:

  ProcessingParameters(const Transaction& txn, const TxnExtras& extras, bool commit=true)
   :m_contractType(Transaction::GetTransactionType(txn)) {
    m_direct = false;
    m_params.m_caller = DataConversion::CharArrayToString(txn.GetSenderAddr().asBytes());
    m_params.m_contract = DataConversion::CharArrayToString( txn.GetToAddr().asBytes());
    m_params.m_code = DataConversion::CharArrayToString( txn.GetCode());
    m_params.m_data = DataConversion::CharArrayToString( txn.GetData());

    // Not sure of these two right now
    m_params.m_available_gas = 0;
    m_params.m_apparent_value = 0;
    // setters required.
    m_params.m_onlyEstimateGas = false;
    m_extras = std::move(extras);
    m_tranID = txn.GetTranID();
    if (commit) {}
  }

  ProcessingParameters(const EvmCallParameters& params,const TxnExtras& extras,bool commit=true)
  : m_contractType(Transaction::NON_CONTRACT){
    m_direct = true;
    m_extras = std::move(extras);
    m_params = std::move(params);
    if (commit) {}
  }

  bool GetCommit(){
    return m_commit;
  }

  Transaction::ContractType
      GetContractType(){
    return m_contractType;
  }

  dev::h256
      GetTranID(){
    return m_tranID;
  }


 private:
  EvmCallParameters         m_params;


  TxnExtras                 m_extras;



  Transaction::ContractType m_contractType;

  bool  m_direct{false};
  bool  m_commit{false};
  // for tracing purposes
  dev::h256      m_tranID;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSING_H_
