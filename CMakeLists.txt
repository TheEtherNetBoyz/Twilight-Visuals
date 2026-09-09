cmake_minimum_required(VERSION 3.25)
project(twilight_visuals CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DUSK_DIR "" CACHE PATH "Path to a compatible Dusk MFB source root")
    if (NOT EXISTS "${DUSK_DIR}/sdk/CMakeLists.txt")
        message(FATAL_ERROR "Set DUSK_DIR to a compatible Dusk MFB checkout containing sdk/CMakeLists.txt")
    endif ()
    option(DUSK_MOD_USE_FULL_TREE "Use full build instead of the minimal mod SDK" OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if (DUSK_MOD_USE_FULL_TREE)
        add_subdirectory("${DUSK_DIR}" dusk EXCLUDE_FROM_ALL)
    else ()
        add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
    endif ()
endif ()

add_mod(twilight_visuals
    FEATURES game fmt webgpu
    SOURCES
        src/mod.cpp
        src/compat.cpp
        src/sky.cpp
        src/boundary.cpp
        src/blood.cpp
        src/running.cpp
        src/run_trail.cpp
        src/sequencing.cpp
        src/geometry.cpp
        src/monochrome.cpp
        src/hooks.cpp
        src/music.cpp
        src/environment.cpp
        src/postprocess.cpp
        src/particles.cpp
        src/native_particles.cpp
        src/runtime.cpp
        src/settings.cpp
        src/cursor.cpp
    MOD_JSON mod.json
    RES_DIR res
    BUNDLE
)
