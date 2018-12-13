link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(zilliqa main.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zilliqa> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(zilliqa PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(zilliqa PUBLIC Consensus Network Utils Zilliqa DirectoryService Node Validator NAT)


add_executable(sendcmd sendcmd.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sendcmd> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(sendcmd PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendcmd PUBLIC Network Utils)

add_executable(genkeypair genkeypair.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:genkeypair> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(genkeypair PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(genkeypair PUBLIC Crypto)

add_executable(gentxn gentxn.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gentxn> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(gentxn PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gentxn PUBLIC AccountData Message Network)

add_executable(signmultisig signmultisig.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:signmultisig> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(signmultisig PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signmultisig PUBLIC Crypto)

add_executable(gensigninitialds gen_sign_initial_ds.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gensigninitialds> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(gensigninitialds PUBLIC ${CMAKE_SOURCE_DIR}/src Crypto ${G3LOG_INCLUDE_DIRS})
target_link_libraries(gensigninitialds PUBLIC Utils Persistence g3logger)

add_executable(grepperf grepperf.cpp)
target_include_directories(grepperf PUBLIC ${CMAKE_SOURCE_DIR}/src)