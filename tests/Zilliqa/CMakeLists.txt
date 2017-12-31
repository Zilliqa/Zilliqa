add_executable (zilliqa main.cpp)
target_include_directories (zilliqa PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (zilliqa LINK_PUBLIC Consensus Network Utils Zilliqa DirectoryService Node)
target_link_libraries (zilliqa LINK_PUBLIC Consensus Network Utils Zilliqa DirectoryService)

add_executable (sendcmd sendcmd.cpp)
target_include_directories (sendcmd PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (sendcmd LINK_PUBLIC Network Utils)

add_executable (sendtxn send_txn.cpp)
target_include_directories (sendtxn PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (sendtxn LINK_PUBLIC Network Utils)

add_executable (genkeypair genkeypair.cpp)
target_include_directories (genkeypair PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (genkeypair LINK_PUBLIC Crypto)