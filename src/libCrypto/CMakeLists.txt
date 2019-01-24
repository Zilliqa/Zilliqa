add_library (Crypto Schnorr.cpp MultiSig.cpp PrivKey.cpp PubKey.cpp Signature.cpp BIGNUMSerialize.cpp ECPOINTSerialize.cpp)

if("${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}" VERSION_LESS "1.1")
	target_sources (Crypto PRIVATE generate_dsa_nonce.c)
endif()

target_include_directories (Crypto PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Crypto Utils OpenSSL::Crypto dl Threads::Threads)
