if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

link_directories(${CMAKE_BINARY_DIR}/lib)
add_executable(Test_LookupNodeForDSBlock Test_LookupNodeForDSBlock.cpp)
target_include_directories(Test_LookupNodeForDSBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_LookupNodeForDSBlock PUBLIC AccountData Message Network TestUtils)
add_test(NAME Test_LookupNodeForDSBlock COMMAND Test_LookupNodeForDSBlock)

add_executable(Test_LookupNodeForTxBlock Test_LookupNodeForTxBlock.cpp)
target_include_directories(Test_LookupNodeForTxBlock PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_LookupNodeForTxBlock PUBLIC AccountData Message Network TestUtils)
add_test(NAME Test_LookupNodeForTxBlock COMMAND Test_LookupNodeForTxBlock)



add_executable(Test_txn_send Test_txn_send.cpp)
target_include_directories(Test_txn_send PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(Test_txn_send PUBLIC Node Mediator TestUtils)
add_test(NAME Test_txn_send COMMAND Test_txn_send)

if (${CMAKE_GENERATOR} STREQUAL "Xcode")
  add_custom_command(TARGET Test_txn_send POST_BUILD
    COMMAND sed -i '' 's,<LOOKUP_NODE_MODE>,<LOOKUP_NODE_MODE>true</LOOKUP_NODE_MODE>,' ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/constants.xml)
else()
  if (APPLE)
    add_custom_command(TARGET Test_txn_send POST_BUILD
      COMMAND sed -i '' 's,<LOOKUP_NODE_MODE>,<LOOKUP_NODE_MODE>true</LOOKUP_NODE_MODE>,' constants.xml)
  else()
    add_custom_command(TARGET Test_txn_send POST_BUILD
      COMMAND sed -i 's,<LOOKUP_NODE_MODE>,<LOOKUP_NODE_MODE>true</LOOKUP_NODE_MODE>,' constants.xml)
  endif()
endif()

add_custom_command(TARGET Test_txn_send POST_BUILD
        COMMAND echo "<nodes></nodes>" > config.xml VERBATIM)
