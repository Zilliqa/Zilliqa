add_executable(Test_Account Test_Account.cpp)
target_include_directories(Test_Account PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Account LINK_PUBLIC AccountData Utils Crypto)

add_executable(Test_AccountStore Test_AccountStore.cpp)
target_include_directories(Test_AccountStore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_AccountStore LINK_PUBLIC AccountData Trie Utils Crypto)

add_executable(Test_CircularArray Test_CircularArray.cpp)
target_include_directories(Test_CircularArray PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_CircularArray LINK_PUBLIC Utils)

add_executable(Test_Transaction Test_Transaction.cpp)
target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Transaction LINK_PUBLIC AccountData Utils)

#add_executable(Test_Block Test_Block.cpp)

#target_include_directories(Test_Transaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_include_directories(Test_Block PUBLIC ${CMAKE_SOURCE_DIR}/src)

#target_link_libraries(Test_Transaction LINK_PUBLIC Data Utils Crypto)
#target_link_libraries(Test_Block LINK_PUBLIC Data Utils Crypto)

# To-do: Test_Transaction and Test_Block need to be updated after Predicate has been temporarily commented out