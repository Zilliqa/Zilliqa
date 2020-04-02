include_directories(${PYTHON_INCLUDE_DIRS})

add_library(PythonRunner PythonRunner.cpp)
target_include_directories(PythonRunner PUBLIC ${PROJECT_SOURCE_DIR}/src Boost ${PYTHON_INCLUDE_DIRS})
target_link_libraries(PythonRunner PUBLIC Boost::${PY_COMP_SUFFIX} ${PYTHON_LIBRARIES})
