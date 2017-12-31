add_library (Common SHARED Common.cpp CommonData.cpp CommonIO.cpp FileSystem.cpp FixedHash.cpp RLP.cpp SHA3.cpp)
target_include_directories (Common PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Common LINK_PUBLIC boost_filesystem boost_system)