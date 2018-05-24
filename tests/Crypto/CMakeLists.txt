
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

link_directories(${CMAKE_BINARY_DIR}/lib)

IF (Boost_FOUND)
    message(STATUS "boost header: ${Boost_INCLUDE_DIRS}")
    message(STATUS "boost libs  : ${Boost_LIBRARIES}")

    include_directories( ${Boost_INCLUDE_DIR} )
    include_directories(${PROJECT_SOURCE_DIR}/src)
    include_directories(${CMAKE_SOURCE_DIR}/depends/libethash/)


    link_directories(${Boost_LIBRARY_DIRS})
    file(GLOB HEADERS "*.h")
    if ((NOT MSVC) AND (NOT APPLE))
        ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)
    endif()


    if (NOT MSVC)
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 ")
    endif()

    if (NOT APPLE)
        add_executable (Test_Sha3 Test_Sha3.cpp)
        add_executable (Test_Sha2 Test_Sha2.cpp)
    endif()

    add_executable (Test_Schnorr Test_Schnorr.cpp)
    add_executable (Test_MultiSig Test_MultiSig.cpp)
    add_executable (GetAddressFromPubKey GetAddressFromPubKey.cpp)
    add_executable(GetPubKeyFromPrivKey GetPubKeyFromPrivKey.cpp)

    target_link_libraries(GetAddressFromPubKey Crypto)
    target_link_libraries(GetPubKeyFromPrivKey Crypto)

    if (NOT APPLE)
        target_link_libraries(Test_Sha3 Utils)
        target_link_libraries(Test_Sha3 Crypto)
        target_link_libraries(Test_Sha3 ${Boost_FILESYSTEM_LIBRARIES})
        target_link_libraries(Test_Sha3 ${Boost_SYSTEM_LIBRARIES})
        target_link_libraries(Test_Sha3 ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})

        target_link_libraries(Test_Sha2 Utils)
        target_link_libraries(Test_Sha2 Crypto)
        target_link_libraries(Test_Sha2 ${Boost_FILESYSTEM_LIBRARIES})
        target_link_libraries(Test_Sha2 ${Boost_SYSTEM_LIBRARIES})
        target_link_libraries(Test_Sha2 ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
    endif()



    target_link_libraries(Test_Schnorr Crypto)

    target_link_libraries(Test_MultiSig Crypto)

    if(CMAKE_CONFIGURATION_TYPES)
        foreach(config ${CMAKE_CONFIGURATION_TYPES})
            configure_file(${CMAKE_SOURCE_DIR}/constants.xml ${config}/constants.xml COPYONLY)
        endforeach(config)
    else(CMAKE_CONFIGURATION_TYPES)
        configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)
    endif(CMAKE_CONFIGURATION_TYPES)

    enable_testing ()
    if (NOT APPLE)
        add_test(NAME Test_Sha3 COMMAND Test_Sha3)
        add_test(NAME Test_Sha2 COMMAND Test_Sha2)
    endif()
    add_test(NAME Test_Schnorr COMMAND Test_Schnorr)
    add_test(NAME Test_MultiSig COMMAND Test_MultiSig)
ENDIF()
