file(GLOB TEST_ZILLIQA_FILES  *.py
                              *.yml
                              *.conf
                              *.sh)

file(COPY ${TEST_ZILLIQA_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
