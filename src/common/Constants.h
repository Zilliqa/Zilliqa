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

#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#include "depends/common/FixedHash.h"

using BlockHash = dev::h256;

// Data sizes
const unsigned int COMMON_HASH_SIZE = 32;
const unsigned int ACC_ADDR_SIZE = 20;
const unsigned int TRAN_HASH_SIZE = 32;
const unsigned int TRAN_SIG_SIZE = 64;
const unsigned int BLOCK_HASH_SIZE = 32;
const unsigned int BLOCK_SIG_SIZE = 64;
const unsigned int STATE_HASH_SIZE = 32;
const unsigned int RESERVED_FIELD_SIZE = 128;

// Numeric types sizes
const unsigned int UINT256_SIZE = 32;
const unsigned int UINT128_SIZE = 16;
const unsigned int INT256_SIZE = 32;

// Cryptographic sizes
const unsigned int PRIV_KEY_SIZE = 32;
const unsigned int PUB_KEY_SIZE = 33;
const unsigned int SIGNATURE_CHALLENGE_SIZE = 32;
const unsigned int SIGNATURE_RESPONSE_SIZE = 32;
const unsigned int COMMIT_SECRET_SIZE = 32;
const unsigned int COMMIT_POINT_SIZE = 33;
const unsigned int CHALLENGE_SIZE = 32;
const unsigned int RESPONSE_SIZE = 32;

const unsigned int BLOCKCHAIN_SIZE = 50;

// Number of nodes sent from lookup node to newly joined node
const unsigned int SEED_PEER_LIST_SIZE = 20;

// Transaction body sharing

const unsigned int NUM_VACUOUS_EPOCHS = 1;

// Networking and mining
const unsigned int POW_SIZE = 32;
const unsigned int IP_SIZE = 16;
const unsigned int PORT_SIZE = 4;

const unsigned int NUM_PEERS_TO_SEND_IN_A_SHARD = 20;
const unsigned int SERVER_PORT = 4201;

// Number of initial ds epoch number, including genesis epoch
const unsigned int INIT_DS_EPOCH_NUM = 2;

// Testing parameters

// Metadata type
enum MetaType : unsigned char {
  STATEROOT = 0x00,
  DSINCOMPLETED,
  LATESTACTIVEDSBLOCKNUM,
  WAKEUPFORUPGRADE,
};

// Sync Type
enum SyncType : unsigned int {
  NO_SYNC = 0,
  NEW_SYNC,
  NORMAL_SYNC,
  DS_SYNC,
  LOOKUP_SYNC,
  RECOVERY_ALL_SYNC,
  NEW_LOOKUP_SYNC,
  GUARD_DS_SYNC
};

const std::string RAND1_GENESIS =
    "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a";
const std::string RAND2_GENESIS =
    "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf";

const std::string REMOTE_TEST_DIR = "zilliqa-test";
const std::string PERSISTENCE_PATH = "persistence";
const std::string TX_BODY_SUBDIR = "txBodies";

const std::string DS_KICKOUT_MSG = "KICKED OUT FROM DS";
const std::string DS_LEADER_MSG = "DS LEADER NOW";
const std::string DS_BACKUP_MSG = "DS BACKUP NOW";

const std::string dsNodeFile = "dsnodes.xml";

const float ONE_HUNDRED_PERCENT = 100.f;

// General constants
extern const unsigned int DEBUG_LEVEL;
extern const unsigned int MSG_VERSION;
extern const bool ENABLE_DO_REJOIN;
extern const bool LOOKUP_NODE_MODE;
extern const unsigned int MAX_ENTRIES_FOR_DIAGNOSTIC_DATA;

// Archival constants
extern const bool ARCHIVAL_NODE;
extern const std::string DB_HOST;

// Seed Node
extern const bool ARCHIVAL_LOOKUP;
extern const unsigned int SEED_TXN_COLLECTION_TIME_IN_SEC;

// Consensus constants
extern const unsigned int COMMIT_WINDOW_IN_SECONDS;
extern const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW;
extern const unsigned int CONSENSUS_OBJECT_TIMEOUT;
extern const unsigned int NUM_CONSENSUS_SUBSETS;

// Data sharing constants
extern const bool BROADCAST_TREEBASED_CLUSTER_MODE;
extern const unsigned int MULTICAST_CLUSTER_SIZE;
extern const unsigned int NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD;
extern const unsigned int NUM_NODES_TO_SEND_LOOKUP;
extern const unsigned int NUM_OF_TREEBASED_CHILD_CLUSTERS;
extern const unsigned int POW_PACKET_SENDERS;
extern const unsigned int TX_SHARING_CLUSTER_SIZE;

