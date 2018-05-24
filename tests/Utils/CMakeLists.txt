if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable (Test_TimeLockedFunction Test_TimeLockedFunction.cpp)
target_include_directories (Test_TimeLockedFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_TimeLockedFunction PUBLIC Utils)
add_test(NAME Test_TimeLockedFunction COMMAND Test_TimeLockedFunction)

add_executable (Test_Logger1 Test_Logger1.cpp)
target_include_directories (Test_Logger1 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger1 PUBLIC Utils)
add_test(NAME Test_Logger1 COMMAND Test_Logger1)

add_executable (Test_Logger2 Test_Logger2.cpp)
target_include_directories (Test_Logger2 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger2 PUBLIC Utils)
add_test(NAME Test_Logger2 COMMAND Test_Logger2)

add_executable (Test_Logger3 Test_Logger3.cpp)
target_include_directories (Test_Logger3 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger3 PUBLIC Utils)
add_test(NAME Test_Logger3 COMMAND Test_Logger3)

add_executable (Test_JoinableFunction Test_JoinableFunction.cpp)
target_include_directories (Test_JoinableFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_JoinableFunction PUBLIC Utils)
add_test(NAME Test_JoinableFunction COMMAND Test_JoinableFunction)

add_executable (Test_DetachedFunction Test_DetachedFunction.cpp)
target_include_directories (Test_DetachedFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_DetachedFunction PUBLIC Utils)
add_test(NAME Test_DetachedFunction COMMAND Test_DetachedFunction)

add_executable (Test_BoostBigNum Test_BoostBigNum.cpp)
target_include_directories (Test_BoostBigNum PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_BoostBigNum PUBLIC Utils)
add_test(NAME Test_BoostBigNum COMMAND Test_BoostBigNum)

add_executable (Test_Serializable Test_Serializable.cpp)
target_include_directories (Test_Serializable PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Serializable PUBLIC Utils)
add_test(NAME Test_Serializable COMMAND Test_Serializable)

add_executable(Test_TxnRootComputation Test_TxnRootComputation.cpp)
target_include_directories(Test_TxnRootComputation PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_TxnRootComputation LINK_PUBLIC Utils Crypto Common Database AccountData)
add_test(NAME Test_TxnRootComputation COMMAND Test_TxnRootComputation)
