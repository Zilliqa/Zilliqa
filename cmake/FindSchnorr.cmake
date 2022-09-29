# - Try to find Schnorr
# Once done this will define
#
#  Schnorr_FOUND - system has Schnorr
#  Schnorr_INCLUDE_DIRS - the Schnorr include directory
#  Schnorr_LIBRARY - Link these to use Schnorr

find_path(
    Schnorr_INCLUDE_DIR
    NAMES Schnorr.h
    DOC "Schnorr include directory"
)

find_library(
    Schnorr_LIBRARY
    NAMES Schnorr
    DOC "Schnorr library"
)

set(Schnorr_INCLUDE_DIRS ${Schnorr_INCLUDE_DIR})
set(Schnorr_LIBRARIES ${Schnorr_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Schnorr DEFAULT_MSG Schnorr_INCLUDE_DIR Schnorr_LIBRARY)
mark_as_advanced(Schnorr_INCLUDE_DIR Schnorr_LIBRARY)

