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
        src/wall_run.cpp
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

# The version-independent Windows SDK stub predates the owned animation path and
# does not list J3DAnmBase's virtual destructor. The real Dusk executable exports
# it, so provide only the missing import at link time when no matching host import
# library was supplied. This does not define or replace the runtime symbol.
if (WIN32 AND MSVC AND NOT DUSK_GAME_EXE)
    set(_twilight_visuals_compat_lib
        "${CMAKE_CURRENT_BINARY_DIR}/twilight_visuals_dusklight_compat.lib")
    add_custom_command(
        OUTPUT "${_twilight_visuals_compat_lib}"
        COMMAND "${CMAKE_AR}" /nologo
                "/def:${CMAKE_CURRENT_SOURCE_DIR}/cmake/dusklight_compat.def"
                /machine:x64
                /name:dusklight.exe
                "/out:${_twilight_visuals_compat_lib}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dusklight_compat.def"
        VERBATIM
    )
    add_custom_target(twilight_visuals_dusklight_compat
        DEPENDS "${_twilight_visuals_compat_lib}")
    add_dependencies(twilight_visuals twilight_visuals_dusklight_compat)
    target_link_libraries(twilight_visuals PRIVATE "${_twilight_visuals_compat_lib}")
endif ()

# Keep live Visual Studio debugging sessions from locking the next mod link.
# The PDB is a build-only artifact and is not included in the .dusk package.
set_target_properties(twilight_visuals PROPERTIES PDB_NAME "twilight_visuals_1_11_6")
