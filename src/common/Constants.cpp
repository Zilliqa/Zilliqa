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
                                 const char* path = "node.general.") {
  auto pt = PTree::GetInstance();
  return pt.get<unsigned int>(path + propertyName);
}

string ReadConstantString(string propertyName,
                          const char* path = "node.general.") {
  auto pt = PTree::GetInstance();
  return pt.get<string>(path + propertyName);
}

const vector<string> ReadAccountsFromConstantsFile(string propName) {
  auto pt = PTree::GetInstance();
  vector<string> result;
  for (auto& acc : pt.get_child("node.accounts")) {
    auto child = acc.second.get_optional<string>(propName);
    if (child) {
      // LOG_GENERAL("constants " << child.get());
      result.push_back(child.get());
    }
  }
  return result;
}

// General constants
const unsigned int MSG_VERSION{ReadConstantNumeric("MSG_VERSION")};
const unsigned int DEBUG_LEVEL{ReadConstantNumeric("DEBUG_LEVEL")};
const bool ENABLE_DO_REJOIN{ReadConstantString("ENABLE_DO_REJOIN") == "true"};
const bool LOOKUP_NODE_MODE{ReadConstantString("LOOKUP_NODE_MODE") == "true"};
const unsigned int NUM_DS_EPOCHS_BEFORE_CLEARING_DIAGNOSTIC_DATA{
    ReadConstantNumeric("NUM_DS_EPOCHS_BEFORE_CLEARING_DIAGNOSTIC_DATA")};

// Archival constants
const bool ARCHIVAL_NODE{
    ReadConstantString("ARCHIVAL_NODE", "node.archival.") == "true"};
const string DB_HOST{ReadConstantString("DB_HOST", "node.archival.")};

// Seed constans
const bool ARCHIVAL_LOOKUP{
    ReadConstantString("ARCHIVAL_LOOKUP", "node.seed.") == "true"};
const unsigned int SEED_TXN_COLLECTION_TIME_IN_SEC{
    ReadConstantNumeric("SEED_TXN_COLLECTION_TIME_IN_SEC", "node.seed.")};

// Consensus constants
const unsigned int COMMIT_WINDOW_IN_SECONDS{
    ReadConstantNumeric("COMMIT_WINDOW_IN_SECONDS", "node.consensus.")};
const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW{
    ReadConstantNumeric("CONSENSUS_MSG_ORDER_BLOCK_WINDOW", "node.consensus.")};
const unsigned int CONSENSUS_OBJECT_TIMEOUT{
    ReadConstantNumeric("CONSENSUS_OBJECT_TIMEOUT", "node.consensus.")};
const unsigned int NUM_CONSENSUS_SUBSETS{
    ReadConstantNumeric("NUM_CONSENSUS_SUBSETS", "node.consensus.")};

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

// Dispatcher constants
const string TXN_PATH{ReadConstantString("TXN_PATH", "node.dispatcher.")};
const bool USE_REMOTE_TXN_CREATOR{
    ReadConstantString("USE_REMOTE_TXN_CREATOR", "node.dispatcher.") == "true"};

// Epoch timing constants
const unsigned int DELAY_FIRSTXNEPOCH_IN_MS{
    ReadConstantNumeric("DELAY_FIRSTXNEPOCH_IN_MS", "node.epoch_timing.")};
const unsigned int FETCHING_MISSING_DATA_TIMEOUT{
    ReadConstantNumeric("FETCHING_MISSING_DATA_TIMEOUT", "node.epoch_timing.")};
const unsigned int FINALBLOCK_DELAY_IN_MS{
    ReadConstantNumeric("FINALBLOCK_DELAY_IN_MS", "node.epoch_timing.")};
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
const unsigned int NEW_LOOKUP_SYNC_DELAY_IN_SECONDS{ReadConstantNumeric(
    "NEW_LOOKUP_SYNC_DELAY_IN_SECONDS", "node.epoch_timing.")};

// Fallback constants
const bool ENABLE_FALLBACK{
    ReadConstantString("ENABLE_FALLBACK", "node.fallback.") == "true"};
const unsigned int FALLBACK_CHECK_INTERVAL{
    ReadConstantNumeric("FALLBACK_CHECK_INTERVAL", "node.fallback.")};
const unsigned int FALLBACK_EXTRA_TIME{
    ReadConstantNumeric("FALLBACK_EXTRA_TIME", "node.fallback.")};
const unsigned int FALLBACK_INTERVAL_STARTED{
    ReadConstantNumeric("FALLBACK_INTERVAL_STARTED", "node.fallback.")};
const unsigned int FALLBACK_INTERVAL_WAITING{
    ReadConstantNumeric("FALLBACK_INTERVAL_WAITING", "node.fallback.")};

// Gas constants
const unsigned int MICROBLOCK_GAS_LIMIT{
    ReadConstantNumeric("MICROBLOCK_GAS_LIMIT", "node.gas.")};
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
const boost::multiprecision::uint128_t GAS_PRICE_MIN_VALUE{
    ReadConstantString("GAS_PRICE_MIN_VALUE", "node.gas.")};
