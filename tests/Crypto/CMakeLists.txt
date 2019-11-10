link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

add_executable(Test_Sha2 Test_Sha2.cpp)
target_link_libraries(Test_Sha2 PUBLIC crypto Utils Boost::unit_test_framework)
add_test(NAME Test_Sha2 COMMAND Test_Sha2)

#add_executable(Test_Schnorr Test_Schnorr.cpp)
#target_link_libraries(Test_Schnorr PUBLIC Crypto)
#add_test(NAME Test_Schnorr COMMAND Test_Schnorr)

#add_executable(Test_MultiSig Test_MultiSig.cpp)
#target_link_libraries(Test_MultiSig PUBLIC Crypto)
#add_test(NAME Test_MultiSig COMMAND Test_MultiSig)
