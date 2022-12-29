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

#ifndef ZILLIQA_SRC_LIBCPS_CPSRUN_H_
#define ZILLIQA_SRC_LIBCPS_CPSRUN_H_

class TransactionReceipt;

namespace libCps {
class CpsAccountStoreInterface;
class CpsRun {
 public:
  virtual ~CpsRun() = default;
  virtual CpsExecuteResult Run(CpsAccountStoreInterface& account_store,
                               TransactionReceipt& receipt) = 0;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSRUN_H_