cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(ClickIndicator VERSION 1.0.0)

# ── Locate Geode SDK ──────────────────────────────────────────────────────────
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "GEODE_SDK environment variable is not set.\n"
        "Point it at the root of your Geode SDK checkout.\n"
        "  Windows: setx GEODE_SDK \"C:\\path\\to\\geode-sdk\"\n"
        "  Then restart your terminal/IDE."
    )
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# ── Mod target ────────────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} geode-sdk)
setup_geode_mod(${PROJECT_NAME})
