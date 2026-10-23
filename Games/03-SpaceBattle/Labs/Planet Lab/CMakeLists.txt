cmake_minimum_required(VERSION 3.25)

project(PlanetLab VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPP_GAMES_WERROR "Treat compiler warnings as errors (used by the local check hook)" OFF)

if(NOT APPLE)
    message(FATAL_ERROR "Planet Lab currently targets macOS only.")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Engine/Planets"
    "${CMAKE_BINARY_DIR}/engine-planets"
)

set(PLANET_METAL "${CMAKE_BINARY_DIR}/generated/planet.metal")
set(RINGS_METAL "${CMAKE_BINARY_DIR}/generated/rings.metal")
set(STARFIELD_METAL "${CMAKE_BINARY_DIR}/generated/starfield.metal")
add_custom_command(
    OUTPUT "${PLANET_METAL}" "${RINGS_METAL}" "${STARFIELD_METAL}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${SLANGC}" "${CPPGAMES_PLANET_SHADER_DIR}/planet.slang"
            -target metal -entry planetVertex -entry planetFragment -entry atmosphereFragment
            -o "${PLANET_METAL}"
    COMMAND "${SLANGC}" "${CPPGAMES_PLANET_SHADER_DIR}/rings.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${RINGS_METAL}"
    COMMAND "${SLANGC}" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/starfield.slang"
            -target metal -entry vertexMain -entry fragmentMain -o "${STARFIELD_METAL}"
    DEPENDS
        "${CPPGAMES_PLANET_SHADER_DIR}/planet.slang"
        "${CPPGAMES_PLANET_SHADER_DIR}/rings.slang"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/starfield.slang"
    COMMENT "Compiling Planet Lab shaders to Metal"
    VERBATIM
)
add_custom_target(PlanetLabShaders DEPENDS
    "${PLANET_METAL}" "${RINGS_METAL}" "${STARFIELD_METAL}")

add_executable(PlanetLab
    Sources/App/Main.cpp
    Sources/Platform/macOS/MetalImpl.mm
    Sources/Platform/macOS/PlanetLabRenderer.cpp
)

target_include_directories(PlanetLab PRIVATE Sources)
target_compile_options(PlanetLab PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(PlanetLab PRIVATE -Werror)
endif()
target_link_libraries(PlanetLab PRIVATE
    SDL3
    metal-cpp
    cppgames_planets
    "${METAL_FRAMEWORK}"
    "${QUARTZCORE_FRAMEWORK}"
    "${FOUNDATION_FRAMEWORK}"
)

add_dependencies(PlanetLab PlanetLabShaders)

add_custom_target(PlanetLabResources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLANET_METAL}"
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Resources/shaders/planet.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RINGS_METAL}"
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Resources/shaders/rings.metal"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STARFIELD_METAL}"
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Resources/shaders/starfield.metal"
    DEPENDS "${PLANET_METAL}" "${RINGS_METAL}" "${STARFIELD_METAL}"
    COMMENT "Bundling Planet Lab shaders"
    VERBATIM
)
add_dependencies(PlanetLab PlanetLabResources)

set_target_properties(PlanetLab PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Planet Lab"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.dlcmh.cppgames.planetlab"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_RPATH "@executable_path/../Frameworks"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
)

add_custom_command(TARGET PlanetLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Frameworks/SDL3.framework"
    COMMAND ditto
            "${SDL3_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:PlanetLab>/Contents/Frameworks/SDL3.framework"
    COMMENT "Bundling Planet Lab SDL3.framework"
    VERBATIM
)
