get_filename_component(_unity_root "${CMAKE_CURRENT_LIST_DIR}" PATH)
get_filename_component(_unity_root "${_unity_root}" PATH)

if(NOT TARGET unofficial::unity-plugin-api)
    add_library(unofficial::unity-plugin-api INTERFACE IMPORTED)
    set_target_properties(unofficial::unity-plugin-api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_unity_root}/include/unity-plugin-api")
endif()

unset(_unity_root)
