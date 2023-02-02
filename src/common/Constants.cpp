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
#include "Constants.h"
#include "libUtils/SafeMath.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std;

using boost::property_tree::ptree;

struct PTree {
  static ptree& GetInstance() {
    static ptree pt;
    read_xml("constants.xml", pt);

    return pt;
  }
  PTree() = delete;
  ~PTree() = delete;
};

unsigned int ReadConstantNumeric(const string& propertyName,
                                 const char* path = "node.general.",
                                 unsigned int defaultVal = 0) {
  try {
    auto pt = PTree::GetInstance();
    return pt.get<unsigned int>(path + propertyName);
  } catch (exception& e) {
    return defaultVal;
  }
}

double ReadConstantDouble(const string& propertyName,
                          const char* path = "node.general.") {
  auto pt = PTree::GetInstance();
  return pt.get<double>(path + propertyName);
}

string ReadConstantString(const string& propertyName,
                          const char* path = "node.general.",
                          const char* defaultVal = "defaulted") {
  auto pt = PTree::GetInstance();

  try {
    return pt.get<string>(path + propertyName);
  } catch (exception& e) {
    return defaultVal;
  }
}

uint64_t ReadConstantUInt64(const string& propertyName,
                            const char* path = "node.general.",
                            uint64_t defaultVal = 0) {
  auto pt = PTree::GetInstance();
  try {
    const auto& _str = pt.get<string>(path + propertyName);
    return strtoull(_str.c_str(), NULL, 0);
  } catch (exception& e) {
    return defaultVal;
  }
}

const vector<string> ReadAccountsFromConstantsFile(
    const string& propName, const string& path = "node.accounts") {
  auto pt = PTree::GetInstance();
  vector<string> result;
  for (auto& acc : pt.get_child(path)) {
    auto child = acc.second.get_optional<string>(propName);
    if (child) {
      result.push_back(child.get());
    }
  }
  return result;
}

const vector<pair<uint64_t, uint32_t>>
ReadVerifierExclusionListFromConstantsFile() {
  auto pt = PTree::GetInstance();
  vector<pair<uint64_t, uint32_t>> result;
  for (auto& entry : pt.get_child("node.verifier.exclusion_list")) {
    result.emplace_back(make_pair(entry.second.get<uint64_t>("TXBLOCK"),
                                  entry.second.get<uint32_t>("MICROBLOCK")));
  }
  return result;
}

const vector<pair<uint64_t, uint32_t>>
ReadVerifierMicroblockExclusionListFromConstantsFile() {
  auto pt = PTree::GetInstance();
  vector<pair<uint64_t, uint32_t>> result;
  for (auto& entry : pt.get_child("node.verifier.microblock_exclusion_list")) {
    result.emplace_back(make_pair(entry.second.get<uint64_t>("TXBLOCK"),
                                  entry.second.get<uint32_t>("MICROBLOCK")));
  }
  return result;
}

bool ISOLATED_SERVER = false;

bool SCILLA_PPLIT_FLAG = true;

// General constants
const unsigned int DEBUG_LEVEL{ReadConstantNumeric("DEBUG_LEVEL")};
const bool ENABLE_DO_REJOIN{ReadConstantString("ENABLE_DO_REJOIN") == "true"};
bool LOOKUP_NODE_MODE{ReadConstantString("LOOKUP_NODE_MODE") == "true"};
const unsigned int MAX_ENTRIES_FOR_DIAGNOSTIC_DATA{
    ReadConstantNumeric("MAX_ENTRIES_FOR_DIAGNOSTIC_DATA")};
const uint16_t CHAIN_ID{(uint16_t)ReadConstantNumeric("CHAIN_ID")};
const uint16_t NETWORK_ID{(uint16_t)ReadConstantNumeric("NETWORK_ID")};
const string GENESIS_PUBKEY{
    ReadConstantString("GENESIS_PUBKEY", "node.general.")};
const string STORAGE_PATH{ReadConstantString("STORAGE_PATH", "node.general.")};
const unsigned int NUM_EPOCHS_PER_PERSISTENT_DB{
    ReadConstantNumeric("NUM_EPOCHS_PER_PERSISTENT_DB")};
const bool KEEP_HISTORICAL_STATE{ReadConstantString("KEEP_HISTORICAL_STATE") ==
                                 "true"};
const bool ENABLE_MEMORY_STATS{ReadConstantString("ENABLE_MEMORY_STATS") ==
                               "true"};
const unsigned int NUM_DS_EPOCHS_STATE_HISTORY{
    ReadConstantNumeric("NUM_DS_EPOCHS_STATE_HISTORY")};

const uint64_t INIT_TRIE_DB_SNAPSHOT_EPOCH{
    ReadConstantUInt64("INIT_TRIE_DB_SNAPSHOT_EPOCH")};

const unsigned int MAX_ARCHIVED_LOG_COUNT{
    ReadConstantNumeric("MAX_ARCHIVED_LOG_COUNT")};
const unsigned int MAX_LOG_FILE_SIZE_KB{
    ReadConstantNumeric("MAX_LOG_FILE_SIZE_KB")};
const bool JSON_LOGGING{ReadConstantString("JSON_LOGGING") == "true"};

// Version constants
const unsigned int MSG_VERSION{
    ReadConstantNumeric("MSG_VERSION", "node.version.")};
const unsigned int TRANSACTION_VERSION{
    ReadConstantNumeric("TRANSACTION_VERSION", "node.version.")};
const unsigned int TRANSACTION_VERSION_ETH = 2;
const unsigned int DSBLOCK_VERSION{
    ReadConstantNumeric("DSBLOCK_VERSION", "node.version.")};
const unsigned int TXBLOCK_VERSION{
    ReadConstantNumeric("TXBLOCK_VERSION", "node.version.")};
const unsigned int MICROBLOCK_VERSION{
    ReadConstantNumeric("MICROBLOCK_VERSION", "node.version.")};
const unsigned int VCBLOCK_VERSION{
    ReadConstantNumeric("VCBLOCK_VERSION", "node.version.")};
const unsigned int BLOCKLINK_VERSION{
    ReadConstantNumeric("BLOCKLINK_VERSION", "node.version.")};
const unsigned int DSCOMMITTEE_VERSION{
    ReadConstantNumeric("DSCOMMITTEE_VERSION", "node.version.")};
const unsigned int SHARDINGSTRUCTURE_VERSION{
    ReadConstantNumeric("SHARDINGSTRUCTURE_VERSION", "node.version.")};
const unsigned int CONTRACT_STATE_VERSION{
    ReadConstantNumeric("CONTRACT_STATE_VERSION", "node.version.")};

// Seed constans
const bool ARCHIVAL_LOOKUP{
    ReadConstantString("ARCHIVAL_LOOKUP", "node.seed.") == "true"};
