# g3log is a KjellKod Logger
# 2015 @author Kjell Hedström, hedstrom@kjellkod.cc
# ==================================================================
# 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own
#    risk and comes  with no warranties.
#
# This code is yours to share, use and modify with no strings attached
#   and no restrictions or obligations.
# ===================================================================



SET(LOG_SRC ${g3log_SOURCE_DIR}/src)
include_directories(${LOG_SRC})
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
SET(ACTIVE_CPP0xx_DIR "Release")

#cmake -DCMAKE_CXX_COMPILER=clang++ ..
  # WARNING: If Clang for Linux does not work with full c++14 support it might be your
  # installation that is faulty. When I tested Clang on Ubuntu I followed the following
  # description
  #  1) http://kjellkod.wordpress.com/2013/09/23/experimental-g3log-with-clang/
  #  2) https://github.com/maidsafe/MaidSafe/wiki/Hacking-with-Clang-llvm-abi-and-llvm-libc
IF (${CMAKE_CXX_COMPILER_ID} MATCHES ".*Clang")
   message( STATUS "" )
   message( STATUS "cmake for Clang " )
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -Wunused -D_GLIBCXX_USE_NANOSLEEP")
   IF (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
       SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++ -pthread")
   ELSE()
       SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   ENDIF()
   IF (${CMAKE_SYSTEM} MATCHES "FreeBSD-([0-9]*)\\.(.*)")
       IF (${CMAKE_MATCH_1} GREATER 9)
           set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
           set(PLATFORM_LINK_LIBRIES execinfo)
       ENDIF()
   ELSEIF (APPLE)
       set(PLATFORM_LINK_LIBRIES c++abi)
   ELSEIF (NOT (${CMAKE_SYSTEM_NAME} STREQUAL "Linux"))
       set(PLATFORM_LINK_LIBRIES rt c++abi)
   ELSE()
       set(PLATFORM_LINK_LIBRIES rt)
   ENDIF()



ELSEIF(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
   message( STATUS "cmake for GCC " )
   IF (APPLE)
       set(CMAKE_CXX_FLAGS   "${CMAKE_CXX_FLAGS} -Wall -Wunused -std=c++14  -pthread -D_GLIBCXX_USE_NANOSLEEP")
   ELSEIF (MINGW)
       set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wunused -std=c++14  -pthread -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD")
       set(PLATFORM_LINK_LIBRIES dbghelp)

       # deal with ERROR level conflicts with windows.h
       ADD_DEFINITIONS (-DNOGDI)
   ELSE()
       set(PLATFORM_LINK_LIBRIES rt)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -rdynamic -Wunused -std=c++14 -pthread -lrt -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD")
   ENDIF()
ELSEIF(MSVC)
   set(PLATFORM_LINK_LIBRIES dbghelp)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") # source code already in utf-8, force it for compilers in non-utf8_windows_locale
   # ERROR level conflicts with windows.h
   ADD_DEFINITIONS (-DNOGDI)
   # support AMD proc on vc2015
   if(${CMAKE_CL_64} STREQUAL "0")
       set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /arch:IA32")
   endif()
endif()

IF (MSVC OR MINGW)
      # VC11 bug: http://code.google.com/p/googletest/issues/detail?id=408
      #          add_definition(-D_VARIADIC_MAX=10)
      # https://github.com/anhstudios/swganh/pull/186/files
      ADD_DEFINITIONS (/D_VARIADIC_MAX=10)
      MESSAGE(STATUS "- MSVC: Set variadic max to 10 for MSVC compatibility")
      # Remember to set set target properties if using GTEST similar to done below on target "unit_test"
      # "set_target_properties(unit_test  PROPERTIES COMPILE_DEFINITIONS "GTEST_USE_OWN_TR1_TUPLE=0")
   message( STATUS "" )
   message( STATUS "Windows: Run cmake with the appropriate Visual Studio generator" )
   message( STATUS "The generator is one number below the official version number. I.e. VS2013 -> Generator 'Visual Studio 12'" )
   MESSAGE( STATUS "I.e. if VS2013: Please run the command [cmake -DCMAKE_BUILD_TYPE=Release -G \"Visual Studio 12\" ..]")
   message( STATUS "if cmake finishes OK, do 'msbuild g3log.sln /p:Configuration=Release'" )
   message( STATUS "then run 'Release\\g3log-FATAL-*' examples" )
   message( STATUS "" )
ENDIF()

   # GENERIC STEPS
   file(GLOB SRC_FILES ${LOG_SRC}/g3log/*.h ${LOG_SRC}/g3log/*.hpp ${LOG_SRC}/*.cpp ${LOG_SRC}/*.ipp)
   file(GLOB HEADER_FILES ${LOG_SRC}/g3log/*.hpp ${LOG_SRC}/*.hpp)
   
   list( APPEND HEADER_FILES ${GENERATED_G3_DEFINITIONS} )
   list( APPEND SRC_FILES ${GENERATED_G3_DEFINITIONS} )

   IF (MSVC OR MINGW)
      list(REMOVE_ITEM SRC_FILES  ${LOG_SRC}/crashhandler_unix.cpp)
   ELSE()
      list(REMOVE_ITEM SRC_FILES  ${LOG_SRC}/crashhandler_windows.cpp ${LOG_SRC}/g3log/stacktrace_windows.hpp ${LOG_SRC}/stacktrace_windows.cpp)
   ENDIF (MSVC OR MINGW)

   set(SRC_FILES ${SRC_FILES} ${SRC_PLATFORM_SPECIFIC})

   # Create the g3log library
   INCLUDE_DIRECTORIES(${LOG_SRC})
   SET(G3LOG_LIBRARY g3logger)

   IF( G3_SHARED_LIB )
      IF( WIN32 )
         IF(NOT(${CMAKE_VERSION} VERSION_LESS "3.4"))
            set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
         ELSE()
            message( FATAL_ERROR "Need CMake version >=3.4 to build shared windows library!" )
         ENDIF()
      ENDIF()
      ADD_LIBRARY(${G3LOG_LIBRARY} SHARED ${SRC_FILES})
   ELSE()
      ADD_LIBRARY(${G3LOG_LIBRARY} STATIC ${SRC_FILES})
   ENDIF()

   SET(${G3LOG_LIBRARY}_VERSION_STRING ${VERSION})
   MESSAGE( STATUS "Creating ${G3LOG_LIBRARY} VERSION: ${VERSION}" )
   SET_TARGET_PROPERTIES(g3logger PROPERTIES LINKER_LANGUAGE CXX SOVERSION ${VERSION})

   set_target_properties(${G3LOG_LIBRARY} PROPERTIES
      LINKER_LANGUAGE CXX
      OUTPUT_NAME g3logger
      CLEAN_DIRECT_OUTPUT 1)

   IF(APPLE)
      set_target_properties(${G3LOG_LIBRARY} PROPERTIES MACOSX_RPATH TRUE)
   ENDIF()

   TARGET_LINK_LIBRARIES(${G3LOG_LIBRARY} ${PLATFORM_LINK_LIBRIES})

   # Kjell: This is likely not necessary, except for Windows?
   TARGET_INCLUDE_DIRECTORIES(${G3LOG_LIBRARY} PUBLIC ${LOG_SRC})


