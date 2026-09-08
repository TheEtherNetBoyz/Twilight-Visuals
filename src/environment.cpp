#include "environment.hpp"

#include "compat.hpp"
#include "runtime.hpp"

#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_wether.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_audio.h"
#include "SSystem/SComponent/c_math.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace twilight_visuals::environment {
namespace {
DEFINE_HOOK(&dScnKy_env_light_c::setLight, EnvironmentSetLight);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_bg, EnvironmentSetLightBg);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_actor, EnvironmentSetLightActor);

struct WeatherState {
    bool saved{};
    int rainCount{};
    int baseRainCount{};
    int snowCount{};
    u8 weather{};
    u8 weatherPat0{};
    u8 weatherPat1{};
    u8 prevGather{0xFF};
    u8 currGather{0xFF};
    float gatherRatio{-1.0f};
    u8 patMode{};
    u8 patModeGather{};
    float patternRatio{1.0f};
    float fogNear{};
    float fogFar{};
    float fogOverrideNear{};
    float fogOverrideFar{};
    float fogOverrideRatio{};
    u8 moyaMode{};
    int moyaCount{};
    u8 snowFogMode{};
    int thunderMode{};
    u8 thunderStatus{};
    cXyz* windOverride{};
    float customWindPower{};
    u8 teachWindExistence{};
};

WeatherState g_weatherState;
cXyz g_stormWind(1.0f, 0.0f, 0.0f);
cXyz g_windStormWind(1.75f, 0.0f, 0.0f);
int g_windGustTimer{};
bool g_windGustActive{};

bool environment_active() {
    const char* stage = dComIfGp_getStartStageName();
    return active() && stage != nullptr && !palace_excluded();
}

void restore_weather() {
    WeatherState& saved = g_weatherState;
    g_windGustTimer = 0;
    g_windGustActive = false;
    if (!saved.saved) return;
    dKyw_rain_set(saved.rainCount);
    g_env_light.base_raincnt = saved.baseRainCount;
    g_env_light.mSnowCount = saved.snowCount;
    g_env_light.mColpatWeather = saved.weather;
    g_env_light.wether_pat0 = saved.weatherPat0;
    g_env_light.wether_pat1 = saved.weatherPat1;
    g_env_light.mColpatPrevGather = saved.prevGather;
    g_env_light.mColpatCurrGather = saved.currGather;
    g_env_light.mColPatBlendGather = saved.gatherRatio;
    g_env_light.mColPatMode = saved.patMode;
    g_env_light.mColPatModeGather = saved.patModeGather;
    g_env_light.pat_ratio = saved.patternRatio;
    g_env_light.mFogNear = saved.fogNear;
    g_env_light.mFogFar = saved.fogFar;
    g_env_light.field_0x11ec = saved.fogOverrideNear;
    g_env_light.field_0x11f0 = saved.fogOverrideFar;
    g_env_light.field_0x11f4 = saved.fogOverrideRatio;
    g_env_light.mMoyaMode = saved.moyaMode;
    g_env_light.mMoyaCount = saved.moyaCount;
    g_env_light.field_0xe92 = saved.snowFogMode;
    g_env_light.mThunderEff.mMode = saved.thunderMode;
    g_env_light.mThunderEff.mStatus = saved.thunderStatus;
    g_env_light.global_wind_influence.vec_override = saved.windOverride;
    g_env_light.custom_windpower = saved.customWindPower;
    g_env_light.TeachWind_existence = saved.teachWindExistence;
    saved.saved = false;
}

