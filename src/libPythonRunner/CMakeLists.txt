find_package(PythonLibs 3 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

add_library(PythonRunner PythonRunner.cpp)
target_include_directories(PythonRunner PUBLIC ${PROJECT_SOURCE_DIR}/src Boost ${PYTHON_INCLUDE_DIRS})
target_link_libraries(PythonRunner PUBLIC Boost::python-py35 ${PYTHON_LIBRARIES})
