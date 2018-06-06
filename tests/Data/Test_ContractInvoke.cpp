/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include <boost/filesystem.hpp>

#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include <array>
#include <regex>
#include <string>
#include <vector>

#include "CrowdFundingCodes.h"
#include "InvokeCrowdFundingCodes.h"

#define BOOST_TEST_MODULE contractinvokingtest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contractinvokingtest)

Address fromAddr;
Address cfAddress, icfAddress;
KeyPair sender;
uint256_t nonce = 0;
std::vector<unsigned char> emptyCode, emptyData;

struct ICFSampleInput
{
    string icfDataStr;
    string icfOutStr;
    int blockNum;
    string sampleName;
};

struct CFSampleInput
{
    string cfDataStr;
    int blockNum;
    vector<ICFSampleInput> icfSamples;
};

bool InvokeFunction(string icfDataStr, string icfOutStr, int blockNum,
                    string sampleName, bool didResetCF, bool didResetICF)
{
    LOG_MARKER();

    std::vector<unsigned char> icfData(icfDataStr.begin(), icfDataStr.end());
    Transaction icfTx(1, nonce, icfAddress, sender, 0, 11, 99, emptyCode,
                      icfData);

    AccountStore::GetInstance().UpdateAccounts(blockNum, icfTx);

    string outStr = icfOutStr;

    outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
    outStr.erase(std::remove(outStr.begin(), outStr.end(), '\n'), outStr.end());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    LOG_GENERAL(INFO, sampleName << ":" << endl << output_file << endl);

    // record the output_file
    string logfile = "LogInvoke";
    if (!(boost::filesystem::exists("./" + logfile)))
    {
        boost::filesystem::create_directories("./" + logfile);
    }

    ofstream output_log(logfile + "/" + (didResetCF ? "R+" : "")
                        + (didResetICF ? "RI+" : "") + sampleName + ".txt");
    output_log << output_file;
    output_log.close();

    // truncate enter and space
    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    // BOOST_CHECK_MESSAGE(outStr == output_file,
    //                     "Error: didn't get desired output for" << sampleName);

    //

    return true;
}

enum ResetType
{
    CF,
    ICF
};

bool CreateContract(const int& blockNum, ResetType rType)
{
    Address tAddress;
    string initStr, codeStr;

    switch (rType)
    {
    case CF:
    {
        // Create CrowdFunding Contract
        cfAddress = Account::GetAddressForContract(fromAddr, nonce);
        LOG_GENERAL(INFO, "CrowdFunding Address: " << cfAddress);
        tAddress = cfAddress;
        initStr
            = regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + tAddress.hex());
        codeStr = cfCodeStr;
        break;
    }
    case ICF:
    {
        icfAddress = Account::GetAddressForContract(fromAddr, nonce);
        LOG_GENERAL(INFO, "Invoker Address: " << icfAddress);
        tAddress = icfAddress;
        initStr = regex_replace(icfInitStr, regex("\\$ADDR"),
                                "0x" + cfAddress.hex());
        codeStr = icfCodeStr;
        break;
    }
    default:
        return false;
    }

    std::vector<unsigned char> code(codeStr.begin(), codeStr.end());

    std::vector<unsigned char> initData(initStr.begin(), initStr.end());

    Transaction createTx(1, nonce, dev::h160(), sender, 0, 11, 66, code,
                         initData);

    AccountStore::GetInstance().UpdateAccounts(blockNum, createTx);

    bool checkAddr = false;
    Account* account = AccountStore::GetInstance().GetAccount(tAddress);
    if (account != nullptr)
    {
        checkAddr = true;
        nonce++;

        if (rType == ICF)
        {
            // transfer balance
            int amount = 122;
            Transaction transferTx(1, nonce, tAddress, sender, amount, 11, 66,
                                   emptyCode, emptyData);
            if (AccountStore::GetInstance().UpdateAccounts(blockNum,
                                                           transferTx))
            {
                nonce++;
            }
        }
    }
    BOOST_CHECK_MESSAGE(checkAddr, "Error with creation of contract account");
    return checkAddr;
}

