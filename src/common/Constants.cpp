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
#include "Constants.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using boost::property_tree::ptree;

struct PTree
{
    static ptree& GetInstance()
    {
        static ptree pt;
        read_xml("constants.xml", pt);

        return pt;
    }
    PTree() = delete;
    ~PTree() = delete;
};

unsigned int ReadFromConstantsFile(std::string propertyName)
{
    auto pt = PTree::GetInstance();
    return pt.get<unsigned int>("node.constants." + propertyName);
}

std::string ReadFromOptionsFile(std::string propertyName)
{
    auto pt = PTree::GetInstance();
    return pt.get<std::string>("node.options." + propertyName);
}

std::string ReadSmartContractConstants(std::string propertyName)
{
    auto pt = PTree::GetInstance();
    return pt.get<std::string>("node.smart_contract." + propertyName);
}

std::string ReadDispatcherConstants(std::string propertyName)
{
    auto pt = PTree::GetInstance();
    return pt.get<std::string>("node.dispatcher." + propertyName);
}

const std::vector<std::string>
ReadAccountsFromConstantsFile(std::string propName)
{
    auto pt = PTree::GetInstance();
    std::vector<std::string> result;
    for (auto& acc : pt.get_child("node.accounts"))
    {
        auto child = acc.second.get_optional<std::string>(propName);
        if (child)
        {
            // LOG_GENERAL("constants " << child.get());
            result.push_back(child.get());
        }
    }
    return result;
}

unsigned int ReadGpuConstants(std::string propertyName)
{
    auto pt = PTree::GetInstance();
    return pt.get<unsigned int>("node.gpu." + propertyName);
}

const unsigned int MSG_VERSION{ReadFromConstantsFile("MSG_VERSION")};
const unsigned int DS_MULTICAST_CLUSTER_SIZE{
    ReadFromConstantsFile("DS_MULTICAST_CLUSTER_SIZE")};
const unsigned int COMM_SIZE{ReadFromConstantsFile("COMM_SIZE")};
const unsigned int POW_WINDOW_IN_SECONDS{
    ReadFromConstantsFile("POW_WINDOW_IN_SECONDS")};
const unsigned int POW_BACKUP_WINDOW_IN_SECONDS{
    ReadFromConstantsFile("POW_BACKUP_WINDOW_IN_SECONDS")};
const unsigned int NEW_NODE_SYNC_INTERVAL{
    ReadFromConstantsFile("NEW_NODE_SYNC_INTERVAL")};
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
const unsigned int VIEWCHANGE_EXTRA_TIME{
    ReadFromConstantsFile("VIEWCHANGE_EXTRA_TIME")};
const unsigned int CONSENSUS_MSG_ORDER_BLOCK_WINDOW{
    ReadFromConstantsFile("CONSENSUS_MSG_ORDER_BLOCK_WINDOW")};
const unsigned int CONSENSUS_OBJECT_TIMEOUT{
    ReadFromConstantsFile("CONSENSUS_OBJECT_TIMEOUT")};
const unsigned int FETCHING_MISSING_TXNS_TIMEOUT{
    ReadFromConstantsFile("FETCHING_MISSING_TXNS_TIMEOUT")};
const unsigned int FINALBLOCK_CONSENSUS_OBJECT_TIMEOUT{
    ReadFromConstantsFile("FINALBLOCK_CONSENSUS_OBJECT_TIMEOUT")};
const unsigned int NUM_FINAL_BLOCK_PER_POW{
    ReadFromConstantsFile("NUM_FINAL_BLOCK_PER_POW")};
const unsigned int NUM_DS_KEEP_TX_BODY{
    ReadFromConstantsFile("NUM_DS_KEEP_TX_BODY")};
const uint32_t MAXMESSAGE{ReadFromConstantsFile("MAXMESSAGE")};
const unsigned int MAXSUBMITTXNPERNODE{
    ReadFromConstantsFile("MAXSUBMITTXNPERNODE")};
const unsigned int MICROBLOCK_GAS_LIMIT{
    ReadFromConstantsFile("MICROBLOCK_GAS_LIMIT")};
const unsigned int TX_SHARING_CLUSTER_SIZE{
    ReadFromConstantsFile("TX_SHARING_CLUSTER_SIZE")};
const unsigned int NEW_NODE_POW_DELAY{
    ReadFromConstantsFile("NEW_NODE_POW_DELAY")};
const unsigned int POST_VIEWCHANGE_BUFFER{
    ReadFromConstantsFile("POST_VIEWCHANGE_BUFFER")};
const unsigned int WAITING_FORWARD{ReadFromConstantsFile("WAITING_FORWARD")};
const unsigned int CONTRACT_CREATE_GAS{
    ReadFromConstantsFile("CONTRACT_CREATE_GAS")};
