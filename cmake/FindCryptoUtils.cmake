# - Try to find CryptoUtils
# Once done this will define
#
#  CryptoUtils_FOUND - system has CryptoUtils
#  CryptoUtils_INCLUDE_DIRS - the CryptoUtils include directory
#  CryptoUtils_LIBRARY - Link these to use CryptoUtils

find_path(
    CryptoUtils_INCLUDE_DIR
    NAMES Snark/Snark.h
    DOC "CryptoUtils include directory"
)

find_library(
    CryptoUtils_LIBRARY
    NAMES CryptoUtils
    DOC "CryptoUtils library"
)

set(CryptoUtils_INCLUDE_DIRS ${CryptoUtils_INCLUDE_DIR})
set(CryptoUtils_LIBRARIES ${CryptoUtils_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CryptoUtils DEFAULT_MSG CryptoUtils_INCLUDE_DIR CryptoUtils_LIBRARY)
mark_as_advanced(CryptoUtils_INCLUDE_DIR CryptoUtils_LIBRARY)


