add_library(cppgames_lasers STATIC
    Sources/LaserRenderer.cpp
)

target_include_directories(cppgames_lasers PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_options(cppgames_lasers PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(cppgames_lasers PRIVATE -Werror)
endif()

find_library(LASER_METAL_FRAMEWORK Metal REQUIRED)
find_library(LASER_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(LASER_FOUNDATION_FRAMEWORK Foundation REQUIRED)

target_link_libraries(cppgames_lasers PUBLIC
    metal-cpp
    "${LASER_METAL_FRAMEWORK}"
    "${LASER_QUARTZCORE_FRAMEWORK}"
    "${LASER_FOUNDATION_FRAMEWORK}"
)

set(CPPGAMES_LASER_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/laser.slang"
    PARENT_SCOPE
)
