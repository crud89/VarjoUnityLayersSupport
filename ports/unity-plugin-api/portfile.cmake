vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Unity-Technologies/NativeRenderingPlugin
    REF 522254181faf188efa8b50c3e3bf6fce720b26e4
    SHA512 225dc4b34944863fc96264c104fdb659d1e3bcf534a2f3e20b339418e33c8fc3686cd6aceda6652434fa420263600fd9a8df1f5a0cba3fb1e9e6db2f9415ff1d
    HEAD_REF master
)

set(VCPKG_BUILD_TYPE release)

file(INSTALL "${SOURCE_PATH}/PluginSource/source/Unity/" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/unity-plugin-api-config.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
