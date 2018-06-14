add_library (Node DSBlockProcessing.cpp FinalBlockProcessing.cpp MicroBlockProcessing.cpp Node.cpp 
			 PoW1Processing.cpp PoW2Processing.cpp ShardingInfoProcessing.cpp ViewChangeBlockProcessing.cpp)
target_include_directories (Node PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Node PUBLIC Mediator POW Trie Utils)