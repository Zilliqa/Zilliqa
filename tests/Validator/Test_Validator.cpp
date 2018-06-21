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

#include "libCrypto/Schnorr.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Peer.h"
#include "libUtils/Logger.h"
#include "libValidator/Validator.h"

#define BOOST_TEST_MODULE validatortest
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <functional>
#include <iostream>
#include <vector>

using namespace std;

BOOST_AUTO_TEST_SUITE(validatortest)

class TestBench : public MediatorView, public AccountStoreView
{
public:
    Validator validator;

    unsigned int shardId = 1;

    unsigned int numShards = 5;

    typedef std::map<Address, boost::multiprecision::uint256_t> AddressToUint;

    AddressToUint balances;

    AddressToUint nonces;

    Address expectedAddAccountAddress;

    bool accountAdded = false;

    static Address createDummyAddress(int id)
    {
        Address addr;
        for (unsigned int i = 0; i < addr.asArray().size(); i++)
        {
            addr.asArray().at(i) = i + 4 * id;
        }
        return addr;
    }

    TestBench()
        : validator(*this, *this)
    {
        validator.name(); // Coverage
    }

    void initADummyAccount(const Address& address)
    {
        balances[address] = 0;
        nonces[address] = 0xdeadbeef;
    }

    virtual ~TestBench() {}

    virtual unsigned int getShardID() const override { return shardId; }

    virtual unsigned int getNumShards() const override { return numShards; }

    virtual string currentEpochNumAsString() const override { return "42"; }

    virtual bool DoesAccountExist(const Address& address) override
    {
        return balances.find(address) != balances.end();
    }

    virtual void AddAccount(const Address& address,
                            const Account& account) override
    {
        BOOST_REQUIRE(address == expectedAddAccountAddress);
        accountAdded = true;
    }

    virtual boost::multiprecision::uint256_t
    GetBalance(const Address& address) override
    {
        return balances.at(address);
    }

    virtual boost::multiprecision::uint256_t
    GetNonce(const Address& address) override
    {
        return nonces.at(address);
    }
};

bool testRound(int round, bool checkNonce)
{
    TestBench tb;

    const Address toAddr = tb.createDummyAddress(1);
    const KeyPair sender = Schnorr::GetInstance().GenKeyPair();
    const Address fromAddr = Account::GetAddressFromPublicKey(sender.second);
    const unsigned int shard
        = Transaction::GetShardIndex(fromAddr, tb.numShards);

    const boost::multiprecision::uint256_t txAmount = 55;
    boost::multiprecision::uint256_t nonce = 5;

    // Round 0. Failing because of invalid shared

    tb.shardId = round > 0 ? shard : (shard + 1);

    // Round 1. Failing because of unknown 'from' address

    if (round > 1)
    {
        // Make the 'from' address exist
        tb.initADummyAccount(fromAddr);
    }

    // Round 2. [If  Nonce Checking] Tx nonce not in line with
    //                        account state (for non-existent account)
    //          [If !Nonce Checking] It will fail with round 3's cause

    if (round > 2)
    {
        // Fix the nonce
        nonce = tb.nonces.at(fromAddr) + 1;
    }

    // Round 3. Failing of insufficient funds, BUT more importantly,
    //          we expect it to create a 'toAddr' account.
    const bool expectedAccountAdded
        = (round == 3) || (!checkNonce && round == 2);
    tb.expectedAddAccountAddress = toAddr;

    if (round > 3)
    {
        // Make the 'toAddr' exist on this shard
        tb.initADummyAccount(toAddr);
    }

    // Round 4. Failing of insufficient funds

    if (round > 4)
    {
        // Set sufficient funds on the 'from' address
        tb.balances.at(fromAddr) = txAmount;
    }

    // Round 5. Success
    const bool expectedSuccess = (round >= 5);

    // Do the actual check
    Transaction tx(1, nonce, toAddr, sender, txAmount, 11, 22, {0x33}, {0x44});

#ifndef IS_LOOKUP_NODE
    const bool success
        = (checkNonce ? tb.validator.CheckCreatedTransactionFromLookup(tx)
                      : tb.validator.CheckCreatedTransaction(tx));

    BOOST_REQUIRE(success == expectedSuccess);

    BOOST_REQUIRE(tb.accountAdded == expectedAccountAdded);
#else // IS_LOOKUP_NODE
    (void)tx;
    (void)expectedAccountAdded;
    (void)expectedSuccess;
#endif // IS_LOOKUP_NODE

    return expectedSuccess;
}

