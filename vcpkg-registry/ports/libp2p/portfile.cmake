vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO libp2p/cpp-libp2p
    REF 42982bf47161ce47606ce9d51a466d10990cd883
    SHA512 4750566e1a50a3dd6010475cb9619f94dd68e43721b24ff5cf486fcf028c3c4b1c1eb54b9b580552a4bb9a80384b176c509918ac5e2132b190801f50e7ef52d8
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
        -DEXPOSE_MOCKS=OFF
        -DMETRICS_ENABLED=OFF
        -DCMAKE_CXX_FLAGS=\"-Wno-deprecated-declarations\"
        -DCMAKE_C_FLAGS=\"-Wno-deprecated-declarations\"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE-APACHE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
file(INSTALL "${SOURCE_PATH}/LICENSE-MIT" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright-mit)