// Dispatcher constants
extern const bool USE_REMOTE_TXN_CREATOR;
extern const std::string TXN_PATH;

// Epoch timing constants
extern const unsigned int DELAY_FIRSTXNEPOCH_IN_MS;
extern const unsigned int FETCHING_MISSING_DATA_TIMEOUT;
extern const unsigned int FINALBLOCK_DELAY_IN_MS;
extern const unsigned int LOOKUP_DELAY_SEND_TXNPACKET_IN_MS;
extern const unsigned int MICROBLOCK_TIMEOUT;
extern const unsigned int NEW_NODE_SYNC_INTERVAL;
extern const unsigned int POW_SUBMISSION_TIMEOUT;
extern const unsigned int POW_WINDOW_IN_SECONDS;
extern const unsigned int POWPACKETSUBMISSION_WINDOW_IN_SECONDS;
extern const unsigned int RECOVERY_SYNC_TIMEOUT;
extern const unsigned int TX_DISTRIBUTE_TIME_IN_MS;
extern const unsigned int NEW_LOOKUP_SYNC_DELAY_IN_SECONDS;

// Fallback constants
extern const bool ENABLE_FALLBACK;
extern const unsigned int FALLBACK_CHECK_INTERVAL;
extern const unsigned int FALLBACK_EXTRA_TIME;
extern const unsigned int FALLBACK_INTERVAL_STARTED;
extern const unsigned int FALLBACK_INTERVAL_WAITING;

// Gas constants
extern const unsigned int MICROBLOCK_GAS_LIMIT;
extern const unsigned int CONTRACT_CREATE_GAS;
extern const unsigned int CONTRACT_INVOKE_GAS;
extern const unsigned int NORMAL_TRAN_GAS;
extern const unsigned int GAS_CONGESTION_PERCENT;
extern const unsigned int UNFILLED_PERCENT_LOW;
extern const unsigned int UNFILLED_PERCENT_HIGH;
extern const boost::multiprecision::uint128_t GAS_PRICE_MIN_VALUE;
extern const unsigned int GAS_PRICE_PRECISION;
extern const boost::multiprecision::uint128_t PRECISION_MIN_VALUE;
extern const unsigned int GAS_PRICE_DROP_RATIO;
extern const unsigned int GAS_PRICE_RAISE_RATIO_LOWER;
extern const unsigned int GAS_PRICE_RAISE_RATIO_UPPER;
extern const unsigned int GAS_PRICE_TOLERANCE;
extern const unsigned int MEAN_GAS_PRICE_DS_NUM;
extern const std::string LEGAL_GAS_PRICE_IP;

// Gossip constants
extern const bool BROADCAST_GOSSIP_MODE;
extern const bool SEND_RESPONSE_FOR_LAZY_PUSH;
extern const bool GOSSIP_CUSTOM_ROUNDS_SETTINGS;
extern const unsigned int MAX_ROUNDS_IN_BSTATE;
extern const unsigned int MAX_ROUNDS_IN_CSTATE;
extern const unsigned int MAX_TOTAL_ROUNDS;
extern const unsigned int MAX_NEIGHBORS_PER_ROUND;
extern const unsigned int NUM_GOSSIP_RECEIVERS;
extern const unsigned int ROUND_TIME_IN_MS;
extern const unsigned int SIMULATED_NETWORK_DELAY_IN_MS;

// GPU mining constants
extern const std::string GPU_TO_USE;
extern const unsigned int OPENCL_LOCAL_WORK_SIZE;
extern const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER;
extern const unsigned int OPENCL_START_EPOCH;
extern const unsigned int CUDA_BLOCK_SIZE;
extern const unsigned int CUDA_GRID_SIZE;
extern const unsigned int CUDA_STREAM_NUM;
extern const unsigned int CUDA_SCHEDULE_FLAG;

// Guard mode constants
extern const bool GUARD_MODE;
extern const bool EXCLUDE_PRIV_IP;
extern const unsigned int WINDOW_FOR_DS_NETWORK_INFO_UPDATE;

// Heartbeat constants
extern const unsigned int HEARTBEAT_INTERVAL_IN_SECONDS;

