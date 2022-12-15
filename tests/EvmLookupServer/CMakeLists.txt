link_directories(${CMAKE_BINARY_DIR}/lib)
# For this test we have an adapted constants.xml
# copy from evm lookup server dir to binary dir
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

# lookup server tests
add_executable(Test_EvmLookupServer Test_EvmLookupServer.cpp)

target_include_directories(Test_EvmLookupServer
                            PUBLIC
                            ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(Test_EvmLookupServer
                      PUBLIC
                      AccountData
                      Message
                      Mediator
                      Server
                      jsonrpc
                      Boost::unit_test_framework
)

add_test(NAME Test_EvmLookupServer COMMAND Test_EvmLookupServer)
