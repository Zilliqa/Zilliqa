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

#pragma once

#include "libData/AccountData/EvmClient.h"

/**
 * @brief Default Mock implementation for the evm client
 */
class EvmClientMock : public EvmClient {
 public:
  EvmClientMock() = default;

  bool OpenServer(bool /*force = false*/) { return true; };

  virtual bool CallRunner(uint32_t /*version*/,                 //
                          const Json::Value& request,           //
                          evmproj::CallResponse& /*response*/,  //
                          uint32_t /*counter = MAXRETRYCONN*/) {
    LOG_GENERAL(DEBUG, "CallRunner json request:" << request);
    return true;
  };
};