const unsigned int CONTRACT_INVOKE_GAS{
    ReadFromConstantsFile("CONTRACT_INVOKE_GAS")};
const unsigned int NORMAL_TRAN_GAS{ReadFromConstantsFile("NORMAL_TRAN_GAS")};
const unsigned int COINBASE_REWARD{ReadFromConstantsFile("COINBASE_REWARD")};
const unsigned int DEBUG_LEVEL{ReadFromConstantsFile("DEBUG_LEVEL")};
const unsigned int BROADCAST_INTERVAL{
    ReadFromConstantsFile("BROADCAST_INTERVAL")};
const unsigned int BROADCAST_EXPIRY{ReadFromConstantsFile("BROADCAST_EXPIRY")};
const unsigned int TX_DISTRIBUTE_TIME_IN_MS{
    ReadFromConstantsFile("TX_DISTRIBUTE_TIME_IN_MS")};
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
const unsigned int NUM_NETWORK_NODE{ReadFromConstantsFile("NUM_NETWORK_NODE")};

const bool EXCLUDE_PRIV_IP{
    ReadFromOptionsFile("EXCLUDE_PRIV_IP") == "true" ? true : false};
const bool TEST_NET_MODE{
    ReadFromOptionsFile("TEST_NET_MODE") == "true" ? true : false};
const bool ENABLE_DO_REJOIN{
    ReadFromOptionsFile("ENABLE_DO_REJOIN") == "true" ? true : false};
const bool FULL_DATASET_MINE{
    ReadFromOptionsFile("FULL_DATASET_MINE") == "true" ? true : false};
const bool OPENCL_GPU_MINE{
    ReadFromOptionsFile("OPENCL_GPU_MINE") == "true" ? true : false};
const bool CUDA_GPU_MINE{
    ReadFromOptionsFile("CUDA_GPU_MINE") == "true" ? true : false};
const bool LOOKUP_NODE_MODE{
    ReadFromOptionsFile("LOOKUP_NODE_MODE") == "true" ? true : false};

const std::vector<std::string> GENESIS_WALLETS{
    ReadAccountsFromConstantsFile("wallet_address")};
const std::vector<std::string> GENESIS_KEYS{
    ReadAccountsFromConstantsFile("private_key")};
const std::string SCILLA_ROOT{ReadSmartContractConstants("SCILLA_ROOT")};
const std::string SCILLA_BINARY{SCILLA_ROOT + '/'
                                + ReadSmartContractConstants("SCILLA_BINARY")};
const std::string SCILLA_FILES{ReadSmartContractConstants("SCILLA_FILES")};
const std::string SCILLA_LOG{ReadSmartContractConstants("SCILLA_LOG")};
const std::string SCILLA_LIB{SCILLA_ROOT + '/'
                             + ReadSmartContractConstants("SCILLA_LIB")};
const std::string INIT_JSON{SCILLA_FILES + '/'
                            + ReadSmartContractConstants("INIT_JSON")};
const std::string INPUT_STATE_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_STATE_JSON")};
const std::string INPUT_BLOCKCHAIN_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_BLOCKCHAIN_JSON")};
const std::string INPUT_MESSAGE_JSON{
    SCILLA_FILES + '/' + ReadSmartContractConstants("INPUT_MESSAGE_JSON")};
const std::string OUTPUT_JSON{SCILLA_FILES + '/'
                              + ReadSmartContractConstants("OUTPUT_JSON")};
const std::string INPUT_CODE{SCILLA_FILES + '/'
                             + ReadSmartContractConstants("INPUT_CODE")};

const std::string TXN_PATH{ReadDispatcherConstants("TXN_PATH")};
const bool USE_REMOTE_TXN_CREATOR{
    ReadDispatcherConstants("USE_REMOTE_TXN_CREATOR") == "true" ? true : false};

const unsigned int OPENCL_LOCAL_WORK_SIZE{
    ReadGpuConstants("opencl.LOCAL_WORK_SIZE")};
const unsigned int OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER{
    ReadGpuConstants("opencl.GLOBAL_WORK_SIZE_MULTIPLIER")};
const unsigned int OPENCL_START_EPOCH{ReadGpuConstants("opencl.START_EPOCH")};
const unsigned int CUDA_BLOCK_SIZE{ReadGpuConstants("cuda.BLOCK_SIZE")};
const unsigned int CUDA_GRID_SIZE{ReadGpuConstants("cuda.GRID_SIZE")};
const unsigned int CUDA_STREAM_NUM{ReadGpuConstants("cuda.STREAM_NUM")};
const unsigned int CUDA_SCHEDULE_FLAG{ReadGpuConstants("cuda.SCHEDULE_FLAG")};
