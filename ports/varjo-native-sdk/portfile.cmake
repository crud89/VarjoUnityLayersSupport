vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_download_distfile(ARCHIVE
    URLS "https://downloads.varjo.com/software/plugins/${VERSION}/Varjo_SDK_for_Custom_Engines_${VERSION}.zip"
    FILENAME "varjo-native-sdk-${VERSION}.zip"
    SHA512 674816779ac0d560d1ac5af41759cd57f74ce58071206bba8816d8dac62d26ac14e6713eafeeaf0b6d6b36d5265dd15bd53c45e252bf3de9171df5be0a41e4a4
)

vcpkg_extract_source_archive(
    PACKAGE_PATH
    ARCHIVE ${ARCHIVE}
)

file(GLOB HEADER_FILES "${PACKAGE_PATH}/include/*.h")
file(INSTALL ${HEADER_FILES} DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(COPY "${PACKAGE_PATH}/bin/VarjoLib.dll" DESTINATION "${CURRENT_PACKAGES_DIR}/bin/")
file(COPY "${PACKAGE_PATH}/bin/VarjoLib.dll" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin/")
file(COPY "${PACKAGE_PATH}/lib/VarjoLib.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib/")
file(COPY "${PACKAGE_PATH}/lib/VarjoLib.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${PACKAGE_PATH}/LICENSE.txt")

configure_file("${CMAKE_CURRENT_LIST_DIR}/varjosdk-config.cmake.in" "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}-config.cmake" @ONLY)
