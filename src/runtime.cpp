#include "runtime.hpp"

#include "settings.hpp"
#include "compat.hpp"
#include "music.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "f_pc/f_pc_name.h"
#include "Z2AudioLib/Z2SceneMgr.h"

#include <algorithm>
#include <cstring>

namespace twilight_visuals {
namespace {
RuntimeSettings g_runtime;
bool g_speedrunSuppressed = false;
bool g_kingBulblinEncounter = false;
char g_enemyProcStage[16] = {};
s32 g_enemyProcRoom = -128;
s32 g_enemyProcLayer = -128;
bool g_musicSceneTemple = false;
bool g_musicScenePalace = false;

bool is_palace_music_stage(const char* stage) {
    return stage != nullptr && std::strncmp(stage, "D_MN08", 6) == 0;
}

bool is_temple_music_stage(const char* stage) {
    // Dungeon scene numbers can temporarily describe the source scene while a load-zone
    // transition is in progress. Stage names are stable for the loaded map, so use the D_MN
    // identity as the authoritative music scope. Palace has its own independent exclusion.
    return stage != nullptr && std::strncmp(stage, "D_MN", 4) == 0 &&
           !is_palace_music_stage(stage);
}

bool is_king_bulblin_stage(const char* stage) {
    if (stage == nullptr) return false;

    // Mounted encounters 1/2, the desert rematch, and the Hyrule Castle rematch. These fights
    // coordinate native rider, mount, boss, and event actor IDs and cannot use Twilight-form
    // actor substitutions.
    return std::strncmp(stage, "F_SP102", 7) == 0 ||
           std::strncmp(stage, "F_SP123", 7) == 0 ||
           std::strncmp(stage, "F_SP118", 7) == 0 ||
           std::strncmp(stage, "D_MN09B", 7) == 0;
}
}

const RuntimeSettings& runtime_settings() { return g_runtime; }

void refresh_runtime_settings() {
    const Settings& config = settings();
    g_runtime.enabled = get_bool(config.enabled);
    g_runtime.style = static_cast<Style>(std::clamp<std::int64_t>(get_int(config.style), 0, 3));
    g_runtime.brightness =
        static_cast<float>(std::clamp<std::int64_t>(get_int(config.brightness, 100), 0, 120)) /
        100.0f;
    g_runtime.chromaticAberration =
        static_cast<int>(std::clamp<std::int64_t>(get_int(config.chromaticAberration, 80), 0, 200));
    g_runtime.skybox =
        static_cast<Skybox>(std::clamp<std::int64_t>(get_int(config.skybox), 0, 16));
    g_runtime.weather =
        g_speedrunSuppressed ? Weather::Current :
        static_cast<Weather>(std::clamp<std::int64_t>(get_int(config.weather), 0, 7));
    g_runtime.musicVolume =
        static_cast<float>(std::clamp<std::int64_t>(get_int(config.musicVolume, 100), 0, 100)) /
        100.0f;
    g_runtime.overrideTempleMusic = get_bool(config.overrideTempleMusic);
    g_runtime.skywardSwordRunning = get_bool(config.skywardSwordRunning);
    g_runtime.humanWolfSenses = get_bool(config.humanWolfSenses);
    g_runtime.excludePalaceOfTwilight = get_bool(config.excludePalaceOfTwilight, true);
    // Keep the linkage policy in the mod: the host only exposes its current
    // master multiplier and applies the value sent here to the streamed mix.
    music::set_volume(g_runtime.musicVolume * compat::get_master_volume());
}

void provide_visual_state(u8* enabled, u8* style, f32* brightness,
                          s32* chromaticAberration, u8* skyVariant, u8* weather,
                          u8* alternateRun) {
    if (enabled != nullptr) *enabled = active() ? 1 : 0;
    if (style != nullptr) *style = static_cast<u8>(g_runtime.style);
    if (brightness != nullptr) *brightness = g_runtime.brightness;
    if (chromaticAberration != nullptr) *chromaticAberration = g_runtime.chromaticAberration;
    if (skyVariant != nullptr) *skyVariant = static_cast<u8>(g_runtime.skybox);
    if (weather != nullptr) *weather = static_cast<u8>(g_runtime.weather);
    if (alternateRun != nullptr) *alternateRun = g_runtime.skywardSwordRunning ? 1 : 0;
}

s16 provide_enemy_proc(s16 procName) {
    const char* stage = dComIfGp_getStartStageName();
    const s32 room = dComIfGp_roomControl_getStayNo();
    const s32 layer = dComIfG_play_c::getLayerNo(0);

    // Actor records for a newly loaded encounter pass through this provider in load order.
    // Reset the encounter guard when that scope changes, then latch it as soon as King Bulblin's
    // controller is encountered. His scripts require the original supporting actor identities.
    if (stage == nullptr || std::strncmp(g_enemyProcStage, stage, sizeof(g_enemyProcStage) - 1) != 0 ||
        room != g_enemyProcRoom || layer != g_enemyProcLayer) {
        g_kingBulblinEncounter = false;
        g_enemyProcRoom = room;
        g_enemyProcLayer = layer;
        if (stage != nullptr) {
            std::strncpy(g_enemyProcStage, stage, sizeof(g_enemyProcStage) - 1);
            g_enemyProcStage[sizeof(g_enemyProcStage) - 1] = '\0';
        } else {
            g_enemyProcStage[0] = '\0';
        }
    }

    if (is_king_bulblin_stage(stage) || procName == fpcNm_B_GM_e) {
        g_kingBulblinEncounter = true;
        return procName;
    }

    if (!active() || stage == nullptr || palace_excluded() ||
        layer == 14 || g_kingBulblinEncounter) {
        return procName;
    }

    // Preserve the scripted Bulblin waiting in Ordon Spring after Link's first wolf
    // transformation. Replacing this one actor with its Twilight variant changes the
    // encounter behavior and can interfere with the story sequence.
    if (procName == fpcNm_E_RD_e && std::strncmp(stage, "F_SP104", 7) == 0) {
        return procName;
    }

    switch (procName) {
    case fpcNm_E_RD_e:
        return fpcNm_E_RDY_e;
    case fpcNm_E_BA_e:
        return fpcNm_E_YK_e;
    case fpcNm_E_DB_e:
        return fpcNm_E_YD_e;
    case fpcNm_E_HB_e:
        return fpcNm_E_YH_e;
    case fpcNm_E_KR_e:
        return fpcNm_E_YR_e;
    case fpcNm_E_MS_e:
        return fpcNm_E_YG_e;
    default:
        return procName;
    }
}

bool king_bulblin_encounter_active() {
    return g_kingBulblinEncounter || is_king_bulblin_stage(dComIfGp_getStartStageName());
}

bool custom_music_allowed() {
    // Vanilla reserves this entire story window for Midna's Lament. Do not let a visual preset
    // replace it between Zant's attack and Midna's revival by Zelda.
    return !dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[104]) ||
           dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[250]);
}

