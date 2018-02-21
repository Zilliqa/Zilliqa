include_directories(${OPENSSL_ROOT_DIR})
add_library (Crypto Sha3.cpp Schnorr.cpp MultiSig.cpp)
target_include_directories (Crypto PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Crypto Utils crypto)