# - Try to find g3sinks
# Once done this will define
#
#  g3sinks_FOUND - system has g3sinks
#  g3sinks_INCLUDE_DIRS - the g3sinks include directory
#  g3sinks_LOGROTATE_LIBRARY - Link these to use g3sinks' log rotation sink

find_path(
    g3sinks_INCLUDE_DIR
    NAMES g3sinks/LogRotate.h
    DOC "g3sinks include directory"
)

find_library(
    g3sinks_LOGROTATE_LIBRARY
    NAMES g3logrotate
    DOC "g3sinks library"
)

set(g3sinks_INCLUDE_DIRS ${g3sinks_INCLUDE_DIR})
set(g3sinks_LIBRARIES ${g3sinks_LOGROTATE_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(g3sinks DEFAULT_MSG g3sinks_INCLUDE_DIR g3sinks_LOGROTATE_LIBRARY)
mark_as_advanced(g3sinks_INCLUDE_DIR g3sinks_LOGROTATE_LIBRARY)
