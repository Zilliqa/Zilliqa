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
#include "libUtils/JsonUtils.h"

using namespace std;
using namespace boost::multiprecision;

TransactionReceipt::TransactionReceipt() { update(); }

unsigned int TransactionReceipt::Serialize(std::vector<unsigned char>& dst,
                                           unsigned int offset) const {
  vector<unsigned char> receiptBytes =
      DataConversion::StringToCharArray(m_tranReceiptStr);

  // size of JsonStr
  SetNumber<uint32_t>(dst, offset, (uint32_t)receiptBytes.size(),
                      sizeof(uint32_t));
  offset += sizeof(uint32_t);

  // JsonStr
  copy(receiptBytes.begin(), receiptBytes.end(), back_inserter(dst));
  offset += receiptBytes.size();

  return offset;
}

int TransactionReceipt::Deserialize(const std::vector<unsigned char>& src,
                                    unsigned int offset) {
  try {
    unsigned int t_offset = offset;

    uint32_t receipt_size = GetNumber<uint32_t>(src, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    vector<unsigned char> receipt_bytes;
    if (receipt_size > 0) {
      copy(src.begin() + offset, src.begin() + offset + receipt_size,
           back_inserter(receipt_bytes));
    }
    offset += receipt_size;

    m_serialized_size = offset - t_offset;

    m_tranReceiptStr.clear();
    m_tranReceiptStr = string(receipt_bytes.begin(), receipt_bytes.end());

    if (!JSONUtils::convertStrtoJson(m_tranReceiptStr, m_tranReceiptObj)) {
      LOG_GENERAL(WARNING, "Error with convert receipt string to json object");
      return -1;
    }
    update();
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with TransactionReceipt::Deserialize."
                             << ' ' << e.what());
    return -1;
  }
  return 0;
}

void TransactionReceipt::SetResult(const bool& result) {
  if (result) {
    m_tranReceiptObj["success"] = "true";
  } else {
    m_tranReceiptObj["success"] = "false";
  }
}

void TransactionReceipt::SetCumGas(const uint256_t& cumGas) {
  m_cumGas = cumGas;
  m_tranReceiptObj["cumulative_gas"] = m_cumGas.convert_to<string>();
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