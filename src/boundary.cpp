#include "boundary.hpp"
#include "sky.hpp"
#include "environment.hpp"
#include "particles.hpp"
#include "runtime.hpp"
#include "compat.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_stage.h"
#include <cstring>
namespace twilight_visuals::boundary {
namespace {
static int s_visual_environment_layer = -1;
static int s_visual_environment_room = -1;
static char s_visual_environment_stage[16] = {};
static bool s_visual_environment_has_twilight_layer = false;
static int s_visual_environment_loaded_layer = 0;
static bool s_visual_environment_loaded_twilight = false;
static bool s_visual_environment_area_initialized = false;
static bool s_visual_environment_forced = false;
static bool s_visual_enemy_form_context = false;
static bool restoring = false;

static void dKy_reset_visual_environment_patterns() {
    // Match the initialization performed by envcolor_init(). This clears any
    // weather/gather transition that was started while the normal layer was
    // active before selecting the Twilight palette set.
    g_env_light.wether_pat0 = g_env_light.mColpatWeather;
    g_env_light.wether_pat1 = g_env_light.mColpatWeather;
    g_env_light.mColpatPrevGather = 0xFF;
    g_env_light.mColpatCurrGather = 0xFF;
    g_env_light.mColPatBlendGather = -1.0f;
    g_env_light.mColPatMode = 0;
    g_env_light.mColPatModeGather = 0;
}



static void update() {
    const char* stageName = dComIfGp_getStartStageName();
    const int roomNo = dComIfGp_roomControl_getStayNo();
    const bool forceTwilight = !restoring && (provide_environment_layer(s_visual_environment_loaded_layer) == 14);
    int layerNo = s_visual_environment_loaded_layer;
    {
        const s32 providedLayer = restoring ? s_visual_environment_loaded_layer :
            provide_environment_layer(s_visual_environment_loaded_layer);
        if (providedLayer >= 0 && providedLayer < 15) {
            layerNo = providedLayer;
        }
    }

    // A room/load transition can expose the destination layer before the
    // current environment has been destroyed. Do not decode that transient
    // layer into the live scene. The loaded layer is committed by
    // envcolor_init() only when a complete area environment is created.
    if (forceTwilight == s_visual_environment_forced) {
        return;
    }

    const bool wasForced = s_visual_environment_forced;
    s_visual_environment_layer = layerNo;
    s_visual_environment_room = roomNo;
    s_visual_environment_forced = forceTwilight;
    if (stageName != NULL) {
        strncpy(s_visual_environment_stage, stageName, sizeof(s_visual_environment_stage) - 1);
        s_visual_environment_stage[sizeof(s_visual_environment_stage) - 1] = '\0';
    } else {
        s_visual_environment_stage[0] = '\0';
    }
    s_visual_environment_has_twilight_layer = sky::select_layer(layerNo, layerNo == 14 ? 10 : 0);

    if (forceTwilight || wasForced || s_visual_environment_has_twilight_layer) {
        if (dComIfGp_getStageEnvrInfo() != NULL) {
            g_env_light.stage_envr_info = dComIfGp_getStageEnvrInfo();
        }
        if (dComIfGp_getStagePaletteInfo() != NULL) {
            g_env_light.stage_palette_info = dComIfGp_getStagePaletteInfo();
        }
        if (dComIfGp_getStagePselectInfo() != NULL) {
            g_env_light.stage_pselect_info = dComIfGp_getStagePselectInfo();
        }
        if (dComIfGp_getStageVrboxcolInfo() != NULL) {
            g_env_light.stage_vrboxcol_info = dComIfGp_getStageVrboxcolInfo();
        }

        g_env_light.light_init_timer = 1;
        g_env_light.PrevCol = roomNo;
        g_env_light.UseCol = roomNo;
        g_env_light.pat_ratio = 1.0f;
        dKy_reset_visual_environment_patterns();
    }
}


void commit() {
    environment::area_reloaded();
    particles::area_reloaded();
    s_visual_environment_loaded_layer = dComIfG_play_c::getLayerNo(0);
    s_visual_environment_loaded_twilight = dComIfGp_world_dark_get() != 0;
    s_visual_environment_area_initialized = true;
    s_visual_environment_forced = false;
}
u8 query(DuskEnvironmentQuery kind, u8 nativeValue) {
    // Disabled (or shutting down) must be transparent to every native query.
    // A snapshot taken under an override is not a reliable native answer.
    if (!active() || restoring) return nativeValue;
    const char* stage = dComIfGp_getStartStageName();
    const bool palace = palace_excluded();
    if (kind == DuskEnvironment_ActorTwilight) {
        // King Bulblin's encounter controller and supporting actors use their native form/state
        // checks to initialize the fight. Keep those checks untouched while retaining the visual
        // Twilight treatment supplied by the remaining query types.
        if (king_bulblin_encounter_active()) return nativeValue;
        return s_visual_enemy_form_context && active() && stage && !palace ? 1 : nativeValue;
    }
    if (kind == DuskEnvironment_SnowStorm) return static_cast<u8>(runtime_settings().weather) == 6;
    if (!stage) return nativeValue;
    if (palace) return kind == DuskEnvironment_VisualTwilight ? 0 : nativeValue;
    if (active()) return 1;
    return nativeValue;
}
void actor_context(u8 enabled) { s_visual_enemy_form_context = enabled != 0; }
}
void initialize() {
    restoring = false;
    commit();
    const DuskEnvironmentHooksV1 hooks{commit, update, query, actor_context};
    compat::host_api()->setEnvironmentHooks(&hooks);
}
void shutdown() {
    restoring = true;
    if (s_visual_environment_area_initialized && s_visual_environment_forced &&
        dComIfGp_getStage() && dComIfGp_roomControl_getStayNo() >= 0) update();
    compat::host_api()->setEnvironmentHooks(nullptr);
    s_visual_enemy_form_context = false;
    s_visual_environment_area_initialized = false;
}
}
