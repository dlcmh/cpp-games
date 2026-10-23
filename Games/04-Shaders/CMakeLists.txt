cmake_minimum_required(VERSION 3.25)

project(Shaders VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_GAMES_WERROR "Treat compiler warnings as errors (used by the local check hook)" OFF)

if(NOT APPLE)
    message(FATAL_ERROR "Shaders currently targets macOS only.")
endif()

if(NOT DEFINED CPP_GAMES_DEPS_DIR)
    message(FATAL_ERROR "CPP_GAMES_DEPS_DIR is required. Run ./run.sh, which fetches the dependencies first.")
endif()

set(SHADERS_DEPS_DIR "${CPP_GAMES_DEPS_DIR}")
set(SDL3_FRAMEWORK "${SHADERS_DEPS_DIR}/sdl3/SDL3.framework")
set(METAL_CPP_DIR "${SHADERS_DEPS_DIR}/metal-cpp")
set(SLANGC "${SHADERS_DEPS_DIR}/slang/bin/slangc")

foreach(required_path
        "${SDL3_FRAMEWORK}"
        "${METAL_CPP_DIR}"
        "${SLANGC}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency: ${required_path}. Run ./run.sh to fetch it.")
    endif()
endforeach()

add_library(SDL3 INTERFACE)
target_include_directories(SDL3 SYSTEM INTERFACE "${SDL3_FRAMEWORK}/Headers")
target_compile_options(SDL3 INTERFACE "-F${SHADERS_DEPS_DIR}/sdl3")
target_link_options(SDL3 INTERFACE "-F${SHADERS_DEPS_DIR}/sdl3")
target_link_libraries(SDL3 INTERFACE "-framework SDL3")

add_library(metal-cpp INTERFACE)
target_include_directories(metal-cpp SYSTEM INTERFACE "${METAL_CPP_DIR}")

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

set(SHADER1 "${CMAKE_BINARY_DIR}/generated/1.metal")
set(SHADER2 "${CMAKE_BINARY_DIR}/generated/2.metal")
add_custom_command(
    OUTPUT "${SHADER1}" "${SHADER2}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/1.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${SHADER1}"
    COMMAND "${SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/2.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${SHADER2}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/1.slang"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/2.slang"
    COMMENT "Compiling Slang shader examples to Metal"
    VERBATIM
)
add_custom_target(ShadersShaders DEPENDS "${SHADER1}" "${SHADER2}")

add_executable(Shaders
    Sources/App/Main.cpp
    Sources/Platform/macOS/MetalImpl.mm
)
target_compile_options(Shaders PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(Shaders PRIVATE -Werror)
endif()
target_link_libraries(Shaders PRIVATE
    SDL3
    metal-cpp
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)
add_dependencies(Shaders ShadersShaders)

add_custom_target(ShadersResources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER1}"
            "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Resources/shaders/1.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER2}"
            "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Resources/shaders/2.metal"
    DEPENDS "${SHADER1}" "${SHADER2}"
    COMMENT "Bundling shader examples"
    VERBATIM
)
add_dependencies(Shaders ShadersResources)

set_target_properties(Shaders PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Shaders"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.shaders"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
    OUTPUT_NAME "Shaders"
)

add_custom_command(TARGET Shaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling shader SDL3.framework"
    VERBATIM
)
