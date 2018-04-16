add_executable(zilliqa main.cpp)
target_include_directories(zilliqa PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(zilliqa PUBLIC Consensus Network Utils Zilliqa DirectoryService Node)

add_executable(sendcmd sendcmd.cpp)
target_include_directories(sendcmd PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendcmd PUBLIC Network Utils)

add_executable(sendtxn send_txn.cpp)
target_include_directories(sendtxn PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendtxn PUBLIC Network Utils)

add_executable(genkeypair genkeypair.cpp)
target_include_directories(genkeypair PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(genkeypair PUBLIC Crypto)

set_target_properties(zilliqa sendcmd sendtxn genkeypair
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
