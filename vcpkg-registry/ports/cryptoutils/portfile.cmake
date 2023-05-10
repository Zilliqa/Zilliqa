set(GIT_REV "e9e580287326ca6b9d9e9ba60c560e9071ddde1d")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Zilliqa/cryptoutils
    REF ${GIT_REV}
    SHA512 1d897a0c19e321703fbd2c85a269cc29cbfdfe57f1194b4f9bd0bc87ee5f77091daaa59cbe94d4fc7df998e8ad03e54935c0170f4bcda2979f686c951845b8ea
    HEAD_REF master
)

# Unfortunately, vcpkg_from_github doesn't have support for submodules so
# we piggyback on its verifying the SHA512 and upon success, continue by
# cloning the same commit and recursively updating the submodules.
if(NOT EXISTS "${SOURCE_PATH}/.git")
  # We only get here if the above succeeded (meaning that the has was verified).
  find_program(GIT git)

  set(GIT_URL "https://github.com/Zilliqa/cryptoutils.git")
  set(SOURCE_PATH "${SOURCE_PATH}.git")
  file(REMOVE_RECURSE ${SOURCE_PATH})
  file(MAKE_DIRECTORY ${SOURCE_PATH})

	message(STATUS "Cloning and fetching submodules into ${SOURCE_PATH}")
  vcpkg_execute_required_process(
    COMMAND ${GIT} clone --recurse-submodules ${GIT_URL} ${SOURCE_PATH}
    WORKING_DIRECTORY ${SOURCE_PATH}
    LOGNAME cryptoutils
  )
#
  message(STATUS "Checkout revision ${GIT_REV}")
  vcpkg_execute_required_process(
    COMMAND ${GIT} checkout ${GIT_REV}
    WORKING_DIRECTORY ${SOURCE_PATH}
    LOGNAME cryptoutils
  )
endif()

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS
      # FIXME: currently, compile Release like RelWithDebInfo as this the only
      #        configuration that's been used & tested so far. A standard Release
      #        configuration fails on array boundaries warning (which turns into an
      #        error due to -Werror) and must be investigated separately.
      #        See ZIL-5019.
      -DCMAKE_CXX_FLAGS_RELEASE=\"-O2 -ggdb -DNDEBUG\"
)

vcpkg_cmake_install()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

