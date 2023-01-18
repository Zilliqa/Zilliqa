/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZILLIQA_SRC_COMMON_CONSTANTS_H_
#define ZILLIQA_SRC_COMMON_CONSTANTS_H_

#include "BaseType.h"
#include "common/Hashes.h"

const size_t BLOCK_NUMERIC_DIGITS =
    std::to_string(std::numeric_limits<uint64_t>::max()).size();

// Data sizes
constexpr const unsigned int COMMON_HASH_SIZE = 32;
constexpr const unsigned int ACC_ADDR_SIZE = 20;
constexpr const unsigned int TRAN_HASH_SIZE = TxnHash::size;
static_assert(TRAN_HASH_SIZE == 32);

constexpr const unsigned int TRAN_SIG_SIZE = 64;
constexpr const unsigned int TRAN_SIG_SIZE_UNCOMPRESSED = 65;
constexpr const unsigned int BLOCK_HASH_SIZE = BlockHash::size;
static_assert(BLOCK_HASH_SIZE == 32);

constexpr const unsigned int BLOCK_SIG_SIZE = 64;
constexpr const unsigned int STATE_HASH_SIZE = StateHash::size;
static_assert(STATE_HASH_SIZE == 32);

constexpr const unsigned int RESERVED_FIELD_SIZE = 128;

// Numeric types sizes
constexpr const unsigned int UINT256_SIZE = 32;
constexpr const unsigned int UINT128_SIZE = 16;
constexpr const unsigned int INT256_SIZE = 32;

// Cryptographic sizes
constexpr const unsigned int PUB_KEY_SIZE = 33;
constexpr const unsigned int SIGNATURE_CHALLENGE_SIZE = 32;
constexpr const unsigned int SIGNATURE_RESPONSE_SIZE = 32;
constexpr const unsigned int CHALLENGE_SIZE = 32;
constexpr const unsigned int RESPONSE_SIZE = 32;

constexpr const unsigned int BLOCKCHAIN_SIZE = 50;

// Transaction body sharing

constexpr const unsigned int NUM_VACUOUS_EPOCHS = 1;

// Networking and mining
constexpr const unsigned int POW_SIZE = 32;

constexpr const unsigned int MAINNET_CHAIN_ID = 1;

// ISOLATED SERVER TOGGLE

extern bool ISOLATED_SERVER;

// Scilla flag to toggle pretty printing of literals. This decides
// whether Scilla lists are printed as JSON arrays or as regular ADTs.
// For testing, life becomes difficult to parse JSONs (because mapdepths
// isn't easily available), where we can't distinguish a JSON `[]` b/w
// Maps and Scilla lists. So we disable pretty print during testing.
extern bool SCILLA_PPLIT_FLAG;

// Testing parameters

// Metadata type
enum MetaType : unsigned char {
  STATEROOT = 0x00,
  LATESTACTIVEDSBLOCKNUM,
  WAKEUPFORUPGRADE,
  LATEST_EPOCH_STATES_UPDATED,  // [deprecated soon]
  EPOCHFIN,
  EARLIEST_HISTORY_STATE_EPOCH,
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
  GUARD_DS_SYNC,
  DB_VERIF,  // Deprecated
  SYNC_TYPE_COUNT
};

enum class ValidateState : unsigned char { IDLE = 0, INPROGRESS, DONE, ERROR };

const std::string RAND1_GENESIS =
    "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a";
const std::string RAND2_GENESIS =
    "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf";

const std::string PERSISTENCE_PATH = "/persistence";
const std::string STATEDELTAFROMS3_PATH = "/StateDeltaFromS3";

const std::string DS_KICKOUT_MSG = "KICKED OUT FROM DS";
const std::string DS_LEADER_MSG = "DS LEADER NOW";
const std::string DS_BACKUP_MSG = "DS BACKUP NOW";

const std::string dsNodeFile = "dsnodes.xml";

constexpr char SCILLA_INDEX_SEPARATOR = 0x16;

constexpr float ONE_HUNDRED_PERCENT = 100.f;

constexpr unsigned int GENESIS_DSBLOCK_VERSION = 1;

