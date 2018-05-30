link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(zilliqa main.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zilliqa> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(zilliqa PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(zilliqa PUBLIC Consensus Network Utils Zilliqa DirectoryService Node Validator)

add_executable(sendcmd sendcmd.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sendcmd> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(sendcmd PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendcmd PUBLIC Network Utils)

add_executable(sendtxn send_txn.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sendtxn> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(sendtxn PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendtxn PUBLIC Network Utils)

add_executable(genkeypair genkeypair.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:genkeypair> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(genkeypair PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(genkeypair PUBLIC Crypto)