void AutoTest(bool doResetCF, bool doResetICF,
              const vector<CFSampleInput>& samples)
{
    bool didCreateCF{false}, didCreateICF{false};

    for (unsigned int i = 0; i < samples.size(); i++)
    {
        if (!didCreateCF)
        {
            if (!CreateContract(samples[i].blockNum, CF))
            {
                continue;
            }
            didCreateCF = true;
        }

        for (unsigned int j = 0; j < samples[i].icfSamples.size(); j++)
        {
            if (!didCreateICF)
            {
                if (!CreateContract(samples[i].icfSamples[j].blockNum, ICF))
                {
                    continue;
                }
                didCreateICF = true;
            }

            if (samples[i].cfDataStr != "")
            {
                std::vector<unsigned char> cfData(samples[i].cfDataStr.begin(),
                                                  samples[i].cfDataStr.end());
                Transaction cfTx(1, nonce, cfAddress, sender, 0, 11, 66,
                                 emptyCode, cfData);
                if (!AccountStore::GetInstance().UpdateAccounts(
                        samples[i].blockNum, cfTx))
                {
                    continue;
                }
                nonce++;
            }

            if (InvokeFunction(samples[i].icfSamples[j].icfDataStr,
                               samples[i].icfSamples[j].icfOutStr,
                               samples[i].icfSamples[j].blockNum,
                               samples[i].icfSamples[j].sampleName, doResetCF,
                               doResetICF))
            {
                nonce++;
            }

            if (doResetCF
                && !(i == samples.size() - 1
                     && j == samples[samples.size() - 1].icfSamples.size() - 1))
            {
                if (!CreateContract(samples[i].blockNum, CF))
                {
                    break;
                }
            }

            if (doResetICF
                && !(i == samples.size() - 1
                     && j == samples[samples.size() - 1].icfSamples.size() - 1))
            {
                if (!CreateContract(samples[i].icfSamples[j].blockNum, ICF))
                {
                    break;
                }
            }
        }
    }
}

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContractInvoking)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    AccountStore::GetInstance().Init();

    sender = Schnorr::GetInstance().GenKeyPair();

    std::vector<unsigned char> vec;
    sender.second.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    const std::vector<unsigned char>& output = sha2.Finalize();

    copy(output.end() - ACC_ADDR_SIZE, output.end(),
         fromAddr.asArray().begin());

    vector<CFSampleInput> samples{
        {"",
         100,
         {{icfDataStr1, icfOutStr1, 100, "State1_Invoke1"},
          {icfDataStr2, icfOutStr2, 100, "State1_Invoke2"},
          {icfDataStr3, icfOutStr3, 100, "State1_Invoke3"}}},
        {cfDataStr,
         100,
         {{icfDataStr1, icfOutStr1, 100, "State2_Invoke1"},
          {icfDataStr2, icfOutStr2, 100, "State2_Invoke2"},
          {icfDataStr3, icfOutStr3, 100, "State2_Invoke3"}}},
        {cfDataStr3,
         100,
         {{icfDataStr1, icfOutStr1, 100, "State3_Invoke1"},
          {icfDataStr2, icfOutStr2, 100, "State3_Invoke2"},
          {icfDataStr3, icfOutStr3, 100, "State3_Invoke3"}}},
        {cfDataStr4,
         200,
         {{icfDataStr1, icfOutStr1, 100, "State4_Invoke1"},
          {icfDataStr2, icfOutStr2, 100, "State4_Invoke2"},
          {icfDataStr3, icfOutStr3, 100, "State4_Invoke3"}}},
        {cfDataStr5,
         300,
         {{icfDataStr1, icfOutStr1, 100, "State5_Invoke1"},
          {icfDataStr2, icfOutStr2, 100, "State5_Invoke2"},
          {icfDataStr3, icfOutStr3, 100, "State5_Invoke3"}}},
    };
    AutoTest(true, true, samples);
    // AutoTest(true, false, samples);
    // AutoTest(false, true, samples);
    // AutoTest(false, false, samples);
}

BOOST_AUTO_TEST_SUITE_END()
