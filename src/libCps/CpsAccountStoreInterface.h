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

#ifndef ZILLIQA_SRC_LIBCPS_CPSACCOUNTSTOREINTERFACE_H_
#define ZILLIQA_SRC_LIBCPS_CPSACCOUNTSTOREINTERFACE_H_

#include "common/FixedHash.h"
#include "libCps/Amount.h"

#include <condition_variable>
#include <map>
#include <string>
#include <vector>

class EvmProcessContext;

namespace libCps {
struct CpsAccountStoreInterface {
  enum AccountType { DoesNotExist = 0, EOA, Contract, Library, Unknown };
  using Address = dev::h160;
  virtual ~CpsAccountStoreInterface() = default;
  virtual Amount GetBalanceForAccountAtomic(const Address& account) = 0;
  virtual uint64_t GetNonceForAccount(const Address& account) = 0;
  virtual bool AddAccountAtomic(const Address& accont) = 0;
  virtual bool AccountExistsAtomic(const Address& accont) = 0;
  virtual Address GetAddressForContract(const Address& account,
                                        uint32_t transaction_version) = 0;
  virtual bool IncreaseBalanceAtomic(const Address& account, Amount amount) = 0;
  virtual bool DecreaseBalanceAtomic(const Address& account, Amount amount) = 0;
  virtual bool DecreaseBalance(const Address& account, Amount amount) = 0;
  virtual void SetBalanceAtomic(const Address& account, Amount amount) = 0;
  virtual bool TransferBalanceAtomic(const Address& from, const Address& to,
                                     Amount amount) = 0;
  virtual void DiscardAtomics() = 0;
  virtual void CommitAtomics() = 0;
  virtual bool UpdateStates(const Address& addr,
                            const std::map<std::string, zbytes>& t_states,
                            const std::vector<std::string>& toDeleteIndices,
                            bool temp, bool revertible = false) = 0;
  virtual bool UpdateStateValue(const Address& addr, const zbytes& q,
                                unsigned int q_offset, const zbytes& v,
                                unsigned int v_offset) = 0;
  virtual std::string GenerateContractStorageKey(
      const Address& addr, const std::string& key,
      const std::vector<std::string>& indices) = 0;
  virtual void AddAddressToUpdateBufferAtomic(const Address& addr) = 0;
  virtual void SetImmutableAtomic(const Address& addr, const zbytes& code,
                                  const zbytes& initData) = 0;
  virtual void IncreaseNonceForAccountAtomic(const Address& account) = 0;
  virtual void IncreaseNonceForAccount(const Address& address) = 0;
  virtual uint64_t GetNonceForAccountAtomic(const Address& account) = 0;
  virtual void FetchStateDataForContract(
      std::map<std::string, zbytes>& states, const dev::h160& address,
      const std::string& vname, const std::vector<std::string>& indices,
      bool temp) = 0;
  virtual void BufferCurrentContractStorageState() = 0;
  virtual void RevertContractStorageState() = 0;
  virtual zbytes GetContractCode(const Address& account) = 0;

  // Scilla specifics
  virtual bool GetContractAuxiliaries(const Address& account, bool& is_library,
                                      uint32_t& scilla_version,
                                      std::vector<Address>& extlibs) = 0;
  virtual zbytes GetContractInitData(const Address& account) = 0;
  virtual std::string& GetScillaRootVersion() = 0;
  virtual bool IsAccountALibrary(const Address& address) = 0;
  virtual std::condition_variable& GetScillaCondVariable() = 0;
  virtual std::mutex& GetScillaMutex() = 0;
  virtual bool GetProcessTimeout() const = 0;
  virtual bool InitContract(const Address& address, const zbytes& code,
                            const zbytes& data, uint64_t blockNum) = 0;
  virtual bool SetBCInfoProvider(uint64_t blockNum, uint64_t dsBlockNum,
                                 const Address& origin,
                                 const Address& destAddress,
                                 uint32_t scillaVersion) = 0;
  virtual void MarkNewLibraryCreated(const Address& address) = 0;
  virtual CpsAccountStoreInterface::AccountType GetAccountType(
      const Address& address) = 0;
  virtual bool isAccountEvmContract(const Address& address) const = 0;
};
}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSACCOUNTSTOREINTERFACE_H_
