#pragma once

#include "dolphin/types.h"
#include "dusk/TwilightHostApi.h"
#include <cstdint>
#include "visual_types.hpp"

namespace twilight_visuals {

enum class Style : std::int64_t {
    Normal = 0,
    BlackAndWhite = 1,
    AstralPlane = 2,
    DarkHour = 3,
};

enum class Skybox : std::int64_t {
    TwilightDay = 0,
    TwilightNight,
    Sunrise,
    Sunset,
    Overcast,
    FaronTwilight,
    EldinTwilight,
    LanayruTwilight,
    PalaceOfTwilight,
    SacredGrove,
    Snowpeak,
    GerudoDesert,
    LakeHylia,
    FishingHole,
    Ordon,
    HyruleField,
    CastleTown,
};

enum class Weather : std::int64_t {
    Current = 0,
    Clear,
    Rain,
    Snow,
    Lightning,
    WindStorm,
    SnowStorm,
    HeavyFog,
};

struct RuntimeSettings {
    bool enabled{};
    Style style{Style::Normal};
    float brightness{1.0f};
    int chromaticAberration{80};
    Skybox skybox{Skybox::TwilightDay};
    Weather weather{Weather::Current};
    float musicVolume{1.0f};
    bool overrideTempleMusic{};
    bool skywardSwordRunning{};
    bool humanWolfSenses{};
    bool excludePalaceOfTwilight{true};
    bool hideGameplayCursor{};
};

const RuntimeSettings& runtime_settings();
void refresh_runtime_settings();
void provide_visual_state(u8* enabled, u8* style, f32* brightness,
                          s32* chromaticAberration, u8* skyVariant, u8* weather,
                          u8* alternateRun);
s16 provide_enemy_proc(s16 procName);
bool king_bulblin_encounter_active();
bool custom_music_allowed();
bool music_override_allowed();
bool palace_excluded();
s32 provide_environment_layer(s32 currentLayer);
u8 provide_bloom_profile(u8 defaultProfile);
bool provide_scene_music(const char* spot, s32 room, s32 layer, s32 sceneNo,
                         bool inDarkness, u8 demoWave, u32* bgmId, u8* bgmWave1,
                         u8* bgmWave2, bool* preserveStreams, bool* fieldBgmPlay,
                         s32* musicStatus);
void provide_audio_sequence(DuskTwilightAudioSequenceV1* state);
void provide_running(DuskTwilightRunningV1* state);
bool provide_grass(bool* monochrome);
void provide_render_policy(DuskTwilightRenderPolicyV1* state);
bool active();
void set_speedrun_suppressed(bool suppressed);

}  // namespace twilight_visuals