constexpr uint16_t MAX_REPUTATION =
    4096;  // This means the max priority is 12. A node need to continually
           // run for 5 days to achieve this reputation.

// General constants
extern const unsigned int DEBUG_LEVEL;
extern const bool ENABLE_DO_REJOIN;
extern bool LOOKUP_NODE_MODE;
extern const unsigned int MAX_ENTRIES_FOR_DIAGNOSTIC_DATA;
extern const uint16_t CHAIN_ID;
extern const uint16_t NETWORK_ID;
extern const std::string GENESIS_PUBKEY;
extern const std::string STORAGE_PATH;
extern const unsigned int NUM_EPOCHS_PER_PERSISTENT_DB;
extern const bool KEEP_HISTORICAL_STATE;
extern const bool ENABLE_MEMORY_STATS;
extern const unsigned int NUM_DS_EPOCHS_STATE_HISTORY;
extern const uint64_t INIT_TRIE_DB_SNAPSHOT_EPOCH;
extern const unsigned int MAX_ARCHIVED_LOG_COUNT;
extern const unsigned int MAX_LOG_FILE_SIZE_KB;
extern const bool JSON_LOGGING;

// Version constants
extern const unsigned int MSG_VERSION;
extern const unsigned int TRANSACTION_VERSION;
extern const unsigned int TRANSACTION_VERSION_ETH;
extern const unsigned int DSBLOCK_VERSION;
extern const unsigned int TXBLOCK_VERSION;
extern const unsigned int MICROBLOCK_VERSION;
extern const unsigned int VCBLOCK_VERSION;
extern const unsigned int BLOCKLINK_VERSION;
extern const unsigned int DSCOMMITTEE_VERSION;
extern const unsigned int SHARDINGSTRUCTURE_VERSION;
extern const unsigned int CONTRACT_STATE_VERSION;

// Seed Node
extern const bool ARCHIVAL_LOOKUP;
extern const unsigned int SEED_TXN_COLLECTION_TIME_IN_SEC;
extern const unsigned int TXN_STORAGE_LIMIT;
extern bool MULTIPLIER_SYNC_MODE;
extern const unsigned int SEED_SYNC_SMALL_PULL_INTERVAL;
extern const unsigned int SEED_SYNC_LARGE_PULL_INTERVAL;
extern const bool ENABLE_SEED_TO_SEED_COMMUNICATION;
extern const unsigned int P2P_SEED_CONNECT_PORT;
extern const unsigned int P2P_SEED_SERVER_CONNECTION_TIMEOUT;
extern const unsigned int FETCH_DS_BLOCK_LIMIT;

// RemoteStorageDB
extern const std::string REMOTESTORAGE_DB_HOST;
extern const std::string REMOTESTORAGE_DB_NAME;
extern const unsigned int REMOTESTORAGE_DB_PORT;
extern const unsigned int REMOTESTORAGE_DB_SERVER_SELECTION_TIMEOUT_MS;
extern const unsigned int REMOTESTORAGE_DB_SOCKET_TIMEOUT_MS;
extern const std::string REMOTESTORAGE_DB_TLS_FILE;
extern bool REMOTESTORAGE_DB_ENABLE;

// Consensus constants
extern const double TOLERANCE_FRACTION;
extern const unsigned int COMMIT_WINDOW_IN_SECONDS;
extern const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW;
extern const unsigned int CONSENSUS_OBJECT_TIMEOUT;
extern const unsigned int DS_NUM_CONSENSUS_SUBSETS;
extern const unsigned int SHARD_NUM_CONSENSUS_SUBSETS;
extern const unsigned int COMMIT_TOLERANCE_PERCENT;
extern const unsigned int SUBSET0_RESPONSE_DELAY_IN_MS;

// Data sharing constants
extern const bool BROADCAST_TREEBASED_CLUSTER_MODE;
extern const unsigned int MULTICAST_CLUSTER_SIZE;
extern const unsigned int NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD;
extern const unsigned int NUM_NODES_TO_SEND_LOOKUP;
extern const unsigned int NUM_OF_TREEBASED_CHILD_CLUSTERS;
extern const unsigned int POW_PACKET_SENDERS;
extern const unsigned int TX_SHARING_CLUSTER_SIZE;
extern const unsigned int NUM_SHARE_PENDING_TXNS;