bool music_override_allowed() {
    if (!custom_music_allowed()) return false;
    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr && *stage != '\0') {
        if (is_palace_music_stage(stage))
            return g_runtime.overrideTempleMusic && !g_runtime.excludePalaceOfTwilight;
        if (is_temple_music_stage(stage)) return g_runtime.overrideTempleMusic;
        return true;
    }
    // Scene-provider state is only a fallback while no current stage is available.
    if (g_musicScenePalace)
        return g_runtime.overrideTempleMusic && !g_runtime.excludePalaceOfTwilight;
    if (g_musicSceneTemple) return g_runtime.overrideTempleMusic;
    return true;
}

bool palace_excluded() {
    const char* stage = dComIfGp_getStartStageName();
    return g_runtime.excludePalaceOfTwilight && stage != nullptr &&
           std::strncmp(stage, "D_MN08", 6) == 0;
}

s32 provide_environment_layer(s32 currentLayer) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || stage == nullptr || palace_excluded()) {
        return -1;
    }
    return 14;
}

u8 provide_bloom_profile(u8 defaultProfile) {
    const char* stage = dComIfGp_getStartStageName();
    return active() && stage && !palace_excluded() &&
        !daPy_py_c::checkNowWolfPowerUp() && g_env_light.field_0x12fc < 0 ? 1 : defaultProfile;
}

