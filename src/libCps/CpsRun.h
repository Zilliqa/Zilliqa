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

/*
 * CpsRun is a base class used by a concrete Runner. It contains some common
 * methods and fields use by its derivatives.
 */

#ifndef ZILLIQA_SRC_LIBCPS_CPSRUN_H_
#define ZILLIQA_SRC_LIBCPS_CPSRUN_H_

#include <memory>

class TransactionReceipt;

namespace libCps {
struct CpsAccountStoreInterface;
struct CpsExecuteResult;
class CpsRun : public std::enable_shared_from_this<CpsRun> {
 public:
  enum Type {
    Call = 0,
    Create,
    Transfer,
    TrapCreate,
    TrapCall,
    TrapScillaCall
  };
  enum Domain { Evm = 0, Scilla, None };
  CpsRun(CpsAccountStoreInterface& accountStore, Domain domain, Type type)
      : mAccountStore(accountStore), mDomain(domain), mType(type) {}
  virtual ~CpsRun() = default;
  virtual CpsExecuteResult Run(TransactionReceipt& receipt) = 0;
  virtual bool IsResumable() const = 0;
  virtual bool HasFeedback() const = 0;
  virtual void ProvideFeedback(const CpsRun& prevRun,
                               const CpsExecuteResult& results) = 0;
  Type GetType() const { return mType; }
  Domain GetDomain() const { return mDomain; }

 protected:
  CpsAccountStoreInterface& mAccountStore;

 private:
  Domain mDomain;
  Type mType;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSRUN_H_
