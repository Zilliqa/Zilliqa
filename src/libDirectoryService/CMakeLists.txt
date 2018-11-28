add_library (DirectoryService DSBlockPostProcessing.cpp DSBlockPreProcessing.cpp DirectoryService.cpp FinalBlockPostProcessing.cpp FinalBlockPreProcessing.cpp MicroBlockProcessing.cpp PoWProcessing.cpp ViewChangePreProcessing.cpp ViewChangePostProcessing.cpp Coinbase.cpp GasPricer.cpp)
target_include_directories (DirectoryService PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (DirectoryService PUBLIC AccountData MiningData Mediator Message Node Persistence Trie Utils)
