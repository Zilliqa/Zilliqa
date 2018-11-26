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

unsigned int ReadFromConstantsFile(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<unsigned int>("node.constants." + propertyName);
}

unsigned int ReadFromTestsFile(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<unsigned int>("node.tests." + propertyName);
}

std::string ReadFromOptionsFile(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.options." + propertyName);
}

unsigned int ReadFromGasFile(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<unsigned int>("node.gas." + propertyName);
}

std::string ReadFromGasFileInString(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.gas." + propertyName);
}

std::string ReadSmartContractConstants(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.smart_contract." + propertyName);
}

std::string ReadDispatcherConstants(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.dispatcher." + propertyName);
}

std::string ReadArchivalConstants(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.archival." + propertyName);
}

const std::vector<std::string> ReadAccountsFromConstantsFile(
    std::string propName) {
  auto pt = PTree::GetInstance();
  std::vector<std::string> result;
  for (auto& acc : pt.get_child("node.accounts")) {
    auto child = acc.second.get_optional<std::string>(propName);
    if (child) {
      // LOG_GENERAL("constants " << child.get());
      result.push_back(child.get());
    }
  }
  return result;
}

unsigned int ReadGpuConstants(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<unsigned int>("node.gpu." + propertyName);
}

std::string ReadGPUVariableFromConstantsFile(std::string propertyName) {
  auto pt = PTree::GetInstance();
  return pt.get<std::string>("node.gpu." + propertyName);
}

const unsigned int MSG_VERSION{ReadFromConstantsFile("MSG_VERSION")};
const unsigned int DS_MULTICAST_CLUSTER_SIZE{
    ReadFromConstantsFile("DS_MULTICAST_CLUSTER_SIZE")};
const unsigned int COMM_SIZE{ReadFromConstantsFile("COMM_SIZE")};
const unsigned int NUM_DS_ELECTION{ReadFromConstantsFile("NUM_DS_ELECTION")};
const unsigned int POW_WINDOW_IN_SECONDS{
    ReadFromConstantsFile("POW_WINDOW_IN_SECONDS")};
const unsigned int NEW_NODE_SYNC_INTERVAL{
    ReadFromConstantsFile("NEW_NODE_SYNC_INTERVAL")};
const unsigned int RECOVERY_SYNC_TIMEOUT{
    ReadFromConstantsFile("RECOVERY_SYNC_TIMEOUT")};
const unsigned int POW_SUBMISSION_TIMEOUT{
    ReadFromConstantsFile("POW_SUBMISSION_TIMEOUT")};
const unsigned int DS_POW_DIFFICULTY{
    ReadFromConstantsFile("DS_POW_DIFFICULTY")};
const unsigned int POW_DIFFICULTY{ReadFromConstantsFile("POW_DIFFICULTY")};
const unsigned int POW_SUBMISSION_LIMIT{
    ReadFromConstantsFile("POW_SUBMISSION_LIMIT")};
const unsigned int MICROBLOCK_TIMEOUT{
    ReadFromConstantsFile("MICROBLOCK_TIMEOUT")};
const unsigned int VIEWCHANGE_TIME{ReadFromConstantsFile("VIEWCHANGE_TIME")};
const unsigned int VIEWCHANGE_PRECHECK_TIME{
    ReadFromConstantsFile("VIEWCHANGE_PRECHECK_TIME")};
const unsigned int VIEWCHANGE_EXTRA_TIME{
    ReadFromConstantsFile("VIEWCHANGE_EXTRA_TIME")};
const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW{
    ReadFromConstantsFile("CONSENSUS_MSG_ORDER_BLOCK_WINDOW")};
const unsigned int CONSENSUS_OBJECT_TIMEOUT{
    ReadFromConstantsFile("CONSENSUS_OBJECT_TIMEOUT")};
const unsigned int FETCHING_MISSING_DATA_TIMEOUT{
    ReadFromConstantsFile("FETCHING_MISSING_DATA_TIMEOUT")};
const unsigned int NUM_FINAL_BLOCK_PER_POW{
    ReadFromConstantsFile("NUM_FINAL_BLOCK_PER_POW")};
const uint32_t MAXMESSAGE{ReadFromConstantsFile("MAXMESSAGE")};
const unsigned int TX_SHARING_CLUSTER_SIZE{
    ReadFromConstantsFile("TX_SHARING_CLUSTER_SIZE")};
const unsigned int NEW_NODE_POW_DELAY{
    ReadFromConstantsFile("NEW_NODE_POW_DELAY")};
const unsigned int POST_VIEWCHANGE_BUFFER{
    ReadFromConstantsFile("POST_VIEWCHANGE_BUFFER")};