// Network composition constants
extern const unsigned int COMM_SIZE;
extern const unsigned int NUM_DS_ELECTION;
extern const unsigned int SHARD_SIZE_THRESHOLD;

// P2PComm constants
extern const unsigned int BROADCAST_INTERVAL;
extern const unsigned int BROADCAST_EXPIRY;
extern const unsigned int FETCH_LOOKUP_MSG_MAX_RETRY;
extern const uint32_t MAXMESSAGE;
extern const unsigned int MAXRETRYCONN;
extern const unsigned int MSGQUEUE_SIZE;
extern const unsigned int PUMPMESSAGE_MILLISECONDS;
extern const unsigned int SENDQUEUE_SIZE;

// PoW constants
extern const bool CUDA_GPU_MINE;
extern const bool FULL_DATASET_MINE;
extern const bool OPENCL_GPU_MINE;
extern const unsigned int DS_POW_DIFFICULTY;
extern const unsigned int POW_DIFFICULTY;
extern const unsigned int POW_SUBMISSION_LIMIT;
extern const unsigned int NUM_FINAL_BLOCK_PER_POW;
extern const unsigned int POW_CHANGE_PERCENT_TO_ADJ_DIFF;
extern const unsigned int EXPECTED_SHARD_NODE_NUM;
extern const unsigned int MAX_SHARD_NODE_NUM;
extern const unsigned int MISORDER_TOLERANCE_IN_PERCENT;
extern const unsigned int DSBLOCK_EXTRA_WAIT_TIME;
extern const unsigned int DIFF_IP_TOLERANCE_IN_PERCENT;
extern const unsigned int TXN_SHARD_TARGET_DIFFICULTY;
extern const unsigned int TXN_DS_TARGET_DIFFICULTY;
extern const unsigned int PRIORITY_TOLERANCE_IN_PERCENT;

// Recovery and upgrading constants
extern const unsigned int WAIT_LOOKUP_WAKEUP_IN_SECONDS;
extern const bool GET_INITIAL_DS_FROM_REPO;
extern const unsigned int SHARD_DELAY_WAKEUP_IN_SECONDS;
extern const unsigned int TERMINATION_COUNTDOWN_IN_SECONDS;
extern const std::string UPGRADE_HOST_ACCOUNT;
extern const std::string UPGRADE_HOST_REPO;
extern const bool RECOVERY_TRIM_INCOMPLETED_BLOCK;
extern const bool REJOIN_NODE_NOT_IN_NETWORK;

// Smart contract constants
extern const std::string SCILLA_ROOT;
extern const std::string SCILLA_CHECKER;
extern const std::string SCILLA_BINARY;
extern const std::string SCILLA_FILES;
extern const std::string SCILLA_LOG;
extern const std::string SCILLA_LIB;
extern const std::string INIT_JSON;
extern const std::string INPUT_STATE_JSON;
extern const std::string INPUT_BLOCKCHAIN_JSON;
extern const std::string INPUT_MESSAGE_JSON;
extern const std::string OUTPUT_JSON;
extern const std::string INPUT_CODE;

// Test constants
extern const bool ENABLE_CHECK_PERFORMANCE_LOG;
#ifdef FALLBACK_TEST
extern const unsigned int FALLBACK_TEST_EPOCH;
#endif  // FALLBACK_TEST
extern const unsigned int NUM_TXN_TO_SEND_PER_ACCOUNT;

// Transaction constants
extern const boost::multiprecision::uint128_t COINBASE_REWARD;
extern const unsigned int LOOKUP_REWARD_IN_PERCENT;
extern const unsigned int MAX_CODE_SIZE_IN_BYTES;
extern const unsigned int MAX_CONTRACT_DEPTH;
extern const unsigned int SYS_TIMESTAMP_VARIANCE_IN_SECONDS;
extern const unsigned int TXN_MISORDER_TOLERANCE_IN_PERCENT;
extern const unsigned int PACKET_EPOCH_LATE_ALLOW;

// Viewchange constants
extern const unsigned int POST_VIEWCHANGE_BUFFER;
extern const unsigned int VIEWCHANGE_EXTRA_TIME;
extern const unsigned int VIEWCHANGE_PRECHECK_TIME;
extern const unsigned int VIEWCHANGE_TIME;

// Genesis accounts
extern const std::vector<std::string> GENESIS_WALLETS;
extern const std::vector<std::string> GENESIS_KEYS;

#endif  // __CONSTANTS_H__
