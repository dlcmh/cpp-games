cmake_minimum_required(VERSION 3.25)

project(DeathStarLab VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_GAMES_WERROR "Treat compiler warnings as errors (used by the local check hook)" OFF)

if(NOT APPLE)
    message(FATAL_ERROR "Death Star Approach Lab currently targets macOS only.")
endif()

if(NOT DEFINED CPP_GAMES_DEPS_DIR)
    message(FATAL_ERROR "CPP_GAMES_DEPS_DIR is required. Run ./run.sh, which fetches the dependencies first.")
endif()

set(LAB_DEPS_DIR "${CPP_GAMES_DEPS_DIR}")
set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SDL3_FRAMEWORK "${LAB_DEPS_DIR}/sdl3/SDL3.framework")
set(METAL_CPP_DIR "${LAB_DEPS_DIR}/metal-cpp")
set(SLANGC "${LAB_DEPS_DIR}/slang/bin/slangc")

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
target_compile_options(SDL3 INTERFACE "-F${LAB_DEPS_DIR}/sdl3")
target_link_options(SDL3 INTERFACE "-F${LAB_DEPS_DIR}/sdl3")
target_link_libraries(SDL3 INTERFACE "-framework SDL3")

add_library(metal-cpp INTERFACE)
target_include_directories(metal-cpp SYSTEM INTERFACE "${METAL_CPP_DIR}")

find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Engine/Cockpit"
    "${CMAKE_BINARY_DIR}/engine-cockpit"
)

set(SCENE_METAL "${CMAKE_BINARY_DIR}/generated/scene.metal")
set(COCKPIT_METAL "${CMAKE_BINARY_DIR}/generated/cockpit.metal")
add_custom_command(
    OUTPUT "${SCENE_METAL}" "${COCKPIT_METAL}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}" "${GAME_DIR}/Shaders/scene.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${SCENE_METAL}"
    COMMAND "${SLANGC}" "${CPPGAMES_COCKPIT_SHADER}"
            -target metal -entry vertexMain -entry fragmentMain -o "${COCKPIT_METAL}"
    DEPENDS
        "${GAME_DIR}/Shaders/scene.slang"
        "${CPPGAMES_COCKPIT_SHADER}"
    COMMENT "Compiling Death Star lab shaders to Metal"
    VERBATIM
)
add_custom_target(DeathStarLabShaders DEPENDS "${SCENE_METAL}" "${COCKPIT_METAL}")

add_executable(DeathStarLab
    Sources/App/Main.cpp
    Sources/Platform/macOS/DeathStarLabRenderer.cpp
    Sources/Platform/macOS/MetalImpl.mm
    "${GAME_DIR}/Sources/Platform/macOS/MeshLibrary.cpp"
    "${GAME_DIR}/Sources/Platform/macOS/Pipelines.cpp"
)

target_include_directories(DeathStarLab PRIVATE
    Sources
    "${GAME_DIR}/Sources"
)
target_compile_options(DeathStarLab PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(DeathStarLab PRIVATE -Werror)
endif()
target_link_libraries(DeathStarLab PRIVATE
    SDL3
    metal-cpp
    cppgames_cockpit
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)

add_dependencies(DeathStarLab DeathStarLabShaders)

add_custom_target(DeathStarLabResources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:DeathStarLab>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SCENE_METAL}"
            "$<TARGET_BUNDLE_DIR:DeathStarLab>/Contents/Resources/shaders/scene.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COCKPIT_METAL}"
            "$<TARGET_BUNDLE_DIR:DeathStarLab>/Contents/Resources/shaders/cockpit.metal"
    DEPENDS "${SCENE_METAL}" "${COCKPIT_METAL}"
    COMMENT "Bundling Death Star lab shaders"
    VERBATIM
)
add_dependencies(DeathStarLab DeathStarLabResources)

set_target_properties(DeathStarLab PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Death Star Approach Lab"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.deathstarlab"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
)

add_custom_command(TARGET DeathStarLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:DeathStarLab>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:DeathStarLab>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling Death Star lab SDL3.framework"
    VERBATIM
)
