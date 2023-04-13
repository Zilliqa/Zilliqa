/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBCPS_CPSRUNSCILLA_H_
#define ZILLIQA_SRC_LIBCPS_CPSRUNSCILLA_H_

#include "libCps/Amount.h"
#include "libCps/CpsExecuteResult.h"
#include "libCps/CpsRun.h"

#include <json/json.h>
#include <variant>

class TransactionReceipt;

namespace libCps {
class Address;
struct CpsAccountStoreInterface;
struct CpsContext;
class CpsExecutor;

struct ScillaInvokeResult {
  bool isSuccess = false;
  std::string returnVal;
};

struct ScillaArgs {
  using Address = dev::h160;
  struct CodeData {
    zbytes code;
    zbytes data;
  };
  Address from;
  Address dest;
  Address origin;
  Amount value;
  std::variant<CodeData, Json::Value> calldata;
  uint32_t edge = 0;
  uint32_t depth = 0;
  uint64_t gasLimit = 0;
};

class CpsRunScilla final : public CpsRun {
  using Address = dev::h160;

 public:
  CpsRunScilla(ScillaArgs args, CpsExecutor& executor, const CpsContext& ctx,
               CpsRun::Type type);
  virtual CpsExecuteResult Run(TransactionReceipt& receipt) override;
  void ProvideFeedback(const CpsRun& /* previousRun */,
                       const CpsExecuteResult& /* results */) override {}
  bool IsResumable() const override { return false; }
  bool HasFeedback() const override {
    return GetType() == CpsRun::TrapScillaCall;
  }

 private:
  enum class INVOKE_TYPE {
    CHECKER = 0,
    RUNNER_CREATE,
    RUNNER_CALL,
    DISAMBIGUATE
  };
  ScillaInvokeResult InvokeScillaInterpreter(INVOKE_TYPE type);
  CpsExecuteResult checkGas();
  CpsExecuteResult runCreate(TransactionReceipt& receipt);
  CpsExecuteResult runCall(TransactionReceipt& receipt);

 private:
  ScillaArgs mArgs;
  CpsExecutor& mExecutor;
  const CpsContext& mCpsContext;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSRUNSCILLA_H_
