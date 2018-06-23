set(G3LOG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/g3log)
set(G3LOG_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/g3log)

if(NOT EXISTS "${G3LOG_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git submodule update --init --recursive src/depends/g3log
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

if (NOT EXISTS "${G3LOG_BINARY_DIR}")
    execute_process(COMMAND ${CMAKE_COMMAND}
            -H${G3LOG_SOURCE_DIR}
            -B${G3LOG_BINARY_DIR}
            -DUSE_DYNAMIC_LOGGING_LEVELS=ON
            -DG3_SHARED_LIB=OFF
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
            -DADD_FATAL_EXAMPLE=OFF
            -Wno-dev
    )
endif()

if ((NOT EXISTS "${CMAKE_BINARY_DIR}/lib/libg3logger.a") OR
    (NOT EXISTS "${CMAKE_BINARY_DIR}/lib/cmake/g3logger/g3loggerConfig.cmake") OR
    (NOT EXISTS "${CMAKE_BINARY_DIR}/include/g3log"))
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${G3LOG_BINARY_DIR} --target install)
endif()

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/g3logger")
