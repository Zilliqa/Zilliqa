add_library (Common SHARED Common.cpp CommonData.cpp CommonIO.cpp FileSystem.cpp FixedHash.cpp RLP.cpp SHA3.cpp)
include_directories(${Boost_INCLUDE_DIRS})
target_include_directories (Common PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Common LINK_PUBLIC ${Boost_LIBRARIES})