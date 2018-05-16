if (MSVC)
    if (NOT BOOST_ROOT)
        set (BOOST_ROOT "$ENV{BOOST_ROOT}")
    endif()
    set (CMAKE_PREFIX_PATH BOOST_ROOT)
endif()

IF( NOT Boost_FOUND )
    # use multithreaded boost libraries, with -mt suffix
    set(Boost_USE_MULTITHREADED ON)

    if (MSVC)
        # TODO handle other msvc versions or it will fail find them
        set(Boost_COMPILER -vc120)
        # use static boost libraries *.lib
        set(Boost_USE_STATIC_LIBS ON)
    elseif (APPLE)

        # use static boost libraries *.a
        set(Boost_USE_STATIC_LIBS ON)

    elseif (UNIX)
        # use dynamic boost libraries .dll
        set(Boost_USE_STATIC_LIBS OFF)

    endif()
    find_package(Boost 1.48.0 COMPONENTS unit_test_framework system filesystem)
ENDIF()

IF (Boost_FOUND)
    message(STATUS "boost header: ${Boost_INCLUDE_DIRS}")
    message(STATUS "boost libs  : ${Boost_LIBRARIES}")

    include_directories( ${Boost_INCLUDE_DIR} )
    include_directories(${PROJECT_SOURCE_DIR}/src)
    include_directories(${PROJECT_SOURCE_DIR}/src/libPOW)
    include_directories(${CMAKE_SOURCE_DIR}/depends/libethash/)

    link_directories(${Boost_LIBRARY_DIRS})
    link_directories(${G3LOG_BINARY_DIR})
    file(GLOB HEADERS "*.h")

    if (NOT MSVC)
         set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 ")
    endif()

    add_executable (Test_POW "./test_POW.cpp" ${HEADERS})

    target_link_libraries(Test_POW ethash)
    target_link_libraries(Test_POW POW)
    target_link_libraries(Test_POW Utils)
    target_link_libraries(Test_POW Crypto)
    target_link_libraries(Test_POW ${Boost_FILESYSTEM_LIBRARIES})
    target_link_libraries(Test_POW ${Boost_SYSTEM_LIBRARIES})
    target_link_libraries(Test_POW ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})

    enable_testing ()
    add_test(NAME Test_POW COMMAND Test_POW)
ENDIF()
