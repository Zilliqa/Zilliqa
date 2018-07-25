link_directories(${CMAKE_BINARY_DIR}/lib)
add_executable (Test_POW test_POW.cpp)
target_link_libraries(Test_POW PUBLIC ethash POW Utils Crypto Boost::unit_test_framework Boost::filesystem)
add_test(NAME Test_POW COMMAND Test_POW)
