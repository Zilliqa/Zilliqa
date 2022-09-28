set(PY_VERSION "${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}")

find_package(Boost COMPONENTS python-py${PY_VERSION} QUIET)
find_package(Boost COMPONENTS python${PY_VERSION} QUIET)
find_package(Boost COMPONENTS python3-py${PY_VERSION} QUIET)

if(Boost_PYTHON-PY${PY_VERSION}_FOUND)
  set(PY_COMP_SUFFIX "python-py${PY_VERSION}")
elseif(Boost_PYTHON${PY_VERSION}_FOUND)
  set(PY_COMP_SUFFIX "python${PY_VERSION}")
elseif(Boost_PYTHON3-PY${PY_VERSION}_FOUND)
  set(PY_COMP_SUFFIX "python3-py${PY_VERSION}")
else()
  message(FATAL_ERROR "Could not find boost python")
endif()

message(STATUS "Python suffix: ${PY_COMP_SUFFIX}")
message(STATUS "Python Version: ${Python3_VERSION}")
find_package(Boost COMPONENTS ${PY_COMP_SUFFIX} REQUIRED)

add_library(PythonRunner PythonRunner.cpp)
target_include_directories(PythonRunner PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PythonRunner PRIVATE Python3::Python)
target_link_libraries(PythonRunner PUBLIC Boost::${PY_COMP_SUFFIX})
