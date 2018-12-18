add_library (Crypto Schnorr.cpp MultiSig.cpp)
target_include_directories (Crypto PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Crypto Utils OpenSSL::Crypto dl Threads::Threads)
