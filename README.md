# Twilight Visuals
THIS MOD WAS MADE FOR FUN WITH AI, so yes its VIBECODED.

A standalone Dusklight MFB mod with visual styles, weather, particles, music integration, and movement options.

## Compatibility

Version 1.2.0 requires an MFB host exposing **Twilight hook ABI revision 4**. This is not a generic mod for every Dusklight version. Build against the matching [Dusk MFB source](https://github.com/TheEtherNetBoyz/dusk-mfb). No game executable, game assets, or music is distributed in this repository.

## Build (Windows)

Requirements: Visual Studio C++ tools, CMake 3.25 or newer, Ninja, and a separately checked-out compatible Dusk MFB SDK with its required dependencies. Use an x64 Native Tools command prompt. From this directory:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDUSK_DIR="C:/path/to/dusk-mfb"
cmake --build build --target twilight_visuals_package
```

Output: `build/mods/twilight_visuals.dusk`. Build the mod independently; do not copy its source into Dusk or modify Aurora. The SDK configuration may download dependencies and a link stub. Successful linking does not replace testing with a compatible host.

## Install

Exit the game. Back up your existing package and replace `twilight_visuals.dusk` in your selected user-data `mods` directory. The usual Windows location is `%APPDATA%/TwilitRealm/Dusklight/mods`. A user-data copy takes priority over an executable-adjacent `mods` copy, so keep installed copies synchronized. Restart the game after replacing it.

Music is optional external user-supplied content. Obtain permission for any music you use or distribute. No MP3 files are included here.

## Testing and known issues

The current source builds and packages independently. The latest disable-cleanup changes have **not** been confirmed in-game. White environmental squares after toggling the feature off remain an open reported issue; do not describe this release as fixing them conclusively.

Test enable/disable, full unload/reload, genuine Twilight areas, ordinary areas, area transitions, all styles, and native weather/particles. `tests/fade_test.cpp` and `tests/mp3_loop_test.cpp` are standalone test sources, not currently wired into CTest. The MP3 test requires user-supplied files.

`PORT_STATUS.md` contains historical development notes, not a guarantee that the current build passed every listed test.

## Licensing and provenance

Original contributions in this project are offered under CC0-1.0 to the extent their contributors hold the applicable rights; see `LICENSE.md`. Third-party material retains its own terms. The published-license audit found CC0-1.0 in the MFB/zeldaret upstream chain and permissive notices in the bundled MP3 decoder, with no specific conflicting license identified in the inspected mod source. See `THIRD_PARTY_NOTICES.md` for sources and scope. This is not a guarantee against third-party claims or a grant of rights to game assets or music.

This is an unofficial project and is not affiliated with Nintendo, Atlus, or the other upstream projects. No rights to their names, games, assets, or music are granted.

## Repository contents

Commit `src`, `tests`, `res/.gitkeep`, `CMakeLists.txt`, and the project documentation. Build outputs, packages, caches, local SDK checkouts, backups, music, and game files are excluded. Publish packages separately as release assets, retaining applicable third-party notices and checking any newly added dependencies or assets.