bool provide_scene_music(const char* spot, s32 room, s32 layer, s32 sceneNo,
                         bool inDarkness, u8 demoWave, u32* bgmId, u8* bgmWave1,
                         u8* bgmWave2, bool* preserveStreams, bool* fieldBgmPlay,
                         s32* musicStatus) {
    (void)room;
    (void)layer;
    (void)inDarkness;
    const bool palaceSpot = is_palace_music_stage(spot);
    const bool templeScene = is_temple_music_stage(spot) ||
        sceneNo == Z2SCENE_FOREST_TEMPLE ||
        sceneNo == Z2SCENE_GORON_MINES ||
        sceneNo == Z2SCENE_LAKEBED_TEMPLE ||
        sceneNo == Z2SCENE_ARBITERS_GROUNDS ||
        sceneNo == Z2SCENE_SNOWPEAK_RUINS ||
        sceneNo == Z2SCENE_TEMPLE_OF_TIME ||
        sceneNo == Z2SCENE_CITY_IN_THE_SKY ||
        sceneNo == Z2SCENE_HYRULE_CASTLE;
    const bool palaceScene = sceneNo >= Z2SCENE_PALACE_OF_TWILIGHT &&
                             sceneNo <= Z2SCENE_PALACE_OF_TWILIGHT_BOSS;
    const bool palaceMusicScene = palaceScene || palaceSpot;
    const bool templeMusicScene = templeScene || palaceMusicScene;
    g_musicSceneTemple = templeMusicScene;
    g_musicScenePalace = palaceMusicScene;
    if (!active() || !custom_music_allowed() || spot == nullptr ||
        (templeMusicScene && !g_runtime.overrideTempleMusic) ||
        (!palaceSpot && spot[0] != 'F' && spot[0] != 'R' &&
            !(g_runtime.overrideTempleMusic && templeMusicScene)) ||
        (demoWave != 0 && sceneNo != Z2SCENE_KAKARIKO_VILLAGE)) {
        return false;
    }

    if ((g_runtime.excludePalaceOfTwilight && palaceMusicScene) ||
        sceneNo == Z2SCENE_FINAL_BATTLE_CUTSCENE) return false;

    if (bgmId != nullptr) *bgmId = Z2BGM_DUNGEON_LV8;
    if (bgmWave1 != nullptr) *bgmWave1 = 0x28;
    if (bgmWave2 != nullptr) *bgmWave2 = 0;
    if (preserveStreams != nullptr) *preserveStreams = true;
    if (fieldBgmPlay != nullptr) *fieldBgmPlay = false;
    if (musicStatus != nullptr) *musicStatus = spot[0] == 'R' ? 1 : 0;
    return true;
}

void provide_audio_sequence(DuskTwilightAudioSequenceV1* state) {
    if (state == nullptr) return;

    const bool enabled = active() && music_override_allowed();
    state->enabled = enabled;
    state->replacementScene = enabled && state->sceneMusicForced;
    state->customMusicEligible = state->replacementScene && state->safeMusicEvent &&
        state->mainReplacementReady && state->subMusicEligible;
    state->battleScope = enabled && state->safeMusicEvent &&
        (state->ordinaryBattle || !state->battleFlagActive) && state->subMusicEligible;
    const u8 selectedMusicMode =
        g_runtime.style == Style::AstralPlane ? 1 :
        g_runtime.style == Style::DarkHour ? 2 : 0;
    state->musicMode = enabled && (state->replacementScene || state->battleScope) ?
        selectedMusicMode : 0;
}

void provide_running(DuskTwilightRunningV1* state) {
    if (state == nullptr) return;
    state->enabled = active() && g_runtime.skywardSwordRunning;
    state->speed = 37.0f;
    state->heavyBootsRate = 0.70f;
}

bool provide_grass(bool* monochrome) {
    if (monochrome == nullptr) return false;
    const char* stage = dComIfGp_getStartStageName();
    *monochrome = active() && g_runtime.style == Style::BlackAndWhite &&
        stage != nullptr && !palace_excluded();
    return true;
}

void provide_render_policy(DuskTwilightRenderPolicyV1* state) {
    if (state == nullptr) return;
    const bool astral = active() && g_runtime.style == Style::AstralPlane;
    state->astralFog = astral;
    state->astralFragments = astral;
}

bool active() {
    return g_runtime.enabled && !g_speedrunSuppressed;
}

void set_speedrun_suppressed(bool suppressed) {
    g_speedrunSuppressed = suppressed;
    refresh_runtime_settings();
}

}  // namespace twilight_visuals