void apply_weather() {
    const Weather weather = runtime_settings().weather;
    if (weather == Weather::Current) {
        restore_weather();
        return;
    }

    WeatherState& saved = g_weatherState;
    if (!saved.saved) {
        saved.saved = true;
        saved.rainCount = g_env_light.raincnt;
        saved.baseRainCount = g_env_light.base_raincnt;
        saved.snowCount = g_env_light.mSnowCount;
        saved.weather = g_env_light.mColpatWeather;
        saved.weatherPat0 = g_env_light.wether_pat0;
        saved.weatherPat1 = g_env_light.wether_pat1;
        saved.prevGather = g_env_light.mColpatPrevGather;
        saved.currGather = g_env_light.mColpatCurrGather;
        saved.gatherRatio = g_env_light.mColPatBlendGather;
        saved.patMode = g_env_light.mColPatMode;
        saved.patModeGather = g_env_light.mColPatModeGather;
        saved.patternRatio = g_env_light.pat_ratio;
        saved.fogNear = g_env_light.mFogNear;
        saved.fogFar = g_env_light.mFogFar;
        saved.fogOverrideNear = g_env_light.field_0x11ec;
        saved.fogOverrideFar = g_env_light.field_0x11f0;
        saved.fogOverrideRatio = g_env_light.field_0x11f4;
        saved.moyaMode = g_env_light.mMoyaMode;
        saved.moyaCount = g_env_light.mMoyaCount;
        saved.snowFogMode = g_env_light.field_0xe92;
        saved.thunderMode = g_env_light.mThunderEff.mMode;
        saved.thunderStatus = g_env_light.mThunderEff.mStatus;
        saved.windOverride = g_env_light.global_wind_influence.vec_override;
        saved.customWindPower = g_env_light.custom_windpower;
        saved.teachWindExistence = g_env_light.TeachWind_existence;
    }

    const bool windStorm = weather == Weather::WindStorm;
    const bool snowStorm = weather == Weather::SnowStorm;
    const bool storm = windStorm || snowStorm;
    if (storm) {
        if (g_windGustTimer <= 0) {
            g_windGustActive = !g_windGustActive;
            g_windGustTimer = g_windGustActive ? 60 + static_cast<int>(cM_rndF(361.0f)) :
                                                60 + static_cast<int>(cM_rndF(120.0f));
        }
        --g_windGustTimer;
    } else {
        g_windGustTimer = 0;
        g_windGustActive = false;
    }

    const bool denseFog = snowStorm || weather == Weather::HeavyFog;
    if (snowStorm) {
        g_env_light.mMoyaMode = 0;
        g_env_light.mMoyaCount = 50;
        g_env_light.field_0xe92 = 1;
        g_env_light.field_0x11ec = 200.0f;
        g_env_light.field_0x11f0 = 2200.0f;
        g_env_light.field_0x11f4 = 1.0f;
        g_mEnvSeMgr.setSnowPower(127.0f);
    } else if (weather == Weather::HeavyFog) {
        g_env_light.mMoyaMode = 2;
        g_env_light.mMoyaCount = 50;
        g_env_light.field_0xe92 = 0;
        g_env_light.field_0x11ec = 200.0f;
        g_env_light.field_0x11f0 = 2200.0f;
        g_env_light.field_0x11f4 = 1.0f;
    } else {
        g_env_light.mMoyaMode = saved.moyaMode;
        g_env_light.mMoyaCount = saved.moyaCount;
        g_env_light.field_0xe92 = saved.snowFogMode;
        g_env_light.field_0x11ec = saved.fogOverrideNear;
        g_env_light.field_0x11f0 = saved.fogOverrideFar;
        g_env_light.field_0x11f4 = saved.fogOverrideRatio;
    }

    const bool wet = weather == Weather::Rain || weather == Weather::Lightning || windStorm;
    const u8 pattern = wet ? 1 : (weather == Weather::Snow || snowStorm) ? 2 : 0;
    g_env_light.mColpatWeather = pattern;
    g_env_light.wether_pat0 = pattern;
    g_env_light.wether_pat1 = pattern;
    g_env_light.mColpatPrevGather = 0xFF;
    g_env_light.mColpatCurrGather = 0xFF;
    g_env_light.mColPatBlendGather = -1.0f;
    g_env_light.mColPatMode = 0;
    g_env_light.mColPatModeGather = 0;
    g_env_light.pat_ratio = 1.0f;

    if (wet) {
        dKyw_rain_set(250);
        g_env_light.mSnowCount = 0;
    } else if (weather == Weather::Snow || snowStorm) {
        dKyw_rain_set(0);
        g_env_light.mSnowCount = 500;
    } else {
        dKyw_rain_set(0);
        g_env_light.mSnowCount = 0;
    }

    const int thunderMode = weather == Weather::Lightning ? 1 : 0;
    if (thunderMode == 0 && g_env_light.mThunderEff.mMode != 0)
        g_env_light.mThunderEff.mStatus = 0;
    g_env_light.mThunderEff.mMode = thunderMode;

    if (storm) {
        g_env_light.global_wind_influence.vec_override = windStorm ? &g_windStormWind : &g_stormWind;
        g_env_light.custom_windpower = g_windGustActive ? 1.0f : 0.0f;
        g_env_light.TeachWind_existence = 1;
    } else {
        g_env_light.global_wind_influence.vec_override = saved.windOverride;
        g_env_light.custom_windpower = saved.customWindPower;
        g_env_light.TeachWind_existence = saved.teachWindExistence;
    }

    if (denseFog) {
        g_env_light.mFogNear = 200.0f;
        g_env_light.mFogFar = 2200.0f;
    } else {
        g_env_light.mFogNear = saved.fogNear;
        g_env_light.mFogFar = saved.fogFar;
    }
}

