if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
    endforeach(config)
else(CMAKE_CONFIGURATION_TYPES)
    configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_Validator Test_Validator.cpp)
target_include_directories(Test_Validator PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Validator PUBLIC AccountData Crypto Trie Utils Mediator Validator)
add_test(NAME Test_Validator COMMAND Test_Validator)
