add_library (Network Peer.cpp PeerStore.cpp PeerManager.cpp P2PComm.cpp)
target_include_directories (Network PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Network LINK_PUBLIC Crypto)