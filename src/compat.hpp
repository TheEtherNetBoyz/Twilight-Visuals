#pragma once

#include "dolphin/types.h"
#include "SSystem/SComponent/c_xyz.h"
#include "dusk/TwilightHostApi.h"
#include "visual_types.hpp"

namespace twilight_visuals::compat {

const DuskTwilightHostApiV1* host_api();

float get_master_volume();
bool is_prelaunch_open();
bool get_authored_sky(DuskTwilightSkyboxV1& outSky, u8 variant);
void set_enemy_proc_provider(DuskTwilightEnemyProcProviderV1 provider);
void set_bloom_provider(DuskTwilightBloomProviderV1 provider);
void set_scene_music_provider(DuskTwilightSceneMusicProviderV1 provider);

}  // namespace twilight_visuals::compat
