add_library(Consensus ConsensusBackup.cpp ConsensusCommon.cpp ConsensusLeader.cpp)
target_include_directories(Consensus PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Consensus PUBLIC Message Network)
