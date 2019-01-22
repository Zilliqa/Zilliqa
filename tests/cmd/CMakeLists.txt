link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(VerifyMultiSignature VerifyMultiSignature.cpp)
target_include_directories(VerifyMultiSignature PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries (VerifyMultiSignature OpenSSL::Crypto)