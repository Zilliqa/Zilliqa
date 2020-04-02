if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_UpdateDSComposition Test_UpdateDSComposition.cpp)
target_include_directories(Test_UpdateDSComposition PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_UpdateDSComposition LINK_PUBLIC Network Block DirectoryService)
add_test(NAME Test_UpdateDSComposition COMMAND Test_UpdateDSComposition)

add_executable(Test_DetermineByzantineNodes Test_DetermineByzantineNodes.cpp)
target_include_directories(Test_DetermineByzantineNodes PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_DetermineByzantineNodes LINK_PUBLIC Network Block DirectoryService)
add_test(NAME Test_DetermineByzantineNodes COMMAND Test_DetermineByzantineNodes)

add_executable(Test_SaveDSPerformance Test_SaveDSPerformance.cpp)
target_include_directories(Test_SaveDSPerformance PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_SaveDSPerformance LINK_PUBLIC Network Block DirectoryService)
add_test(NAME Test_SaveDSPerformance COMMAND Test_SaveDSPerformance)
