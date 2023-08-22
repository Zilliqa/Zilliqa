vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO xDimon/soralog
    REF 761f1e9a226955a08c5cb33e827e5dc33cb04900
    SHA512 4b6c7b53e85cb391e851938a08889d5939c9666633e514e841d135279b68e3afae328e45945535d08dc287467b198d33c29e3688c894ac362e30a2faabd251dc
    HEAD_REF master
    PATCHES
      remove-hunter.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCMAKE_CXX_STANDARD=17
        -DTESTING=OFF
        -DEXAMPLES=OFF
        -DCLANG_FORMAT=OFF
        -DCLANG_TIDY=OFF
        -DCOVERAGE=OFF
        -DASAN=OFF
        -DLSAN=OFF
        -DMSAN=OFF
        -DTSAN=OFF
        -DUBSAN=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
