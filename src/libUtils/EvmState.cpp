#include "libUtils/EvmState.h"
#include <iostream>

bool EvmStateMap::Get(const std::string& key, EvmState& other) {
  std::lock_guard<std::mutex> guard(m_mapMutex);
  auto it = m_map.find(key);

  if (it != m_map.end()) {
    other = it->second;
  } else {
    return false;
  }
  return true;
}

bool EvmStateMap::Add(EvmState other) {
  std::lock_guard<std::mutex> guard(m_mapMutex);
  std::string key = other.GetContractAddress();
  m_map[key] = std::move(other);
  return true;
}

bool EvmStateMap::Delete(const std::string& otherKey) {
  std::lock_guard<std::mutex> guard(m_mapMutex);
  auto it = m_map.find(otherKey);

  if (it != m_map.end()) {
    m_map.erase(it);
  } else {
    return false;
  }
  return true;
}

EvmState::EvmState(std::string contractAddress, std::string evmOriginalCode,
                   std::string evmNewCode)
    : m_contractAddressId(std::move(contractAddress)),
      m_EvmOriginalCode(std::move(evmOriginalCode)),
      m_EvmNewCode(std::move(evmNewCode)) {}

const std::string& EvmState::GetContractAddress() const {
  return m_contractAddressId;
}

const std::string& EvmState::GetOriginalCode() const {
  return m_EvmOriginalCode;
}

const std::string& EvmState::GetModifiedCode() const { return m_EvmNewCode; }

std::ostream& operator<<(std::ostream& os, EvmState& evm) {
  os << "code mapper" << std::endl;
  os << evm.GetContractAddress() << ":" << std::endl;
  os << evm.GetOriginalCode() << ":" << std::endl;
  os << evm.GetModifiedCode() << ":" << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, EvmStateMap& evmmap) {
  for (std::pair<const std::basic_string<char>, EvmState> it : evmmap.m_map) {
    os << it.second << std::endl;
  }
  return os;
}
