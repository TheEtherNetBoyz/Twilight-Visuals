#include "settings.hpp"

#include "mods/service.hpp"
#include "mods/svc/ui.h"

#include <array>

IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

namespace twilight_visuals {
namespace {
Settings g_settings;
UiWindowHandle g_settingsWindow{};
UiMenuTabHandle g_quickMenuTab{};

constexpr std::array<const char*, 4> kStyles{
    "Normal Twilight", "Black and White", "Astral Plane", "The Dark Hour"};
constexpr std::array<const char*, 17> kSkyboxes{
    "Twilight Day", "Twilight Night", "Sunrise", "Sunset", "Overcast / Storm",
    "Faron Twilight", "Eldin Twilight", "Lanayru Twilight", "Palace of Twilight",
    "Sacred Grove", "Snowpeak", "Gerudo Desert", "Lake Hylia", "Fishing Hole",
    "Ordon", "Hyrule Field", "Castle Town"};
constexpr std::array<const char*, 8> kWeather{
    "Current", "Clear", "Rain", "Snow", "Lightning", "Wind Storm", "Snow Storm",
    "Heavy Fog"};

ModResult register_bool(const char* name, bool defaultValue, ConfigVarHandle& out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = defaultValue;
    return svc_config->register_var(mod_ctx, &desc, &out);
}

ModResult register_int(
    const char* name, int64_t defaultValue, ConfigVarHandle& out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = defaultValue;
    return svc_config->register_var(mod_ctx, &desc, &out);
}

void add_control(UiElementHandle pane, UiControlDesc& control) {
    svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
}

void add_toggle(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    add_control(pane, control);
}

template <size_t N>
void add_select(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var,
    const std::array<const char*, N>& options) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_SELECT;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.options = options.data();
    control.option_count = options.size();
    add_control(pane, control);
}

void add_number(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var,
    int64_t min, int64_t max, int64_t step, const char* suffix) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.min = min;
    control.max = max;
    control.step = step;
    control.suffix = suffix;
    add_control(pane, control);
}

ModResult build_settings_tab(ModContext*, UiWindowHandle, UiElementHandle left,
    UiElementHandle right, void*, ModError*) {
    (void)right;
    svc_ui->pane_add_section(mod_ctx, left, "Twilight Visuals");
    add_toggle(left, "Enable Twilight Visuals",
        "Apply Twilight visuals, particles, enemy variants, and the selected style anywhere.",
        g_settings.enabled);
    add_select(left, "Visual Style & Music",
        "Select the complete environment style and its matching music. Normal Twilight and Black "
        "and White use Palace music; Astral Plane and The Dark Hour use their matching tracks.",
        g_settings.style,
        kStyles);
    add_number(left, "Brightness",
        "Adjust complete environment lighting and bloom intensity.", g_settings.brightness, 0,
        120, 5, "%");
    add_number(left, "Astral Chromatic Aberration",
        "Adjust Astral Plane red/blue edge separation.", g_settings.chromaticAberration, 0, 200,
        5, "%");
    add_select(left, "Skybox", "Choose the authored sky palette used by Twilight Visuals.",
        g_settings.skybox, kSkyboxes);
    add_toggle(left, "Exclude Palace of Twilight",
        "Keep the Palace of Twilight's native visuals and music instead of applying the selected "
        "Twilight Visuals preset.",
        g_settings.excludePalaceOfTwilight);

    svc_ui->pane_add_section(mod_ctx, left, "Music");
    add_number(left, "Custom Music Volume",
        "Adjust Astral Plane and Dark Hour replacement music volume.",
        g_settings.musicVolume, 0, 100, 5, "%");
    add_toggle(left, "Override Temple Music",
        "Use the selected visual preset's music in temples and dungeons. Boss and miniboss "
        "themes remain unchanged.",
        g_settings.overrideTempleMusic);

    svc_ui->pane_add_section(mod_ctx, left, "Weather");
    add_select(left, "Weather", "Override weather independently from the visual toggle.",
        g_settings.weather, kWeather);

    svc_ui->pane_add_section(mod_ctx, left, "Movement");
    add_toggle(left, "Skyward Sword Running",
        "Hold A while moving as human Link to run at 37 units. Includes the custom attack, roll, "
        "snow, and Magic Armor water-running behavior.",
        g_settings.skywardSwordRunning);
    add_toggle(left, "Wolf Senses as Human",
        "Press D-pad Down as human Link to toggle Wolf Senses after the ability has been unlocked.",
        g_settings.humanWolfSenses);
    return MOD_OK;
}