s16 scale_channel(s16 value, float factor) {
    return static_cast<s16>(std::clamp(value * factor, -1024.0f, 1023.0f));
}

u8 scale_channel(u8 value, float factor) {
    return static_cast<u8>(std::clamp(value * factor, 0.0f, 255.0f));
}

void scale_color(GXColorS10& color, float factor) {
    color.r = scale_channel(color.r, factor);
    color.g = scale_channel(color.g, factor);
    color.b = scale_channel(color.b, factor);
}

void scale_light(J3DLightObj& light, float factor) {
    J3DLightInfo* info = light.getLightInfo();
    info->mColor.r = scale_channel(info->mColor.r, factor);
    info->mColor.g = scale_channel(info->mColor.g, factor);
    info->mColor.b = scale_channel(info->mColor.b, factor);
}

void grayscale(GXColorS10& color) {
    const s32 luma = (static_cast<s32>(color.r) * 77 + static_cast<s32>(color.g) * 150 +
                         static_cast<s32>(color.b) * 29) >>
                     8;
    color.r = color.g = color.b = static_cast<s16>(std::clamp(luma, -1024, 1023));
}

void grayscale(GXColor& color) {
    const u8 luma = static_cast<u8>((static_cast<u32>(color.r) * 77 +
                                       static_cast<u32>(color.g) * 150 +
                                       static_cast<u32>(color.b) * 29) >>
                                   8);
    color.r = color.g = color.b = luma;
}

void grayscale(J3DLightObj& light) {
    J3DLightInfo* info = light.getLightInfo();
    const u8 luma = static_cast<u8>((static_cast<u32>(info->mColor.r) * 77 +
                                       static_cast<u32>(info->mColor.g) * 150 +
                                       static_cast<u32>(info->mColor.b) * 29) >>
                                   8);
    info->mColor.r = info->mColor.g = info->mColor.b = luma;
}

float brightness() {
    if (!environment_active()) return 1.0f;
    float value = runtime_settings().brightness;
    if (runtime_settings().style == Style::AstralPlane) value *= 0.65f;
    return std::clamp(value, 0.0f, 1.2f);
}

void tint_astral_light(J3DLightObj& light, bool redAccent) {
    if (!environment_active() || runtime_settings().style != Style::AstralPlane) return;
    J3DLightInfo* info = light.getLightInfo();
    const float luma =
        (info->mColor.r * 0.299f + info->mColor.g * 0.587f + info->mColor.b * 0.114f) * 0.55f;
    info->mColor.r = static_cast<u8>(
        std::clamp(luma * (redAccent ? 1.30f : 0.30f), 0.0f, 255.0f));
    info->mColor.g = static_cast<u8>(
        std::clamp(luma * (redAccent ? 0.20f : 0.48f), 0.0f, 255.0f));
    info->mColor.b = static_cast<u8>(
        std::clamp(luma * (redAccent ? 0.28f : 1.05f), 0.0f, 255.0f));
}

