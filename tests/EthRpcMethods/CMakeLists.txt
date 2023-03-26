link_directories(${CMAKE_BINARY_DIR}/lib)

configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

# lookup server tests
add_executable(Test_EthRpcMethods Test_EthRpcMethods.cpp)

target_include_directories(Test_EthRpcMethods
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        )

target_link_libraries(Test_EthRpcMethods
        PUBLIC
        Server
        Mediator
        Boost::unit_test_framework
        )

add_test(NAME Test_EthRpcMethods COMMAND Test_EthRpcMethods)
