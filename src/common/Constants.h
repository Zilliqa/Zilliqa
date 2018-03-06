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

#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#include <cstring>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "depends/common/FixedHash.h"

using BlockHash = dev::h256;

// Data sizes
const unsigned int ACC_ADDR_SIZE = 20;
const unsigned int TRAN_HASH_SIZE = 32;
const unsigned int TRAN_SIG_SIZE = 64;
const unsigned int BLOCK_HASH_SIZE = 32;
const unsigned int BLOCK_SIG_SIZE = 64;

// Numeric types sizes
const unsigned int UINT256_SIZE = 32;
const unsigned int UINT128_SIZE = 16;

// Cryptographic sizes
const unsigned int PRIV_KEY_SIZE = 32;
const unsigned int PUB_KEY_SIZE = 33;
const unsigned int SIGNATURE_CHALLENGE_SIZE = 32;
const unsigned int SIGNATURE_RESPONSE_SIZE = 32;
const unsigned int COMMIT_SECRET_SIZE = 32;
const unsigned int COMMIT_POINT_SIZE = 33;
const unsigned int CHALLENGE_SIZE = 32;
const unsigned int RESPONSE_SIZE = 32;

// Acount related sizes
const unsigned int ACCOUNT_SIZE = ACC_ADDR_SIZE + PUB_KEY_SIZE + UINT256_SIZE + UINT256_SIZE;

const unsigned int DS_BLOCKCHAIN_SIZE = 50;
const unsigned int TX_BLOCKCHAIN_SIZE = 50;

// Number of nodes sent from lookup node to newly joined node
const unsigned int SEED_PEER_LIST_SIZE = 20;

// Transaction body sharing
const unsigned int TX_SHARING_CLUSTER_SIZE = 20;

const unsigned int NUM_VACUOUS_EPOCHS = 1;

// Networking and mining 
const unsigned int POW_SIZE = 32;
const unsigned int IP_SIZE = 16;
const unsigned int PORT_SIZE = 4;


// Testing parameters


const std::string RAND1_GENESIS = "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a";
const std::string RAND2_GENESIS = "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf";

static unsigned int ReadFromConstantsFile(std::string propertyName)
{
    // Populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml("constants.xml", pt);

    return pt.get<unsigned int>("node.constants." + propertyName);
}

const unsigned int DS_MULTICAST_CLUSTER_SIZE(ReadFromConstantsFile("DS_MULTICAST_CLUSTER_SIZE"));
const unsigned int COMM_SIZE(ReadFromConstantsFile("COMM_SIZE"));
static const unsigned int MAX_POW1_WINNERS(ReadFromConstantsFile("MAX_POW1_WINNERS"));
static const unsigned int POW1_WINDOW_IN_SECONDS(ReadFromConstantsFile("POW1_WINDOW_IN_SECONDS"));
static const unsigned int POW1_BACKUP_WINDOW_IN_SECONDS(
	ReadFromConstantsFile("POW1_BACKUP_WINDOW_IN_SECONDS"));
static const unsigned int LEADER_SHARDING_PREPARATION_IN_SECONDS(
	ReadFromConstantsFile("LEADER_SHARDING_PREPARATION_IN_SECONDS"));
static const unsigned int LEADER_POW2_WINDOW_IN_SECONDS(
	ReadFromConstantsFile("LEADER_POW2_WINDOW_IN_SECONDS"));
static const unsigned int BACKUP_POW2_WINDOW_IN_SECONDS(
	ReadFromConstantsFile("BACKUP_POW2_WINDOW_IN_SECONDS"));
static const unsigned int NEW_NODE_POW2_TIMEOUT_IN_SECONDS(
	ReadFromConstantsFile("NEW_NODE_POW2_TIMEOUT_IN_SECONDS"));
static const unsigned int POW_SUB_BUFFER_TIME(ReadFromConstantsFile("POW_SUB_BUFFER_TIME")); //milliseconds
static const unsigned int POW1_DIFFICULTY(ReadFromConstantsFile("POW1_DIFFICULTY"));
static const unsigned int POW2_DIFFICULTY(ReadFromConstantsFile("POW2_DIFFICULTY"));
static const unsigned int NUM_FINAL_BLOCK_PER_POW(ReadFromConstantsFile("NUM_FINAL_BLOCK_PER_POW"));
static const uint32_t MAXMESSAGE(ReadFromConstantsFile("MAXMESSAGE"));

#endif // __CONSTANTS_H__