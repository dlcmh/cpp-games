add_library(cppgames_cockpit STATIC
    Sources/CockpitOverlay.cpp
)

target_include_directories(cppgames_cockpit PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_options(cppgames_cockpit PRIVATE -Wall -Wextra -Wpedantic)
if(CPP_GAMES_WERROR)
    target_compile_options(cppgames_cockpit PRIVATE -Werror)
endif()

find_library(COCKPIT_METAL_FRAMEWORK Metal REQUIRED)
find_library(COCKPIT_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(COCKPIT_FOUNDATION_FRAMEWORK Foundation REQUIRED)

target_link_libraries(cppgames_cockpit PUBLIC
    metal-cpp
    "${COCKPIT_METAL_FRAMEWORK}"
    "${COCKPIT_QUARTZCORE_FRAMEWORK}"
    "${COCKPIT_FOUNDATION_FRAMEWORK}"
)

set(CPPGAMES_COCKPIT_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/cockpit.slang"
    PARENT_SCOPE
)
