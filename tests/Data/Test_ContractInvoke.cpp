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

#include <boost/filesystem.hpp>

#include <array>
#include <regex>
#include <string>
#include <vector>
#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#include "CrowdFundingCodes.h"
#include "InvokeCrowdFundingCodes.h"

#define BOOST_TEST_MODULE contractinvokingtest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contractinvokingtest)

Address fromAddr, fromAddr2;
Address cfAddress, icfAddress;
KeyPair sender, sender2;
uint256_t nonce, nonce2 = 0;

struct ICFSampleInput {
  string icfDataStr;
  string icfOutStr;
  uint256_t amount;
  uint256_t gasPrice;
  uint256_t gasLimit;
  int blockNum;
  string sampleName;
};

struct CFSampleInput {
  string cfDataStr;
  KeyPair cfSender;
  uint256_t amount;
  uint256_t gasPrice;
  uint256_t gasLimit;
  int blockNum;
  vector<ICFSampleInput> icfSamples;
};

bool InvokeFunction(string icfDataStr, string icfOutStr, int blockNum,
                    uint256_t amount, uint256_t gasPrice, uint256_t gasLimit,
                    string sampleName, bool didResetCF, bool didResetICF) {
  LOG_MARKER();

  std::vector<unsigned char> icfData(icfDataStr.begin(), icfDataStr.end());
  Transaction icfTx(1, nonce, icfAddress, sender, amount, gasPrice, gasLimit,
                    {}, icfData);
  TransactionReceipt icfTr;
  if (!AccountStore::GetInstance().UpdateAccounts(blockNum, 1, true, icfTx,
                                                  icfTr)) {
    LOG_GENERAL(INFO, "InvokeFunction Failed");
    return false;
  }

  string outStr = icfOutStr;

  outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
  outStr.erase(std::remove(outStr.begin(), outStr.end(), '\n'), outStr.end());

  ifstream infile{OUTPUT_JSON};
  std::string output_file{istreambuf_iterator<char>(infile),
                          istreambuf_iterator<char>()};

  LOG_GENERAL(INFO, sampleName << ":" << endl << output_file << endl);

  // record the output_file
  string logfile = "LogInvoke";
  if (!(boost::filesystem::exists("./" + logfile))) {
    boost::filesystem::create_directories("./" + logfile);
  }

  ofstream output_log(logfile + "/" + (didResetCF ? "R+" : "") +
                      (didResetICF ? "RI+" : "") + sampleName + ".txt");
  output_log << output_file;
  output_log.close();

  // truncate enter and space
  output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                    output_file.end());
  output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                    output_file.end());

  // BOOST_CHECK_MESSAGE(outStr == output_file,
  //                     "Error: didn't get desired output for" << sampleName);

  return true;
}

enum ResetType { CF, ICF };

bool CreateContract(const int& blockNum, ResetType rType) {
  LOG_MARKER();

  Address tAddress;
  string initStr, codeStr;

  switch (rType) {
    case CF: {
      // Create CrowdFunding Contract
      cfAddress = Account::GetAddressForContract(fromAddr, nonce);
      LOG_GENERAL(INFO, "CrowdFunding Address: " << cfAddress);
      tAddress = cfAddress;
      initStr =
          regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + tAddress.hex());
      codeStr = cfCodeStr;
      break;
    }
    case ICF: {
      icfAddress = Account::GetAddressForContract(fromAddr, nonce);
      LOG_GENERAL(INFO, "Invoker Address: " << icfAddress);
      tAddress = icfAddress;
      initStr = regex_replace(icfInitStr, regex("\\$CONTRACT"),
                              "0x" + cfAddress.hex());
      initStr =
          regex_replace(initStr, regex("\\$OWNER"), "0x" + fromAddr.hex());
      codeStr = icfCodeStr;
      break;
    }
    default:
      return false;
  }

  std::vector<unsigned char> code(codeStr.begin(), codeStr.end());

  std::vector<unsigned char> initData(initStr.begin(), initStr.end());

  // LOG_GENERAL(INFO, "nonce: " << nonce);

  Transaction createTx(1, nonce, dev::h160(), sender, 0, 1, 50, code, initData);
  TransactionReceipt createTr;

  AccountStore::GetInstance().UpdateAccounts(blockNum, 1, true, createTx,
                                             createTr);

  bool checkAddr = false;
  Account* account = AccountStore::GetInstance().GetAccount(tAddress);
  if (account != nullptr) {
    checkAddr = true;
    nonce++;

    // if (rType == ICF)
    // {
    //     // transfer balance
    //     LOG_GENERAL(INFO, "Try making normal transaction to invoker");
    //     int amount = 122;
    //     Transaction transferTx(1, nonce, tAddress, sender, amount, 1, 1, {},
    //                            {});
    //     if (AccountStore::GetInstance().UpdateAccounts(blockNum,
    //                                                    transferTx))
    //     {
    //         nonce++;
    //     }
    // }
  }
  BOOST_CHECK_MESSAGE(checkAddr, "Error with creation of contract account");
  return checkAddr;
}

