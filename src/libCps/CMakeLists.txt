add_library(Cps CpsExecutor.cpp CpsExecuteValidator.cpp CpsRunEvm.cpp CpsRunScilla.cpp CpsRunTransfer.cpp CpsUtils.cpp
            ScillaHelpers.cpp ScillaHelpersCreate.cpp ScillaHelpersCall.cpp)

target_include_directories(Cps PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Cps PRIVATE Eth)
