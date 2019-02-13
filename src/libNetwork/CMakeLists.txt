add_library (Network Peer.cpp P2PComm.cpp Guard.cpp Blacklist.cpp ReputationManager.cpp RumorManager.cpp DataSender.cpp)
target_include_directories (Network PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Network PUBLIC Crypto Constants event RumorSpreading Message)