const unsigned int COINBASE_REWARD{ReadFromConstantsFile("COINBASE_REWARD")};
const unsigned int DEBUG_LEVEL{ReadFromConstantsFile("DEBUG_LEVEL")};
const unsigned int BROADCAST_INTERVAL{
    ReadFromConstantsFile("BROADCAST_INTERVAL")};
const unsigned int BROADCAST_EXPIRY{ReadFromConstantsFile("BROADCAST_EXPIRY")};
const unsigned int TX_DISTRIBUTE_TIME_IN_MS{
    ReadFromConstantsFile("TX_DISTRIBUTE_TIME_IN_MS")};
const unsigned int FINALBLOCK_DELAY_IN_MS{
    ReadFromConstantsFile("FINALBLOCK_DELAY_IN_MS")};
const unsigned int NUM_TXN_TO_SEND_PER_ACCOUNT{
    ReadFromConstantsFile("NUM_TXN_TO_SEND_PER_ACCOUNT")};
const unsigned int NUM_NODES_TO_SEND_LOOKUP{
    ReadFromConstantsFile("NUM_NODES_TO_SEND_LOOKUP")};
const unsigned int MAX_INDEXES_PER_TXN{
    ReadFromConstantsFile("MAX_INDEXES_PER_TXN")};
const unsigned int SENDQUEUE_SIZE{ReadFromConstantsFile("SENDQUEUE_SIZE")};
const unsigned int MSGQUEUE_SIZE{ReadFromConstantsFile("MSGQUEUE_SIZE")};
const unsigned int POW_CHANGE_PERCENT_TO_ADJ_DIFF{
    ReadFromConstantsFile("POW_CHANGE_PERCENT_TO_ADJ_DIFF")};
const unsigned int FALLBACK_INTERVAL_STARTED{
    ReadFromConstantsFile("FALLBACK_INTERVAL_STARTED")};
const unsigned int FALLBACK_INTERVAL_WAITING{
    ReadFromConstantsFile("FALLBACK_INTERVAL_WAITING")};
const unsigned int FALLBACK_CHECK_INTERVAL{
    ReadFromConstantsFile("FALLBACK_CHECK_INTERVAL")};
const unsigned int FALLBACK_EXTRA_TIME{
    ReadFromConstantsFile("FALLBACK_EXTRA_TIME")};
const unsigned int MAX_ROUNDS_IN_BSTATE{
    ReadFromConstantsFile("MAX_ROUNDS_IN_BSTATE")};
const unsigned int MAX_ROUNDS_IN_CSTATE{
    ReadFromConstantsFile("MAX_ROUNDS_IN_CSTATE")};
const unsigned int MAX_TOTAL_ROUNDS{ReadFromConstantsFile("MAX_TOTAL_ROUNDS")};
const unsigned int ROUND_TIME_IN_MS{ReadFromConstantsFile("ROUND_TIME_IN_MS")};
const unsigned int MAX_NEIGHBORS_PER_ROUND{
    ReadFromConstantsFile("MAX_NEIGHBORS_PER_ROUND")};
const unsigned int NUM_NODE_INCR_DIFFICULTY{
    ReadFromConstantsFile("NUM_NODE_INCR_DIFFICULTY")};
const unsigned int MAX_SHARD_NODE_NUM{
    ReadFromConstantsFile("MAX_SHARD_NODE_NUM")};
const unsigned int NUM_MICROBLOCK_SENDERS{
    ReadFromConstantsFile("NUM_MICROBLOCK_SENDERS")};
const unsigned int NUM_MICROBLOCK_GOSSIP_RECEIVERS{
    ReadFromConstantsFile("NUM_MICROBLOCK_GOSSIP_RECEIVERS")};
const unsigned int NUM_FINALBLOCK_GOSSIP_RECEIVERS_PER_SHARD{
    ReadFromConstantsFile("NUM_FINALBLOCK_GOSSIP_RECEIVERS_PER_SHARD")};
const unsigned int HEARTBEAT_INTERVAL_IN_SECONDS{
    ReadFromConstantsFile("HEARTBEAT_INTERVAL_IN_SECONDS")};
const unsigned int TERMINATION_COUNTDOWN_IN_SECONDS{
    ReadFromConstantsFile("TERMINATION_COUNTDOWN_IN_SECONDS")};
const unsigned int DS_DELAY_WAKEUP_IN_SECONDS{
    ReadFromConstantsFile("DS_DELAY_WAKEUP_IN_SECONDS")};
const unsigned int SHARD_DELAY_WAKEUP_IN_SECONDS{
    ReadFromConstantsFile("SHARD_DELAY_WAKEUP_IN_SECONDS")};
const unsigned int NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD{
    ReadFromConstantsFile("NUM_FORWARDED_BLOCK_RECEIVERS_PER_SHARD")};
