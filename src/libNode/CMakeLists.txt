add_library (Node DSBlockProcessing.cpp FinalBlockProcessing.cpp MicroBlockPreProcessing.cpp MicroBlockPostProcessing.cpp Node.cpp PoWProcessing.cpp ViewChangeBlockProcessing.cpp FallbackPreProcessing.cpp FallbackPostProcessing.cpp FallbackBlockProcessing.cpp)
target_include_directories (Node PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Node PUBLIC Mediator Message POW Trie Utils Constants Lookup Server)
