configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

link_directories(${CMAKE_BINARY_DIR}/lib)

#add_executable(Test_Account Test_Account.cpp)
#target_include_directories(Test_Account PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
#target_link_libraries(Test_Account PUBLIC AccountData Trie Utils Persistence TestUtils)
#add_test(NAME Test_Account COMMAND Test_Account)

add_executable(Test_AccountStore Test_AccountStore.cpp ../ScillaTestUtil.cpp)
target_include_directories(Test_AccountStore PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_AccountStore PUBLIC AccountData Trie Utils Message TestUtils)
add_test(NAME Test_AccountStore COMMAND Test_AccountStore)

add_executable(Test_TransactionReceipt Test_TransactionReceipt.cpp)
target_include_directories(Test_TransactionReceipt PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TransactionReceipt PUBLIC AccountData Trie Utils Persistence TestUtils)
add_test(NAME Test_TransactionReceipt COMMAND Test_TransactionReceipt)

add_executable(Test_Transaction Test_Transaction.cpp)
target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_Transaction PUBLIC Validator TestUtils)
add_test(NAME Test_Transaction COMMAND Test_Transaction)

add_executable(Test_TxnPool Test_TxnPool.cpp)
target_include_directories(Test_TxnPool PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_TxnPool PUBLIC AccountData Trie Utils Persistence TestUtils)
add_test(NAME Test_TxnPool COMMAND Test_TransactionReceipt)
