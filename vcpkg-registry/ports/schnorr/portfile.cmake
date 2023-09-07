vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Zilliqa/schnorr
    REF c54f4cadc88234d58bfdf83a4d7348444ea7845d #v8.2.0
    SHA512 7f445c407fd1049ab41ad580b91263698af405b6015ec4a33715a40e488459afb5f2a8f06e6db9759b18b4f9ba443e2da746408821931ccd31e4caf4101048a7
    HEAD_REF master
    PATCHES
      fix-compiler-flags.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS
      -DCMAKE_CXX_STANDARD=20
      -DCMAKE_CXX_FLAGS="-Wall -Werror -Wextra -Wno-dev -Wno-deprecated-declarations"
      -DCMAKE_C_FLAGS="-Wall -Werror -Wextra -Wno-dev -Wno-deprecated-declarations"
)

vcpkg_cmake_install()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

