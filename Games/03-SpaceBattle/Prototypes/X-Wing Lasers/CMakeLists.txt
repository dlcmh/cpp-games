cmake_minimum_required(VERSION 3.25)

project(XWingLaserLab VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_GAMES_WERROR "Treat compiler warnings as errors (used by the local check hook)" OFF)

if(NOT APPLE)
    message(FATAL_ERROR "The X-Wing laser lab currently targets macOS only.")
endif()

if(NOT DEFINED CPP_GAMES_DEPS_DIR)
    message(FATAL_ERROR "CPP_GAMES_DEPS_DIR is required. Run ./run.sh, which fetches the dependencies first.")
endif()

set(LAB_DEPS_DIR "${CPP_GAMES_DEPS_DIR}")
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
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Engine/Lasers"
    "${CMAKE_BINARY_DIR}/engine-lasers"
)

set(BACKDROP_METAL "${CMAKE_BINARY_DIR}/generated/backdrop.metal")
set(COCKPIT_METAL "${CMAKE_BINARY_DIR}/generated/cockpit.metal")
set(LASER_METAL "${CMAKE_BINARY_DIR}/generated/laser.metal")
add_custom_command(
    OUTPUT "${BACKDROP_METAL}" "${COCKPIT_METAL}" "${LASER_METAL}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/backdrop.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${BACKDROP_METAL}"
    COMMAND "${SLANGC}" "${CPPGAMES_COCKPIT_SHADER}"
            -target metal -entry vertexMain -entry fragmentMain -o "${COCKPIT_METAL}"
    COMMAND "${SLANGC}" "${CPPGAMES_LASER_SHADER}"
            -target metal -entry vertexMain -entry fragmentMain -o "${LASER_METAL}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/backdrop.slang"
        "${CPPGAMES_COCKPIT_SHADER}"
        "${CPPGAMES_LASER_SHADER}"
    COMMENT "Compiling laser lab shaders to Metal"
    VERBATIM
)
add_custom_target(XWingLaserLabShaders DEPENDS
    "${BACKDROP_METAL}" "${COCKPIT_METAL}" "${LASER_METAL}")

add_executable(XWingLaserLab
    Sources/App/Main.cpp
    Sources/Demo/LaserDemo.cpp
    Sources/Platform/macOS/LaserLabRenderer.cpp
    Sources/Platform/macOS/MetalImpl.mm
)

target_include_directories(XWingLaserLab PRIVATE Sources)
target_compile_options(XWingLaserLab PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(XWingLaserLab PRIVATE -Werror)
endif()
target_link_libraries(XWingLaserLab PRIVATE
    SDL3
    metal-cpp
    cppgames_cockpit
    cppgames_lasers
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)

add_dependencies(XWingLaserLab XWingLaserLabShaders)

add_custom_target(XWingLaserLabResources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BACKDROP_METAL}"
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Resources/shaders/backdrop.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COCKPIT_METAL}"
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Resources/shaders/cockpit.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LASER_METAL}"
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Resources/shaders/laser.metal"
    DEPENDS "${BACKDROP_METAL}" "${COCKPIT_METAL}" "${LASER_METAL}"
    COMMENT "Bundling laser lab shaders"
    VERBATIM
)
add_dependencies(XWingLaserLab XWingLaserLabResources)

set_target_properties(XWingLaserLab PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "X-Wing Laser Lab"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.laserlab"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
    OUTPUT_NAME "XWingLaserLab"
)

add_custom_command(TARGET XWingLaserLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:XWingLaserLab>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling laser lab SDL3.framework"
    VERBATIM
)