BOOST_AUTO_TEST_CASE(validator_baseline)
{
    INIT_STDOUT_LOGGER();

    for (bool checkNonce : {false, true})
    {
        for (int last = 0, round = 0; !last; ++round)
        {
            LOG_GENERAL(INFO,
                        "Test round=" << round
                                      << " with checkNonce=" << checkNonce);

            last = testRound(round, checkNonce);
        }
    }
}

BOOST_AUTO_TEST_CASE(validator_nonce_corner_cases)
{
    INIT_STDOUT_LOGGER();
#ifndef IS_LOOKUP_NODE
    TestBench tb;

    const Address toAddr = tb.createDummyAddress(1);
    const KeyPair sender = Schnorr::GetInstance().GenKeyPair();
    const Address fromAddr = Account::GetAddressFromPublicKey(sender.second);
    tb.shardId = Transaction::GetShardIndex(fromAddr, tb.numShards);
    tb.initADummyAccount(fromAddr);
    tb.initADummyAccount(toAddr);

    const boost::multiprecision::uint256_t txAmount = 55;
    boost::multiprecision::uint256_t nonce = tb.nonces.at(fromAddr) + 1;

    tb.balances.at(fromAddr) = 3 * txAmount;

    Transaction tx1(1, nonce, toAddr, sender, txAmount, 11, 22, {0x33}, {0x44});
    BOOST_REQUIRE(tb.validator.CheckCreatedTransactionFromLookup(tx1));
    // Twice the same, fails
    BOOST_REQUIRE(!tb.validator.CheckCreatedTransactionFromLookup(tx1));

    // Increase nonce and it should work
    ++nonce;
    Transaction tx2(1, nonce, toAddr, sender, txAmount, 11, 22, {0x33}, {0x44});
    BOOST_REQUIRE(tb.validator.CheckCreatedTransactionFromLookup(tx2));

    tb.validator.CleanVariables();
    // After cleaning, this won't work
    ++nonce;
    Transaction tx3(1, nonce, toAddr, sender, txAmount, 11, 22, {0x33}, {0x44});
    BOOST_REQUIRE(!tb.validator.CheckCreatedTransactionFromLookup(tx3));

    // But if we reset nonce, again, it will work
    nonce = tb.nonces.at(fromAddr) + 1;
    Transaction tx4(1, nonce, toAddr, sender, txAmount, 11, 22, {0x33}, {0x44});
    BOOST_REQUIRE(tb.validator.CheckCreatedTransactionFromLookup(tx4));
#endif // IS_LOOKUP_NODE
}

BOOST_AUTO_TEST_CASE(exercise_utilities_for_coverage)
{
    INIT_STDOUT_LOGGER();

#ifndef IS_LOOKUP_NODE
    DefaultAccountStoreView dasv;
    Address dummy(TestBench::createDummyAddress(1));

    dasv.DoesAccountExist(dummy);
    dasv.AddAccount(dummy, {0, 0});
    dasv.GetBalance(dummy);
    dasv.GetNonce(dummy);
#endif // IS_LOOKUP_NODE

    Mediator m(Schnorr::GetInstance().GenKeyPair(),
               Peer(std::rand(), std::rand()));
    MediatorAdapter ma(m);
    ma.currentEpochNumAsString();
}

BOOST_AUTO_TEST_SUITE_END()
