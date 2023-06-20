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

#ifndef ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_
#define ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_

#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/GasConv.h"

namespace libCps {
struct GasTracker {
 public:
  static GasTracker CreateFromEth(uint64_t ethGas) {
    GasTracker tracker;
    tracker.m_gasCore = GasConv::GasUnitsFromEthToCore(ethGas);
    tracker.m_ethRemainder = ethGas % GasConv::GetScalingFactor();
    return tracker;
  }

  static GasTracker CreateFromCore(uint64_t coreGas) {
    GasTracker tracker;
    tracker.m_gasCore = coreGas;
    return tracker;
  }
  void DecreaseByCore(uint64_t coreVal) {
    m_gasCore = m_gasCore > coreVal ? m_gasCore - coreVal : 0;
  }
  void DecreaseByEth(uint64_t ethVal) {
    uint64_t absolute =
        GasConv::GasUnitsFromCoreToEth(m_gasCore) + m_ethRemainder;
    absolute = absolute > ethVal ? absolute - ethVal : 0;
    m_gasCore = GasConv::GasUnitsFromEthToCore(absolute);
    m_ethRemainder = absolute % GasConv::GetScalingFactor();
  }
  void IncreaseByCore(uint64_t coreVal) { m_gasCore += coreVal; }
  void SetGasCore(uint64_t coreVal) { m_gasCore = coreVal; }
  void IncreaseByEth(uint64_t ethVal) {
    uint64_t absolute =
        GasConv::GasUnitsFromCoreToEth(m_gasCore) + m_ethRemainder;
    absolute += ethVal;
    m_gasCore = GasConv::GasUnitsFromEthToCore(absolute);
    m_ethRemainder = absolute % GasConv::GetScalingFactor();
  }
  uint64_t GetEthGas() const {
    return GasConv::GasUnitsFromCoreToEth(m_gasCore) + m_ethRemainder;
  }
  uint64_t GetCoreGas() const { return m_gasCore; }

 private:
  uint64_t m_gasCore = 0;
  uint64_t m_ethRemainder = 0;
};

struct CpsContext {
  using Address = dev::h160;
  Address origSender;
  bool isStatic = false;
  bool estimate = false;
  GasTracker gasTracker;
  evm::EvmEvalExtras evmExtras;
  ScillaProcessContext scillaExtras;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSCONTEXT_H_