const unsigned int NUM_OF_TREEBASED_CHILD_CLUSTERS{
    ReadFromConstantsFile("NUM_OF_TREEBASED_CHILD_CLUSTERS")};
const unsigned int FETCH_LOOKUP_MSG_MAX_RETRY{
    ReadFromConstantsFile("FETCH_LOOKUP_MSG_MAX_RETRY")};
const unsigned int MAX_CONTRACT_DEPTH{
    ReadFromConstantsFile("MAX_CONTRACT_DEPTH")};
const unsigned int COMMIT_WINDOW_IN_SECONDS{
    ReadFromConstantsFile("COMMIT_WINDOW_IN_SECONDS")};
const unsigned int NUM_CONSENSUS_SUBSETS{
    ReadFromConstantsFile("NUM_CONSENSUS_SUBSETS")};
const unsigned int MISORDER_TOLERANCE_IN_PERCENT{
    ReadFromConstantsFile("MISORDER_TOLERANCE_IN_PERCENT")};
const unsigned int MAX_CODE_SIZE_IN_BYTES{
    ReadFromConstantsFile("MAX_CODE_SIZE_IN_BYTES")};
const unsigned int LOOKUP_REWARD_IN_PERCENT{
    ReadFromConstantsFile("LOOKUP_REWARD_IN_PERCENT")};
const unsigned int PUMPMESSAGE_MILLISECONDS{
    ReadFromConstantsFile("PUMPMESSAGE_MILLISECONDS")};
const unsigned int MAXRETRYCONN{ReadFromConstantsFile("MAXRETRYCONN")};
const unsigned int SIMULATED_NETWORK_DELAY_IN_MS{
    ReadFromConstantsFile("SIMULATED_NETWORK_DELAY_IN_MS")};
const unsigned int POW_PACKET_SENDERS{
    ReadFromConstantsFile("POW_PACKET_SENDERS")};
const unsigned int POWPACKETSUBMISSION_WINDOW_IN_SECONDS{
    ReadFromConstantsFile("POWPACKETSUBMISSION_WINDOW_IN_SECONDS")};

#ifdef FALLBACK_TEST
const unsigned int FALLBACK_TEST_EPOCH{
    ReadFromTestsFile("FALLBACK_TEST_EPOCH")};
#endif  // FALLBACK_TEST

// options
const bool EXCLUDE_PRIV_IP{ReadFromOptionsFile("EXCLUDE_PRIV_IP") == "true"};
const bool GUARD_MODE{ReadFromOptionsFile("GUARD_MODE") == "true"};
const bool ENABLE_DO_REJOIN{ReadFromOptionsFile("ENABLE_DO_REJOIN") == "true"};
const bool FULL_DATASET_MINE{ReadFromOptionsFile("FULL_DATASET_MINE") ==
                             "true"};
const bool OPENCL_GPU_MINE{ReadFromOptionsFile("OPENCL_GPU_MINE") == "true"};
const bool CUDA_GPU_MINE{ReadFromOptionsFile("CUDA_GPU_MINE") == "true"};
const bool LOOKUP_NODE_MODE{ReadFromOptionsFile("LOOKUP_NODE_MODE") == "true"};
const bool BROADCAST_GOSSIP_MODE{ReadFromOptionsFile("BROADCAST_GOSSIP_MODE") ==
                                 "true"};
const bool GOSSIP_CUSTOM_ROUNDS_SETTINGS{
    ReadFromOptionsFile("GOSSIP_CUSTOM_ROUNDS_SETTINGS") == "true"};
const bool BROADCAST_TREEBASED_CLUSTER_MODE{
    ReadFromOptionsFile("BROADCAST_TREEBASED_CLUSTER_MODE") == "true"};
const bool GET_INITIAL_DS_FROM_REPO{
    ReadFromOptionsFile("GET_INITIAL_DS_FROM_REPO") == "true"};
const std::string UPGRADE_HOST_ACCOUNT{
    ReadFromOptionsFile("UPGRADE_HOST_ACCOUNT")};
const std::string UPGRADE_HOST_REPO{ReadFromOptionsFile("UPGRADE_HOST_REPO")};
const bool ARCHIVAL_NODE{ReadFromOptionsFile("ARCHIVAL_NODE") == "true"};

// gas
const unsigned int MICROBLOCK_GAS_LIMIT{
    ReadFromGasFile("MICROBLOCK_GAS_LIMIT")};
const unsigned int CONTRACT_CREATE_GAS{ReadFromGasFile("CONTRACT_CREATE_GAS")};
const unsigned int CONTRACT_INVOKE_GAS{ReadFromGasFile("CONTRACT_INVOKE_GAS")};
const unsigned int NORMAL_TRAN_GAS{ReadFromGasFile("NORMAL_TRAN_GAS")};
const unsigned int GAS_CONGESTION_PERCENT{
    ReadFromGasFile("GAS_CONGESTION_PERCENT")};
