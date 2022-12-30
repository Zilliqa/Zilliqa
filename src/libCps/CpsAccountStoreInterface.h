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

#include <map>
#include <string>
#include <vector>

class EvmProcessContext;

namespace libCps {
struct CpsAccountStoreInterface {
  using Address = dev::h160;
  virtual ~CpsAccountStoreInterface() = default;
  virtual Amount GetBalanceForAccountAtomic(const Address& account) = 0;
  virtual uint64_t GetNonceForAccount(const Address& account) = 0;
  virtual void SetNonceForAccount(const Address& account, uint64_t nonce) = 0;
  virtual bool AccountExists(const Address& account) = 0;
  virtual bool AddAccountAtomic(const Address& accont) = 0;
  virtual bool AccountExistsAtomic(const Address& accont) = 0;
  virtual Address GetAddressForContract(const Address& account,
                                        uint32_t transaction_version) = 0;
  virtual bool IncreaseBalance(const Address& account, Amount amount) = 0;
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
  virtual void AddAddressToUpdateBufferAtomic(const Address& addr) = 0;
  virtual void SetImmutableAtomic(const Address& addr, const zbytes& code,
                                  const zbytes& initData) = 0;
  virtual void SetNonceForAccountAtomic(const Address& account, uint64_t) = 0;
  virtual uint64_t GetNonceForAccountAtomic(const Address& account) = 0;
  virtual void FetchStateDataForContract(
      std::map<std::string, zbytes>& states, const dev::h160& address,
      const std::string& vname, const std::vector<std::string>& indices,
      bool temp) = 0;
};
}  // namespace libCps

#endif /* ZILLIQA_SRC_LIBCPS_CPSACCOUNTSTOREINTERFACE_H_ */