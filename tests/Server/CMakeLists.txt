link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_ScillaIPCServer Test_ScillaIPCServer.cpp)
target_include_directories(Test_ScillaIPCServer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_ScillaIPCServer PUBLIC Server Utils Persistence)

