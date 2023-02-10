if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO open-telemetry/opentelemetry-cpp
    REF v1.8.2
    SHA512 5d3efb7c31626acd9655b795da497ef3829a88f1e33532f26d011fda01ec41e08e88539900151224f5c19161d9bc1f76e502f14a358c4e01440d832432cd0c0b
    HEAD_REF main
    PATCHES
        mac-fix.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        etw WITH_ETW
        zipkin WITH_ZIPKIN
        prometheus WITH_PROMETHEUS
        elasticsearch WITH_ELASTICSEARCH
        jaeger WITH_JAEGER
        otlp WITH_OTLP
        otlp WITH_OTLP_HTTP
        zpages WITH_ZPAGES
)

# opentelemetry-proto is a third party submodule and opentelemetry-cpp release did not pack it.
if(WITH_OTLP)
    set(OTEL_PROTO_VERSION "0.19.0")
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/open-telemetry/opentelemetry-proto/archive/v${OTEL_PROTO_VERSION}.tar.gz"
        FILENAME "opentelemetry-proto-${OTEL_PROTO_VERSION}.tar.gz"
        SHA512 b6d47aaa90ff934eb24047757d5fdb8a5be62963a49b632460511155f09a725937fb7535cf34f738b81cc799600adbbc3809442aba584d760891c0a1f0ce8c03
    )

    vcpkg_extract_source_archive(src ARCHIVE "${ARCHIVE}")
    file(REMOVE_RECURSE "${SOURCE_PATH}/third_party/opentelemetry-proto")
    file(COPY "${src}/." DESTINATION "${SOURCE_PATH}/third_party/opentelemetry-proto")
    # Create empty .git directory to prevent opentelemetry from cloning it during build time
    file(MAKE_DIRECTORY "${SOURCE_PATH}/third_party/opentelemetry-proto/.git")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DWITH_EXAMPLES=OFF
        -DWITH_LOGS_PREVIEW=ON
        -DWITH_STL=ON
        -DWITH_OTLP_GRPC=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
