#include "music.hpp"
#include "compat.hpp"
#include "TwilightMusicFade.h"
#include "Z2AudioLib/Z2Param.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "mods/svc/log.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstddef>
#define DR_MP3_IMPLEMENTATION
#include "third_party/dr_mp3.h"

namespace twilight_visuals::music {
namespace {
using dusk::audio::TwilightMusicFade;
struct Track {
    drmp3 decoder{};
    std::filesystem::path path;
    bool available = false;
    bool playbackStarted = false;
    float audibleGain = 0;
    std::array<float, 4096> decoded{};
    size_t frameIndex = 0, frameCount = 0;
    std::array<float, 2> a{}, b{};
    bool primed = false;
    double position = 0;

    void close() {
        if (available) drmp3_uninit(&decoder);
        decoder = {};
        available = playbackStarted = primed = false;
        audibleGain = 0;
        frameIndex = frameCount = 0;
        position = 0;
    }
    void open(const std::filesystem::path& file) {
        close();
        path = file;
        available = drmp3_init_file_w(&decoder, path.c_str(), nullptr) != 0;
        if (available && (decoder.channels == 0 || decoder.channels > 2 || decoder.sampleRate == 0))
            close();
        const auto name = path.u8string();
        const std::string message = std::string(available ? "Streaming music: " : "Music unavailable (missing or unsupported): ") +
            std::string(reinterpret_cast<const char*>(name.data()), name.size());
        svc_log->write(mod_ctx, available ? LOG_LEVEL_INFO : LOG_LEVEL_WARN, message.c_str());
    }
    bool rewind() {
        if (!available) return false;
        if (!drmp3_seek_to_pcm_frame(&decoder, 0)) {
            drmp3_uninit(&decoder);
            decoder = {};
            available = drmp3_init_file_w(&decoder, path.c_str(), nullptr) != 0;
        }
        frameIndex = frameCount = 0;
        return available;
    }
    bool next(std::array<float, 2>& sample) {
        if (frameIndex == frameCount) {
            frameIndex = 0;
            frameCount = static_cast<size_t>(drmp3_read_pcm_frames_f32(
                &decoder, decoded.size() / decoder.channels, decoded.data()));
            if (!frameCount) {
                if (!rewind()) return false;
                frameCount = static_cast<size_t>(drmp3_read_pcm_frames_f32(
                    &decoder, decoded.size() / decoder.channels, decoded.data()));
                if (!frameCount) return false;
            }
        }
        const size_t index = frameIndex++ * decoder.channels;
        sample = {decoded[index], decoded[index + (decoder.channels == 2 ? 1 : 0)]};
        return true;
    }
    void setGain(float target, float elapsed, bool smoothReduction = false,
                 bool rewindWhenSilent = false, bool immediate = false) {
        if (!available) return;
        if (target > 0) playbackStarted = true;
        const float step = std::clamp(elapsed, 0.0f, 0.05f);
        audibleGain = immediate ? target : smoothReduction
            ? audibleGain + std::clamp(target - audibleGain, -step, step)
            : std::min(target, audibleGain + step);
        if (rewindWhenSilent && target <= 0 && audibleGain <= 0.0001f && playbackStarted) {
            rewind();
            playbackStarted = primed = false;
            position = 0;
        }
    }
    void mix(float* output, u32 frames, u32 rate, float calibration, float volume,
             bool pauseWhenSilent = false) {
        if (!available || !playbackStarted || !rate ||
            (pauseWhenSilent && audibleGain <= 0.0001f)) return;
        if (!primed) {
            if (!next(a) || !next(b)) return;
            primed = true;
        }
        const double step = static_cast<double>(decoder.sampleRate) / rate;
        const float gain = audibleGain * calibration * volume * Z2Param::VOL_BGM_DEFAULT;
        for (u32 i = 0; i < frames; ++i) {
            for (u32 channel = 0; channel < 2; ++channel)
                output[2 * i + channel] +=
                    (a[channel] + (b[channel] - a[channel]) * static_cast<float>(position)) * gain;
            position += step;
            while (position >= 1.0) {
                a = b;
                if (!next(b)) return;
                position -= 1.0;
            }
        }
    }
};
Track AstralMp3Ambient, AstralMp3Combat, DarkHourAmbient, DarkHourCombat, MasterOfShadow;
std::atomic<float> TwilightMusicVolume{1};
std::atomic<float> PalaceGain{1}, BattleGain{1}, BossGain{1};
std::atomic<u32> BossNativeMain{0xffffffff}, BossNativeSub{0xffffffff};
bool registered = false;
std::atomic<bool> sceneStartPending{true};
TwilightMusicFade fade, encounterFade, bossFade;
std::chrono::steady_clock::time_point lastTick{};
void update_sequence(bool replacementScene, bool eligible, int musicMode,
                     float gain, bool battleScope, bool battleActive, float battleVolume,
                     bool bossActive, float bossVolume, u32 bossMain, u32 bossSub) {
    const auto tick = std::chrono::steady_clock::now();
    const float elapsed = lastTick.time_since_epoch().count() == 0 ? 0.0f :
        std::chrono::duration<float>(tick - lastTick).count();
    lastTick = tick;
    const bool astral = musicMode == 1;
    const bool darkHour = musicMode == 2;
    const bool ready = AstralMp3Ambient.available;
    const bool combatReady = AstralMp3Combat.available;
    const bool darkHourReady = DarkHourAmbient.available;
    const bool darkHourCombatReady = DarkHourCombat.available;
    const bool bossReady = MasterOfShadow.available;
    const bool selectionScope = replacementScene || battleScope;
    const bool customSelected = astral || darkHour;
    const bool selectionReady = astral ? ready : (darkHour && darkHourReady);
    const bool currentTrackReady = battleActive ? (astral ? combatReady : darkHour && darkHourCombatReady) : selectionReady;
    const bool sceneStart = sceneStartPending.load() && replacementScene;
    // Silence the placeholder before it becomes audible, but do not start the
    // decoder until the native scene is ready to play music.
    fade.select(customSelected && currentTrackReady, selectionScope, elapsed, sceneStart);
    // A prepared stream remains alive and advances silently after its style is
    // deselected. Track identity must therefore gate every audible path; ready
    // alone does not mean that track is currently selected.
    const bool replaceAstralBattle = astral && battleScope && combatReady;
    const bool replaceDarkHourBattle = darkHour && battleScope && darkHourCombatReady;
    const bool replaceBattle = replaceAstralBattle || replaceDarkHourBattle;
    const bool enteringCombat = battleActive && replaceBattle;
    // Give the combat outro and exploration re-entry about 1.5 seconds each.
    // Keep combat entry/Palace selection at their existing speed, and preserve
    // the exclusive handoff so the two Astral tracks never play over each other.
    if (sceneStart) encounterFade.position = enteringCombat ? 1.0f : 0.0f;
    else encounterFade.update(enteringCombat, elapsed, enteringCombat ? 2.0f : 3.0f);
    bossFade.update(bossActive && bossReady, elapsed, 1.5f);
    if (bossActive) {
        BossNativeMain.store(bossMain);
        BossNativeSub.store(bossSub);
    }
    // Keep the replacement Palace sequence silent through interruptions and
    // AST preparation. Native Palace areas are outside replacementScene.
    PalaceGain.store(replacementScene && currentTrackReady ? fade.palace() : 1.0f);
    // Gate all ordinary battle sequences at their native channel output,
    // including detached fade-out tails. Never tag boss/miniboss themes.
    BattleGain.store(replaceBattle ? fade.palace() : 1.0f);
    BossGain.store(bossReady ? bossFade.palace() : 1.0f);
    // Zero volume is deliberately NOT stop or pause. The native loop and its
    // sample position keep advancing through battles, menus and track changes.
    const float targetGain = astral && eligible && ready && !bossActive &&
        (!battleActive || replaceAstralBattle) ?
        std::clamp(gain, 0.0f, 1.0f) * fade.astral() * encounterFade.palace() : 0.0f;
    // Gentle re-entry, immediate reductions: never let a fade-out trail cross
    // the exclusive handoff into Palace or protected music.
    AstralMp3Ambient.setGain(targetGain, elapsed, false, false, sceneStart);
    AstralMp3Combat.setGain(astral && replaceAstralBattle && !bossActive ? std::clamp(battleVolume, 0.0f, 1.0f) *
        fade.astral() * encounterFade.astral() : 0.0f, elapsed, true, true, sceneStart);
    DarkHourAmbient.setGain(darkHour && eligible && darkHourReady && !bossActive ? std::clamp(gain, 0.0f, 1.0f) *
        fade.astral() * encounterFade.palace() : 0.0f, elapsed, false, false, sceneStart);
    DarkHourCombat.setGain(replaceDarkHourBattle && battleActive && !bossActive ?
        std::clamp(battleVolume, 0.0f, 1.0f) * fade.astral() * encounterFade.astral() : 0.0f,
        elapsed, true, false, sceneStart);
    MasterOfShadow.setGain(bossActive && bossReady ?
        std::clamp(bossVolume, 0.0f, 1.0f) * bossFade.astral() : 0.0f,
        elapsed, true, !bossActive, sceneStart);
    if (sceneStart && eligible && gain > 0.0f) sceneStartPending.store(false);
}


void mix(float* output, u32 frames, u32 rate) {
    const float volume = TwilightMusicVolume.load();
    AstralMp3Ambient.mix(output, frames, rate, 0.85f, volume);
    AstralMp3Combat.mix(output, frames, rate, 0.85f, volume);
    DarkHourAmbient.mix(output, frames, rate, 0.65f, volume);
    // Preserve the combat track's decoder position between encounters. It advances during the
    // outro fade, pauses once silent, and resumes from that point on the next battle.
    DarkHourCombat.mix(output, frames, rate, 0.65f, volume, true);
    // Mix the replacement separately, then fit it around every existing native sample. This
    // keeps game voices and effects bit-for-bit intact and prevents the MP3 from clipping over
    // them without applying a buffer-wide duck or delaying the start of the boss track.
    std::array<float, 4096> bossMix{};
    u32 offset = 0;
    while (offset < frames) {
        const u32 chunk = std::min<u32>(frames - offset, bossMix.size() / 2);
        std::fill_n(bossMix.data(), chunk * 2, 0.0f);
        // The native boss sequence is muted below, so the signal already in
        // output is game SFX/voices/ambience. Side-chain only the replacement
        // music around that signal; never attenuate or overwrite native audio.
        float nativePeak = 0.0f;
        for (u32 i = 0; i < chunk * 2; ++i) {
            nativePeak = std::max(nativePeak, std::abs(output[offset * 2 + i]));
        }
        const float sfxDuck = std::clamp(1.0f - nativePeak * 1.25f, 0.28f, 1.0f);
        MasterOfShadow.mix(bossMix.data(), chunk, rate, 0.75f * sfxDuck, volume, true);
        for (u32 i = 0; i < chunk * 2; ++i) {
            const u32 outputIndex = offset * 2 + i;
            const float native = output[outputIndex];
            const float room = std::max(0.0f, 1.0f - std::abs(native));
            output[outputIndex] = native + std::clamp(bossMix[i], -room, room);
        }
        offset += chunk;
    }
}
float channel_gain(u32 channel) {
    if (channel == Z2BGM_DUNGEON_LV8) return PalaceGain.load();
    if (channel == Z2BGM_BATTLE_NORMAL || channel == Z2BGM_BATTLE_TWILIGHT) return BattleGain.load();
    // Mute the native boss score while the streamed replacement fades in.
    // Ordinary action SFX use the SE buses and are deliberately unaffected.
    // 0xffffffff means "no sequence" and is also the default tag carried by
    // many non-BGM tracks. Never compare that sentinel as a real boss ID or it
    // will silence Link, enemy, and item sounds along with the music.
    const u32 bossMain = BossNativeMain.load();
    const u32 bossSub = BossNativeSub.load();
    const bool savedBoss = (bossMain != 0xffffffff && channel == bossMain) ||
                           (bossSub != 0xffffffff && channel == bossSub);
    if (is_boss_bgm(channel) || savedBoss) return BossGain.load();
    return 1.0f;
}
}
bool is_boss_bgm(u32 id) {
    switch (id) {
    case Z2BGM_FACE_OFF_BATTLE:
    case Z2BGM_BOOMERAMG_MONKEY:
    case Z2BGM_BOSSBABA_0:
    case Z2BGM_BOSSBABA_1:
    case Z2BGM_BOSSBABA_2:
    case Z2BGM_BOSSFIREMAN_0:
    case Z2BGM_BOSSFIREMAN_1:
    case Z2BGM_MAGNE_GORON:
    case Z2BGM_DEKUTOAD:
    case Z2BGM_BOSS_OCTAEEL_0:
    case Z2BGM_BOSS_OCTAEEL_1:
    case Z2BGM_VARIANT:
    case Z2BGM_BOSS_SNOWWOMAN_0:
    case Z2BGM_BOSS_SNOWWOMAN_1:
    case Z2BGM_IB_MBOSS:
    case Z2BGM_BOSS_ZANT:
    case Z2BGM_TN_MBOSS:
    case Z2BGM_GG_MBOSS:
    case Z2BGM_P_ZANT:
    case Z2BGM_VS_GANON_01:
    case Z2BGM_VS_GANON_02:
    case Z2BGM_VS_GANON_04:
    case Z2BGM_HARAGIGANT_BTL01:
    case Z2BGM_HARAGIGANT_BTL02:
    case Z2BGM_DRAGON_BTL01:
    case Z2BGM_DRAGON_BTL02:
    case Z2BGM_GOMA_BTL01:
    case Z2BGM_GOMA_BTL02:
    case Z2BGM_FACE_OFF_BATTLE2:
    case Z2BGM_FACE_OFF_BATTLE3:
    case Z2BGM_TN_MBOSS_LV9:
        return true;
    default:
        return false;
    }
}
ModResult initialize() {
    const auto* api = compat::host_api();
    if (!api || !(api->capabilities & DUSK_TWILIGHT_HOST_CAP_AUDIO_HOOKS) ||
        api->structSize < offsetof(DuskTwilightHostApiV1, setAudioHooks) + sizeof(api->setAudioHooks) ||
        !api->setAudioHooks) return MOD_UNSUPPORTED;
    std::array<wchar_t, 32768> exe{};
    const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
    if (!length || length >= exe.size()) return MOD_UNAVAILABLE;
    const auto directory = std::filesystem::path(exe.data()).parent_path();
    AstralMp3Ambient.open(directory / L"Astral Plane.mp3");
    AstralMp3Combat.open(directory / L"Astral Plane CM.mp3");
    DarkHourAmbient.open(directory / L"tartarus 0d06.mp3");
    DarkHourCombat.open(directory / L"Mass Destruction.mp3");
    MasterOfShadow.open(directory / L"Master of Shadow.mp3");
    const DuskAudioHooksV1 hooks{mix, channel_gain};
    api->setAudioHooks(&hooks);
    registered = true;
    return MOD_OK;
}
void set_volume(float value) { TwilightMusicVolume.store(std::clamp(value, 0.0f, 1.0f)); }
void prepare_scene() { sceneStartPending.store(true); }
void sequence(bool scene, bool eligible, int mode, float gain, bool scope, bool battle,
              float battleVolume, bool boss, float bossVolume, u32 bossMain, u32 bossSub) {
    update_sequence(scene, eligible, mode, gain, scope, battle, battleVolume, boss, bossVolume,
                    bossMain, bossSub);
}
void shutdown() {
    if (registered) {
        compat::host_api()->setAudioHooks(nullptr);
        registered = false;
    }
    AstralMp3Ambient.close();
    AstralMp3Combat.close();
    DarkHourAmbient.close();
    DarkHourCombat.close();
    MasterOfShadow.close();
    fade = {};
    encounterFade = {};
    bossFade = {};
    lastTick = {};
    sceneStartPending.store(true);
    PalaceGain.store(1);
    BattleGain.store(1);
    BossGain.store(1);
    BossNativeMain.store(0xffffffff);
    BossNativeSub.store(0xffffffff);
}
}
