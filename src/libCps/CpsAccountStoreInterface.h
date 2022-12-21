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

class EvmProcessContext;

namespace libCps {
struct CpsAccountStoreInterface {
  using Address = dev::h160;
  virtual ~CpsAccountStoreInterface() = default;
  virtual Amount GetBalanceForAccount(const Address& account) = 0;
  virtual uint64_t GetNonceForAccount(const Address& account) = 0;
  virtual bool AccountExists(const Address& account) = 0;
  virtual bool AddAccountAtomic(const Address& accont) = 0;
  virtual Address GetAddressForContract(const Address& account,
                                        uint32_t transaction_version) = 0;
  virtual bool IncreaseBalance(const Address& account, Amount amount) = 0;
  virtual bool DecreaseBalance(const Address& account, Amount amount) = 0;
  virtual bool TransferBalanceAtomic(const Address& from, const Address& to,
                                     Amount amount) = 0;
  virtual void DiscardAtomics() = 0;
  virtual void CommitAtomics() = 0;
  virtual void UpdateStates(const Address& addr,
                            const std::map<std::string, zbytes>& states,
                            const std::vector<std::string>& toDeleteIndices,
                            bool temp, bool revertible) = 0;
};
}  // namespace libCps

#endif /* ZILLIQA_SRC_LIBCPS_CPSACCOUNTSTOREINTERFACE_H_ */