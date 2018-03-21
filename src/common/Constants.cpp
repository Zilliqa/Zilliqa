#include "Constants.h"

const unsigned int DS_MULTICAST_CLUSTER_SIZE{ReadFromConstantsFile("DS_MULTICAST_CLUSTER_SIZE")};
const unsigned int COMM_SIZE(ReadFromConstantsFile("COMM_SIZE"));
const unsigned int MAX_POW1_WINNERS(ReadFromConstantsFile("MAX_POW1_WINNERS"));
const unsigned int POW1_WINDOW_IN_SECONDS(ReadFromConstantsFile("POW1_WINDOW_IN_SECONDS"));
const unsigned int POW1_BACKUP_WINDOW_IN_SECONDS(ReadFromConstantsFile("POW1_BACKUP_WINDOW_IN_SECONDS"));
const unsigned int LEADER_SHARDING_PREPARATION_IN_SECONDS(ReadFromConstantsFile("LEADER_SHARDING_PREPARATION_IN_SECONDS"));
const unsigned int LEADER_POW2_WINDOW_IN_SECONDS(ReadFromConstantsFile("LEADER_POW2_WINDOW_IN_SECONDS"));
const unsigned int BACKUP_POW2_WINDOW_IN_SECONDS(ReadFromConstantsFile("BACKUP_POW2_WINDOW_IN_SECONDS"));
const unsigned int NEW_NODE_POW2_TIMEOUT_IN_SECONDS(ReadFromConstantsFile("NEW_NODE_POW2_TIMEOUT_IN_SECONDS"));
const unsigned int POW_SUB_BUFFER_TIME(ReadFromConstantsFile("POW_SUB_BUFFER_TIME")); //milliseconds
const unsigned int POW1_DIFFICULTY(ReadFromConstantsFile("POW1_DIFFICULTY"));
const unsigned int POW2_DIFFICULTY(ReadFromConstantsFile("POW2_DIFFICULTY"));
const unsigned int NUM_FINAL_BLOCK_PER_POW(ReadFromConstantsFile("NUM_FINAL_BLOCK_PER_POW"));
const uint32_t MAXMESSAGE(ReadFromConstantsFile("MAXMESSAGE"));
