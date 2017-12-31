add_executable (Test_TimeLockedFunction Test_TimeLockedFunction.cpp)
target_include_directories (Test_TimeLockedFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_TimeLockedFunction LINK_PUBLIC Utils)

add_executable (Test_Logger1 Test_Logger1.cpp)
target_include_directories (Test_Logger1 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger1 LINK_PUBLIC Utils)

add_executable (Test_Logger2 Test_Logger2.cpp)
target_include_directories (Test_Logger2 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger2 LINK_PUBLIC Utils)

add_executable (Test_Logger3 Test_Logger3.cpp)
target_include_directories (Test_Logger3 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Logger3 LINK_PUBLIC Utils)

add_executable (Test_JoinableFunction Test_JoinableFunction.cpp)
target_include_directories (Test_JoinableFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_JoinableFunction LINK_PUBLIC Utils)

add_executable (Test_DetachedFunction Test_DetachedFunction.cpp)
target_include_directories (Test_DetachedFunction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_DetachedFunction LINK_PUBLIC Utils)

add_executable (Test_BoostBigNum Test_BoostBigNum.cpp)
target_include_directories (Test_BoostBigNum PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_BoostBigNum LINK_PUBLIC Utils)

add_executable (Test_Serializable Test_Serializable.cpp)
target_include_directories (Test_Serializable PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (Test_Serializable LINK_PUBLIC Utils)