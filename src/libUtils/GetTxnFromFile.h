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

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>

#ifndef __GetTxnFromFile_H__
#define __GetTxnFromFile_H__

#include "Logger.h"
#include "libData/AccountData/Transaction.h"
#include "libMessage/Messenger.h"

bool getTransactionsFromFile(std::fstream& f, unsigned int startNum,
                             unsigned int totalNum,
                             std::vector<Transaction>& txns) {
  f.seekg(0, std::ios::beg);

  bytes buffOffsetInfo(sizeof(uint32_t));
  f.read((char*)&buffOffsetInfo[0], sizeof(uint32_t));
  uint32_t txnOffsetInfoSize = SerializableDataBlock::GetNumber<uint32_t>(
      buffOffsetInfo, 0, sizeof(uint32_t));
  if (txnOffsetInfoSize <= 0 && txnOffsetInfoSize >= 1000000) {
    LOG_GENERAL(WARNING, "The txn offset information size" << txnOffsetInfoSize
                                                           << " is invalid.");
    return false;
  }

  bytes buffTxnOffsets(txnOffsetInfoSize);
  f.read((char*)&buffTxnOffsets[0], txnOffsetInfoSize);

  uint32_t txnDataStart = f.tellg();

  std::vector<uint32_t> txnOffsets;
  if (!Messenger::GetTransactionFileOffset(buffTxnOffsets, 0, txnOffsets)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransactionFileOffset failed.");
    return false;
  }

  if (txnOffsets.empty()) {
    LOG_GENERAL(WARNING, "The transaction offset information is empty.");
    return false;
  }

  f.seekg(txnDataStart + txnOffsets[startNum], std::ios::beg);
  for (unsigned int i = startNum;
       i < startNum + totalNum && i < (txnOffsets.size() - 1); ++i) {
    uint32_t txnSize = txnOffsets[i + 1] - txnOffsets[i];
    bytes buffTxn(txnSize);
    f.read((char*)&buffTxn[0], txnSize);

    Transaction txn;
    if (!Messenger::GetTransaction(buffTxn, 0, txn)) {
      LOG_GENERAL(WARNING, "Messenger::GetTransaction failed.");
      return false;
    }
    txns.push_back(txn);
  }

  return true;
}

class GetTxnFromFile {
 public:
  // clears vec
  static bool GetFromFile(Address addr, unsigned int startNum,
                          unsigned int totalNum,
                          std::vector<Transaction>& txns) {
    if (NUM_TXN_TO_SEND_PER_ACCOUNT == 0) {
      return true;
    }

    LOG_MARKER();

    const auto num_txn = NUM_TXN_TO_SEND_PER_ACCOUNT;
    std::fstream file;
    txns.clear();

    if (totalNum > num_txn) {
      LOG_GENERAL(WARNING, "A single file is holding too many txns ("
                               << totalNum << " > " << num_txn);
      return false;
    }

    auto getFile = [&addr](const unsigned int& num, std::fstream& file,
                           const auto num_txn) {
      std::string fileString = TXN_PATH + "/" + addr.hex() + "_" +
                               std::to_string(num * num_txn + 1) + ".zil";

      file.open(fileString, std::ios::binary | std::ios::in);

      if (!file.is_open()) {
        LOG_GENERAL(WARNING, "File failed to open " << fileString);
        return false;
      }

      return true;
    };

    unsigned int fileNum = (startNum - 1) / num_txn;
    bool breakCall = false;
    bool b = false;
    std::vector<Transaction> remainTxn;
    unsigned int startNumInFile = (startNum - 1) % num_txn;
    if (startNumInFile + totalNum > num_txn) {
      breakCall = true;
      unsigned int remainNum = totalNum + startNumInFile - num_txn;
      if (!getFile(fileNum + 1, file, num_txn)) {
        return false;
      }

      b = getTransactionsFromFile(file, 0, remainNum, remainTxn);

      file.close();

      if (!b) {
        return false;
      }

      totalNum -= remainNum;
    }

    if (!getFile(fileNum, file, num_txn)) {
      return false;
    }

    b = getTransactionsFromFile(file, startNumInFile, totalNum, txns);

    file.close();

    if (!b) {
      return false;
    }

    if (breakCall) {
      copy(remainTxn.begin(), remainTxn.end(), back_inserter(txns));
    }

    return true;
  }
};

#endif
