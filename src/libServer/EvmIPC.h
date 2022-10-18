/*
 * Copyright (C) 2019 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBSERVER_EVMIPC_H_
#define ZILLIQA_SRC_LIBSERVER_EVMIPC_H_

#include <string>

/**
 * @brief Implementation of the EVM IPC method called from Evm-ds
 */
class EvmIPC {
 public:
  EvmIPC() = default;
  ~EvmIPC() = default;
  EvmIPC(const EvmIPC&) = delete;
  EvmIPC(EvmIPC&&) = delete;
  EvmIPC& operator=(const EvmIPC&) = delete;
  EvmIPC& operator=(EvmIPC&&) = delete;

  /**
   * @brief Fetch the external state of and account at defined block number
   * @param addr contract address
   * @param query name of the field to query data from
   * @param value
   * @param found true when found
   * @param type
   * @return true when found else false
   */
  [[nodiscard]] bool fetchExternalStateValueEvm(const std::string& addr,   //
                                                const std::string& query,  //
                                                std::string& value,        //
                                                bool& found,               //
                                                std::string& type);

  /**
   * @brief fetch block chain info at a defined block
   * @param queryName name of the field to query for
   * @param blockTag query arguments, the block tag string like 'latest',
   * 'pending', 'earliest' or a block number
   * @param value the found value that will be returned
   * @return true when found else false
   */
  [[nodiscard]] bool fetchBlockchainInfoEvm(const std::string& queryName,
                                            const std::string& blockTag,
                                            std::string& value);
};
#endif