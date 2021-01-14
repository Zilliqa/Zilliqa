configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_LogEntry Test_LogEntry.cpp)
target_include_directories(Test_LogEntry PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_LogEntry PUBLIC AccountData Utils TestUtils)
add_test(NAME Test_LogEntry COMMAND Test_LogEntry)

add_executable(Test_CircularArray Test_CircularArray.cpp)
target_include_directories(Test_CircularArray PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_CircularArray PUBLIC Utils)
add_test(NAME Test_CircularArray COMMAND Test_CircularArray)

add_executable(Test_TransactionPerformance Test_TransactionPerformance.cpp)
target_include_directories(Test_TransactionPerformance PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TransactionPerformance PUBLIC AccountData Utils Message)
add_test(NAME Test_TransactionPerformance COMMAND Test_TransactionPerformance)

add_executable(Test_TxnOrder Test_TxnOrder.cpp)
target_include_directories(Test_TxnOrder PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TxnOrder PUBLIC AccountData Utils Message)
add_test(NAME Test_TxnOrder COMMAND Test_TxnOrder)

#add_executable(Test_Get_Txn Test_Get_Txn.cpp)
#target_include_directories(Test_Get_Txn PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(Test_Get_Txn PUBLIC AccountData Utils Message)
#add_test(NAME Test_Get_Txn COMMAND Test_Get_Txn)

add_executable(Test_Contract Test_Contract.cpp ScillaTestUtil.cpp)
target_include_directories(Test_Contract PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Contract PUBLIC AccountData Trie Utils Persistence Message)
add_test(NAME Test_Contract COMMAND Test_Contract)

add_executable(Test_EIP Test_EIP.cpp)
target_include_directories(Test_EIP PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_EIP PUBLIC AccountData Trie Utils Persistence)
add_test(NAME Test_EIP COMMAND Test_EIP)

add_executable(Test_DSPowSolution Test_DSPowSolution.cpp)
target_include_directories(Test_DSPowSolution PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_DSPowSolution PUBLIC TestUtils MiningData AccountData Message)
add_test(NAME Test_DSPowSolution COMMAND Test_DSPowSolution)

#add_executable(Test_ContractInvoke Test_ContractInvoke.cpp)
#target_include_directories(Test_ContractInvoke PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(Test_ContractInvoke PUBLIC AccountData Crypto Trie Utils Persistence)
#add_test(NAME Test_ContractInvoke COMMAND Test_ContractInvoke)

#add_executable(Test_Block Test_Block.cpp)

#target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_include_directories(Test_Block PUBLIC ${CMAKE_SOURCE_DIR}/src)

#target_link_libraries(Test_Transaction PUBLIC Data Utils Crypto)
#target_link_libraries(Test_Block PUBLIC Data Utils Crypto)

add_subdirectory (BlockChainData)
add_subdirectory (AccountData)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out
