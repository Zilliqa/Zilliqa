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

#include <array>
#include <string>
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/Logger.h"
#include "libUtils/DataConversion.h"

#define BOOST_TEST_MODULE transactiontest
#include <boost/test/included/unit_test.hpp>

using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE (transactiontest)

BOOST_AUTO_TEST_CASE (test1)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    Address toAddr;

    for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }

    Address fromAddr;

    for (unsigned int i = 0; i < fromAddr.asArray().size(); i++)
    {
        fromAddr.asArray().at(i) = i + 8;
    }

    std::array<unsigned char, TRAN_SIG_SIZE> signature;

    for (unsigned int i = 0; i < signature.size(); i++)
    {
        signature.at(i) = i + 16;
    }

    // Predicate pred(3, fromAddr, 2, 1, toAddr, fromAddr, 33, 1);
    // Transaction tx1(1, 5, toAddr, fromAddr, 55, signature, pred);

    Transaction tx1(1, 5, toAddr, fromAddr, 55, signature);
    std::vector<unsigned char> message1;
    tx1.Serialize(message1, 0);

    LOG_PAYLOAD("Transaction1 serialized", message1, Logger::MAX_BYTES_TO_DISPLAY);

    Transaction tx2(message1, 0);

    std::vector<unsigned char> message2;
    tx2.Serialize(message2, 0);

    LOG_PAYLOAD("Transaction2 serialized", message2, Logger::MAX_BYTES_TO_DISPLAY);

    const std::array<unsigned char, TRAN_HASH_SIZE> & tranID2 = tx2.GetTranID().asArray();
    uint32_t version2 = tx2.GetVersion();
    uint256_t nonce2 = tx2.GetNonce();
    const Address & toAddr2 = tx2.GetToAddr();
    const Address & fromAddr2 = tx2.GetFromAddr();
    uint256_t amount2 = tx2.GetAmount();
    const std::array<unsigned char, TRAN_SIG_SIZE> & signature2 = tx2.GetSignature();
    // Predicate pred2 = tx2.GetPredicate();

    std::vector<unsigned char> byteVec;
    byteVec.resize(TRAN_HASH_SIZE);
    copy(tranID2.begin(), tranID2.end(), byteVec.begin());
    LOG_PAYLOAD("Transaction2 tranID", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    
    std::string expectedStr = "57B55967946EF01E023F01CEFDCD8C1C69F07ED2F9A6A961525663F6942EAB6F";
    std::vector<unsigned char> expectedVec = DataConversion::HexStrToUint8Vec(expectedStr);
    bool is_tranID_equal = std::equal(byteVec.begin(), byteVec.end(), expectedVec.begin());
    BOOST_CHECK_MESSAGE(is_tranID_equal == true, "expected: " << expectedStr << " actual: " <<
                        DataConversion::Uint8VecToHexStr(byteVec));

    LOG_MESSAGE("Transaction2 version: " << version2);
    BOOST_CHECK_MESSAGE(version2 == 1, "expected: "<<1<<" actual: "<<version2<<"\n");

    LOG_MESSAGE("Transaction2 nonce: " << nonce2);
    BOOST_CHECK_MESSAGE(nonce2 == 5, "expected: "<<5<<" actual: "<<nonce2<<"\n");

    byteVec.clear();
    byteVec.resize(ACC_ADDR_SIZE);
    copy(toAddr2.begin(), toAddr2.end(), byteVec.begin());
    LOG_PAYLOAD("Transaction2 toAddr", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    BOOST_CHECK_MESSAGE(byteVec.at(19) == 23, "expected: "<<23<<" actual: "<<byteVec.at(19)<<"\n");
  
    copy(fromAddr2.begin(), fromAddr2.end(), byteVec.begin());
    LOG_PAYLOAD("Transaction2 fromAddr", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    BOOST_CHECK_MESSAGE(byteVec.at(19) == 27, "expected: "<<27<<" actual: "<<byteVec.at(19)<<"\n");

    LOG_MESSAGE("Transaction2 amount: " << amount2);
    BOOST_CHECK_MESSAGE(amount2 == 55, "expected: "<<55<<" actual: "<<amount2<<"\n");

    byteVec.clear();
    byteVec.resize(TRAN_SIG_SIZE);
    copy(signature2.begin(), signature2.end(), byteVec.begin());
    LOG_PAYLOAD("Transaction2 signature", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    BOOST_CHECK_MESSAGE(byteVec.at(63) == 79, "expected: "<<79<<" actual: "<<byteVec.at(63)<<"\n");

    // byteVec.clear();
    // byteVec.resize(1);
    // byteVec.at(0) = pred2.GetType();
    // LOG_PAYLOAD("Transaction2 predicate type", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(pred2.GetType() == 3, "expected: "<<3<<" actual: "<<pred2.GetType()<<"\n");

    // byteVec.at(0) = pred2.GetAccConOp();
    // LOG_PAYLOAD("Transaction2 predicate accConOp", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(pred2.GetAccConOp() == 2, "expected: "<<2<<" actual: "<<pred2.GetAccConOp()<<"\n");

    // uint256_t accConBalance2 = pred2.GetAccConBalance();
    // LOG_MESSAGE("Transaction2 predicate accConBalance: " << accConBalance2);
    // BOOST_CHECK_MESSAGE(accConBalance2 == 1, "expected: "<<1<<" actual: "<<accConBalance2<<"\n");

    // byteVec.clear();
    // byteVec.resize(ACC_ADDR_SIZE);
    // const std::array<unsigned char, ACC_ADDR_SIZE> & accConAddr2 = pred2.GetAccConAddr();
    // copy(accConAddr2.begin(), accConAddr2.end(), byteVec.begin());
    // LOG_PAYLOAD("Transaction2 predicate accConAddr", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(byteVec.at(8) == 16, "expected: "<<16<<" actual: "<<byteVec.at(8)<<"\n");

    // byteVec.clear();
    // byteVec.resize(1);
    // byteVec.at(0) = pred2.GetTxConOp();
    // LOG_PAYLOAD("Transaction2 predicate txConOp", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(pred2.GetTxConOp() == 1, "expected: "<<1<<" actual: "<<pred2.GetTxConOp()<<"\n");    

    // uint256_t txConAmount2 = pred2.GetTxConAmount();
    // LOG_MESSAGE("Transaction2 predicate txConAmount: " << txConAmount2);
    // BOOST_CHECK_MESSAGE(txConAmount2 == 33, "expected: "<<55<<" actual: "<<txConAmount2<<"\n");

    // byteVec.clear();
    // byteVec.resize(ACC_ADDR_SIZE);
    // const std::array<unsigned char, ACC_ADDR_SIZE> & txConToAddr2 = pred2.GetTxConToAddr();
    // copy(txConToAddr2.begin(), txConToAddr2.end(), byteVec.begin());
    // LOG_PAYLOAD("Transaction2 predicate txConToAddr", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(byteVec.at(8) == 12, "expected: "<<12<<" actual: "<<byteVec.at(8)<<"\n"); 

    // byteVec.clear();
    // byteVec.resize(ACC_ADDR_SIZE);
    // const std::array<unsigned char, ACC_ADDR_SIZE> & txConFromAddr2 = pred2.GetTxConFromAddr();
    // copy(txConFromAddr2.begin(), txConFromAddr2.end(), byteVec.begin());
    // LOG_PAYLOAD("Transaction2 predicate txConFromAddr", byteVec, Logger::MAX_BYTES_TO_DISPLAY);
    // BOOST_CHECK_MESSAGE(byteVec.at(8) == 16, "expected: "<<16<<" actual: "<<byteVec.at(8)<<"\n");
}

BOOST_AUTO_TEST_SUITE_END ()