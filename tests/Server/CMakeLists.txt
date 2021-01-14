link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_ScillaIPCServer Test_ScillaIPCServer.cpp)
target_include_directories(Test_ScillaIPCServer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_ScillaIPCServer PUBLIC  AccountData Message Server jsonrpc::client)
add_test(NAME Test_ScillaIPCServer COMMAND Test_ScillaIPCServer)

# To be tested with a live network
#add_executable(Test_DSBlockSer Test_DSBlockSer.cpp)
#target_include_directories(Test_DSBlockSer PUBLIC ${CMAKE_SOURCE_DIR}/src)
#target_link_libraries(Test_DSBlockSer PUBLIC Server)
#add_test(NAME Test_DSBlockSer COMMAND Test_DSBlockSer)