void adjust_high_camera_fog(float& fogNear, float& fogFar) {
    auto* camera = static_cast<camera_process_class*>(dComIfGp_getCamera(0));
    auto* player = dComIfGp_getLinkPlayer();
    if (camera == nullptr || player == nullptr || fogFar <= 1.0f) return;

    // Overhead puzzle cameras make the floor geometrically distant even though it is the
    // gameplay foreground. Move only the fog start outward as camera height increases; retain
    // the authored fog end so mountains and other genuinely distant geometry stay concealed.
    const float height = std::max(0.0f, camera->view.lookat.eye.y - player->current.pos.y);
    const float blend = std::clamp((height - 700.0f) / 2300.0f, 0.0f, 1.0f);
    if (blend <= 0.0f) return;

    // A 500-2000 unit authored end plane becomes an opaque screen in top-down views. Expand
    // the visible depth with camera altitude, then keep the final 22% as the same colored fog.
    const float originalNear = fogNear;
    const float expandedFar = std::max(fogFar, 24000.0f);
    fogFar += (expandedFar - fogFar) * blend;
    const float expandedNear = fogFar * 0.78f;
    fogNear = std::clamp(originalNear + (expandedNear - originalNear) * blend,
                         0.0f, fogFar - 1.0f);
}

void apply_distance_fog(GXColorS10& fog, float& fogNear, float& fogFar) {
    if (!environment_active()) return;
    if (runtime_settings().style == Style::AstralPlane) {
        const GXColorS10& ambient = g_env_light.bg_amb_col[0];
        const auto channel = [](s16 fogValue, s16 ambientValue) {
            return static_cast<s16>(
                std::clamp((fogValue * 3 + ambientValue * 2) / 5, 0, 1023));
        };
        fog.r = channel(g_env_light.fog_col.r, ambient.r);
        fog.g = channel(g_env_light.fog_col.g, ambient.g);
        fog.b = channel(g_env_light.fog_col.b, ambient.b);
        fogFar = std::clamp(fogFar > 100.0f ? fogFar : 9000.0f, 500.0f, 9000.0f);
        const float authoredNear = fogNear > 0.0f ? fogNear : fogFar * 0.20f;
        fogNear = std::clamp(std::min(authoredNear, fogFar * 0.28f), 0.0f, fogFar - 1.0f);
        adjust_high_camera_fog(fogNear, fogFar);
    } else if (runtime_settings().style == Style::DarkHour) {
        const GXColorS10& ambient = g_env_light.bg_amb_col[0];
        const float luma =
            std::max(0.0f, ambient.r * 0.20f + ambient.g * 0.70f + ambient.b * 0.10f);
        fog.r = static_cast<s16>(std::clamp(luma * 0.20f, 0.0f, 1023.0f));
        fog.g = static_cast<s16>(std::clamp(luma * 0.90f + 24.0f, 0.0f, 1023.0f));
        fog.b = static_cast<s16>(std::clamp(luma * 0.32f, 0.0f, 1023.0f));
        fogFar = std::clamp(fogFar > 100.0f ? fogFar : 7000.0f, 500.0f, 7000.0f);
        const float authoredNear = fogNear > 0.0f ? fogNear : fogFar * 0.18f;
        fogNear = std::clamp(std::min(authoredNear, fogFar * 0.24f), 0.0f, fogFar - 1.0f);
        adjust_high_camera_fog(fogNear, fogFar);
    }
}

