configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_Account Test_Account.cpp)
target_include_directories(Test_Account PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Account PUBLIC AccountData Crypto Trie Utils Persistence)
add_test(NAME Test_Account COMMAND Test_Account)

add_executable(Test_AccountStore Test_AccountStore.cpp)
target_include_directories(Test_AccountStore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_AccountStore PUBLIC AccountData Trie Utils Crypto)
add_test(NAME Test_AccountStore COMMAND Test_AccountStore)

add_executable(Test_CircularArray Test_CircularArray.cpp)
target_include_directories(Test_CircularArray PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_CircularArray PUBLIC Utils)
add_test(NAME Test_CircularArray COMMAND Test_CircularArray)

add_executable(Test_Transaction Test_Transaction.cpp)
target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Transaction PUBLIC AccountData Utils Validator)
add_test(NAME Test_Transaction COMMAND Test_Transaction)

add_executable(Test_TransactionPerformance Test_TransactionPerformance.cpp)
target_include_directories(Test_TransactionPerformance PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TransactionPerformance PUBLIC AccountData Utils)
add_test(NAME Test_TransactionPerformance COMMAND Test_TransactionPerformance)

#add_executable(Test_Contract Test_Contract.cpp)
#target_include_directories(Test_Contract PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(Test_Contract PUBLIC AccountData Crypto Trie Utils Persistence)
#add_test(NAME Test_Contract COMMAND Test_Contract)

#add_executable(Test_ContractInvoke Test_ContractInvoke.cpp)
#target_include_directories(Test_ContractInvoke PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(Test_ContractInvoke PUBLIC AccountData Crypto Trie Utils Persistence)
#add_test(NAME Test_ContractInvoke COMMAND Test_ContractInvoke)

#add_executable(Test_Block Test_Block.cpp)

#target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_include_directories(Test_Block PUBLIC ${CMAKE_SOURCE_DIR}/src)

#target_link_libraries(Test_Transaction PUBLIC Data Utils Crypto)
#target_link_libraries(Test_Block PUBLIC Data Utils Crypto)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out
