add_library (Common SHARED Common.cpp CommonData.cpp CommonIO.cpp FileSystem.cpp FixedHash.cpp RLP.cpp SHA3.cpp)
target_include_directories (Common PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Common LINK_PUBLIC ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY})
