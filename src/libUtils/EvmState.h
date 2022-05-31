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




#include <unordered_map>
#include <mutex>

class    EvmState {
 public:
  EvmState() = default;
  EvmState(std::string transactionId,std::string evmOriginalCode,std::string evmNewCode);
  EvmState(const EvmState& other) = default;
  EvmState(EvmState && other) = default;
  EvmState& operator=(const EvmState& other) =default;
  EvmState& operator=(EvmState&& other) =default;
  ~EvmState() = default;

  const std::string& GetContractAddress() const;
  const std::string& GetOriginalCode() const;
  const std::string& GetModifiedCode() const;
 private:
  std::string   m_contractAddressId;
  std::string   m_EvmOriginalCode;
  std::string   m_EvmNewCode;

  friend std::ostream& operator<<(std::ostream& os, EvmState& evm);
};

class   EvmStateMap{
 public:
  EvmStateMap() = default;
  EvmStateMap(const EvmStateMap& other) = delete;
  EvmStateMap(EvmStateMap && other) = delete;
  EvmStateMap& operator=(EvmStateMap&& other) =delete;
  ~EvmStateMap() = default;

  bool    Add(EvmState other);
  bool    Get(const std::string& key,EvmState& other);
  bool    Delete(const std::string& otherKey);

 private:
  std::unordered_map<std::string,EvmState>    m_map;
  std::mutex                                  m_mapMutex;

  friend std::ostream& operator<<(std::ostream& os, EvmStateMap& evm);
};




