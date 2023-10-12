find_package(unofficial-secp256k1 CONFIG REQUIRED)

add_library(EthCrypto EthCrypto.cpp)
target_include_directories(EthCrypto PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(EthCrypto PRIVATE Eth unofficial::secp256k1 unofficial::secp256k1_precomputed ${CryptoUtils_LIBRARY})
