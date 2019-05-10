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

#include "TransactionReceipt.h"
#include "libMessage/Messenger.h"
#include "libUtils/JsonUtils.h"

using namespace std;
using namespace boost::multiprecision;

TransactionReceipt::TransactionReceipt() {
  update();
  m_errorObj[to_string(m_depth)] = Json::arrayValue;
}

bool TransactionReceipt::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTransactionReceipt(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransactionReceipt failed.");
    return false;
  }

  return true;
}

bool TransactionReceipt::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetTransactionReceipt(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransactionReceipt failed.");
    return false;
  }

  if (!JSONUtils::GetInstance().convertStrtoJson(m_tranReceiptStr,
                                                 m_tranReceiptObj)) {
    LOG_GENERAL(WARNING, "Error with convert receipt string to json object");
    return false;
  }

  try {
    update();
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with TransactionReceipt::Deserialize."
                             << ' ' << e.what());
    return false;
  }
  return true;
}

void TransactionReceipt::SetResult(const bool& result) {
  if (result) {
    m_tranReceiptObj["success"] = true;
  } else {
    m_tranReceiptObj["success"] = false;
  }
}

void TransactionReceipt::AddDepth() {
  LOG_MARKER();
  m_depth++;
  m_errorObj[to_string(m_depth)] = Json::arrayValue;
}

void TransactionReceipt::AddError(const unsigned int& errCode) {
  LOG_GENERAL(INFO, "AddError: " << errCode);
  m_errorObj[to_string(m_depth)].append(errCode);
}

void TransactionReceipt::SetCumGas(const uint64_t& cumGas) {
  m_cumGas = cumGas;
  m_tranReceiptObj["cumulative_gas"] = to_string(m_cumGas);
}

void TransactionReceipt::SetEpochNum(const uint64_t& epochNum) {
  m_tranReceiptObj["epoch_num"] = to_string(epochNum);
}

void TransactionReceipt::SetString(const std::string& tranReceiptStr) {
  if (!JSONUtils::GetInstance().convertStrtoJson(tranReceiptStr,
                                                 m_tranReceiptObj)) {
    LOG_GENERAL(WARNING, "Error with convert receipt string to json object");
    return;
  }
  m_tranReceiptStr = tranReceiptStr;
}

void TransactionReceipt::AddEntry(const LogEntry& entry) {
  m_tranReceiptObj["event_logs"].append(entry.GetJsonObject());
}

void TransactionReceipt::clear() {
  m_tranReceiptStr.clear();
  m_tranReceiptObj.clear();
  m_errorObj.clear();
  m_depth = 0;
  update();
}

void TransactionReceipt::InstallError() {
  Json::Value errorObj;
  unsigned int depth = 0;
  for (const auto& e : m_errorObj) {
    if (!e.empty()) {
      errorObj[to_string(depth)] = e;
    }
    depth++;
  }
  if (!errorObj.empty()) {
    m_tranReceiptObj["errors"] = errorObj;
  }
}

void TransactionReceipt::update() {
  if (m_tranReceiptObj == Json::nullValue) {
    m_tranReceiptStr = "{}";
    return;
  }
  InstallError();
  m_tranReceiptStr =
      JSONUtils::GetInstance().convertJsontoStr(m_tranReceiptObj);
}

/// Implements the Serialize function inherited from Serializable.
bool TransactionWithReceipt::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTransactionWithReceipt(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransactionWithReceipt failed.");
    return false;
  }
  return true;
}

/// Implements the Deserialize function inherited from Serializable.
bool TransactionWithReceipt::Deserialize(const bytes& src,
                                         unsigned int offset) {
  if (!Messenger::GetTransactionWithReceipt(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransactionWithReceipt failed.");
    return false;
  }
  return true;
}
