link_directories(${CMAKE_BINARY_DIR}/lib)

# lookup server tests
add_executable(Test_EvmFiltersAPI Test_EvmFiltersAPI.cpp)

target_include_directories(Test_EvmFiltersAPI
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        )

target_link_libraries(Test_EvmFiltersAPI
        PUBLIC
        Filters
        Boost::unit_test_framework
        )

add_test(NAME Test_EvmFiltersAPI COMMAND Test_EvmFiltersAPI)
