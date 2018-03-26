# Additional targets to perform clang-format/clang-tidy
# It requires clang-format/clang-tidy 5.0.0+

set(BREW_LLVM5_PATH /usr/local/opt/llvm@5/bin)

# Get all project files
file(GLOB_RECURSE ALL_CXX_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.h
)

# Get vendored files
file(GLOB_RECURSE ALL_CXX_VENDOR_SOURCES
    ${CMAKE_SOURCE_DIR}/src/depends/*.cpp
    ${CMAKE_SOURCE_DIR}/src/depends/*.h
)

# Exclude third-party libraries in src/depends
# list(FILTER ALL_CXX_SOURCES EXCLUDE REGEX "^.*src/depends.*$") # CMake 3.5.2
list(REMOVE_ITEM ALL_CXX_SOURCES ${ALL_CXX_VENDOR_SOURCES}) # CMake 3.5.1

####################### clang-format ##############################

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/.clang-format)
    message(AUTHOR_WARNING ".clang-format is missing in the project root")
endif()

find_program(
    CLANG_FORMAT
    NAMES clang-format-5.0 clang-format
    PATHS ${BREW_LLVM5_PATH} # The brew version on MacOS
)

find_program(
    RUN_CLANG_FORMAT
    NAMES run-clang-format.py
    PATHS ${CMAKE_SOURCE_DIR}/scripts
)

if(CLANG_FORMAT AND RUN_CLANG_FORMAT)
    execute_process(
        COMMAND "${CLANG_FORMAT}" --version
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _CLANG_FORMAT_VERSION_RESULT
    )

    string(REGEX REPLACE "^.*version ([^ ]*) .*$" "\\1" CLANG_FORMAT_VERSION ${CLANG_FORMAT_VERSION_OUTPUT})

    if("${CLANG_FORMAT_VERSION}" VERSION_EQUAL "5.0.0" OR "${CLANG_FORMAT_VERSION}" VERSION_GREATER "5.0.0")
        # message(${CLANG_FORMAT_VERSION})
        add_custom_target(
            clang-format
            COMMAND ${RUN_CLANG_FORMAT}
            --clang-format-executable ${CLANG_FORMAT}
            ${ALL_CXX_SOURCES}
        )

        add_custom_target(
            clang-format-fix
            COMMAND ${CLANG_FORMAT}
            -i
            -style=file
            ${ALL_CXX_SOURCES}
        )
    else()
        message(AUTHOR_WARNING "clang-format version (${CLANG_FORMAT_VERSION}) not satisify (>5.0.0)")
    endif()
endif()

##################### clang-tidy #############################

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/.clang-tidy)
    message(AUTHOR_WARNING ".clang-tidy is missing in the project root")
endif()

find_program(
    CLANG_TIDY
    NAMES clang-tidy-5.0 clang-tidy
    PATHS ${BREW_LLVM5_PATH} # The brew version on MacOS
)

find_program(
    CLANG_APPLY_REPLACEMENTS
    NAMES clang-apply-replacements-5.0 clang-apply-replacements
    PATHS ${BREW_LLVM5_PATH} # The brew version on MacOS
)

find_program(
    RUN_CLANG_TIDY
    NAMES run-clang-tidy.py
    PATHS "${CMAKE_SOURCE_DIR}/scripts/"
)

# a workaround to exclude the third-party headers in 'src/depends'
# This is a known issue https://reviews.llvm.org/D34654
set(HEADER_DIR_REGEX "^${CMAKE_SOURCE_DIR}/\"(src/(common|lib)|tests)\"")

if(CLANG_TIDY)
    execute_process(
        COMMAND "${CLANG_TIDY}" --version
        OUTPUT_VARIABLE CLANG_TIDY_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _CLANG_TIDY_VERSION_RESULT
    )

    string(REGEX REPLACE "^.*version ([^ ]*).*$" "\\1" CLANG_TIDY_VERSION ${CLANG_TIDY_VERSION_OUTPUT})

    if("${CLANG_TIDY_VERSION}" VERSION_EQUAL "5.0.0" OR "${CLANG_TIDY_VERSION}" VERSION_GREATER "5.0.0")
        # message(${CLANG_TIDY_VERSION})
        add_custom_target(
            clang-tidy
            COMMAND "${RUN_CLANG_TIDY}"
            -clang-tidy-binary ${CLANG_TIDY}
            -config=''
            -header-filter ${HEADER_DIR_REGEX}
            -style='file'
            ${ALL_CXX_SOURCES}
        )
        if(CLANG_APPLY_REPLACEMENTS)
            add_custom_target(
                clang-tidy-fix
                COMMAND "${RUN_CLANG_TIDY}"
                -clang-tidy-binary ${CLANG_TIDY}
                -clang-apply-replacements-binary ${CLANG_APPLY_REPLACEMENTS}
                -fix
                -format
                -config=''
                -header-filter ${HEADER_DIR_REGEX}
                -style='file'
                ${ALL_CXX_SOURCES}
            )
        endif()
    else()
        message(AUTHOR_WARNING "clang-tidy version (${CLANG_TIDY_VERSION}) not satisify (>5.0.0)")
    endif()
endif()