const unsigned int SEED_TXN_COLLECTION_TIME_IN_SEC{
    ReadConstantNumeric("SEED_TXN_COLLECTION_TIME_IN_SEC", "node.seed.")};
const unsigned int TXN_STORAGE_LIMIT{
    ReadConstantNumeric("TXN_STORAGE_LIMIT", "node.seed.")};
bool MULTIPLIER_SYNC_MODE = true;
const unsigned int SEED_SYNC_SMALL_PULL_INTERVAL{
    ReadConstantNumeric("SEED_SYNC_SMALL_PULL_INTERVAL", "node.seed.")};
const unsigned int SEED_SYNC_LARGE_PULL_INTERVAL{
    ReadConstantNumeric("SEED_SYNC_LARGE_PULL_INTERVAL", "node.seed.")};
const bool ENABLE_SEED_TO_SEED_COMMUNICATION{
    ReadConstantString("ENABLE_SEED_TO_SEED_COMMUNICATION", "node.seed.") ==
    "true"};
const unsigned int P2P_SEED_CONNECT_PORT{
    ReadConstantNumeric("P2P_SEED_CONNECT_PORT", "node.seed.")};

const unsigned int P2P_SEED_SERVER_CONNECTION_TIMEOUT{
    ReadConstantNumeric("P2P_SEED_SERVER_CONNECTION_TIMEOUT", "node.seed.")};

const unsigned int FETCH_DS_BLOCK_LIMIT{
    ReadConstantNumeric("FETCH_DS_BLOCK_LIMIT", "node.seed.")};

// RemotestorageDB constants
const string REMOTESTORAGE_DB_HOST{
    ReadConstantString("REMOTESTORAGE_DB_HOST", "node.remotestorageDB.")};
const string REMOTESTORAGE_DB_NAME{
    ReadConstantString("REMOTESTORAGE_DB_NAME", "node.remotestorageDB.")};
const unsigned int REMOTESTORAGE_DB_PORT{
    ReadConstantNumeric("REMOTESTORAGE_DB_PORT", "node.remotestorageDB.")};
const unsigned int REMOTESTORAGE_DB_SERVER_SELECTION_TIMEOUT_MS{
    ReadConstantNumeric("REMOTESTORAGE_DB_SERVER_SELECTION_TIMEOUT_MS",
                        "node.remotestorageDB.")};
const unsigned int REMOTESTORAGE_DB_SOCKET_TIMEOUT_MS{ReadConstantNumeric(
    "REMOTESTORAGE_DB_SOCKET_TIMEOUT_MS", "node.remotestorageDB.")};
const string REMOTESTORAGE_DB_TLS_FILE{
    ReadConstantString("REMOTESTORAGE_DB_TLS_FILE", "node.remotestorageDB.")};
bool REMOTESTORAGE_DB_ENABLE{
    ReadConstantString("REMOTESTORAGE_DB_ENABLE", "node.remotestorageDB.") ==
    "true"};

// Consensus constants
const double TOLERANCE_FRACTION{
    ReadConstantDouble("TOLERANCE_FRACTION", "node.consensus.")};
const unsigned int COMMIT_WINDOW_IN_SECONDS{
    ReadConstantNumeric("COMMIT_WINDOW_IN_SECONDS", "node.consensus.")};
const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW{
    ReadConstantNumeric("CONSENSUS_MSG_ORDER_BLOCK_WINDOW", "node.consensus.")};
const unsigned int CONSENSUS_OBJECT_TIMEOUT{
    ReadConstantNumeric("CONSENSUS_OBJECT_TIMEOUT", "node.consensus.")};
const unsigned int DS_NUM_CONSENSUS_SUBSETS{
    ReadConstantNumeric("DS_NUM_CONSENSUS_SUBSETS", "node.consensus.")};
const unsigned int SHARD_NUM_CONSENSUS_SUBSETS{
    ReadConstantNumeric("SHARD_NUM_CONSENSUS_SUBSETS", "node.consensus.")};
const unsigned int COMMIT_TOLERANCE_PERCENT{
    ReadConstantNumeric("COMMIT_TOLERANCE_PERCENT", "node.consensus.")};
const unsigned int SUBSET0_RESPONSE_DELAY_IN_MS{
    ReadConstantNumeric("SUBSET0_RESPONSE_DELAY_IN_MS", "node.consensus.")};

// Data sharing constants
const bool BROADCAST_TREEBASED_CLUSTER_MODE{
    ReadConstantString("BROADCAST_TREEBASED_CLUSTER_MODE",
                       "node.data_sharing.") == "true"};
const unsigned int MULTICAST_CLUSTER_SIZE{
    ReadConstantNumeric("MULTICAST_CLUSTER_SIZE", "node.data_sharing.")};
const unsigned int NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD{ReadConstantNumeric(
    "NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD", "node.data_sharing.")};
const unsigned int NUM_NODES_TO_SEND_LOOKUP{
    ReadConstantNumeric("NUM_NODES_TO_SEND_LOOKUP", "node.data_sharing.")};
const unsigned int NUM_OF_TREEBASED_CHILD_CLUSTERS{ReadConstantNumeric(
    "NUM_OF_TREEBASED_CHILD_CLUSTERS", "node.data_sharing.")};
const unsigned int POW_PACKET_SENDERS{
    ReadConstantNumeric("POW_PACKET_SENDERS", "node.data_sharing.")};
const unsigned int TX_SHARING_CLUSTER_SIZE{
    ReadConstantNumeric("TX_SHARING_CLUSTER_SIZE", "node.data_sharing.")};
const unsigned int NUM_SHARE_PENDING_TXNS{
    (ReadConstantNumeric("NUM_SHARE_PENDING_TXNS", "node.data_sharing."))};

// Dispatcher constants
const string TXN_PATH{ReadConstantString("TXN_PATH", "node.dispatcher.")};
const bool USE_REMOTE_TXN_CREATOR{
    ReadConstantString("USE_REMOTE_TXN_CREATOR", "node.dispatcher.") == "true"};
const unsigned int NUM_DISPATCHERS{
    (ReadConstantNumeric("NUM_DISPATCHERS", "node.dispatcher."))};

// Epoch timing constants
const unsigned int DELAY_FIRSTXNEPOCH_IN_MS{
    ReadConstantNumeric("DELAY_FIRSTXNEPOCH_IN_MS", "node.epoch_timing.")};
const unsigned int FETCHING_MISSING_DATA_TIMEOUT{
    ReadConstantNumeric("FETCHING_MISSING_DATA_TIMEOUT", "node.epoch_timing.")};
const unsigned int DS_ANNOUNCEMENT_DELAY_IN_MS{
    ReadConstantNumeric("DS_ANNOUNCEMENT_DELAY_IN_MS", "node.epoch_timing.")};
