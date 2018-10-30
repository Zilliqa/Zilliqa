link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_ConsensusLeader Test_ConsensusLeader.cpp)
target_include_directories (Test_ConsensusLeader PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_ConsensusLeader PUBLIC AccountData Block BlockHeader Consensus Message Utils Boost::unit_test_framework)
add_test(NAME Test_ConsensusLeader COMMAND Test_ConsensusLeader)

add_executable(Test_ConsensusBackup Test_ConsensusBackup.cpp)
target_include_directories (Test_ConsensusBackup PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_ConsensusBackup PUBLIC Consensus Network Utils)
add_test(NAME Test_ConsensusBackup COMMAND Test_ConsensusBackup)
