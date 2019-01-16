add_library (Mediator Mediator.cpp)
target_include_directories (Mediator PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (Mediator PUBLIC DirectoryService Node BlockChainData)
