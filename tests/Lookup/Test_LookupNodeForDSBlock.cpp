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

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE lookupnodedsblocktest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(lookupnodedsblocktest)

BOOST_AUTO_TEST_CASE(testDSBlockStoring)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    uint32_t listen_port = 5000;
    struct in_addr ip_addr;
    inet_aton("127.0.0.1", &ip_addr);
    Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

    vector<unsigned char> dsblockmsg
        = {MessageType::NODE, NodeInstructionType::DSBLOCK};
    unsigned int curr_offset = MessageOffset::BODY;

    BlockHash prevHash1;

    for (unsigned int i = 0; i < prevHash1.asArray().size(); i++)
    {
        prevHash1.asArray().at(i) = i + 1;
    }

    std::pair<PrivKey, PubKey> pubKey1 = Schnorr::GetInstance().GenKeyPair();

    DSBlock dsblock(DSBlockHeader(20, prevHash1, 12344, pubKey1.first,
                                  pubKey1.second, 0, 789, 0),
                    CoSignatures());

    curr_offset += dsblock.Serialize(dsblockmsg, curr_offset);

    dsblockmsg.resize(curr_offset + 32);
    Serializable::SetNumber<uint256_t>(dsblockmsg, curr_offset, 0,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    struct sockaddr_in localhost;
    inet_aton("127.0.0.1", &localhost.sin_addr);

    dsblockmsg.resize(curr_offset + 16);
    Serializable::SetNumber<uint128_t>(dsblockmsg, curr_offset,
                                       (uint128_t)localhost.sin_addr.s_addr,
                                       UINT128_SIZE);
    curr_offset += UINT128_SIZE;

    dsblockmsg.resize(curr_offset + 4);
    Serializable::SetNumber<uint32_t>(dsblockmsg, curr_offset, (uint32_t)5001,
                                      4);
    curr_offset += 4;

    P2PComm::GetInstance().SendMessage(lookup_node, dsblockmsg);
}

BOOST_AUTO_TEST_CASE(testDSBlockRetrieval)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    long long i = 0;
    for (; i < 1000000000; i++)
    {
        ;
    }
    LOG_GENERAL(INFO, i);

    uint32_t listen_port = 5000;
    struct in_addr ip_addr;
    inet_aton("127.0.0.1", &ip_addr);
    Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

    vector<unsigned char> getDSBlockMessage
        = {MessageType::LOOKUP, LookupInstructionType::GETDSBLOCKFROMSEED};
    unsigned int curr_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset, 0,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint256_t>(getDSBlockMessage, curr_offset, 1,
                                       UINT256_SIZE);
    curr_offset += UINT256_SIZE;

    Serializable::SetNumber<uint32_t>(getDSBlockMessage, curr_offset, 5000,
                                      sizeof(uint32_t));
    curr_offset += sizeof(uint32_t);

    P2PComm::GetInstance().SendMessage(lookup_node, getDSBlockMessage);
}

// BOOST_AUTO_TEST_CASE (testTxBlockRetrieval)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     uint32_t listen_port = 5000;
//     struct in_addr ip_addr;
//     inet_aton("127.0.0.1", &ip_addr);
//     Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

//     vector<unsigned char> txblockmsg = { MessageType::NODE, NodeInstructionType::FINALBLOCK };
//     unsigned int curr_offset = MessageOffset::BODY;

//     // 32-byte DS blocknum
//     Serializable::SetNumber<uint256_t>(txblockmsg, curr_offset, 0, sizeof(uint256_t));
//     curr_offset += sizeof(uint256_t);

//     // 4-byte consensusid
//     Serializable::SetNumber<uint32_t>(txblockmsg, curr_offset, 0, sizeof(uint32_t));
//     curr_offset += sizeof(uint32_t);

//     // shard-id
//     txblockmsg.resize(curr_offset + 1);
// 	Serializable::SetNumber<uint8_t>(txblockmsg, curr_offset, (uint8_t) 0, 1);
//     curr_offset += 1;

//     // std::array<unsigned char, TRAN_HASH_SIZE> emptyHash = {0};

//     std::pair<PrivKey, PubKey> pubKey1 = Schnorr::GetInstance().GenKeyPair();

//     TxBlockHeader header(TXBLOCKTYPE::FINAL, BLOCKVERSION::VERSION1, 1, 1, BlockHash(), 0,
//                             get_time_as_int(), TxnHash(), 0, 5, pubKey1.second, 0, BlockHash());

//     array<unsigned char, BLOCK_SIG_SIZE> emptySig = { 0 };

//     std::vector<TxnHash> tranHashes;

//     for(int i=0; i<5; i++)
//     {
//         tranHashes.push_back(TxnHash());
//     }

//     TxBlock txblock(header, emptySig, tranHashes);

//     curr_offset += txblock.Serialize(txblockmsg, curr_offset);

//    	P2PComm::GetInstance().SendMessage(lookup_node, txblockmsg);

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value from DB not equal to inserted value");
// }

// BOOST_AUTO_TEST_CASE (testTxBodyRetrieval)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     uint32_t listen_port = 5000;
//     struct in_addr ip_addr;
//     inet_aton("127.0.0.1", &ip_addr);
//     Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

//     vector<unsigned char> txbodymsg = { MessageType::NODE, NodeInstructionType::FORWARDTRANSACTION };
//     unsigned int curr_offset = MessageOffset::BODY;

//     txbodymsg.resize(curr_offset + UINT256_SIZE);
// 	Serializable::SetNumber<uint256_t>(txbodymsg, curr_offset, (uint8_t) 0, UINT256_SIZE);
//     curr_offset += UINT256_SIZE;

//     for (unsigned int i = 0; i < 3; i++)
//     {
//     	Address addr;
//     	array<unsigned char, BLOCK_SIG_SIZE> sign;
//     	Transaction transaction(0, i, addr, addr, i+1, sign);
//         transaction.Serialize(txbodymsg, curr_offset);
//         curr_offset += Transaction::GetSerializedSize();
//     }

//    	P2PComm::GetInstance().SendMessage(lookup_node, txbodymsg);

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value from DB not equal to inserted value");
// }

BOOST_AUTO_TEST_SUITE_END()
