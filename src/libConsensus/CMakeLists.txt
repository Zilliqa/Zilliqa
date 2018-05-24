add_library(Consensus ConsensusBackup.cpp ConsensusCommon.cpp ConsensusLeader.cpp ConsensusUser.cpp)
target_include_directories(Consensus PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries(Consensus PUBLIC Network)
