/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "TransactionReceipt.h"
#include "libMessage/Messenger.h"
#include "libUtils/JsonUtils.h"

using namespace std;
using namespace boost::multiprecision;

TransactionReceipt::TransactionReceipt() { update(); }

bool TransactionReceipt::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetTransactionReceipt(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTransactionReceipt failed.");
    return false;
  }

  return true;
}

bool TransactionReceipt::Deserialize(const bytes& src, unsigned int offset) {
  try {
    if (!Messenger::GetTransactionReceipt(src, offset, *this)) {
      LOG_GENERAL(WARNING, "Messenger::GetTransactionReceipt failed.");
      return false;
    }

    if (!JSONUtils::convertStrtoJson(m_tranReceiptStr, m_tranReceiptObj)) {
      LOG_GENERAL(WARNING, "Error with convert receipt string to json object");
      return false;
    }
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

void TransactionReceipt::SetCumGas(const uint64_t& cumGas) {
  m_cumGas = cumGas;
  m_tranReceiptObj["cumulative_gas"] = to_string(m_cumGas);
}

void TransactionReceipt::SetString(const std::string& tranReceiptStr) {
  try {
    if (!JSONUtils::convertStrtoJson(tranReceiptStr, m_tranReceiptObj)) {
      LOG_GENERAL(WARNING, "Error with convert receipt string to json object");
      return;
    }
    m_tranReceiptStr = tranReceiptStr;
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with TransactionReceipt::SetString." << ' ' << e.what());
    return;
  }
}

void TransactionReceipt::AddEntry(const LogEntry& entry) {
  m_tranReceiptObj["event_logs"].append(entry.GetJsonObject());
}

void TransactionReceipt::clear() {
  m_tranReceiptStr.clear();
  m_tranReceiptObj.clear();
  update();
}

void TransactionReceipt::update() {
  if (m_tranReceiptObj == Json::nullValue) {
    m_tranReceiptStr = "{}";
    return;
  }
  m_tranReceiptStr = JSONUtils::convertJsontoStr(m_tranReceiptObj);
  m_tranReceiptStr.erase(
      std::remove(m_tranReceiptStr.begin(), m_tranReceiptStr.end(), ' '),
      m_tranReceiptStr.end());
  m_tranReceiptStr.erase(
      std::remove(m_tranReceiptStr.begin(), m_tranReceiptStr.end(), '\n'),
      m_tranReceiptStr.end());
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