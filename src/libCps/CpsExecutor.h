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

#ifndef ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_
#define ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_

#include "CpsAccountStoreInterface.h"
#include "CpsExecuteResult.h"

class AccountStore;
class EvmProcessContext;

namespace libCps {
class CpsExecutor final {
 public:
  explicit CpsExecutor(CpsAccountStoreInterface& account_store);
  CpsExecuteResult Run(const EvmProcessContext& context);

 private:
  CpsExecuteResult preValidateRun(const EvmProcessContext& context) const;

 private:
  CpsAccountStoreInterface& m_account_store;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_