const unsigned int SHARD_ANNOUNCEMENT_DELAY_IN_MS{ReadConstantNumeric(
    "SHARD_ANNOUNCEMENT_DELAY_IN_MS", "node.epoch_timing.")};
const unsigned int LOOKUP_DELAY_SEND_TXNPACKET_IN_MS{ReadConstantNumeric(
    "LOOKUP_DELAY_SEND_TXNPACKET_IN_MS", "node.epoch_timing.")};
const unsigned int MICROBLOCK_TIMEOUT{
    ReadConstantNumeric("MICROBLOCK_TIMEOUT", "node.epoch_timing.")};
const unsigned int NEW_NODE_SYNC_INTERVAL{
    ReadConstantNumeric("NEW_NODE_SYNC_INTERVAL", "node.epoch_timing.")};
const unsigned int POW_SUBMISSION_TIMEOUT{
    ReadConstantNumeric("POW_SUBMISSION_TIMEOUT", "node.epoch_timing.")};
const unsigned int POW_WINDOW_IN_SECONDS{
    ReadConstantNumeric("POW_WINDOW_IN_SECONDS", "node.epoch_timing.")};
const unsigned int POWPACKETSUBMISSION_WINDOW_IN_SECONDS{ReadConstantNumeric(
    "POWPACKETSUBMISSION_WINDOW_IN_SECONDS", "node.epoch_timing.")};
const unsigned int RECOVERY_SYNC_TIMEOUT{
    ReadConstantNumeric("RECOVERY_SYNC_TIMEOUT", "node.epoch_timing.")};
const unsigned int TX_DISTRIBUTE_TIME_IN_MS{
    ReadConstantNumeric("TX_DISTRIBUTE_TIME_IN_MS", "node.epoch_timing.")};
const unsigned int EXTRA_TX_DISTRIBUTE_TIME_IN_MS{ReadConstantNumeric(
    "EXTRA_TX_DISTRIBUTE_TIME_IN_MS", "node.epoch_timing.")};
const unsigned int DS_TX_PROCESSING_TIMEOUT{
    ReadConstantNumeric("DS_TX_PROCESSING_TIMEOUT", "node.epoch_timing.")};
const unsigned int NEW_LOOKUP_SYNC_DELAY_IN_SECONDS{ReadConstantNumeric(
    "NEW_LOOKUP_SYNC_DELAY_IN_SECONDS", "node.epoch_timing.")};
const unsigned int GETSHARD_TIMEOUT_IN_SECONDS{
    ReadConstantNumeric("GETSHARD_TIMEOUT_IN_SECONDS", "node.epoch_timing.")};
const unsigned int GETSTATEDELTAS_TIMEOUT_IN_SECONDS{ReadConstantNumeric(
    "GETSTATEDELTAS_TIMEOUT_IN_SECONDS", "node.epoch_timing.")};
const unsigned int GETCOSIGREWARDS_TIMEOUT_IN_SECONDS{ReadConstantNumeric(
    "GETCOSIGREWARDS_TIMEOUT_IN_SECONDS", "node.epoch_timing.")};
const unsigned int RETRY_REJOINING_TIMEOUT{
    ReadConstantNumeric("RETRY_REJOINING_TIMEOUT", "node.epoch_timing.")};
const unsigned int RETRY_GETSTATEDELTAS_COUNT{
    ReadConstantNumeric("RETRY_GETSTATEDELTAS_COUNT", "node.epoch_timing.")};
const unsigned int RETRY_COSIGREWARDS_COUNT{
    ReadConstantNumeric("RETRY_COSIGREWARDS_COUNT", "node.epoch_timing.")};
const unsigned int MAX_FETCHMISSINGMBS_NUM{
    ReadConstantNumeric("MAX_FETCHMISSINGMBS_NUM", "node.epoch_timing.")};
const unsigned int LAST_N_TXBLKS_TOCHECK_FOR_MISSINGMBS{ReadConstantNumeric(
    "LAST_N_TXBLKS_TOCHECK_FOR_MISSINGMBS", "node.epoch_timing.")};
const unsigned int REMOVENODEFROMBLACKLIST_DELAY_IN_SECONDS{ReadConstantNumeric(
    "REMOVENODEFROMBLACKLIST_DELAY_IN_SECONDS", "node.epoch_timing.")};

// Gas constants
const uint64_t MIN_ETH_GAS{
    ReadConstantUInt64("MIN_ETH_GAS", "node.gas.", 21000)};
const unsigned int DS_MICROBLOCK_GAS_LIMIT{
    ReadConstantNumeric("DS_MICROBLOCK_GAS_LIMIT", "node.gas.")};
const unsigned int SHARD_MICROBLOCK_GAS_LIMIT{
    ReadConstantNumeric("SHARD_MICROBLOCK_GAS_LIMIT", "node.gas.")};
const unsigned int CONTRACT_CREATE_GAS{
    ReadConstantNumeric("CONTRACT_CREATE_GAS", "node.gas.")};
const unsigned int CONTRACT_INVOKE_GAS{
    ReadConstantNumeric("CONTRACT_INVOKE_GAS", "node.gas.")};
const unsigned int NORMAL_TRAN_GAS{
    ReadConstantNumeric("NORMAL_TRAN_GAS", "node.gas.")};
const unsigned int GAS_CONGESTION_PERCENT{
    ReadConstantNumeric("GAS_CONGESTION_PERCENT", "node.gas.")};
const unsigned int UNFILLED_PERCENT_LOW{
    ReadConstantNumeric("UNFILLED_PERCENT_LOW", "node.gas.")};
const unsigned int UNFILLED_PERCENT_HIGH{
    ReadConstantNumeric("UNFILLED_PERCENT_HIGH", "node.gas.")};
const uint128_t GAS_PRICE_MIN_VALUE{
    ReadConstantString("GAS_PRICE_MIN_VALUE", "node.gas.")};
const unsigned int GAS_PRICE_PRECISION{
    ReadConstantNumeric("GAS_PRICE_PRECISION", "node.gas.")};
const uint128_t PRECISION_MIN_VALUE{
    SafeMath<uint128_t>::power(10, GAS_PRICE_PRECISION, true)};
const unsigned int GAS_PRICE_DROP_RATIO{
    ReadConstantNumeric("GAS_PRICE_DROP_RATIO", "node.gas.")};
const unsigned int GAS_PRICE_RAISE_RATIO_LOWER{
    ReadConstantNumeric("GAS_PRICE_RAISE_RATIO_LOWER", "node.gas.")};
const unsigned int GAS_PRICE_RAISE_RATIO_UPPER{
    ReadConstantNumeric("GAS_PRICE_RAISE_RATIO_UPPER", "node.gas.")};
const unsigned int GAS_PRICE_TOLERANCE{
    ReadConstantNumeric("GAS_PRICE_TOLERANCE", "node.gas.")};
