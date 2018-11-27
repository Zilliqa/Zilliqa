file(GLOB SOURCES *.cpp)
add_library (RumorSpreading ${SOURCES})
target_include_directories (RumorSpreading PUBLIC ${PROJECT_SOURCE_DIR}/src ${G3LOG_INCLUDE_DIRS})
target_link_libraries (RumorSpreading PUBLIC Constants)
