set(G3LOG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/g3log)
set(G3LOG_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/g3log)

# update submodule to the latest commit
execute_process(
    COMMAND git submodule update --init --recursive --remote src/depends/g3log
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# generate build directory
execute_process(COMMAND ${CMAKE_COMMAND}
        -H${G3LOG_SOURCE_DIR}
        -B${G3LOG_BINARY_DIR}
        -DUSE_DYNAMIC_LOGGING_LEVELS=ON
        -DG3_SHARED_LIB=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
        -DADD_FATAL_EXAMPLE=OFF
        -DENABLE_FATAL_SIGNALHANDLING=OFF
        -Wno-dev
)

# build and install g3log
execute_process(COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR} --target install)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/g3logger")
