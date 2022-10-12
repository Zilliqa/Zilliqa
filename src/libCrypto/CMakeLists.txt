add_library(EthCrypto EthCrypto.cpp)
target_include_directories(EthCrypto PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(EthCrypto Secp256k1 CryptoUtils)