const unsigned int GAS_PRICE_PRECISION{
    ReadConstantNumeric("GAS_PRICE_PRECISION", "node.gas.")};
const boost::multiprecision::uint128_t PRECISION_MIN_VALUE{
    SafeMath<boost::multiprecision::uint128_t>::power(10, GAS_PRICE_PRECISION)};
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

// GPU mining constants
const string GPU_TO_USE{ReadConstantString("GPU_TO_USE", "node.gpu.")};
const unsigned int OPENCL_LOCAL_WORK_SIZE{
    ReadConstantNumeric("LOCAL_WORK_SIZE", "node.gpu.opencl.")};
const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER{
    ReadConstantNumeric("GLOBAL_WORK_SIZE_MULTIPLIER", "node.gpu.opencl.")};
const unsigned int OPENCL_START_EPOCH{
    ReadConstantNumeric("START_EPOCH", "node.gpu.opencl.")};
const unsigned int CUDA_BLOCK_SIZE{
    ReadConstantNumeric("BLOCK_SIZE", "node.gpu.cuda.")};
const unsigned int CUDA_GRID_SIZE{
    ReadConstantNumeric("GRID_SIZE", "node.gpu.cuda.")};
const unsigned int CUDA_STREAM_NUM{
    ReadConstantNumeric("STREAM_NUM", "node.gpu.cuda.")};
const unsigned int CUDA_SCHEDULE_FLAG{
    ReadConstantNumeric("SCHEDULE_FLAG", "node.gpu.cuda.")};

// Guard mode constants
const bool GUARD_MODE{ReadConstantString("GUARD_MODE", "node.guard_mode.") ==
                      "true"};
const bool EXCLUDE_PRIV_IP{
    ReadConstantString("EXCLUDE_PRIV_IP", "node.guard_mode.") == "true"};
const unsigned int WINDOW_FOR_DS_NETWORK_INFO_UPDATE{ReadConstantNumeric(
    "WINDOW_FOR_DS_NETWORK_INFO_UPDATE", "node.guard_mode.")};

// Heartbeat constants
const unsigned int HEARTBEAT_INTERVAL_IN_SECONDS{
    ReadConstantNumeric("HEARTBEAT_INTERVAL_IN_SECONDS", "node.heartbeat.")};

// Network composition constants
const unsigned int COMM_SIZE{
    ReadConstantNumeric("COMM_SIZE", "node.network_composition.")};
const unsigned int NUM_DS_ELECTION{
    ReadConstantNumeric("NUM_DS_ELECTION", "node.network_composition.")};
const unsigned int SHARD_SIZE_THRESHOLD{
    ReadConstantNumeric("SHARD_SIZE_THRESHOLD", "node.network_composition.")};

// P2PComm constants
const unsigned int BROADCAST_INTERVAL{
    ReadConstantNumeric("BROADCAST_INTERVAL", "node.p2pcomm.")};
const unsigned int BROADCAST_EXPIRY{
    ReadConstantNumeric("BROADCAST_EXPIRY", "node.p2pcomm.")};
const unsigned int FETCH_LOOKUP_MSG_MAX_RETRY{
    ReadConstantNumeric("FETCH_LOOKUP_MSG_MAX_RETRY", "node.p2pcomm.")};
const uint32_t MAXMESSAGE{ReadConstantNumeric("MAXMESSAGE", "node.p2pcomm.")};
const unsigned int MAXRETRYCONN{
    ReadConstantNumeric("MAXRETRYCONN", "node.p2pcomm.")};
const unsigned int MSGQUEUE_SIZE{
    ReadConstantNumeric("MSGQUEUE_SIZE", "node.p2pcomm.")};
const unsigned int PUMPMESSAGE_MILLISECONDS{
    ReadConstantNumeric("PUMPMESSAGE_MILLISECONDS", "node.p2pcomm.")};
const unsigned int SENDQUEUE_SIZE{
    ReadConstantNumeric("SENDQUEUE_SIZE", "node.p2pcomm.")};

// PoW constants
const bool CUDA_GPU_MINE{ReadConstantString("CUDA_GPU_MINE", "node.pow.") ==
                         "true"};
const bool FULL_DATASET_MINE{
    ReadConstantString("FULL_DATASET_MINE", "node.pow.") == "true"};
const bool OPENCL_GPU_MINE{ReadConstantString("OPENCL_GPU_MINE", "node.pow.") ==
                           "true"};
const unsigned int DS_POW_DIFFICULTY{
    ReadConstantNumeric("DS_POW_DIFFICULTY", "node.pow.")};
const unsigned int POW_DIFFICULTY{
    ReadConstantNumeric("POW_DIFFICULTY", "node.pow.")};
const unsigned int POW_SUBMISSION_LIMIT{
    ReadConstantNumeric("POW_SUBMISSION_LIMIT", "node.pow.")};
const unsigned int NUM_FINAL_BLOCK_PER_POW{
    ReadConstantNumeric("NUM_FINAL_BLOCK_PER_POW", "node.pow.")};
