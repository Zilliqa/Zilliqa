link_directories(${CMAKE_BINARY_DIR}/lib)
configure_file(${CMAKE_SOURCE_DIR}/constants.xml constants.xml COPYONLY)

# add_executable(Test_Sha2 Test_Sha2.cpp)
# target_link_libraries(Test_Sha2 PUBLIC OpenSSL::Crypto Utils Boost::unit_test_framework)
# add_test(NAME Test_Sha2 COMMAND Test_Sha2)

add_executable(Test_HashCalculator Test_HashCalculator.cpp)
target_include_directories(Test_HashCalculator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_HashCalculator PRIVATE Utils OpenSSL::Crypto Boost::unit_test_framework)
add_test(NAME Test_HashCalculator COMMAND Test_HashCalculator)

add_executable(Test_EthCrypto Test_EthCrypto.cpp)
target_link_libraries(Test_EthCrypto PUBLIC Utils Boost::unit_test_framework)
target_link_libraries(Test_EthCrypto PUBLIC EthCrypto Eth OpenSSL::Crypto Common jsonrpc)
add_test(NAME Test_EthCrypto COMMAND Test_EthCrypto)

#add_executable(Test_Schnorr Test_Schnorr.cpp)
#target_link_libraries(Test_Schnorr PUBLIC Crypto)
#add_test(NAME Test_Schnorr COMMAND Test_Schnorr)

#add_executable(Test_MultiSig Test_MultiSig.cpp)
#target_link_libraries(Test_MultiSig PUBLIC Crypto)
#add_test(NAME Test_MultiSig COMMAND Test_MultiSig)

