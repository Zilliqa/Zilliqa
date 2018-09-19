add_library (Common SHARED Common.cpp CommonData.cpp CommonIO.cpp FileSystem.cpp FixedHash.cpp RLP.cpp SHA3.cpp Miner.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories (Common PUBLIC ${PROJECT_SOURCE_DIR}/src  ${G3LOG_INCLUDE_DIRS})
target_link_libraries (Common PUBLIC ${Boost_LIBRARIES})