const unsigned int POW_CHANGE_PERCENT_TO_ADJ_DIFF{
    ReadConstantNumeric("POW_CHANGE_PERCENT_TO_ADJ_DIFF", "node.pow.")};
const unsigned int EXPECTED_SHARD_NODE_NUM{
    ReadConstantNumeric("EXPECTED_SHARD_NODE_NUM", "node.pow.")};
const unsigned int MAX_SHARD_NODE_NUM{
    ReadConstantNumeric("MAX_SHARD_NODE_NUM", "node.pow.")};
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
const unsigned int PRIORITY_TOLERANCE_IN_PERCENT{
    ReadConstantNumeric("PRIORITY_TOLERANCE_IN_PERCENT", "node.pow.")};

// Recovery and upgrading constants
const unsigned int WAIT_LOOKUP_WAKEUP_IN_SECONDS{
    ReadConstantNumeric("WAIT_LOOKUP_WAKEUP_IN_SECONDS", "node.recovery.")};
const bool GET_INITIAL_DS_FROM_REPO{
    ReadConstantString("GET_INITIAL_DS_FROM_REPO", "node.recovery.") == "true"};
const unsigned int SHARD_DELAY_WAKEUP_IN_SECONDS{
    ReadConstantNumeric("SHARD_DELAY_WAKEUP_IN_SECONDS", "node.recovery.")};
const unsigned int TERMINATION_COUNTDOWN_IN_SECONDS{
    ReadConstantNumeric("TERMINATION_COUNTDOWN_IN_SECONDS", "node.recovery.")};
const string UPGRADE_HOST_ACCOUNT{
    ReadConstantString("UPGRADE_HOST_ACCOUNT", "node.recovery.")};
const string UPGRADE_HOST_REPO{
    ReadConstantString("UPGRADE_HOST_REPO", "node.recovery.")};
const bool RECOVERY_TRIM_INCOMPLETED_BLOCK{
    ReadConstantString("RECOVERY_TRIM_INCOMPLETED_BLOCK", "node.recovery.") ==
    "true"};
const bool REJOIN_NODE_NOT_IN_NETWORK{
    ReadConstantString("REJOIN_NODE_NOT_IN_NETWORK", "node.recovery.") ==
    "true"};

// Smart contract constants
const string SCILLA_ROOT{
    ReadConstantString("SCILLA_ROOT", "node.smart_contract.")};
const string SCILLA_CHECKER{
    SCILLA_ROOT + '/' +
    ReadConstantString("SCILLA_CHECKER", "node.smart_contract.")};
const string SCILLA_BINARY{
    SCILLA_ROOT + '/' +
    ReadConstantString("SCILLA_BINARY", "node.smart_contract.")};
const string SCILLA_FILES{
    ReadConstantString("SCILLA_FILES", "node.smart_contract.")};
const string SCILLA_LOG{
    ReadConstantString("SCILLA_LOG", "node.smart_contract.")};
const string SCILLA_LIB{
    SCILLA_ROOT + '/' +
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

// Test constants
const bool ENABLE_CHECK_PERFORMANCE_LOG{
    ReadConstantString("ENABLE_CHECK_PERFORMANCE_LOG", "node.tests.") ==
    "true"};
#ifdef FALLBACK_TEST
const unsigned int FALLBACK_TEST_EPOCH{
    ReadConstantNumeric("FALLBACK_TEST_EPOCH", "node.tests.")};
#endif  // FALLBACK_TEST
const unsigned int NUM_TXN_TO_SEND_PER_ACCOUNT{
    ReadConstantNumeric("NUM_TXN_TO_SEND_PER_ACCOUNT", "node.tests.")};

// Transaction constants
const boost::multiprecision::uint128_t COINBASE_REWARD{
    ReadConstantString("COINBASE_REWARD", "node.transactions.")};
const unsigned int LOOKUP_REWARD_IN_PERCENT{
    ReadConstantNumeric("LOOKUP_REWARD_IN_PERCENT", "node.transactions.")};
const unsigned int MAX_CODE_SIZE_IN_BYTES{
    ReadConstantNumeric("MAX_CODE_SIZE_IN_BYTES", "node.transactions.")};
const unsigned int MAX_CONTRACT_DEPTH{
    ReadConstantNumeric("MAX_CONTRACT_DEPTH", "node.transactions.")};
const unsigned int SYS_TIMESTAMP_VARIANCE_IN_SECONDS{ReadConstantNumeric(
    "SYS_TIMESTAMP_VARIANCE_IN_SECONDS", "node.transactions.")};
const unsigned int TXN_MISORDER_TOLERANCE_IN_PERCENT{ReadConstantNumeric(
    "TXN_MISORDER_TOLERANCE_IN_PERCENT", "node.transactions.")};
const unsigned int PACKET_EPOCH_LATE_ALLOW{
    ReadConstantNumeric("PACKET_EPOCH_LATE_ALLOW", "node.transactions.")};

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
