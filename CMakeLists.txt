cmake_minimum_required(VERSION 3.23)

project(VarjoXRLayersSupport LANGUAGES CXX)

find_package(varjo-native-sdk CONFIG REQUIRED)
find_package(unity-plugin-api CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(${PROJECT_NAME} SHARED
    "src/plugin.h"
    "src/plugin.cxx"
    "src/log.h"
    "src/log.cxx"
    "src/hook.h"
    "src/hook.cxx"
    "src/varjo_api.h"
    "src/varjo_api.cxx"
    "src/unity_interface.h"
    "src/unity_interface.cxx"
    "src/graphics_backend.h"
    "src/dxgi_formats.h"
    "src/d3d11_backend.h"
    "src/d3d11_backend.cxx"
    "src/d3d12_backend.h"
    "src/d3d12_backend.cxx"
    "src/layer_types.h"
    "src/layer_stack.h"
    "src/layer_stack.cxx"
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::VarjoHeaders unofficial::unity-plugin-api)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)