// Dispatcher constants
extern const bool USE_REMOTE_TXN_CREATOR;
extern const unsigned int NUM_DISPATCHERS;
extern const std::string TXN_PATH;

// Epoch timing constants
extern const unsigned int DELAY_FIRSTXNEPOCH_IN_MS;
extern const unsigned int FETCHING_MISSING_DATA_TIMEOUT;
extern const unsigned int DS_ANNOUNCEMENT_DELAY_IN_MS;
extern const unsigned int SHARD_ANNOUNCEMENT_DELAY_IN_MS;
extern const unsigned int LOOKUP_DELAY_SEND_TXNPACKET_IN_MS;
extern const unsigned int MICROBLOCK_TIMEOUT;
extern const unsigned int NEW_NODE_SYNC_INTERVAL;
extern const unsigned int POW_SUBMISSION_TIMEOUT;
extern const unsigned int POW_WINDOW_IN_SECONDS;
extern const unsigned int POWPACKETSUBMISSION_WINDOW_IN_SECONDS;
extern const unsigned int RECOVERY_SYNC_TIMEOUT;
extern const unsigned int TX_DISTRIBUTE_TIME_IN_MS;
extern const unsigned int EXTRA_TX_DISTRIBUTE_TIME_IN_MS;
extern const unsigned int DS_TX_PROCESSING_TIMEOUT;
extern const unsigned int NEW_LOOKUP_SYNC_DELAY_IN_SECONDS;
extern const unsigned int GETSHARD_TIMEOUT_IN_SECONDS;
extern const unsigned int GETSTATEDELTAS_TIMEOUT_IN_SECONDS;
extern const unsigned int GETCOSIGREWARDS_TIMEOUT_IN_SECONDS;
extern const unsigned int RETRY_REJOINING_TIMEOUT;
extern const unsigned int RETRY_GETSTATEDELTAS_COUNT;
extern const unsigned int RETRY_COSIGREWARDS_COUNT;
extern const unsigned int MAX_FETCHMISSINGMBS_NUM;
extern const unsigned int LAST_N_TXBLKS_TOCHECK_FOR_MISSINGMBS;
extern const unsigned int REMOVENODEFROMBLACKLIST_DELAY_IN_SECONDS;

// Gas constants
extern const uint64_t MIN_ETH_GAS;
extern const unsigned int DS_MICROBLOCK_GAS_LIMIT;
extern const unsigned int SHARD_MICROBLOCK_GAS_LIMIT;
extern const unsigned int CONTRACT_CREATE_GAS;
extern const unsigned int CONTRACT_INVOKE_GAS;
extern const unsigned int NORMAL_TRAN_GAS;
extern const unsigned int GAS_CONGESTION_PERCENT;
extern const unsigned int UNFILLED_PERCENT_LOW;
extern const unsigned int UNFILLED_PERCENT_HIGH;
extern const uint128_t GAS_PRICE_MIN_VALUE;
extern const unsigned int GAS_PRICE_PRECISION;
extern const uint128_t PRECISION_MIN_VALUE;
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
extern const unsigned int KEEP_RAWMSG_FROM_LAST_N_ROUNDS;
extern const bool SIGN_VERIFY_EMPTY_MSGTYP;
extern const bool SIGN_VERIFY_NONEMPTY_MSGTYP;

// GPU mining constants
extern const std::string GPU_TO_USE;
extern const unsigned int OPENCL_LOCAL_WORK_SIZE;
extern const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER;
extern const unsigned int OPENCL_START_EPOCH;

// Guard mode constants
extern const bool GUARD_MODE;
extern const bool EXCLUDE_PRIV_IP;
extern const unsigned int WINDOW_FOR_DS_NETWORK_INFO_UPDATE;
extern const double SHARD_GUARD_TOL;
extern const unsigned int SHARD_LEADER_SELECT_TOL;

// Heartbeat constants
extern const unsigned int HEARTBEAT_INTERVAL_IN_SECONDS;

// RPC Constants
extern const unsigned int LOOKUP_RPC_PORT;
extern const unsigned int STAKING_RPC_PORT;
extern const unsigned int STATUS_RPC_PORT;
// EVM

