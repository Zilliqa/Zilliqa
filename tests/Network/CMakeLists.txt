# This test is no longer up-to-date after P2PComm has been changed to include broadcast_list_retriever
#add_executable (Test_P2PComm Test_P2PComm.cpp)
#target_include_directories (Test_P2PComm PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries (Test_P2PComm LINK_PUBLIC Network Utils)

# This test is no longer up-to-date after P2PComm has been changed to include state
# (i.e., we can't run more than one Zilliqa instance now per process)
#add_executable (Test_PeerManager Test_PeerManager.cpp)
#target_include_directories (Test_PeerManager PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries (Test_PeerManager LINK_PUBLIC Utils Network Zilliqa)

add_executable (Test_PeerStore Test_PeerStore.cpp)
target_include_directories (Test_PeerStore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_PeerStore LINK_PUBLIC Network Utils)
