add_library (Node STATIC DSBlockProcessing.cpp FinalBlockProcessing.cpp MicroBlockPreProcessing.cpp MicroBlockPostProcessing.cpp Node.cpp PoWProcessing.cpp RootComputation.cpp ViewChangeBlockProcessing.cpp)
target_include_directories (Node PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Node PUBLIC Validator Message POW Trie Utils Constants Lookup Server)
