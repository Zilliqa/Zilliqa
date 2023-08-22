vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO soramitsu/libp2p-sqlite-modern-cpp
    REF fc3b700064cb57ab6b598c9bc7a12b2842f78da2
    SHA512 a09c25f25fe4e84f302e222732f8b6a2aba977141f4da3fd6195793af70ffbf430cf469f3667e7b7de9f10bfb652f5a931a13a63b9a960857bbcab4fe7440a86
    HEAD_REF master
    PATCHES
      remove-hunter.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCMAKE_CXX_STANDARD=17
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/SQLiteModernCpp)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib" "${CURRENT_PACKAGES_DIR}/debug")
file(INSTALL "${SOURCE_PATH}/License.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
