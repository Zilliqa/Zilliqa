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

#define BOOST_TEST_MODULE ConsensusBackup
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libConsensus/ConsensusBackup.h"
#include "common/Messages.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(ConsensusBackupTestSuite)

/**
* \brief DSworkflow test case for class ConsensusLeader
*
* \details Leader is instationted, consensus started and message processing state machine tested
*/
BOOST_AUTO_TEST_CASE(ConsensusBackup_DSworkflow)
{
    ///Test initialization
    BOOST_TEST_MESSAGE("Running test ConsensusBackup_DSworkflow");

    ///Generated private and public keys
    vector<unsigned char> privkey_vec(32);
    vector<unsigned char> pubkey_vec(33);

    string in("03D2844A78C799551D34CB699D110CFADA7A473A9B725A918635B8EF3C26AF1668");
    size_t len = in.length();
    for(size_t i = 0; i < len; i += 2) {
        std::istringstream instream(in.substr(i, 2));
        uint8_t x;
        instream >> std::hex >> x;
        privkey_vec.push_back(x);
    }
    in = "03D2844A78C799551D34CB699D110CFADA7A473A9B725A918635B8EF3C26AF1668";
    len = in.length();
    for(size_t i = 0; i < len; i += 2) {
        std::istringstream instream(in.substr(i, 2));
        uint8_t x;
        instream >> std::hex >> x;
        pubkey_vec.push_back(x);
    }
    [[gnu::unused]]PrivKey dummy_privkey(privkey_vec, 0); // leader's private key
    [[gnu::unused]]PubKey dummy_pubkey(pubkey_vec, 0); // leader's public key

    [[gnu::unused]]uint32_t dummy_consensus_id = 0; // unique identifier for this consensus session
    [[gnu::unused]]uint16_t dummy_block_number = 0;
    [[gnu::unused]]vector<unsigned char> dummy_block_hash(BLOCK_HASH_SIZE);
    fill(dummy_block_hash.begin(), dummy_block_hash.end(), 0x88);

    [[gnu::unused]]uint16_t dummy_node_id = 0; // backup's identifier (= index in some ordered lookup table shared by all nodes)
    [[gnu::unused]]uint16_t dummy_leader_id = 0; // leader's identifier (= index in some ordered lookup table shared by all nodes)

    boost::multiprecision::uint128_t dummy_ipAddress = 3232235521; // 192.168.0.1 encoded by https://www.ipaddressguide.com/ip
    Peer dummy_peer(dummy_ipAddress, 4444); // ip , port;
    [[gnu::unused]]deque<std::pair<PubKey, Peer>> dummy_committee; //(pubkey&(),peer&()); // ordered lookup table of pubkeys for this committee (includes leader)
    pair<PubKey, Peer> dummy_pair(dummy_pubkey, dummy_peer);
    dummy_committee.push_back(dummy_pair);
    dummy_committee.push_back(dummy_pair);

    [[gnu::unused]]unsigned char dummy_class_byte = 0; // class byte representing Ex =  = 0x00ecutable class using this instance of ConsensusLeader
    [[gnu::unused]]unsigned char dummy_ins_byte = 0; // instruction byte representing consensus messages for the Executable class

    [[gnu::unused]]auto func = [&]([[gnu::unused]]const vector<unsigned char>& input, [[gnu::unused]]unsigned int offset,
        [[gnu::unused]]vector<unsigned char>& errorMsg,
        [[gnu::unused]]const uint32_t consensusID, [[gnu::unused]]const uint64_t blockNumber,
        [[gnu::unused]]const vector<unsigned char>& blockHash,
        [[gnu::unused]]const uint16_t leaderID, [[gnu::unused]]const PubKey& leaderKey,
        [[gnu::unused]]vector<unsigned char>& messageToCosign) mutable -> bool {
        return true;
    };
/*
    std::shared_ptr<ConsensusCommon> dummy_consensusObjectBackup;
    dummy_consensusObjectBackup.reset(new ConsensusBackup(
            dummy_consensus_id, dummy_block_number, dummy_block_hash,
            dummy_nclang-format-7clang-format-7   ode_id, dummy_leader_id, dummy_privkey,
            dummy_committee, static_cast<unsigned char>(DIRECTORY),
            static_cast<unsigned char>(DSBLOCKCONSENSUS), func));
*/
/*
    [[gnu::unused]]ConsensusBackup* dummy_backup = dynamic_cast<ConsensusBackup*>(dummy_consensusObject.get());

    //ProcessMessage test
    vector<unsigned char> test_message(48);
    fill(test_message.begin(), test_message.end(), 0x00);
    //Message COMMIT
    test_message[0] = 0x01;
    BOOST_CHECK_EQUAL(dummy_backup->ProcessMessage(test_message, 0, dummy_peer), false);

    //Message COMMITFAILURE
    test_message[0] = 0x02;
    BOOST_CHECK_EQUAL(dummy_backup->ProcessMessage(test_message, 0, dummy_peer), false);

    //Message RESPONSE
    test_message[0] = 0x03;
    BOOST_CHECK_EQUAL(dummy_backup->ProcessMessage(test_message, 0, dummy_peer), false);

    //Message FINALCOMMIT
    test_message[0] = 0x04;
    BOOST_CHECK_EQUAL(dummy_backup->ProcessMessage(test_message, 0, dummy_peer), false);

    //Message FINALRESPONSE
    test_message[0] = 0x05;
    BOOST_CHECK_EQUAL(dummy_backup->ProcessMessage(test_message, 0, dummy_peer), false);
*/
}
BOOST_AUTO_TEST_SUITE_END()
