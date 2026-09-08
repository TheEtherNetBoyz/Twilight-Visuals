#pragma once

#include "mods/api.h"
#include "dolphin/types.h"

namespace twilight_visuals::music {
ModResult initialize();
void shutdown();
void set_volume(float value);
void prepare_scene();
bool is_boss_bgm(u32 id);
void sequence(bool scene, bool eligible, int mode, float gain, bool scope, bool battle,
              float battleVolume, bool boss, float bossVolume, u32 bossMain, u32 bossSub);
}
