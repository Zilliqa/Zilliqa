# Additional targets to perform clang-format/clang-tidy
# It requires clang-format/clang-tidy 5.0.0+

# Get all project files
file(GLOB_RECURSE ALL_CXX_SOURCES *.cpp *.h)

# Exclude third-party libraries in src/depends
list(FILTER ALL_CXX_SOURCES EXCLUDE REGEX "^.*src/depends.*$")

# Adding clang-format target if executable is found
find_program(
    CLANG_FORMAT
    NAMES clang-format-5.0 clang-format
    PATHS /usr/local/opt/llvm@5/bin # The brew version on MacOS
)

if(CLANG_FORMAT)
    execute_process(
        COMMAND "${CLANG_FORMAT}" --version
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _CLANG_FORMAT_VERSION_RESULT
    )

    string(REGEX REPLACE "^.*version ([^ ]*) .*$" "\\1" CLANG_FORMAT_VERSION ${CLANG_FORMAT_VERSION_OUTPUT})

    if("${CLANG_FORMAT_VERSION}" VERSION_GREATER "5.0.0")
        # message(${CLANG_FORMAT_VERSION})
        add_custom_target(
            clang-format
            COMMAND ${CLANG_FORMAT}
            -i
            -sort-includes
            -style=file
            ${ALL_CXX_SOURCES}
        )
    else()
        message(AUTHOR_WARNING "clang-format version (${CLANG_FORMAT_VERSION}) not satisify (>5.0.0)")
    endif()
endif()

# Adding clang-tidy target if executable is found
find_program(
    CLANG_TIDY
    NAMES clang-tidy-5.0 clang-tidy
    PATHS /usr/local/opt/llvm@5/bin # The brew version on MacOS
)

find_program(
    RUN_CLANG_TIDY
    NAMES run-clang-tidy.py
    PATHS "${CMAKE_SOURCE_DIR}/scripts/"
)

if(CLANG_TIDY)
    execute_process(
        COMMAND "${CLANG_TIDY}" --version
        OUTPUT_VARIABLE CLANG_TIDY_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _CLANG_TIDY_VERSION_RESULT
    )

    string(REGEX REPLACE "^.*version ([^ ]*).*$" "\\1" CLANG_TIDY_VERSION ${CLANG_TIDY_VERSION_OUTPUT})

    if("${CLANG_TIDY_VERSION}" VERSION_GREATER "5.0.0")
        # message(${CLANG_TIDY_VERSION})
        add_custom_target(
            clang-tidy
            COMMAND "${RUN_CLANG_TIDY}"
            -config=''
            -format-style='file'
            ${ALL_CXX_SOURCES}
        )
    else()
        message(AUTHOR_WARNING "clang-tidy version (${CLANG_TIDY_VERSION}) not satisify (>5.0.0)")
    endif()
endif()
