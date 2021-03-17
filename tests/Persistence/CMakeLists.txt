if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)


link_directories(${CMAKE_BINARY_DIR}/lib)
add_executable(Test_MetaPersistence Test_MetaPersistence.cpp)
target_include_directories(Test_MetaPersistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_MetaPersistence PUBLIC Utils Persistence Message)

add_executable(Test_TrieDB Test_TrieDB.cpp)
target_include_directories(Test_TrieDB PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TrieDB PUBLIC Utils Trie AccountData Persistence)

add_executable(Test_DSPersistence Test_DSPersistence.cpp)
target_include_directories(Test_DSPersistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_DSPersistence PUBLIC AccountData Utils Persistence Message TestUtils)

add_executable(Test_TxPersistence Test_TxPersistence.cpp)
target_include_directories(Test_TxPersistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TxPersistence PUBLIC AccountData Utils Persistence Message)

add_executable(Test_TxBody Test_TxBody.cpp)
target_include_directories(Test_TxBody PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxBody PUBLIC AccountData Utils Persistence Message TestUtils)

add_executable(Test_Diagnostic Test_Diagnostic.cpp)
target_include_directories(Test_Diagnostic PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Diagnostic PUBLIC AccountData Utils Persistence Message Boost::unit_test_framework TestUtils)

#FIXME: built but not enabled
add_executable(ReadBlock ReadBlock.cpp)
target_include_directories(ReadBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ReadBlock PUBLIC AccountData Utils Persistence)

#add_executable(ReadTxBlock ReadTxBlock.cpp)
#target_include_directories(ReadTxBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(ReadTxBlock PUBLIC Crypto AccountData Utils Persistence)

#add_executable(ReadTransactions ReadTransactions.cpp)
#target_include_directories(ReadTransactions PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(ReadTransactions PUBLIC Crypto AccountData Utils Persistence)

add_executable(Test_ExtSeedPubKeys Test_ExtSeedPubKeys.cpp)
target_include_directories(Test_ExtSeedPubKeys PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_ExtSeedPubKeys PUBLIC Utils Persistence TestUtils)

add_executable(Test_ContractStorage Test_ContractStorage.cpp)
target_include_directories(Test_ContractStorage PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_ContractStorage PUBLIC AccountData Utils Persistence Message TestUtils)

set(TESTCASES_ENABLED Test_MetaPersistence Test_TrieDB Test_DSPersistence Test_TxPersistence Test_TxBody Test_Diagnostic Test_ExtSeedPubKeys)

foreach(testcase ${TESTCASES_ENABLED})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${testcase}_run)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${CMAKE_CURRENT_BINARY_DIR}/${testcase}_run/constants.xml)
    add_test(NAME ${testcase} COMMAND ${testcase} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${testcase}_run)
endforeach(testcase)
