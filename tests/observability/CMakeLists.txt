#message(STATUS "Building Masks for metrics")
#find_package(CURL)
#find_package(opentelemetry-cpp REQUIRED)

# A workaround for incorrect link libraries in vcpkg's opentelemtry-cpp-target.cmake
# which includes non-existent target (i.e. opentelemetry-cpp::pull & opentelemetry-cpp::core).
#set_target_properties(opentelemetry-cpp::opentelemetry_exporter_prometheus PROPERTIES
#        INTERFACE_LINK_LIBRARIES "opentelemetry-cpp::metrics"
#        )

link_directories(${CMAKE_BINARY_DIR}/bin)

add_executable(mask masks.cpp)

target_include_directories(mask
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        )

target_link_libraries(mask
        PUBLIC
        Constants
        Utils)


