add_library(Updater STATIC
  DaemonListener.cpp)

target_link_libraries(Updater PUBLIC Utils)