void apply_astral_palette(dScnKy_env_light_c& env) {
    if (!environment_active() || runtime_settings().style != Style::AstralPlane) return;
    const auto tint = [](GXColorS10& color, bool warm) {
        const float luma = 0.60f * std::max(
                                      0.0f, color.r * 0.299f + color.g * 0.587f + color.b * 0.114f);
        color.r = static_cast<s16>(
            std::clamp(luma * (warm ? 1.20f : 0.30f), 0.0f, 1023.0f));
        color.g = static_cast<s16>(
            std::clamp(luma * (warm ? 0.22f : 0.48f), 0.0f, 1023.0f));
        color.b = static_cast<s16>(
            std::clamp(luma * (warm ? 0.30f : 0.95f), 0.0f, 1023.0f));
    };
    for (int i = 0; i < 4; ++i) tint(env.bg_amb_col[i], i == 1);
    for (int i = 0; i < 6; ++i) tint(env.dungeonlight_col[i], true);
    tint(env.actor_amb_col, false);
    env.fog_col = {49, 73, 91, env.fog_col.a};
    env.vrbox_sky_col = {24, 43, 62, env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {110, 58, 72, env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {31, 48, 68, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {17, 28, 45, env.vrbox_kumo_shadow_col.a};
    env.vrbox_kasumi_outer_col = {48, 70, 88, env.vrbox_kasumi_outer_col.a};
    env.vrbox_kasumi_inner_col = {119, 72, 85, env.vrbox_kasumi_inner_col.a};
    scale_color(env.fog_col, 0.75f);
    scale_color(env.vrbox_sky_col, 0.75f);
    scale_color(env.vrbox_kumo_top_col, 0.75f);
    scale_color(env.vrbox_kumo_bottom_col, 0.75f);
    scale_color(env.vrbox_kumo_shadow_col, 0.75f);
    scale_color(env.vrbox_kasumi_outer_col, 0.75f);
    scale_color(env.vrbox_kasumi_inner_col, 0.75f);
}

void apply_dark_hour_palette(dScnKy_env_light_c& env) {
    if (!environment_active() || runtime_settings().style != Style::DarkHour) return;
    const auto tint = [](GXColorS10& color) {
        const float luma = std::max(0.0f, color.r * 0.25f + color.g * 0.65f + color.b * 0.10f);
        color.r = static_cast<s16>(std::clamp(luma * 0.28f, 0.0f, 1023.0f));
        color.g = static_cast<s16>(std::clamp(luma * 1.08f, 0.0f, 1023.0f));
        color.b = static_cast<s16>(std::clamp(luma * 0.40f, 0.0f, 1023.0f));
    };
    for (int i = 0; i < 4; ++i) tint(env.bg_amb_col[i]);
    for (int i = 0; i < 6; ++i) tint(env.dungeonlight_col[i]);
    env.vrbox_sky_col = {12, 54, 25, env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {34, 102, 49, env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {15, 61, 29, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {5, 27, 13, env.vrbox_kumo_shadow_col.a};
    env.vrbox_kasumi_outer_col = {18, 67, 32, env.vrbox_kasumi_outer_col.a};
    env.vrbox_kasumi_inner_col = {42, 112, 56, env.vrbox_kasumi_inner_col.a};
}

void apply_authored_skybox(dScnKy_env_light_c& env) {
    if (!environment_active()) return;

    // Load the authored palette in the mod; no host-side sky cache survives a reload.
    DuskTwilightSkyboxV1 sky{};
    const u8 variant = static_cast<u8>(runtime_settings().skybox);
    if (!compat::get_authored_sky(sky, variant)) return;

    env.vrbox_sky_col = {sky.sky.r, sky.sky.g, sky.sky.b,
                         env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {sky.cloudTop.r, sky.cloudTop.g, sky.cloudTop.b,
                              env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {sky.cloudBottom.r, sky.cloudBottom.g,
                                 sky.cloudBottom.b, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {sky.cloudShadow.r, sky.cloudShadow.g,
                                 sky.cloudShadow.b, sky.cloudShadow.a};
    env.vrbox_kasumi_outer_col = {sky.hazeOuter.r, sky.hazeOuter.g,
                                  sky.hazeOuter.b, sky.hazeOuter.a};
    env.vrbox_kasumi_inner_col = {sky.hazeInner.r, sky.hazeInner.g,
                                  sky.hazeInner.b, sky.hazeInner.a};
}

void set_light_post(ModContext*, void* args, void*, void*) {
    if (!environment_active()) return;
    auto* env = mods::arg<dScnKy_env_light_c*>(args, 0);
    apply_authored_skybox(*env);
    apply_astral_palette(*env);
    apply_dark_hour_palette(*env);

    const float factor = brightness();
    scale_color(env->actor_amb_col, factor);
    for (int i = 0; i < 4; ++i) scale_color(env->bg_amb_col[i], factor);
    for (int i = 0; i < 6; ++i) {
        scale_color(env->dungeonlight_col[i], factor);
        env->dungeonlight[i].mColor.r =
            static_cast<u8>(std::clamp<s16>(env->dungeonlight_col[i].r, 0, 255));
        env->dungeonlight[i].mColor.g =
            static_cast<u8>(std::clamp<s16>(env->dungeonlight_col[i].g, 0, 255));
        env->dungeonlight[i].mColor.b =
            static_cast<u8>(std::clamp<s16>(env->dungeonlight_col[i].b, 0, 255));
    }
    scale_color(env->fog_col, factor);
    scale_color(env->vrbox_sky_col, factor);
    scale_color(env->vrbox_kumo_top_col, factor);
    scale_color(env->vrbox_kumo_bottom_col, factor);
    scale_color(env->vrbox_kumo_shadow_col, factor);
    scale_color(env->vrbox_kasumi_outer_col, factor);
    scale_color(env->vrbox_kasumi_inner_col, factor);

    if (runtime_settings().style == Style::BlackAndWhite) {
        for (int i = 0; i < 4; ++i) grayscale(env->bg_amb_col[i]);
        grayscale(env->fog_col);
        grayscale(env->vrbox_sky_col);
        grayscale(env->vrbox_kumo_top_col);
        grayscale(env->vrbox_kumo_bottom_col);
        grayscale(env->vrbox_kumo_shadow_col);
        grayscale(env->vrbox_kasumi_outer_col);
        grayscale(env->vrbox_kasumi_inner_col);
    }

    GXColor blend = *mDoGph_gInf_c::getBloom()->getBlendColor();
    GXColor mono = *mDoGph_gInf_c::getBloom()->getMonoColor();
    if (runtime_settings().style == Style::AstralPlane) {
        blend.r = 180;
        blend.g = 40;
        blend.b = 130;
        mono.a = 0;
    } else if (runtime_settings().style == Style::DarkHour) {
        blend.r = 24;
        blend.g = 220;
        blend.b = 52;
        mono.a = 0;
    } else if (runtime_settings().style == Style::BlackAndWhite) {
        grayscale(blend);
        grayscale(mono);
    }
    mDoGph_gInf_c::getBloom()->setBlendColor(blend);
    mDoGph_gInf_c::getBloom()->setMonoColor(mono);
}

HookAction set_light_pre(ModContext*, void*, void*, void*) {
    apply_weather();
    return HOOK_CONTINUE;
}

void set_light_bg_post(ModContext*, void* args, void*, void*) {
    if (!environment_active()) return;
    auto* tev = mods::arg<dKy_tevstr_c*>(args, 1);
    auto* colors = mods::arg<GXColorS10*>(args, 2);
    auto* fog = mods::arg<GXColorS10*>(args, 3);
    auto* fogNear = mods::arg<float*>(args, 4);
    auto* fogFar = mods::arg<float*>(args, 5);
    apply_distance_fog(*fog, *fogNear, *fogFar);
    const float factor = brightness();
    for (int i = 0; i < 4; ++i) scale_color(colors[i], factor);
    for (int i = 0; i < 6; ++i) {
        scale_light(tev->mLights[i], factor);
        tint_astral_light(tev->mLights[i], i == 1 || i == 4);
    }
    scale_color(*fog, factor);
    if (runtime_settings().style == Style::BlackAndWhite) {
        for (int i = 0; i < 4; ++i) grayscale(colors[i]);
        for (int i = 0; i < 6; ++i) grayscale(tev->mLights[i]);
        grayscale(*fog);
    }
}

void set_light_actor_post(ModContext*, void* args, void*, void*) {
    if (!environment_active()) return;
    auto* tev = mods::arg<dKy_tevstr_c*>(args, 1);
    auto* fog = mods::arg<GXColorS10*>(args, 2);
    auto* fogNear = mods::arg<float*>(args, 3);
    auto* fogFar = mods::arg<float*>(args, 4);
    apply_distance_fog(*fog, *fogNear, *fogFar);
    const float factor = brightness();
    scale_color(tev->AmbCol, factor);
    for (int i = 0; i < 6; ++i) {
        scale_light(tev->mLights[i], factor);
        tint_astral_light(tev->mLights[i], i == 1 || i == 4);
    }
    scale_color(*fog, factor);
}
}  // namespace

ModResult install_hooks() {
    ModResult result = mods::hook::add_pre<EnvironmentSetLight>(set_light_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<EnvironmentSetLight>(set_light_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<EnvironmentSetLightBg>(set_light_bg_post);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<EnvironmentSetLightActor>(set_light_actor_post);
}

void area_reloaded() {
    // Previous area's baseline must never be restored into the new area.
    g_weatherState = {};
    g_windGustTimer = 0;
    g_windGustActive = false;
}

void uninstall_hooks() {
    restore_weather();
    mods::hook::uninstall<EnvironmentSetLightActor>();
    mods::hook::uninstall<EnvironmentSetLightBg>();
    mods::hook::uninstall<EnvironmentSetLight>();
}

}  // namespace twilight_visuals::environment