extern bool ENABLE_EVM;
extern const std::string EVM_SERVER_SOCKET_PATH;
extern const std::string EVM_SERVER_BINARY;
extern const std::string EVM_LOG_CONFIG;
extern const uint64_t ETH_CHAINID;
extern const uint64_t EVM_ZIL_SCALING_FACTOR;
extern const bool LAUNCH_EVM_DAEMON;

extern const std::string IP_TO_BIND;  // Only for non-lookup nodes
extern const bool ENABLE_STAKING_RPC;
extern const bool ENABLE_STATUS_RPC;
extern const unsigned int NUM_SHARD_PEER_TO_REVEAL;
extern const std::string SCILLA_IPC_SOCKET_PATH;
extern const std::string SCILLA_SERVER_SOCKET_PATH;
extern const std::string SCILLA_SERVER_BINARY;
extern bool ENABLE_WEBSOCKET;
extern const unsigned int WEBSOCKET_PORT;
extern const bool ENABLE_GETTXNBODIESFORTXBLOCK;
extern const unsigned int NUM_TXNS_PER_PAGE;
extern const unsigned int PENDING_TXN_QUERY_NUM_EPOCHS;
extern const unsigned int PENDING_TXN_QUERY_MAX_RESULTS;
extern const bool CONNECTION_IO_USE_EPOLL;
extern const unsigned int CONNECTION_ALL_TIMEOUT;
extern const unsigned int CONNECTION_CALLBACK_TIMEOUT;

// Network composition constants
extern const unsigned int COMM_SIZE;
extern const unsigned int NUM_DS_ELECTION;
extern const double DS_PERFORMANCE_THRESHOLD_PERCENT;
extern const unsigned int NUM_DS_BYZANTINE_REMOVED;
extern const unsigned int SHARD_SIZE_TOLERANCE_LO;
extern const unsigned int SHARD_SIZE_TOLERANCE_HI;
extern const unsigned int STORE_DS_COMMITTEE_INTERVAL;

// P2PComm constants
extern const unsigned int BROADCAST_INTERVAL;
extern const unsigned int BROADCAST_EXPIRY;
extern const unsigned int FETCH_LOOKUP_MSG_MAX_RETRY;
extern const uint32_t MAXSENDMESSAGE;
extern const uint32_t MAXRECVMESSAGE;
extern const unsigned int MAXRETRYCONN;
extern const unsigned int MSGQUEUE_SIZE;
extern const unsigned int PUMPMESSAGE_MILLISECONDS;
extern const unsigned int SENDQUEUE_SIZE;
extern const unsigned int MAX_GOSSIP_MSG_SIZE_IN_BYTES;
extern const unsigned int MIN_READ_WATERMARK_IN_BYTES;
extern const unsigned int MAX_READ_WATERMARK_IN_BYTES;
extern const unsigned int BLACKLIST_NUM_TO_POP;
extern const unsigned int MAX_PEER_CONNECTION;
extern const unsigned int MAX_PEER_CONNECTION_P2PSEED;
extern const unsigned int MAX_WHITELISTREQ_LIMIT;
extern const unsigned int SENDJOBPEERS_TIMEOUT;

