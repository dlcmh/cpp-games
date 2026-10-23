cmake_minimum_required(VERSION 3.25)

project(Rotatables VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_GAMES_WERROR "Treat compiler warnings as errors (used by the local check hook)" OFF)

if(NOT APPLE)
    message(FATAL_ERROR "Rotatables currently targets macOS only.")
endif()

if(NOT DEFINED CPP_GAMES_DEPS_DIR)
    message(FATAL_ERROR "CPP_GAMES_DEPS_DIR is required. Run ./run.sh, which fetches the dependencies first.")
endif()

set(ROTATABLES_DEPS_DIR "${CPP_GAMES_DEPS_DIR}")
set(SDL3_FRAMEWORK "${ROTATABLES_DEPS_DIR}/sdl3/SDL3.framework")
set(METAL_CPP_DIR "${ROTATABLES_DEPS_DIR}/metal-cpp")
set(SLANGC "${ROTATABLES_DEPS_DIR}/slang/bin/slangc")

foreach(required_path
        "${SDL3_FRAMEWORK}"
        "${METAL_CPP_DIR}"
        "${SLANGC}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency: ${required_path}. Run ./run.sh to fetch it.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

add_library(SDL3 INTERFACE)
target_include_directories(SDL3 SYSTEM INTERFACE "${SDL3_FRAMEWORK}/Headers")
target_compile_options(SDL3 INTERFACE "-F${ROTATABLES_DEPS_DIR}/sdl3")
target_link_options(SDL3 INTERFACE "-F${ROTATABLES_DEPS_DIR}/sdl3")
target_link_libraries(SDL3 INTERFACE "-framework SDL3")

add_library(metal-cpp INTERFACE)
target_include_directories(metal-cpp SYSTEM INTERFACE "${METAL_CPP_DIR}")

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

# ---------------------------------------------------------------------------
# Shaders: Slang is the single source of truth and emits Metal source. The app
# compiles that source at runtime, so no full Xcode toolchain is required.
# ---------------------------------------------------------------------------

set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/generated/scene.metal")
add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/scene.slang"
            -target metal
            -entry vertexMain
            -entry fragmentMain
            -o "${SHADER_OUTPUT}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/scene.slang"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/common.slang"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/skins.slang"
    COMMENT "Compiling Slang skin shaders to Metal"
    VERBATIM
)
add_custom_target(RotatablesShaders DEPENDS "${SHADER_OUTPUT}")

# ---------------------------------------------------------------------------
# Game executable + macOS app bundle
# ---------------------------------------------------------------------------

add_executable(Rotatables
    Sources/App/Main.cpp
    Sources/Core/Game.cpp
    Sources/Core/GameLoop.cpp
    Sources/Platform/macOS/MetalImpl.mm
    Sources/Platform/macOS/MetalRenderer.cpp
    Sources/Platform/macOS/ShaderPipeline.cpp
)

target_include_directories(Rotatables PRIVATE Sources)
target_compile_options(Rotatables PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(Rotatables PRIVATE -Werror)
endif()
target_link_libraries(Rotatables PRIVATE
    SDL3
    metal-cpp
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)

add_dependencies(Rotatables RotatablesShaders)

add_custom_target(RotatablesResources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:Rotatables>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER_OUTPUT}"
            "$<TARGET_BUNDLE_DIR:Rotatables>/Contents/Resources/shaders/scene.metal"
    DEPENDS "${SHADER_OUTPUT}"
    COMMENT "Bundling Rotatables shaders"
    VERBATIM
)
add_dependencies(Rotatables RotatablesResources)

set_target_properties(Rotatables PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Rotatables"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.rotatables"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
)

add_custom_command(TARGET Rotatables POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:Rotatables>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:Rotatables>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling skin shaders and SDL3.framework"
    VERBATIM
)
