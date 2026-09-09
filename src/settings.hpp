#pragma once

#include "mods/svc/config.h"

namespace twilight_visuals {

struct Settings {
    ConfigVarHandle enabled{};
    ConfigVarHandle style{};
    ConfigVarHandle brightness{};
    ConfigVarHandle chromaticAberration{};
    ConfigVarHandle skybox{};
    ConfigVarHandle weather{};
    ConfigVarHandle musicVolume{};
    ConfigVarHandle overrideTempleMusic{};
    ConfigVarHandle skywardSwordRunning{};
    ConfigVarHandle humanWolfSenses{};
    ConfigVarHandle excludePalaceOfTwilight{};
    ConfigVarHandle hideGameplayCursor{};
};

Settings& settings();
ModResult register_settings(ModError* error);
ModResult register_quick_menu_tab(ModError* error);
void unregister_quick_menu_tab();
void close_settings_window();

bool get_bool(ConfigVarHandle handle, bool fallback = false);
int64_t get_int(ConfigVarHandle handle, int64_t fallback = 0);

}  // namespace twilight_visuals