// PoW constants
extern const bool FULL_DATASET_MINE;
extern const bool OPENCL_GPU_MINE;
extern const bool REMOTE_MINE;
extern const std::string MINING_PROXY_URL;
extern const unsigned int MINING_PROXY_TIMEOUT_IN_MS;
extern const unsigned int MAX_RETRY_SEND_POW_TIME;
extern const unsigned int CHECK_MINING_RESULT_INTERVAL;
extern const bool GETWORK_SERVER_MINE;
extern const unsigned int GETWORK_SERVER_PORT;
extern const unsigned int DS_POW_DIFFICULTY;
extern const unsigned int POW_DIFFICULTY;
extern const unsigned int POW_BOUNDARY_N_DIVIDED;
extern const unsigned int POW_BOUNDARY_N_DIVIDED_START;
extern const unsigned int POW_SUBMISSION_LIMIT;
extern const unsigned int NUM_FINAL_BLOCK_PER_POW;
extern const unsigned int POW_CHANGE_TO_ADJ_DIFF;
extern const unsigned int POW_CHANGE_TO_ADJ_DS_DIFF;
extern const unsigned int DIFFICULTY_DIFF_TOL;
extern const unsigned int EXPECTED_SHARD_NODE_NUM;
extern const unsigned int MAX_SHARD_NODE_NUM;
extern const uint8_t MIN_NODE_REPUTATION_PRIORITY;
extern const unsigned int MISORDER_TOLERANCE_IN_PERCENT;
extern const unsigned int DSBLOCK_EXTRA_WAIT_TIME;
extern const unsigned int DIFF_IP_TOLERANCE_IN_PERCENT;
extern const unsigned int TXN_SHARD_TARGET_DIFFICULTY;
extern const unsigned int TXN_DS_TARGET_DIFFICULTY;
extern const unsigned int TXN_DS_TARGET_NUM;
extern const unsigned int PRIORITY_TOLERANCE_IN_PERCENT;
extern const bool SKIP_POW_REATTEMPT_FOR_DS_DIFF;
extern const std::string POW_SUBMISSION_VERSION_TAG;

// Recovery and upgrading constants
extern const unsigned int WAIT_LOOKUP_WAKEUP_IN_SECONDS;
extern const bool GET_INITIAL_DS_FROM_REPO;
extern const unsigned int SHARD_DELAY_WAKEUP_IN_SECONDS;
extern const unsigned int TERMINATION_COUNTDOWN_IN_SECONDS;
extern const std::string UPGRADE_HOST_ACCOUNT;
extern const std::string UPGRADE_HOST_REPO;
extern const bool REJOIN_NODE_NOT_IN_NETWORK;
extern const unsigned int RESUME_BLACKLIST_DELAY_IN_SECONDS;
extern const unsigned int INCRDB_DSNUMS_WITH_STATEDELTAS;
extern const bool CONTRACT_STATES_MIGRATED;
extern const unsigned int MAX_IPCHANGE_REQUEST_LIMIT;
extern const unsigned int MAX_REJOIN_NETWORK_ATTEMPTS;
extern const unsigned int RELEASE_CACHE_INTERVAL;
extern const unsigned int DIRBLOCK_FETCH_LIMIT;

// Smart contract constants
extern const bool ENABLE_SC;
extern std::string SCILLA_ROOT;
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
extern const std::string CONTRACT_FILE_EXTENSION;
extern const std::string LIBRARY_CODE_EXTENSION;
extern const std::string EXTLIB_FOLDER;
extern const bool ENABLE_SCILLA_MULTI_VERSION;
extern bool ENABLE_SCILLA;

extern const bool LOG_SC;
extern const bool DISABLE_SCILLA_LIB;
extern const unsigned int SCILLA_SERVER_PENDING_IN_MS;
extern unsigned int SCILLA_SERVER_LOOP_WAIT_MICROSECONDS;
const std::string FIELDS_MAP_DEPTH_INDICATOR = "_fields_map_depth";
const std::string MAP_DEPTH_INDICATOR = "_depth";
const std::string SCILLA_VERSION_INDICATOR = "_version";
const std::string TYPE_INDICATOR = "_type";
const std::string HAS_MAP_INDICATOR = "_hasmap";
const std::string CONTRACT_ADDR_INDICATOR = "_addr";

// TODO : replace the above with belows
const char FIELDS_MAP_DEPTH_SIGN = 0x10;
const char MAP_DEPTH_SIGN = 0x11;
const char SCILLA_VERSION_SIGN = 0x12;
const char TYPE_SIGN = 0x13;
const char CONTRACT_ADDR_SIGN = 0x14;

// Test constants
extern const bool ENABLE_CHECK_PERFORMANCE_LOG;
extern const unsigned int NUM_TXN_TO_SEND_PER_ACCOUNT;
extern const bool ENABLE_ACCOUNTS_POPULATING;
extern const bool UPDATE_PREGENED_ACCOUNTS;
extern const unsigned int NUM_ACCOUNTS_PREGENERATE;
extern const unsigned int PREGEN_ACCOUNT_TIMES;
extern const std::string PREGENED_ACCOUNTS_FILE;
extern const bool LOG_PARAMETERS;

