set(MONGO_C_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/mongo-c-driver)
set(MONGO_C_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/mongo-c-driver)

set(MONGO_CXX_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/depends/mongo-cxx-driver)
set(MONGO_CXX_BINARY_DIR ${CMAKE_BINARY_DIR}/src/depends/mongo-cxx-driver)

# generate build directory
execute_process(COMMAND ${CMAKE_COMMAND}
        -H${MONGO_C_SOURCE_DIR}
        -B${MONGO_C_BINARY_DIR}
        -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
)

# build and install g3log
execute_process(COMMAND ${CMAKE_COMMAND} --build ${MONGO_C_BINARY_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} --build ${MONGO_C_BINARY_DIR} --target install)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/libmongoc-1.0")

# generate build directory
execute_process(COMMAND ${CMAKE_COMMAND}
        -H${MONGO_CXX_SOURCE_DIR}
        -B${MONGO_CXX_BINARY_DIR}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
)

# build and install g3log
execute_process(COMMAND ${CMAKE_COMMAND} --build ${MONGO_CXX_SOURCE_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} --build ${MONGO_CXX_BINARY_DIR} --target install)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/lib/cmake/libbsoncxx-3.3.1")
