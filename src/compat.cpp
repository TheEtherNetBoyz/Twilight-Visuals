#include "compat.hpp"
#include "sky.hpp"

#include "mods/svc/hook.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>

extern const HookService* svc_hook;

namespace twilight_visuals::compat {
namespace {
template <typename T>
T resolve(const char* symbol) {
    static HMODULE executable = GetModuleHandleW(nullptr);
    return executable == nullptr ? nullptr : reinterpret_cast<T>(GetProcAddress(executable, symbol));
}

}  // namespace

const DuskTwilightHostApiV1* host_api() {
    using Fn = const DuskTwilightHostApiV1* (*)();
    static Fn fn = resolve<Fn>("DuskGetTwilightHostApiV1");
    static const DuskTwilightHostApiV1* api = fn != nullptr ? fn() : nullptr;
    constexpr u32 requiredSize = static_cast<u32>(offsetof(DuskTwilightHostApiV1, getMasterVolume) +
                                                   sizeof(float (*)()));
    return api != nullptr && api->abiVersion == DUSK_TWILIGHT_HOST_ABI_V1 &&
                   api->structSize >= requiredSize ?
        api : nullptr;
}






float get_master_volume() {
    if (const auto* api = host_api(); api != nullptr &&
        (api->capabilities & DUSK_TWILIGHT_HOST_CAP_MASTER_VOLUME) != 0) {
        return api->getMasterVolume();
    }
    using Fn = float (*)();
    static Fn fn = resolve<Fn>("DuskGetMasterVolume");
    return fn != nullptr ? fn() : 1.0f;
}

bool is_prelaunch_open() {
    static bool resolved = false;
    static bool (*query)() = nullptr;
    if (!resolved) {
        resolved = true;
        void* address = nullptr;
        HookSymbolFlags flags{};
        if (svc_hook != nullptr &&
            svc_hook->resolve(mod_ctx, "dusk::ui::is_prelaunch_open", &address, &flags) == MOD_OK &&
            (static_cast<u32>(flags) & HOOK_SYMBOL_CODE) != 0) {
            query = reinterpret_cast<bool (*)()>(address);
        }
    }
    return query != nullptr && query();
}


void set_enemy_proc_provider(DuskTwilightEnemyProcProviderV1 provider) {
    if (const auto* api = host_api(); api != nullptr &&
        (api->capabilities & DUSK_TWILIGHT_HOST_CAP_ENEMY_PROC_PROVIDER) != 0 &&
        api->structSize >= offsetof(DuskTwilightHostApiV1, setEnemyProcProvider) +
                               sizeof(DuskTwilightEnemyProcProviderV1) &&
        api->setEnemyProcProvider != nullptr) {
        api->setEnemyProcProvider(provider);
    }
}


void set_bloom_provider(DuskTwilightBloomProviderV1 provider) {
    if (const auto* api = host_api(); api != nullptr &&
        (api->capabilities & DUSK_TWILIGHT_HOST_CAP_BLOOM_PROVIDER) != 0 &&
        api->structSize >= offsetof(DuskTwilightHostApiV1, setBloomProvider) +
                               sizeof(DuskTwilightBloomProviderV1) &&
        api->setBloomProvider != nullptr) {
        api->setBloomProvider(provider);
    }
}

void set_scene_music_provider(DuskTwilightSceneMusicProviderV1 provider) {
    if (const auto* api = host_api(); api != nullptr &&
        (api->capabilities & DUSK_TWILIGHT_HOST_CAP_SCENE_MUSIC_PROVIDER) != 0 &&
        api->structSize >= offsetof(DuskTwilightHostApiV1, setSceneMusicProvider) +
                               sizeof(DuskTwilightSceneMusicProviderV1) &&
        api->setSceneMusicProvider != nullptr) {
        api->setSceneMusicProvider(provider);
    }
}





bool get_authored_sky(DuskTwilightSkyboxV1& outSky, u8 variant) {
    struct SkyVariantSource {
        const char* stageName;
        u8 layer;
        u8 paletteSlot;
    };
    static constexpr SkyVariantSource kSources[] = {
        {"F_SP108", 14, 0}, // Twilight day
        {"F_SP108", 0, 5},  // Twilight night
        {"F_SP121", 0, 1},  // Hyrule Field sunrise
        {"F_SP121", 0, 4},  // Hyrule Field sunset
        {"F_SP114", 0, 0},  // Snowpeak overcast/storm
        {"F_SP108", 14, 0}, // Faron Twilight
        {"F_SP109", 14, 0}, // Eldin Twilight
        {"F_SP115", 14, 0}, // Lanayru Twilight
        {"D_MN08", 0, 0},   // Palace of Twilight
        {"F_SP117", 0, 0},  // Sacred Grove
        {"F_SP114", 0, 0},  // Snowpeak
        {"F_SP124", 0, 0},  // Gerudo Desert
        {"F_SP115", 0, 0},  // Lake Hylia
        {"F_SP127", 0, 0},  // Fishing Hole
        {"F_SP103", 0, 0},  // Ordon
        {"F_SP121", 0, 0},  // Hyrule Field
        {"F_SP116", 0, 0},  // Castle Town
    };

    if (variant >= sizeof(kSources)/sizeof(kSources[0])) return false;
    const auto& source = kSources[variant];
    return sky::read(&outSky, source.stageName, source.layer, source.paletteSlot,
                                   source.layer == 14 ? 10 : 0);
}


}  // namespace twilight_visuals::compat
