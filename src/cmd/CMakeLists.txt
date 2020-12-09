link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(zilliqa main.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zilliqa> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(zilliqa PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(zilliqa PUBLIC Consensus Network Utils Zilliqa DirectoryService Node Validator NAT Boost::program_options)


add_executable(sendcmd sendcmd.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sendcmd> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(sendcmd PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sendcmd PUBLIC Network Utils -s)

add_executable(genaccounts genaccounts.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:genaccounts> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(genaccounts PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(genaccounts PUBLIC AccountData Schnorr Utils -s)

add_executable(genkeypair genkeypair.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:genkeypair> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(genkeypair PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(genkeypair PUBLIC Schnorr Utils -s)

add_executable(gentxn gentxn.cpp)
add_custom_command(TARGET zilliqa
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gentxn> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(gentxn PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gentxn PUBLIC AccountData Message Network -s)

add_executable(signmultisig signmultisig.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:signmultisig> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(signmultisig PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(signmultisig PUBLIC Schnorr Utils -s)

add_executable(verifymultisig verifymultisig.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:verifymultisig> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(verifymultisig PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(verifymultisig PUBLIC Schnorr Utils -s)

add_executable(gensigninitialds gen_sign_initial_ds.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gensigninitialds> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(gensigninitialds PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gensigninitialds PUBLIC Utils Persistence Schnorr
    g3logger -s)


add_executable(grepperf grepperf.cpp)
target_include_directories(grepperf PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(grepperf PUBLIC Boost::program_options -s)

add_executable(getaddr GetAddressFromPubKey.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:getaddr> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(getaddr PUBLIC ${CMAKE_SOURCE_DIR}/src Schnorr)
target_link_libraries(getaddr PUBLIC Schnorr Utils crypto -s)

add_executable(getpub GetPubKeyFromPrivKey.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:getpub> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(getpub PUBLIC ${CMAKE_SOURCE_DIR}/src Schnorr)
target_link_libraries(getpub PUBLIC Schnorr Utils crypto -s)

add_executable(validateDB validateDB.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:validateDB> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(validateDB PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(validateDB PUBLIC Node Mediator Validator -s)

add_executable(restore restore.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:restore> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(restore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(restore PUBLIC Node Mediator Validator -s)

add_executable(genTxnBodiesFromS3 genTxnBodiesFromS3.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:genTxnBodiesFromS3> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(genTxnBodiesFromS3 PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(genTxnBodiesFromS3 PUBLIC AccountData Utils Persistence
    Server -s)

add_executable(isolatedServer isolated_server.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:isolatedServer> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(isolatedServer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(isolatedServer PUBLIC AccountData Mediator Persistence Server Validator)

add_executable(data_migrate data_migrate.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:data_migrate> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(data_migrate PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(data_migrate PUBLIC Mediator Utils Persistence AccountData)

add_executable(websocketsubscriber websocketsubscriber.cpp)
add_custom_command(TARGET zilliqa
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:websocketsubscriber> ${CMAKE_BINARY_DIR}/tests/Zilliqa)
target_include_directories(websocketsubscriber PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(websocketsubscriber PUBLIC Utils Server)