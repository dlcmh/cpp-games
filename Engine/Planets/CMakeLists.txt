add_library(cppgames_planets STATIC
    Sources/PlanetGenerator.cpp
    Sources/PlanetRenderer.cpp
)

target_include_directories(cppgames_planets PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_options(cppgames_planets PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(cppgames_planets PRIVATE -Werror)
endif()

find_library(PLANET_METAL_FRAMEWORK Metal REQUIRED)
find_library(PLANET_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(PLANET_FOUNDATION_FRAMEWORK Foundation REQUIRED)

target_link_libraries(cppgames_planets PUBLIC
    metal-cpp
    "${PLANET_METAL_FRAMEWORK}"
    "${PLANET_QUARTZCORE_FRAMEWORK}"
    "${PLANET_FOUNDATION_FRAMEWORK}"
)

set(CPPGAMES_PLANET_SHADER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
    PARENT_SCOPE
)
