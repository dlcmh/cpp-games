cmake_minimum_required(VERSION 3.25)

project(Barebones VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT APPLE)
    message(FATAL_ERROR "Barebones currently targets macOS only.")
endif()

if(NOT DEFINED CPP_GAMES_DEPS_DIR)
    message(FATAL_ERROR "CPP_GAMES_DEPS_DIR is required. Run ./run.sh, which fetches the dependencies first.")
endif()

set(BAREBONES_DEPS_DIR "${CPP_GAMES_DEPS_DIR}")
set(SDL3_FRAMEWORK "${BAREBONES_DEPS_DIR}/sdl3/SDL3.framework")
set(METAL_CPP_DIR "${BAREBONES_DEPS_DIR}/metal-cpp")
set(SLANGC "${BAREBONES_DEPS_DIR}/slang/bin/slangc")

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
target_compile_options(SDL3 INTERFACE "-F${BAREBONES_DEPS_DIR}/sdl3")
target_link_options(SDL3 INTERFACE "-F${BAREBONES_DEPS_DIR}/sdl3")
target_link_libraries(SDL3 INTERFACE "-framework SDL3")

add_library(metal-cpp INTERFACE)
target_include_directories(metal-cpp SYSTEM INTERFACE "${METAL_CPP_DIR}")

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

# ---------------------------------------------------------------------------
# Shaders: Slang is the single source of truth; it emits Metal Shading Language.
# macOS compiles the generated .metal source at runtime, so the app does not
# require a full Xcode installation just to invoke `metal`.
# ---------------------------------------------------------------------------

set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/generated/sprite.metal")
add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/sprite.slang"
            -target metal
            -entry vertexMain
            -entry fragmentMain
            -o "${SHADER_OUTPUT}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/sprite.slang"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/common.slang"
    COMMENT "Compiling Slang shaders to Metal"
    VERBATIM
)
add_custom_target(BarebonesShaders DEPENDS "${SHADER_OUTPUT}")

# ---------------------------------------------------------------------------
# Game executable + macOS app bundle
# ---------------------------------------------------------------------------

add_executable(Barebones
    Sources/App/Main.cpp
    Sources/Core/Game.cpp
    Sources/Core/GameLoop.cpp
    Sources/Platform/macOS/MetalImpl.mm
    Sources/Platform/macOS/MetalRenderer.cpp
)

target_include_directories(Barebones PRIVATE Sources)
target_compile_options(Barebones PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(Barebones PRIVATE
    SDL3
    metal-cpp
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)

add_dependencies(Barebones BarebonesShaders)

set_target_properties(Barebones PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Barebones"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.barebones"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
)

add_custom_command(TARGET Barebones POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:Barebones>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${SHADER_OUTPUT}"
            "$<TARGET_BUNDLE_DIR:Barebones>/Contents/Resources/shaders/sprite.metal"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:Barebones>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:Barebones>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling shaders and SDL3.framework"
    VERBATIM
)
