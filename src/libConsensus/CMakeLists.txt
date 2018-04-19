add_library(Consensus ConsensusBackup.cpp ConsensusCommon.cpp ConsensusLeader.cpp ConsensusUser.cpp)
target_include_directories(Consensus PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(Consensus PUBLIC ${PROJECT_SOURCE_DIR}/src/depends/g3log/include)
target_include_directories(Consensus PUBLIC ${PROJECT_SOURCE_DIR}/src/depends/g3log/src)
