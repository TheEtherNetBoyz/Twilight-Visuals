#include "sequencing.hpp"
#include "runtime.hpp"
#include "compat.hpp"
#include "music.hpp"
#include "d/d_com_inf_game.h"
#include "Z2AudioLib/Z2SceneMgr.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "Z2AudioLib/Z2StatusMgr.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_audio.h"
namespace twilight_visuals::sequencing {
namespace {
Z2SceneMgr* scene_manager() { return static_cast<Z2SceneMgr*>(compat::host_api()->audioManager(DuskAudioManager_Scene)); }
Z2SeqMgr* sequence_manager() { return static_cast<Z2SeqMgr*>(compat::host_api()->audioManager(DuskAudioManager_Sequence)); }
Z2StatusMgr* status_manager() { return static_cast<Z2StatusMgr*>(compat::host_api()->audioManager(DuskAudioManager_Status)); }
bool forced = false, pending = false, previousEnabled = false;
s32 status = 0, originalStatus = -1;
bool musicOnlyRefresh = false;
bool refreshingSelection = false;
s32 restoreAfterRefresh = -1;
s32 query(DuskSequenceEvent point) {
    if (point == DuskSequence_IsReplacementScene) return forced;
    if (point == DuskSequence_IsRefreshPending) return pending;
    if (point == DuskSequence_MusicStatusOverride) return forced ? status : -1;
    if (point == DuskSequence_UseTwilightBattleMusic) return active() && music_override_allowed();
    if (point == DuskSequence_IsBgmOnlyRefresh) return musicOnlyRefresh;
    if (point == DuskSequence_OnWaveRefreshFinished) musicOnlyRefresh = false;
    if (point == DuskSequence_OnSceneBgmStarted && restoreAfterRefresh >= 0) {
        if (auto* manager = sequence_manager()) manager->changeBgmStatus(restoreAfterRefresh);
        restoreAfterRefresh = -1;
    }
    return 0;
}
void tick() {
    const bool enabled = active() && music_override_allowed();
    if (enabled == previousEnabled && !(enabled && pending)) return;
    if (!scene_manager() || !sequence_manager() || !status_manager() ||
        !scene_manager()->isSceneExist() || dComIfGp_isEnableNextStage() ||
        status_manager()->getDemoStatus() != 0 || dComIfGp_event_runCheck()) return;
    const char* stage = dComIfGp_getStartStageName();
    if (!stage || !*stage) return;
    s32 restore = -1;
    if (enabled && !previousEnabled) originalStatus = sequence_manager()->getBgmStatus();
    else if (!enabled) { restore = originalStatus; originalStatus = -1; }
    const s8 room = dComIfGp_roomControl_getStayNo();
    const s8 layer = dComIfG_play_c::getLayerNo_common(stage, room, dComIfGp_getStartStageLayer());
    if (restore >= 0) restoreAfterRefresh = restore;
    refreshingSelection = true;
    mDoAud_setSceneName(stage, room, layer);
    refreshingSelection = false;
    musicOnlyRefresh = true;
    auto* scene = scene_manager();
    scene->timer = 0;
    scene->setSceneExist(false);
    if (scene->load1stWait == 0) scene->_load1stWaveInner_1();
    mDoAud_zelAudio_c::onBgmSet();
    previousEnabled = enabled;
}
void update(void* raw, f32 base) {
    auto* p = static_cast<Z2SeqMgr*>(raw);
    if (!p || !status_manager() || !mDoRst::getResetData()) return;
    const u8 demo = status_manager()->getDemoStatus();
    const u32 sub = p->getSubBgmID();
    const bool ordinary = sub == Z2BGM_BATTLE_NORMAL || sub == Z2BGM_BATTLE_TWILIGHT;
    const bool safe = (demo == 0 || demo == 1) && p->getStreamBgmID() == 0xffffffff && !mDoRst::isReset();
    DuskTwilightAudioSequenceV1 state{};
    state.sceneMusicForced = forced;
    state.safeMusicEvent = safe;
    state.mainReplacementReady = p->mMainBgmHandle && p->getMainBgmID() == Z2BGM_DUNGEON_LV8 && p->mSceneBgm.get() > 0;
    state.ordinaryBattle = ordinary;
    state.battleFlagActive = p->mFlags.mBattleBgmOff;
    state.subMusicEligible = sub == 0xffffffff || ordinary;
    provide_audio_sequence(&state);
    state.gain = base * (ordinary && state.battleScope ? 1.0f : p->mMainBgmMaster.get()) *
        p->mSceneBgm.get() * p->mStreamBgmMaster.get() * p->field_0x84.get() * p->field_0xa4.get();
    state.battleVolume = base * p->mStreamBgmMaster.get();
    music::sequence(state.replacementScene,state.customMusicEligible,state.musicMode,state.gain,
                    state.battleScope,ordinary,state.battleVolume);
}
}
bool scene(const char* spot,s32 room,s32 layer,s32 sceneNo,bool darkness,u8 demoWave,
           u32* bgm,u8* wave1,u8* wave2,bool* streams,bool* field,s32* musicStatus) {
    const bool priorForced = forced;
    pending = false;
    if (!refreshingSelection) music::prepare_scene();
    if (!status_manager() || !sequence_manager()) return false;
    if (!provide_scene_music(spot,room,layer,sceneNo,darkness,demoWave,bgm,wave1,wave2,streams,field,musicStatus)) {
        // A deliberate music exclusion must win immediately. In particular, do not carry the
        // previous replacement-scene latch through a temple load event when temple overrides
        // are disabled; doing so leaves the visual preset stream audible over native music.
        if (!music_override_allowed()) {
            forced = false;
            pending = false;
            status = -1;
            return false;
        }
        // Keep suppressing the Palace placeholder while a load-zone callback is exposing
        // incomplete destination state. A stable callback still restores native music normally.
        const bool transientLoad = dComIfGp_isEnableNextStage() || dComIfGp_event_runCheck() ||
                                   demoWave != 0;
        forced = priorForced && active() && transientLoad;
        return false;
    }
    pending = status_manager()->getDemoStatus() != 0;
    if (!pending) {
        forced = true;
        status = musicStatus ? *musicStatus : -1;
    }
    return true;
}
void initialize() {
    const DuskSequenceHooksV1 hooks{update,tick,query};
    compat::host_api()->setSequenceHooks(&hooks);
    compat::set_scene_music_provider(scene);
}
void shutdown() {
    compat::host_api()->setSequenceHooks(nullptr);
    forced = pending = previousEnabled = false;
    originalStatus = -1;
    musicOnlyRefresh = false;
    restoreAfterRefresh = -1;
}
}
