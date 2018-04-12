# g3log is a KjellKod Logger
# 2015 @author Kjell Hedström, hedstrom@kjellkod.cc 
# ==================================================================
# 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own
#    risk and comes  with no warranties.
#
# This code is yours to share, use and modify with no strings attached
#   and no restrictions or obligations.
# ===================================================================


   # ============================================================================
   # TEST OPTIONS: Turn OFF the ones that is of no interest to you
   # ---- by default all is OFF: except 'g3log-FATAL-example -----
   # ---- the reason for this is that
   # ----- 1) the performance tests were only thoroughly tested on Ubuntu, not windows-
   #           (g3log windows/linux, but Google's glog only on linux)
   #
   #       2) The unit test were tested windows/linux,. but must be unzipped
   #          before it can be "cmake'd" and compiled --- leaving it as OFF for now
   # ============================================================================


   # Unit test for g3log  (cmake -DUSE_G3LOG_UNIT_TEST=ON ..)
   #    remember to unzip gtest at g3log/3rdParty/gtest
   option (ADD_G3LOG_UNIT_TEST "g3log unit tests" OFF)


   # 4. create the unit tests for g3log --- ONLY TESTED THE UNIT TEST ON LINUX
   # =========================
   IF (ADD_G3LOG_UNIT_TEST)
      set(DIR_UNIT_TEST ${g3log_SOURCE_DIR}/test_unit)
      message( STATUS "-DADD_G3LOG_UNIT_TEST=ON" )  
      set(GTEST_DIR ${g3log_SOURCE_DIR}/3rdParty/gtest/gtest-1.7.0)
      set(GTEST_INCLUDE_DIRECTORIES ${GTEST_DIR}/include ${GTEST_DIR} ${GTEST_DIR}/src)
      include_directories(${GTEST_INCLUDE_DIRECTORIES})
      add_library(gtest_170_lib ${GTEST_DIR}/src/gtest-all.cc)
      set_target_properties(gtest_170_lib  PROPERTIES COMPILE_DEFINITIONS "GTEST_HAS_RTTI=0")
      enable_testing(true)

     # obs see this: http://stackoverflow.com/questions/9589192/how-do-i-change-the-number-of-template-arguments-supported-by-msvcs-stdtupl
     # and this: http://stackoverflow.com/questions/2257464/google-test-and-visual-studio-2010-rc
  

     IF (MSVC OR MINGW)  
        SET(OS_SPECIFIC_TEST test_crashhandler_windows)
     ENDIF(MSVC OR MINGW)

      SET(tests_to_run test_message test_filechange test_io test_cpp_future_concepts test_concept_sink test_sink ${OS_SPECIFIC_TEST})
      SET(helper ${DIR_UNIT_TEST}/testing_helpers.h ${DIR_UNIT_TEST}/testing_helpers.cpp)
      include_directories(${DIR_UNIT_TEST})

      FOREACH(test ${tests_to_run} )
        SET(all_tests  ${all_tests} ${DIR_UNIT_TEST}/${test}.cpp )
         IF(${test} STREQUAL "test_filechange")
           add_executable(test_filechange ${DIR_UNIT_TEST}/${test}.cpp ${helper})
         ELSE()
           add_executable(${test} ${g3log_SOURCE_DIR}/test_main/test_main.cpp ${DIR_UNIT_TEST}/${test}.cpp ${helper})
         ENDIF(${test} STREQUAL "test_filechange")

        set_target_properties(${test} PROPERTIES COMPILE_DEFINITIONS "GTEST_HAS_TR1_TUPLE=0")
        set_target_properties(${test} PROPERTIES COMPILE_DEFINITIONS "GTEST_HAS_RTTI=0")
        IF( NOT(MSVC))
           set_target_properties(${test} PROPERTIES COMPILE_FLAGS "-isystem -pthread ")
        ENDIF( NOT(MSVC)) 
        target_link_libraries(${test} g3logger gtest_170_lib)
		add_test( ${test} ${test} )
      ENDFOREACH(test)
   
    #
    # Test for Linux, runtime loading of dynamic libraries
    #     
    IF (NOT WIN32 AND NOT ("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang") AND G3_SHARED_LIB)
       add_library(tester_sharedlib SHARED ${DIR_UNIT_TEST}/tester_sharedlib.h ${DIR_UNIT_TEST}/tester_sharedlib.cpp)
       target_link_libraries(tester_sharedlib ${G3LOG_LIBRARY})

       add_executable(test_dynamic_loaded_shared_lib ${g3log_SOURCE_DIR}/test_main/test_main.cpp ${DIR_UNIT_TEST}/test_linux_dynamic_loaded_sharedlib.cpp)
       set_target_properties(test_dynamic_loaded_shared_lib PROPERTIES COMPILE_DEFINITIONS "GTEST_HAS_TR1_TUPLE=0")
       set_target_properties(test_dynamic_loaded_shared_lib PROPERTIES COMPILE_DEFINITIONS "GTEST_HAS_RTTI=0")
       target_link_libraries(test_dynamic_loaded_shared_lib  ${G3LOG_LIBRARY} -ldl  gtest_170_lib )
    ENDIF()
ELSE() 
  message( STATUS "-DADD_G3LOG_UNIT_TEST=OFF" ) 
ENDIF (ADD_G3LOG_UNIT_TEST)