const unsigned int UNFILLED_PERCENT_LOW{
    ReadFromGasFile("UNFILLED_PERCENT_LOW")};
const unsigned int UNFILLED_PERCENT_HIGH{
    ReadFromGasFile("UNFILLED_PERCENT_HIGH")};
const unsigned int GAS_PRICE_PRECISION{ReadFromGasFile("GAS_PRICE_PRECISION")};
const unsigned int GAS_PRICE_DROP_RATIO{
    ReadFromGasFile("GAS_PRICE_DROP_RATIO")};
const unsigned int GAS_PRICE_RAISE_RATIO_LOWER{
    ReadFromGasFile("GAS_PRICE_RAISE_RATIO_LOWER")};
const unsigned int GAS_PRICE_RAISE_RATIO_UPPER{
    ReadFromGasFile("GAS_PRICE_RAISE_RATIO_UPPER")};
const unsigned int GAS_PRICE_TOLERANCE{ReadFromGasFile("GAS_PRICE_TOLERANCE")};
const unsigned int MEAN_GAS_PRICE_DS_NUM{
    ReadFromGasFile("MEAN_GAS_PRICE_DS_NUM")};
const boost::multiprecision::uint128_t PRECISION_MIN_VALUE{
    SafeMath<boost::multiprecision::uint128_t>::power(10, GAS_PRICE_PRECISION)};
const std::string LEGAL_GAS_PRICE_IP{
    ReadFromGasFileInString("LEGAL_GAS_PRICE_IP")};

// accounts
const std::vector<std::string> GENESIS_WALLETS{
    ReadAccountsFromConstantsFile("wallet_address")};
const std::vector<std::string> GENESIS_KEYS{
    ReadAccountsFromConstantsFile("private_key")};

// smart contract
const std::string SCILLA_ROOT{ReadSmartContractConstants("SCILLA_ROOT")};
const std::string SCILLA_CHECKER{SCILLA_ROOT + '/' +
                                 ReadSmartContractConstants("SCILLA_CHECKER")};
const std::string SCILLA_BINARY{SCILLA_ROOT + '/' +
                                ReadSmartContractConstants("SCILLA_BINARY")};
const std::string SCILLA_FILES{ReadSmartContractConstants("SCILLA_FILES")};
const std::string SCILLA_LOG{ReadSmartContractConstants("SCILLA_LOG")};
const std::string SCILLA_LIB{SCILLA_ROOT + '/' +
                             ReadSmartContractConstants("SCILLA_LIB")};
const std::string INIT_JSON{SCILLA_FILES + '/' +
                            ReadSmartContractConstants("INIT_JSON")};
const std::string INPUT_STATE_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_STATE_JSON")};
const std::string INPUT_BLOCKCHAIN_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_BLOCKCHAIN_JSON")};
const std::string INPUT_MESSAGE_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_MESSAGE_JSON")};
const std::string OUTPUT_JSON{SCILLA_FILES + '/' +
                              ReadSmartContractConstants("OUTPUT_JSON")};
const std::string INPUT_CODE{SCILLA_FILES + '/' +
                             ReadSmartContractConstants("INPUT_CODE")};

// dispatcher
const std::string TXN_PATH{ReadDispatcherConstants("TXN_PATH")};
const bool USE_REMOTE_TXN_CREATOR{
    ReadDispatcherConstants("USE_REMOTE_TXN_CREATOR") == "true"};

// archival
const std::string DB_HOST{ReadArchivalConstants("DB_HOST")};

// GPU
const std::string GPU_TO_USE{ReadGPUVariableFromConstantsFile("GPU_TO_USE")};
const unsigned int OPENCL_LOCAL_WORK_SIZE{
    ReadGpuConstants("opencl.LOCAL_WORK_SIZE")};
const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER{
    ReadGpuConstants("opencl.GLOBAL_WORK_SIZE_MULTIPLIER")};
const unsigned int OPENCL_START_EPOCH{ReadGpuConstants("opencl.START_EPOCH")};
const unsigned int CUDA_BLOCK_SIZE{ReadGpuConstants("cuda.BLOCK_SIZE")};
const unsigned int CUDA_GRID_SIZE{ReadGpuConstants("cuda.GRID_SIZE")};
const unsigned int CUDA_STREAM_NUM{ReadGpuConstants("cuda.STREAM_NUM")};
const unsigned int CUDA_SCHEDULE_FLAG{ReadGpuConstants("cuda.SCHEDULE_FLAG")};