void AutoTest(bool doResetCF, bool doResetICF,
              const vector<CFSampleInput>& samples) {
  LOG_MARKER();

  bool didCreateCF{false}, didCreateICF{false};

  for (unsigned int i = 0; i < samples.size(); i++) {
    if (!didCreateCF) {
      if (!CreateContract(samples[i].blockNum, CF)) {
        continue;
      }
      didCreateCF = true;
    }

    for (unsigned int j = 0; j < samples[i].icfSamples.size(); j++) {
      if (!didCreateICF) {
        if (!CreateContract(samples[i].icfSamples[j].blockNum, ICF)) {
          continue;
        }
        didCreateICF = true;
      }

      if (samples[i].cfDataStr != "") {
        std::vector<unsigned char> cfData(samples[i].cfDataStr.begin(),
                                          samples[i].cfDataStr.end());

        uint256_t* t_nonce;
        if (samples[i].cfSender == sender) {
          t_nonce = &nonce;
        } else if (samples[i].cfSender == sender2) {
          t_nonce = &nonce2;
        } else {
          LOG_GENERAL(WARNING, "1. Why we have this sender? "
                                   << samples[i].cfSender.second);
          continue;
        }

        Transaction cfTx(1, *t_nonce, cfAddress, samples[i].cfSender,
                         samples[i].amount, samples[i].gasPrice,
                         samples[i].gasLimit, {}, cfData);
        TransactionReceipt cfTr;
        if (!AccountStore::GetInstance().UpdateAccounts(samples[i].blockNum, 1,
                                                        true, cfTx, cfTr)) {
          continue;
        }

        (*t_nonce)++;
      }

      if (InvokeFunction(samples[i].icfSamples[j].icfDataStr,
                         samples[i].icfSamples[j].icfOutStr,
                         samples[i].icfSamples[j].blockNum,
                         samples[i].icfSamples[j].amount,
                         samples[i].icfSamples[j].gasPrice,
                         samples[i].icfSamples[j].gasLimit,
                         samples[i].icfSamples[j].sampleName, doResetCF,
                         doResetICF)) {
        nonce++;
      }

      LOG_GENERAL(INFO, "Balance: ");
      LOG_GENERAL(INFO, "fromAddr:" << AccountStore::GetInstance().GetBalance(
                            fromAddr));
      LOG_GENERAL(INFO, "fromAddr2:" << AccountStore::GetInstance().GetBalance(
                            fromAddr2));
      LOG_GENERAL(INFO, "cfAddress:" << AccountStore::GetInstance().GetBalance(
                            cfAddress));
      LOG_GENERAL(INFO, "icfAddress:" << AccountStore::GetInstance().GetBalance(
                            icfAddress));

      if (doResetCF &&
          !(i == samples.size() - 1 &&
            j == samples[samples.size() - 1].icfSamples.size() - 1)) {
        if (!CreateContract(samples[i].blockNum, CF)) {
          break;
        }
      }

      if (doResetICF &&
          !(i == samples.size() - 1 &&
            j == samples[samples.size() - 1].icfSamples.size() - 1)) {
        if (!CreateContract(samples[i].icfSamples[j].blockNum, ICF)) {
          break;
        }
      }
    }
  }
}

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContractInvoking) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  sender = Schnorr::GetInstance().GenKeyPair();
  sender2 = Schnorr::GetInstance().GenKeyPair();

  fromAddr = Account::GetAddressFromPublicKey(sender.second);
  LOG_GENERAL(INFO, "fromAddr: " << fromAddr);
  fromAddr2 = Account::GetAddressFromPublicKey(sender2.second);
  LOG_GENERAL(INFO, "fromAddr2: " << fromAddr2);

  vector<CFSampleInput> samples{
      {"",
       sender,
       0,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 10, 100, "State1_Invoke1_NG"},
        {icfDataStr2, icfOutStr2, 0, 1, 10, 100, "State1_Invoke2_NG"},
        {icfDataStr3, icfOutStr3, 0, 1, 10, 100, "State1_Invoke3_NG"}}},
      {"",
       sender,
       0,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 30, 100, "State1_Invoke1_G"},
        {icfDataStr2, icfOutStr2, 0, 1, 30, 100, "State1_Invoke2_G"},
        {icfDataStr3, icfOutStr3, 0, 1, 30, 100, "State1_Invoke3_G"}}},

      {cfDataDonateStr,
       sender,
       100,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 10, 100, "State2_Invoke1_NG"},
        {icfDataStr2, icfOutStr2, 0, 1, 10, 100, "State2_Invoke2_NG"},
        {icfDataStr3, icfOutStr3, 0, 1, 10, 100, "State2_Invoke3_NG"}}},
      {cfDataDonateStr,
       sender,
       100,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 30, 100, "State2_Invoke1_G"},
        {icfDataStr2, icfOutStr2, 0, 1, 30, 100, "State2_Invoke2_G"},
        {icfDataStr3, icfOutStr3, 0, 1, 30, 100, "State2_Invoke3_G"}}},

      {cfDataDonateStr,
       sender2,
       200,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 10, 100, "State3_Invoke1_NG"},
        {icfDataStr2, icfOutStr2, 0, 1, 10, 100, "State3_Invoke2_NG"},
        {icfDataStr3, icfOutStr3, 0, 1, 10, 100, "State3_Invoke3_NG"}}},
      {cfDataDonateStr,
       sender2,
       200,
       1,
       10,
       100,
       {{icfDataStr1, icfOutStr1, 100, 1, 30, 100, "State3_Invoke1_G"},
        {icfDataStr2, icfOutStr2, 0, 1, 30, 100, "State3_Invoke2_G"},
        {icfDataStr3, icfOutStr3, 0, 1, 30, 100, "State3_Invoke3_G"}}},

      {cfDataGetFundsStr,
       sender2,
       0,
       1,
       10,
       200,
       {{icfDataStr1, icfOutStr1, 100, 1, 10, 100, "State4_Invoke1_NG"},
        {icfDataStr2, icfOutStr2, 0, 1, 10, 100, "State4_Invoke2_NG"},
        {icfDataStr3, icfOutStr3, 0, 1, 10, 100, "State4_Invoke3_NG"}}},
      {cfDataGetFundsStr,
       sender2,
       0,
       1,
       10,
       200,
       {{icfDataStr1, icfOutStr1, 100, 1, 30, 100, "State4_Invoke1_G"},
        {icfDataStr2, icfOutStr2, 0, 1, 30, 100, "State4_Invoke2_G"},
        {icfDataStr3, icfOutStr3, 0, 1, 30, 100, "State4_Invoke3_G"}}},

      {cfDataClaimBackStr,
       sender,
       0,
       1,
       10,
       300,
       {{icfDataStr1, icfOutStr1, 100, 1, 10, 100, "State5_Invoke1_NG"},
        {icfDataStr2, icfOutStr2, 0, 1, 10, 100, "State5_Invoke2_NG"},
        {icfDataStr3, icfOutStr3, 0, 1, 10, 100, "State5_Invoke3_NG"}}},
      {cfDataClaimBackStr,
       sender,
       0,
       1,
       10,
       300,
       {{icfDataStr1, icfOutStr1, 100, 1, 30, 100, "State5_Invoke1_G"},
        {icfDataStr2, icfOutStr2, 0, 1, 30, 100, "State5_Invoke2_G"},
        {icfDataStr3, icfOutStr3, 0, 1, 30, 100, "State5_Invoke3_G"}}},
  };

  AutoTest(true, true, samples);
  // AutoTest(true, false, samples);
  // AutoTest(false, true, samples);
  // AutoTest(false, false, samples);
}

BOOST_AUTO_TEST_SUITE_END()
