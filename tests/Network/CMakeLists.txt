if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

# This test is no longer up-to-date after P2PComm has been changed to include broadcast_list_retriever
#add_executable (Test_P2PComm Test_P2PComm.cpp)
#target_include_directories (Test_P2PComm PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries (Test_P2PComm PUBLIC Network Utils)

# This test is no longer up-to-date after P2PComm has been changed to include state
# (i.e., we can't run more than one Zilliqa instance now per process)
#add_executable (Test_PeerManager Test_PeerManager.cpp)
#target_include_directories (Test_PeerManager PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries (Test_PeerManager PUBLIC Utils Network Zilliqa)

link_directories(${CMAKE_BINARY_DIR}/lib)
add_executable (Test_PeerStore Test_PeerStore.cpp)
target_include_directories (Test_PeerStore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_PeerStore PUBLIC Network Utils)
add_test(NAME Test_PeerStore COMMAND Test_PeerStore)
