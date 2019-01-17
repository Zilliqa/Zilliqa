file(GLOB SOURCES *.cpp)
add_library (RumorSpreading ${SOURCES})
target_include_directories (RumorSpreading PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries (RumorSpreading PUBLIC Constants)