const unsigned int MEAN_GAS_PRICE_DS_NUM{
    ReadConstantNumeric("MEAN_GAS_PRICE_DS_NUM", "node.gas.")};
const string LEGAL_GAS_PRICE_IP{
    ReadConstantString("LEGAL_GAS_PRICE_IP", "node.gas.")};

// Gossip constants
const bool BROADCAST_GOSSIP_MODE{
    ReadConstantString("BROADCAST_GOSSIP_MODE", "node.gossip.") == "true"};
const bool SEND_RESPONSE_FOR_LAZY_PUSH{
    ReadConstantString("SEND_RESPONSE_FOR_LAZY_PUSH", "node.gossip.") ==
    "true"};
const bool GOSSIP_CUSTOM_ROUNDS_SETTINGS{
    ReadConstantString("GOSSIP_CUSTOM_ROUNDS_SETTINGS", "node.gossip.") ==
    "true"};
const unsigned int MAX_ROUNDS_IN_BSTATE{ReadConstantNumeric(
    "MAX_ROUNDS_IN_BSTATE", "node.gossip.gossip_custom_rounds.")};
const unsigned int MAX_ROUNDS_IN_CSTATE{ReadConstantNumeric(
    "MAX_ROUNDS_IN_CSTATE", "node.gossip.gossip_custom_rounds.")};
const unsigned int MAX_TOTAL_ROUNDS{ReadConstantNumeric(
    "MAX_TOTAL_ROUNDS", "node.gossip.gossip_custom_rounds.")};
const unsigned int MAX_NEIGHBORS_PER_ROUND{
    ReadConstantNumeric("MAX_NEIGHBORS_PER_ROUND", "node.gossip.")};
const unsigned int NUM_GOSSIP_RECEIVERS{
    ReadConstantNumeric("NUM_GOSSIP_RECEIVERS", "node.gossip.")};
const unsigned int ROUND_TIME_IN_MS{
    ReadConstantNumeric("ROUND_TIME_IN_MS", "node.gossip.")};
const unsigned int SIMULATED_NETWORK_DELAY_IN_MS{
    ReadConstantNumeric("SIMULATED_NETWORK_DELAY_IN_MS", "node.gossip.")};
const unsigned int KEEP_RAWMSG_FROM_LAST_N_ROUNDS{
    ReadConstantNumeric("KEEP_RAWMSG_FROM_LAST_N_ROUNDS", "node.gossip.")};
const bool SIGN_VERIFY_EMPTY_MSGTYP{
    ReadConstantString("SIGN_VERIFY_EMPTY_MSGTYP", "node.gossip.") == "true"};
const bool SIGN_VERIFY_NONEMPTY_MSGTYP{
    ReadConstantString("SIGN_VERIFY_NONEMPTY_MSGTYP", "node.gossip.") ==
    "true"};

// GPU mining constants
const string GPU_TO_USE{ReadConstantString("GPU_TO_USE", "node.gpu.")};
const unsigned int OPENCL_LOCAL_WORK_SIZE{
    ReadConstantNumeric("LOCAL_WORK_SIZE", "node.gpu.opencl.")};
const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER{
    ReadConstantNumeric("GLOBAL_WORK_SIZE_MULTIPLIER", "node.gpu.opencl.")};
const unsigned int OPENCL_START_EPOCH{
    ReadConstantNumeric("START_EPOCH", "node.gpu.opencl.")};

// Guard mode constants
const bool GUARD_MODE{ReadConstantString("GUARD_MODE", "node.guard_mode.") ==
                      "true"};
const bool EXCLUDE_PRIV_IP{
    ReadConstantString("EXCLUDE_PRIV_IP", "node.guard_mode.") == "true"};
const unsigned int WINDOW_FOR_DS_NETWORK_INFO_UPDATE{ReadConstantNumeric(
    "WINDOW_FOR_DS_NETWORK_INFO_UPDATE", "node.guard_mode.")};
const double SHARD_GUARD_TOL{
    ReadConstantDouble("SHARD_GUARD_TOL", "node.guard_mode.")};
const unsigned int SHARD_LEADER_SELECT_TOL{
    ReadConstantNumeric("SHARD_LEADER_SELECT_TOL", "node.guard_mode.")};
// Heartbeat constants
const unsigned int HEARTBEAT_INTERVAL_IN_SECONDS{
    ReadConstantNumeric("HEARTBEAT_INTERVAL_IN_SECONDS", "node.heartbeat.")};

// RPC Constants
const unsigned int LOOKUP_RPC_PORT{
    ReadConstantNumeric("LOOKUP_RPC_PORT", "node.jsonrpc.")};
const unsigned int STAKING_RPC_PORT{
    ReadConstantNumeric("STAKING_RPC_PORT", "node.jsonrpc.")};
const unsigned int STATUS_RPC_PORT{
    ReadConstantNumeric("STATUS_RPC_PORT", "node.jsonrpc.")};
const std::string IP_TO_BIND{ReadConstantString("IP_TO_BIND", "node.jsonrpc.")};
const bool ENABLE_STAKING_RPC{
    ReadConstantString("ENABLE_STAKING_RPC", "node.jsonrpc.") == "true"};
const bool ENABLE_STATUS_RPC{
    ReadConstantString("ENABLE_STATUS_RPC", "node.jsonrpc.") == "true"};
const unsigned int NUM_SHARD_PEER_TO_REVEAL{
    ReadConstantNumeric("NUM_SHARD_PEER_TO_REVEAL", "node.jsonrpc.")};
const std::string SCILLA_IPC_SOCKET_PATH{
    ReadConstantString("SCILLA_IPC_SOCKET_PATH", "node.jsonrpc.")};
const std::string SCILLA_SERVER_SOCKET_PATH{
    ReadConstantString("SCILLA_SERVER_SOCKET_PATH", "node.jsonrpc.")};
const std::string SCILLA_SERVER_BINARY{
    ReadConstantString("SCILLA_SERVER_BINARY", "node.jsonrpc.")};
bool ENABLE_WEBSOCKET{ReadConstantString("ENABLE_WEBSOCKET", "node.jsonrpc.") ==
                      "true"};
const unsigned int WEBSOCKET_PORT{
    ReadConstantNumeric("WEBSOCKET_PORT", "node.jsonrpc.")};
const bool ENABLE_GETTXNBODIESFORTXBLOCK{
    ReadConstantString("ENABLE_GETTXNBODIESFORTXBLOCK", "node.jsonrpc.") ==
    "true"};
const unsigned int NUM_TXNS_PER_PAGE{
    ReadConstantNumeric("NUM_TXNS_PER_PAGE", "node.jsonrpc.")};
