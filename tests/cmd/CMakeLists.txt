link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(verifyMultiSignature verifyMultiSignature.cpp)
target_include_directories(verifyMultiSignature PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries (verifyMultiSignature Boost::program_options OpenSSL::Crypto)