// Transaction constants
extern const uint128_t TOTAL_COINBASE_REWARD;
extern const uint128_t COINBASE_REWARD_PER_DS;
extern const uint128_t TOTAL_GENESIS_TOKEN;
extern const unsigned int BASE_REWARD_IN_PERCENT;
extern const unsigned int LOOKUP_REWARD_IN_PERCENT;
extern const unsigned int MAX_CODE_SIZE_IN_BYTES;
extern const unsigned int MAX_CONTRACT_EDGES;
extern const unsigned int SCILLA_CHECKER_INVOKE_GAS;
extern const unsigned int SCILLA_RUNNER_INVOKE_GAS;
extern const unsigned int SYS_TIMESTAMP_VARIANCE_IN_SECONDS;
extern const unsigned int TXN_MISORDER_TOLERANCE_IN_PERCENT;
extern const unsigned int TXNS_MISSING_TOLERANCE_IN_PERCENT;
extern const unsigned int PACKET_EPOCH_LATE_ALLOW;
extern const unsigned int PACKET_BYTESIZE_LIMIT;
extern const unsigned int SMALL_TXN_SIZE;
extern const unsigned int ACCOUNT_IO_BATCH_SIZE;
extern const bool ENABLE_REPOPULATE;
extern const unsigned int REPOPULATE_STATE_PER_N_DS;
extern const unsigned int REPOPULATE_STATE_IN_DS;
extern const unsigned int NUM_STORE_TX_BODIES_INTERVAL;
extern const std::string BUCKET_NAME;
extern const std::string TXN_PERSISTENCE_NAME;
extern const bool ENABLE_TXNS_BACKUP;
extern const bool SHARDLDR_SAVE_TXN_LOCALLY;
extern const double BLOOM_FILTER_FALSE_RATE;
extern const unsigned int TXN_DISPATCH_ATTEMPT_LIMIT;
extern const uint64_t EVM_RPC_TIMEOUT_SECONDS;

// TxBlockAux constants
constexpr auto MAX_TX_BLOCK_NUM_KEY = "MaxTxBlockNumber";

// Viewchange constants
extern const unsigned int POST_VIEWCHANGE_BUFFER;
extern const unsigned int VIEWCHANGE_EXTRA_TIME;
extern const unsigned int VIEWCHANGE_PRECHECK_TIME;
extern const unsigned int VIEWCHANGE_TIME;

// Genesis accounts
extern const std::vector<std::string> GENESIS_WALLETS;
extern const std::vector<std::string> GENESIS_KEYS;

// Genesis accounts for ds txn dispatching ( TEST Purpose Only )
extern const std::vector<std::string> DS_GENESIS_WALLETS;
extern const std::vector<std::string> DS_GENESIS_KEYS;

// DBVerifier constants
extern const std::vector<std::pair<uint64_t, uint32_t>> VERIFIER_EXCLUSION_LIST;
extern const bool IGNORE_BLOCKCOSIG_CHECK;
extern const std::vector<std::pair<uint64_t, uint32_t>>
    VERIFIER_MICROBLOCK_EXCLUSION_LIST;

// Metrics constants
extern const std::string METRIC_ZILLIQA_HOSTNAME;
extern const std::string METRIC_ZILLIQA_PROVIDER;
extern const unsigned int METRIC_ZILLIQA_PORT;
extern const unsigned int METRIC_ZILLIQA_READER_EXPORT_MS;
extern const unsigned int METRIC_ZILLIQA_READER_TIMEOUT_MS;
extern const std::string METRIC_ZILLIQA_SCHEMA;
extern const std::string METRIC_ZILLIQA_SCHEMA_VERSION;
extern const std::string METRIC_ZILLIQA_MASK;
extern const std::string TRACE_ZILLIQA_MASK;
extern const std::string TRACE_ZILLIQA_PROVIDER;
extern const std::string TRACE_ZILLIQA_HOSTNAME;
extern const unsigned int TRACE_ZILLIQA_PORT;

#endif  // ZILLIQA_SRC_COMMON_CONSTANTS_H_
