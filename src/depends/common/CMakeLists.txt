add_library (Common SHARED CommonData.cpp FixedHash.cpp RLP.cpp SHA3.cpp Miner.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories (Common PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Common PUBLIC Boost::system g3log)