const unsigned int PENDING_TXN_QUERY_NUM_EPOCHS{
    ReadConstantNumeric("PENDING_TXN_QUERY_NUM_EPOCHS", "node.jsonrpc.")};
const unsigned int PENDING_TXN_QUERY_MAX_RESULTS{
    ReadConstantNumeric("PENDING_TXN_QUERY_MAX_RESULTS", "node.jsonrpc.")};
const bool CONNECTION_IO_USE_EPOLL{
    ReadConstantString("CONNECTION_IO_USE_EPOLL", "node.jsonrpc.") == "true"};
const unsigned int CONNECTION_ALL_TIMEOUT{
    ReadConstantNumeric("CONNECTION_ALL_TIMEOUT", "node.jsonrpc.")};
const unsigned int CONNECTION_CALLBACK_TIMEOUT{
    ReadConstantNumeric("CONNECTION_CALLBACK_TIMEOUT", "node.jsonrpc.")};

// Network composition constants
const unsigned int COMM_SIZE{
    ReadConstantNumeric("COMM_SIZE", "node.network_composition.")};
const unsigned int NUM_DS_ELECTION{
    ReadConstantNumeric("NUM_DS_ELECTION", "node.network_composition.")};
const double DS_PERFORMANCE_THRESHOLD_PERCENT{ReadConstantDouble(
    "DS_PERFORMANCE_THRESHOLD_PERCENT", "node.network_composition.")};
const unsigned int NUM_DS_BYZANTINE_REMOVED{ReadConstantNumeric(
    "NUM_DS_BYZANTINE_REMOVED", "node.network_composition.")};
const unsigned int SHARD_SIZE_TOLERANCE_LO{ReadConstantNumeric(
    "SHARD_SIZE_TOLERANCE_LO", "node.network_composition.")};
const unsigned int SHARD_SIZE_TOLERANCE_HI{ReadConstantNumeric(
    "SHARD_SIZE_TOLERANCE_HI", "node.network_composition.")};
const unsigned int STORE_DS_COMMITTEE_INTERVAL{ReadConstantNumeric(
    "STORE_DS_COMMITTEE_INTERVAL", "node.network_composition.")};

// P2PComm constants
const unsigned int BROADCAST_INTERVAL{
    ReadConstantNumeric("BROADCAST_INTERVAL", "node.p2pcomm.")};
const unsigned int BROADCAST_EXPIRY{
    ReadConstantNumeric("BROADCAST_EXPIRY", "node.p2pcomm.")};
const unsigned int FETCH_LOOKUP_MSG_MAX_RETRY{
    ReadConstantNumeric("FETCH_LOOKUP_MSG_MAX_RETRY", "node.p2pcomm.")};
const uint32_t MAXSENDMESSAGE{
    ReadConstantNumeric("MAXSENDMESSAGE", "node.p2pcomm.")};
const uint32_t MAXRECVMESSAGE{
    ReadConstantNumeric("MAXRECVMESSAGE", "node.p2pcomm.")};
const unsigned int MAXRETRYCONN{
    ReadConstantNumeric("MAXRETRYCONN", "node.p2pcomm.")};
const unsigned int MSGQUEUE_SIZE{
    ReadConstantNumeric("MSGQUEUE_SIZE", "node.p2pcomm.")};
const unsigned int PUMPMESSAGE_MILLISECONDS{
    ReadConstantNumeric("PUMPMESSAGE_MILLISECONDS", "node.p2pcomm.")};
const unsigned int SENDQUEUE_SIZE{
    ReadConstantNumeric("SENDQUEUE_SIZE", "node.p2pcomm.")};
const unsigned int MAX_GOSSIP_MSG_SIZE_IN_BYTES{
    ReadConstantNumeric("MAX_GOSSIP_MSG_SIZE_IN_BYTES", "node.p2pcomm.")};
const unsigned int MIN_READ_WATERMARK_IN_BYTES{
    ReadConstantNumeric("MIN_READ_WATERMARK_IN_BYTES", "node.p2pcomm.")};
const unsigned int MAX_READ_WATERMARK_IN_BYTES{
    ReadConstantNumeric("MAX_READ_WATERMARK_IN_BYTES", "node.p2pcomm.")};
const unsigned int BLACKLIST_NUM_TO_POP{
    ReadConstantNumeric("BLACKLIST_NUM_TO_POP", "node.p2pcomm.")};
const unsigned int MAX_PEER_CONNECTION{
    ReadConstantNumeric("MAX_PEER_CONNECTION", "node.p2pcomm.")};
const unsigned int MAX_PEER_CONNECTION_P2PSEED{
    ReadConstantNumeric("MAX_PEER_CONNECTION_P2PSEED", "node.p2pcomm.")};
const unsigned int MAX_WHITELISTREQ_LIMIT{
    ReadConstantNumeric("MAX_WHITELISTREQ_LIMIT", "node.p2pcomm.")};
const unsigned int SENDJOBPEERS_TIMEOUT{
    ReadConstantNumeric("SENDJOBPEERS_TIMEOUT", "node.p2pcomm.")};

// PoW constants
const bool FULL_DATASET_MINE{
    ReadConstantString("FULL_DATASET_MINE", "node.pow.") == "true"};
const bool OPENCL_GPU_MINE{ReadConstantString("OPENCL_GPU_MINE", "node.pow.") ==
                           "true"};
const bool REMOTE_MINE{ReadConstantString("REMOTE_MINE", "node.pow.") ==
                       "true"};
const std::string MINING_PROXY_URL{
    ReadConstantString("MINING_PROXY_URL", "node.pow.")};
const unsigned int MINING_PROXY_TIMEOUT_IN_MS{
    ReadConstantNumeric("MINING_PROXY_TIMEOUT_IN_MS", "node.pow.")};
const unsigned int MAX_RETRY_SEND_POW_TIME{
    ReadConstantNumeric("MAX_RETRY_SEND_POW_TIME", "node.pow.")};
const unsigned int CHECK_MINING_RESULT_INTERVAL{
    ReadConstantNumeric("CHECK_MINING_RESULT_INTERVAL", "node.pow.")};
const bool GETWORK_SERVER_MINE{
    ReadConstantString("GETWORK_SERVER_MINE", "node.pow.") == "true"};
const unsigned int GETWORK_SERVER_PORT{
    ReadConstantNumeric("GETWORK_SERVER_PORT", "node.pow.")};
const unsigned int DS_POW_DIFFICULTY{
    ReadConstantNumeric("DS_POW_DIFFICULTY", "node.pow.")};
const unsigned int POW_DIFFICULTY{
    ReadConstantNumeric("POW_DIFFICULTY", "node.pow.")};
const unsigned int POW_BOUNDARY_N_DIVIDED{
    ReadConstantNumeric("POW_BOUNDARY_N_DIVIDED", "node.pow.")};
