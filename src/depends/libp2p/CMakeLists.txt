file(GLOB sources "*.cpp" "*.h")


add_library(p2p ${sources})
target_link_libraries(p2p PUBLIC cryptopp-static)
target_include_directories(p2p SYSTEM PRIVATE ${CRYPTOPP_ROOT}/include)
