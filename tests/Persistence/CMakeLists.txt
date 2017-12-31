add_executable(Test_DSPersistence Test_DSPersistence.cpp)
target_include_directories(Test_DSPersistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_DSPersistence LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)

add_executable(Test_TxPersistence Test_TxPersistence.cpp)
target_include_directories(Test_TxPersistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TxPersistence LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)

add_executable(Test_TxBody Test_TxBody.cpp)
target_include_directories(Test_TxBody PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TxBody LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)

add_executable(ReadBlock ReadBlock.cpp)
target_include_directories(ReadBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ReadBlock LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)

#add_executable(ReadTxBlock ReadTxBlock.cpp)
#target_include_directories(ReadTxBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(ReadTxBlock LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)

#add_executable(ReadTransactions ReadTransactions.cpp)
#target_include_directories(ReadTransactions PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(ReadTransactions LINK_PUBLIC Crypto AccountData BlockChainData Utils Persistence)