const unsigned int POW_BOUNDARY_N_DIVIDED_START{
    ReadConstantNumeric("POW_BOUNDARY_N_DIVIDED_START", "node.pow.")};
const unsigned int POW_SUBMISSION_LIMIT{
    ReadConstantNumeric("POW_SUBMISSION_LIMIT", "node.pow.")};
const unsigned int NUM_FINAL_BLOCK_PER_POW{
    ReadConstantNumeric("NUM_FINAL_BLOCK_PER_POW", "node.pow.")};
const unsigned int POW_CHANGE_TO_ADJ_DIFF{
    ReadConstantNumeric("POW_CHANGE_TO_ADJ_DIFF", "node.pow.")};
const unsigned int POW_CHANGE_TO_ADJ_DS_DIFF{
    ReadConstantNumeric("POW_CHANGE_TO_ADJ_DS_DIFF", "node.pow.")};
const unsigned int DIFFICULTY_DIFF_TOL{
    ReadConstantNumeric("DIFFICULTY_DIFF_TOL", "node.pow.")};
const unsigned int EXPECTED_SHARD_NODE_NUM{
    ReadConstantNumeric("EXPECTED_SHARD_NODE_NUM", "node.pow.")};
const unsigned int MAX_SHARD_NODE_NUM{
    ReadConstantNumeric("MAX_SHARD_NODE_NUM", "node.pow.")};
const uint8_t MIN_NODE_REPUTATION_PRIORITY{static_cast<uint8_t>(
    ReadConstantNumeric("MIN_NODE_REPUTATION_PRIORITY", "node.pow."))};
const unsigned int MISORDER_TOLERANCE_IN_PERCENT{
    ReadConstantNumeric("MISORDER_TOLERANCE_IN_PERCENT", "node.pow.")};
const unsigned int DSBLOCK_EXTRA_WAIT_TIME{
    ReadConstantNumeric("DSBLOCK_EXTRA_WAIT_TIME", "node.pow.")};
const unsigned int DIFF_IP_TOLERANCE_IN_PERCENT{
    ReadConstantNumeric("DIFF_IP_TOLERANCE_IN_PERCENT", "node.pow.")};
const unsigned int TXN_SHARD_TARGET_DIFFICULTY{
    ReadConstantNumeric("TXN_SHARD_TARGET_DIFFICULTY", "node.pow.")};
const unsigned int TXN_DS_TARGET_DIFFICULTY{
    ReadConstantNumeric("TXN_DS_TARGET_DIFFICULTY", "node.pow.")};
const unsigned int TXN_DS_TARGET_NUM{
    ReadConstantNumeric("TXN_DS_TARGET_NUM", "node.pow.")};
const unsigned int PRIORITY_TOLERANCE_IN_PERCENT{
    ReadConstantNumeric("PRIORITY_TOLERANCE_IN_PERCENT", "node.pow.")};
const bool SKIP_POW_REATTEMPT_FOR_DS_DIFF{
    ReadConstantString("SKIP_POW_REATTEMPT_FOR_DS_DIFF", "node.pow.") ==
    "true"};
const string POW_SUBMISSION_VERSION_TAG{
    ReadConstantString("POW_SUBMISSION_VERSION_TAG", "node.pow.")};

// Recovery and upgrading constants
const unsigned int WAIT_LOOKUP_WAKEUP_IN_SECONDS{
    ReadConstantNumeric("WAIT_LOOKUP_WAKEUP_IN_SECONDS", "node.recovery.")};
const unsigned int SHARD_DELAY_WAKEUP_IN_SECONDS{
    ReadConstantNumeric("SHARD_DELAY_WAKEUP_IN_SECONDS", "node.recovery.")};
const unsigned int TERMINATION_COUNTDOWN_IN_SECONDS{
    ReadConstantNumeric("TERMINATION_COUNTDOWN_IN_SECONDS", "node.recovery.")};
const bool REJOIN_NODE_NOT_IN_NETWORK{
    ReadConstantString("REJOIN_NODE_NOT_IN_NETWORK", "node.recovery.") ==
    "true"};
const unsigned int RESUME_BLACKLIST_DELAY_IN_SECONDS{
    ReadConstantNumeric("RESUME_BLACKLIST_DELAY_IN_SECONDS", "node.recovery.")};
const unsigned int INCRDB_DSNUMS_WITH_STATEDELTAS{
    ReadConstantNumeric("INCRDB_DSNUMS_WITH_STATEDELTAS", "node.recovery.")};
const bool CONTRACT_STATES_MIGRATED{
    ReadConstantString("CONTRACT_STATES_MIGRATED", "node.recovery.") == "true"};
const unsigned int MAX_IPCHANGE_REQUEST_LIMIT{
    ReadConstantNumeric("MAX_IPCHANGE_REQUEST_LIMIT", "node.recovery.")};
const unsigned int MAX_REJOIN_NETWORK_ATTEMPTS{
    ReadConstantNumeric("MAX_REJOIN_NETWORK_ATTEMPTS", "node.recovery.")};
const unsigned int RELEASE_CACHE_INTERVAL{
    ReadConstantNumeric("RELEASE_CACHE_INTERVAL", "node.recovery.")};
const unsigned int DIRBLOCK_FETCH_LIMIT{
    ReadConstantNumeric("DIRBLOCK_FETCH_LIMIT", "node.recovery.")};

// Smart contract constants
const bool ENABLE_SC{ReadConstantString("ENABLE_SC", "node.smart_contract.") ==
                     "true"};
string scilla_root_raw{
    ReadConstantString("SCILLA_ROOT", "node.smart_contract.")};
string SCILLA_ROOT{scilla_root_raw.back() == '/'
                       ? scilla_root_raw.substr(0, scilla_root_raw.size() - 1)
                       : scilla_root_raw};
const string SCILLA_CHECKER{
    ReadConstantString("SCILLA_CHECKER", "node.smart_contract.")};
const string SCILLA_BINARY{
    ReadConstantString("SCILLA_BINARY", "node.smart_contract.")};
const string SCILLA_FILES{
    ReadConstantString("SCILLA_FILES", "node.smart_contract.")};
const string SCILLA_LOG{
    ReadConstantString("SCILLA_LOG", "node.smart_contract.")};
const string SCILLA_LIB{
    ReadConstantString("SCILLA_LIB", "node.smart_contract.")};
const string INIT_JSON{SCILLA_FILES + '/' +
                       ReadConstantString("INIT_JSON", "node.smart_contract.")};
const string INPUT_STATE_JSON{
    SCILLA_FILES + '/' +
    ReadConstantString("INPUT_STATE_JSON", "node.smart_contract.")};
const string INPUT_BLOCKCHAIN_JSON{
    SCILLA_FILES + '/' +
    ReadConstantString("INPUT_BLOCKCHAIN_JSON", "node.smart_contract.")};
