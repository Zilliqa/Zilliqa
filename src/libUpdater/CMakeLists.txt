add_library(Updater STATIC
  DaemonListener.cpp
  UpdatePipe.cpp)

target_link_libraries(Updater PUBLIC Utils)
