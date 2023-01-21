add_library(Blockchain STATIC
  DSBlock.cpp
  MicroBlock.cpp
  TxBlock.cpp
  VCBlock.cpp
  Serialization.cpp
  BlockHeaderBase.cpp
  DSBlockHeader.cpp
  MicroBlockHeader.cpp
  TxBlockHeader.cpp
  VCBlockHeader.cpp
  Serialization.cpp)
target_include_directories(Blockchain PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Blockchain PRIVATE ${Schnorr_LIBRARY} OpenSSL::Crypto MessageCommon Utils)