const string INPUT_MESSAGE_JSON{
    SCILLA_FILES + '/' +
    ReadConstantString("INPUT_MESSAGE_JSON", "node.smart_contract.")};
const string OUTPUT_JSON{
    SCILLA_FILES + '/' +
    ReadConstantString("OUTPUT_JSON", "node.smart_contract.")};
const string INPUT_CODE{
    SCILLA_FILES + '/' +
    ReadConstantString("INPUT_CODE", "node.smart_contract.")};
const string CONTRACT_FILE_EXTENSION{
    ReadConstantString("CONTRACT_FILE_EXTENSION", "node.smart_contract.")};
const string LIBRARY_CODE_EXTENSION{
    ReadConstantString("LIBRARY_CODE_EXTENSION", "node.smart_contract.")};
const string EXTLIB_FOLDER{
    ReadConstantString("EXTLIB_FOLDER", "node.smart_contract.")};
const bool ENABLE_SCILLA_MULTI_VERSION{
    ReadConstantString("ENABLE_SCILLA_MULTI_VERSION", "node.smart_contract.") ==
    "true"};
bool ENABLE_SCILLA{ReadConstantString("ENABLE_SCILLA", "node.smart_contract.",
                                      "true") == "true"};
const bool LOG_SC{ReadConstantString("LOG_SC", "node.smart_contract.") ==
                  "true"};
const bool DISABLE_SCILLA_LIB{
    ReadConstantString("DISABLE_SCILLA_LIB", "node.smart_contract.") == "true"};
const unsigned int SCILLA_SERVER_PENDING_IN_MS{
    ReadConstantNumeric("SCILLA_SERVER_PENDING_IN_MS", "node.smart_contract.")};
unsigned int SCILLA_SERVER_LOOP_WAIT_MICROSECONDS{ReadConstantNumeric(
    "SCILLA_SERVER_LOOP_WAIT_MICROSECONDS", "node.smart_contract.")};

// Test constants
const bool ENABLE_CHECK_PERFORMANCE_LOG{
    ReadConstantString("ENABLE_CHECK_PERFORMANCE_LOG", "node.tests.") ==
    "true"};
const unsigned int NUM_TXN_TO_SEND_PER_ACCOUNT{
    ReadConstantNumeric("NUM_TXN_TO_SEND_PER_ACCOUNT", "node.tests.")};
const bool ENABLE_ACCOUNTS_POPULATING{
    ReadConstantString("ENABLE_ACCOUNTS_POPULATING", "node.tests.") == "true"};
const bool UPDATE_PREGENED_ACCOUNTS{
    ReadConstantString("UPDATE_PREGENED_ACCOUNTS", "node.tests.") == "true"};
const unsigned int NUM_ACCOUNTS_PREGENERATE{
    ReadConstantNumeric("NUM_ACCOUNTS_PREGENERATE", "node.tests.")};
const unsigned int PREGEN_ACCOUNT_TIMES{
    ReadConstantNumeric("PREGEN_ACCOUNT_TIMES", "node.tests.")};
const string PREGENED_ACCOUNTS_FILE{
    ReadConstantString("PREGENED_ACCOUNTS_FILE", "node.tests.")};

// Transaction constants
const uint128_t TOTAL_COINBASE_REWARD{
    ReadConstantString("TOTAL_COINBASE_REWARD", "node.transactions.")};
const uint128_t COINBASE_REWARD_PER_DS{
    ReadConstantString("COINBASE_REWARD_PER_DS", "node.transactions.")};
const uint128_t TOTAL_GENESIS_TOKEN{
    ReadConstantString("TOTAL_GENESIS_TOKEN", "node.transactions.")};
const unsigned int BASE_REWARD_IN_PERCENT{
    ReadConstantNumeric("BASE_REWARD_IN_PERCENT", "node.transactions.")};
const unsigned int LOOKUP_REWARD_IN_PERCENT{
    ReadConstantNumeric("LOOKUP_REWARD_IN_PERCENT", "node.transactions.")};
const unsigned int MAX_CODE_SIZE_IN_BYTES{
    ReadConstantNumeric("MAX_CODE_SIZE_IN_BYTES", "node.transactions.")};
const unsigned int MAX_CONTRACT_EDGES{
    ReadConstantNumeric("MAX_CONTRACT_EDGES", "node.transactions.")};
const unsigned int SCILLA_CHECKER_INVOKE_GAS{
    ReadConstantNumeric("SCILLA_CHECKER_INVOKE_GAS", "node.transactions.")};
const unsigned int SCILLA_RUNNER_INVOKE_GAS{
    ReadConstantNumeric("SCILLA_RUNNER_INVOKE_GAS", "node.transactions.")};
const unsigned int SYS_TIMESTAMP_VARIANCE_IN_SECONDS{ReadConstantNumeric(
    "SYS_TIMESTAMP_VARIANCE_IN_SECONDS", "node.transactions.")};
const unsigned int TXN_MISORDER_TOLERANCE_IN_PERCENT{ReadConstantNumeric(
    "TXN_MISORDER_TOLERANCE_IN_PERCENT", "node.transactions.")};
const unsigned int TXNS_MISSING_TOLERANCE_IN_PERCENT{ReadConstantNumeric(
    "TXNS_MISSING_TOLERANCE_IN_PERCENT", "node.transactions.")};
const unsigned int PACKET_EPOCH_LATE_ALLOW{
    ReadConstantNumeric("PACKET_EPOCH_LATE_ALLOW", "node.transactions.")};
const unsigned int PACKET_BYTESIZE_LIMIT{
    ReadConstantNumeric("PACKET_BYTESIZE_LIMIT", "node.transactions.")};
const unsigned int SMALL_TXN_SIZE{
    ReadConstantNumeric("SMALL_TXN_SIZE", "node.transactions.")};
const unsigned int ACCOUNT_IO_BATCH_SIZE{
    ReadConstantNumeric("ACCOUNT_IO_BATCH_SIZE", "node.transactions.")};
const bool ENABLE_REPOPULATE{
    ReadConstantString("ENABLE_REPOPULATE", "node.transactions.") == "true"};
const unsigned int REPOPULATE_STATE_PER_N_DS{
    ReadConstantNumeric("REPOPULATE_STATE_PER_N_DS", "node.transactions.")};
const unsigned int REPOPULATE_STATE_IN_DS{std::min(
    ReadConstantNumeric("REPOPULATE_STATE_IN_DS", "node.transactions."),
    REPOPULATE_STATE_PER_N_DS - 1)};
const unsigned int NUM_STORE_TX_BODIES_INTERVAL{
    ReadConstantNumeric("NUM_STORE_TX_BODIES_INTERVAL", "node.transactions.")};
