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

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.slang")

set(SHADER_OUTPUTS)
set(SHADER_RESOURCE_COMMANDS)
foreach(shader IN LISTS SHADER_SOURCES)
    get_filename_component(shader_name "${shader}" NAME_WE)
    set(output "${CMAKE_BINARY_DIR}/generated/${shader_name}.metal")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
        COMMAND "${SLANGC}" "${shader}"
                -target metal -entry vertexMain -entry fragmentMain -o "${output}"
        DEPENDS "${shader}"
        COMMENT "Compiling ${shader_name}.slang to Metal"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS "${output}")
    list(APPEND SHADER_RESOURCE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${output}"
                "$<TARGET_BUNDLE_DIR:Shaders>/Contents/Resources/shaders/${shader_name}.metal"
    )
endforeach()

add_custom_target(ShadersShaders DEPENDS ${SHADER_OUTPUTS})

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
    ${SHADER_RESOURCE_COMMANDS}
    DEPENDS ${SHADER_OUTPUTS}
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
