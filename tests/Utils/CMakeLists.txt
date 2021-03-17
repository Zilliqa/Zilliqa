if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

link_directories(${CMAKE_BINARY_DIR}/lib)


add_executable (Test_AddressConversion Test_AddressConversion.cpp)
target_include_directories (Test_AddressConversion PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_AddressConversion PUBLIC Utils)
add_test(NAME Test_AddressConversion COMMAND Test_AddressConversion)

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

add_executable (Test_DnsUtils Test_DnsUtils.cpp)
target_include_directories (Test_DnsUtils PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_DnsUtils PUBLIC AccountData Utils)
add_test(NAME Test_DnsUtils COMMAND Test_DnsUtils)

add_executable (Test_BoostBigNum Test_BoostBigNum.cpp)
target_include_directories (Test_BoostBigNum PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_BoostBigNum PUBLIC Utils)
add_test(NAME Test_BoostBigNum COMMAND Test_BoostBigNum)

add_executable (Test_Serializable Test_Serializable.cpp)
target_include_directories (Test_Serializable PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Serializable PUBLIC Utils)
add_test(NAME Test_Serializable COMMAND Test_Serializable)

add_executable(Test_RootComputation Test_RootComputation.cpp)
target_include_directories(Test_RootComputation PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_RootComputation LINK_PUBLIC Utils Common Database AccountData Message)
add_test(NAME Test_RootComputation COMMAND Test_RootComputation)

add_executable(Test_IPConverter Test_IPConverter.cpp)
target_include_directories(Test_IPConverter PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_IPConverter LINK_PUBLIC Utils)
add_test(NAME Test_IPConverter COMMAND Test_IPConverter)

add_executable(Test_SafeMath Test_SafeMath.cpp)
target_include_directories(Test_SafeMath PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_SafeMath PUBLIC Utils)
add_test(NAME Test_SafeMath COMMAND Test_SafeMath)

add_executable(Test_ShardSizeCalculator Test_ShardSizeCalculator.cpp)
target_include_directories(Test_ShardSizeCalculator PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries (Test_ShardSizeCalculator PUBLIC Utils TestUtils)
add_test(NAME Test_ShardSizeCalculator COMMAND Test_ShardSizeCalculator)

add_executable(Test_SWInfo Test_SWInfo.cpp)
target_include_directories(Test_SWInfo PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries (Test_SWInfo PUBLIC Utils TestUtils)
add_test(NAME Test_SWInfo COMMAND Test_SWInfo)

add_executable(Test_UpgradeManager Test_UpgradeManager.cpp)
target_include_directories(Test_UpgradeManager PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_UpgradeManager PUBLIC Utils curl AccountData Message)

# The network is unstable between Travis server & GitHub, thus disable Test_UpgradeManager to avoid potential Travis build failed.
#add_test(NAME Test_UpgradeManager COMMAND Test_UpgradeManager)

add_executable(Test_DataConversion Test_DataConversion.cpp)
target_include_directories(Test_DataConversion PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries (Test_DataConversion PUBLIC Utils)
add_test(NAME Test_DataConversion COMMAND Test_DataConversion)

add_executable(Test_SafeMath_Exhaustive Test_SafeMath_Exhaustive.cpp)
target_include_directories(Test_SafeMath_Exhaustive PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_SafeMath_Exhaustive PUBLIC Utils)
add_test(NAME Test_SafeMath_Exhaustive COMMAND Test_SafeMath_Exhaustive)
