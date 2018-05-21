#add_subdirectory (Consensus)
#add_subdirectory (Contracts)
add_subdirectory (Crypto)
add_subdirectory (Data)
add_subdirectory (depends)
#add_subdirectory (Incentives)
#add_subdirectory (libTestUtils)
add_subdirectory (Lookup)
add_subdirectory (Network)
add_subdirectory (Persistence)
add_subdirectory (POW)
add_subdirectory (Utils)
add_subdirectory (Zilliqa)

file(COPY ${CMAKE_SOURCE_DIR}/constants_local.xml DESTINATION ${CMAKE_BINARY_DIR})
#FIXME: constants.xml is not used for local_run, thus shouldn't be copied
# presently, it's a workaround to silence the error thrown by tests_zilliqa_local.py
file(COPY ${CMAKE_SOURCE_DIR}/constants.xml DESTINATION ${CMAKE_BINARY_DIR})
file(COPY Node DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