const string BUCKET_NAME{
    ReadConstantString("BUCKET_NAME", "node.transactions.")};
const string TXN_PERSISTENCE_NAME{
    ReadConstantString("TXN_PERSISTENCE_NAME", "node.transactions.")};
const bool ENABLE_TXNS_BACKUP{
    ReadConstantString("ENABLE_TXNS_BACKUP", "node.transactions.") == "true"};
const bool SHARDLDR_SAVE_TXN_LOCALLY{
    ReadConstantString("SHARDLDR_SAVE_TXN_LOCALLY", "node.transactions.") ==
    "true"};
const double BLOOM_FILTER_FALSE_RATE{
    ReadConstantDouble("BLOOM_FILTER_FALSE_RATE", "node.transactions.")};
const unsigned int TXN_DISPATCH_ATTEMPT_LIMIT{
    ReadConstantNumeric("TXN_DISPATCH_ATTEMPT_LIMIT", "node.transactions.")};

// Viewchange constants
const unsigned int POST_VIEWCHANGE_BUFFER{
    ReadConstantNumeric("POST_VIEWCHANGE_BUFFER", "node.viewchange.")};
const unsigned int VIEWCHANGE_EXTRA_TIME{
    ReadConstantNumeric("VIEWCHANGE_EXTRA_TIME", "node.viewchange.")};
const unsigned int VIEWCHANGE_PRECHECK_TIME{
    ReadConstantNumeric("VIEWCHANGE_PRECHECK_TIME", "node.viewchange.")};
const unsigned int VIEWCHANGE_TIME{
    ReadConstantNumeric("VIEWCHANGE_TIME", "node.viewchange.")};

// Genesis accounts
const vector<string> GENESIS_WALLETS{
    ReadAccountsFromConstantsFile("wallet_address")};
const vector<string> GENESIS_KEYS{ReadAccountsFromConstantsFile("private_key")};

// Genesis accounts for ds txn dispatching ( TEST Purpose Only )
const vector<string> DS_GENESIS_WALLETS{
    ReadAccountsFromConstantsFile("wallet_address", "node.ds_accounts")};
const vector<string> DS_GENESIS_KEYS{
    ReadAccountsFromConstantsFile("private_key", "node.ds_accounts")};

// Verifier
const vector<pair<uint64_t, uint32_t>> VERIFIER_EXCLUSION_LIST{
    ReadVerifierExclusionListFromConstantsFile()};
const bool IGNORE_BLOCKCOSIG_CHECK{
    ReadConstantString("IGNORE_BLOCKCOSIG_CHECK", "node.verifier.") == "true"};
const vector<pair<uint64_t, uint32_t>> VERIFIER_MICROBLOCK_EXCLUSION_LIST{
    ReadVerifierMicroblockExclusionListFromConstantsFile()};
bool ENABLE_EVM{ReadConstantString("ENABLE_EVM", "node.jsonrpc.", "true") ==
                "true"};
const std::string EVM_SERVER_SOCKET_PATH{ReadConstantString(
    "EVM_SERVER_SOCKET_PATH", "node.jsonrpc.", "/tmp/evm-server.sock")};
const std::string EVM_SERVER_BINARY{ReadConstantString(
    "EVM_SERVER_BINARY", "node.jsonrpc.", "/usr/local/bin/evm-ds")};
const std::string EVM_LOG_CONFIG{ReadConstantString(
    "EVM_LOG_CONFIG", "node.jsonrpc.", "/usr/local/etc/log4rs.yml")};
const uint64_t ETH_CHAINID{ReadConstantNumeric("CHAIN_ID") + 0x8000};
const uint64_t EVM_ZIL_SCALING_FACTOR{
    ReadConstantUInt64("EVM_ZIL_SCALING_FACTOR", "node.jsonrpc.", 1)};
const uint64_t EVM_BLOCK_LOOKUP_LIMIT{
    ReadConstantUInt64("EVM_BLOCK_LOOKUP_LIMIT", "node.jsonrpc.", 50)};
const uint64_t EVM_RPC_TIMEOUT_SECONDS{
    ReadConstantUInt64("EVM_RPC_TIMEOUT_SECONDS", "node.jsonrpc.", 60)};
const bool LAUNCH_EVM_DAEMON{
    ReadConstantString("LAUNCH_EVM_DAEMON", "node.jsonrpc.", "true") == "true"};
const bool ENABLE_CPS{
    ReadConstantString("ENABLE_CPS", "node.jsonrpc.", "true") == "true"};
const std::string METRIC_ZILLIQA_HOSTNAME{ReadConstantString(
    "METRIC_ZILLIQA_HOSTNAME", "node.metric.zilliqa.", "localhost")};
const std::string METRIC_ZILLIQA_PROVIDER{ReadConstantString(
    "METRIC_ZILLIQA_PROVIDER", "node.metric.zilliqa.", "prometheus")};
const unsigned int METRIC_ZILLIQA_PORT{
    ReadConstantNumeric("METRIC_ZILLIQA_PORT", "node.metric.zilliqa.", 8090)};
const unsigned int METRIC_ZILLIQA_READER_EXPORT_MS{ReadConstantNumeric(
    "METRIC_ZILLIQA_READER_EXPORT_MS", "node.metric.zilliqa.", 1000)};
const unsigned int METRIC_ZILLIQA_READER_TIMEOUT_MS{ReadConstantNumeric(
    "METRIC_ZILLIQA_READER_TIMEOUT_MS", "node.metric.zilliqa.", 500)};
const std::string METRIC_ZILLIQA_SCHEMA{
    ReadConstantString("METRIC_ZILLIQA_SCHEMA", "node.metric.zilliqa.",
                       "https://opentelemetry.io/schemas/1.2.0")};
const std::string METRIC_ZILLIQA_SCHEMA_VERSION{ReadConstantString(
    "METRIC_ZILLIQA_SCHEMA_VERSION", "node.metric.zilliqa.", "1.2.0")};
const std::string METRIC_ZILLIQA_MASK{
    ReadConstantString("METRIC_ZILLIQA_MASK", "node.metric.zilliqa.", "NONE")};
const std::string TRACE_ZILLIQA_MASK{
    ReadConstantString("TRACE_ZILLIQA_MASK", "node.trace.zilliqa.", "NONE")};
const std::string TRACE_ZILLIQA_PROVIDER{ReadConstantString(
    "TRACE_ZILLIQA_PROVIDER", "node.trace.zilliqa.", "STDOUT")};
const std::string TRACE_ZILLIQA_HOSTNAME{ReadConstantString(
    "TRACE_ZILLIQA_HOSTNAME", "node.trace.zilliqa.", "localhost")};
const unsigned int TRACE_ZILLIQA_PORT{
    ReadConstantNumeric("TRACE_ZILLIQA_PORT", "node.trace.zilliqa.", 4318)};