void settings_window_closed(ModContext*, UiWindowHandle, void*) { g_settingsWindow = 0; }

void open_settings_window(ModContext*, void*) {
    if (g_settingsWindow != 0) return;
    UiTabDesc tabs[1] = {UI_TAB_DESC_INIT};
    tabs[0].title = "Twilight Visuals";
    tabs[0].build = build_settings_tab;
    UiWindowDesc window = UI_WINDOW_DESC_INIT;
    window.tabs = tabs;
    window.tab_count = 1;
    window.on_closed = settings_window_closed;
    svc_ui->window_push(mod_ctx, &window, &g_settingsWindow);
}

}  // namespace

Settings& settings() { return g_settings; }

bool get_bool(ConfigVarHandle handle, bool fallback) {
    bool value = fallback;
    return handle != 0 && svc_config->get_bool(mod_ctx, handle, &value) == MOD_OK ? value : fallback;
}

int64_t get_int(ConfigVarHandle handle, int64_t fallback) {
    int64_t value = fallback;
    return handle != 0 && svc_config->get_int(mod_ctx, handle, &value) == MOD_OK ? value : fallback;
}

ModResult register_settings(ModError*) {
    ModResult result = register_bool("twilight-visuals", false, g_settings.enabled);
    if (result != MOD_OK) return result;
    result = register_int("visual-style", 0, g_settings.style);
    if (result != MOD_OK) return result;
    result = register_int("brightness", 100, g_settings.brightness);
    if (result != MOD_OK) return result;
    result = register_int("chromatic-aberration", 80, g_settings.chromaticAberration);
    if (result != MOD_OK) return result;
    result = register_int("skybox", 0, g_settings.skybox);
    if (result != MOD_OK) return result;
    result = register_int("weather", 0, g_settings.weather);
    if (result != MOD_OK) return result;
    result = register_int("music-volume", 100, g_settings.musicVolume);
    if (result != MOD_OK) return result;
    result = register_bool("override-temple-music", false, g_settings.overrideTempleMusic);
    if (result != MOD_OK) return result;
    result = register_bool("skyward-sword-running", false, g_settings.skywardSwordRunning);
    if (result != MOD_OK) return result;
    result = register_bool("human-wolf-senses", false, g_settings.humanWolfSenses);
    if (result != MOD_OK) return result;
    return register_bool("exclude-palace-of-twilight", true,
        g_settings.excludePalaceOfTwilight);
}

ModResult register_quick_menu_tab(ModError*) {
    UiMenuTabDesc tab = UI_MENU_TAB_DESC_INIT;
    tab.label = "Twilight Visuals";
    tab.on_selected = open_settings_window;
    return svc_ui->register_menu_tab(mod_ctx, &tab, &g_quickMenuTab);
}

void unregister_quick_menu_tab() {
    if (g_quickMenuTab != 0) {
        svc_ui->unregister_menu_tab(mod_ctx, g_quickMenuTab);
        g_quickMenuTab = 0;
    }
}

void close_settings_window() {
    if (g_settingsWindow != 0) {
        svc_ui->window_close(mod_ctx, g_settingsWindow);
        g_settingsWindow = 0;
    }
}

}  // namespace twilight_visuals
