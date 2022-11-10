set(PATCHES
    # The following patch is used to allow simple sink filtering based on a predicate to get
    # around only having one logger. See ZIL-4972 for more details.
    # (License is completely free; see: https://github.com/KjellKod/g3sinks/blob/master/LICENSE)
    find-library-g3log.patch)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KjellKod/g3sinks
    REF f70307964f49144a4663630697a7f34ad649bd30 #v2.0.1
    SHA512 bcd15537d4bad22ea050b791e7f6f671488c3b0644697dc64b95018b3af687c7eae322d406a7a137014f4da11a46538ddcca79dd110a647193371d3116f51c06
    HEAD_REF master
    PATCHES ${PATCHES}
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" CHOICE_BUILD_DEBUG)

set(VERSION "2.0.1")

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DCHOICE_BUILD_EXAMPLES=OFF
        -DCHOICE_BUILD_TESTS=OFF
        -DCHOICE_SINK_LOGROTATE=ON
        -DCHOICE_BUILD_DEBUG=${CHOICE_BUILD_DEBUG}
)

vcpkg_install_cmake()

vcpkg_copy_pdbs()

vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/g3sinks)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

# Handle copyright
configure_file(${SOURCE_PATH}/LICENSE ${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright COPYONLY)

