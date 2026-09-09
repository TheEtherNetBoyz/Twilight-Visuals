#include "cursor.hpp"

#include "runtime.hpp"

#include "helpers/string.hpp"
#include "d/d_meter2_info.h"
#include "f_ap/f_ap_game.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/ui.h"

#include <cstring>

extern const UiService* svc_ui;
extern const HookService* svc_hook;

namespace twilight_visuals::cursor {
namespace {
DEFINE_HOOK_SYMBOL("dusk::mouse::`anonymous namespace'::set_cursor_visible", void(bool),
    SetCursorVisible);
DEFINE_HOOK_SYMBOL("ImGui::Begin", bool(const char*, bool*, int), ImGuiBegin);
DEFINE_HOOK_SYMBOL("dusk::mouse::read", void(), MouseRead);
using VisibilityQuery = bool (*)();
VisibilityQuery s_imguiWindowsVisible{};
bool s_sawInputViewer{};
bool s_sawOtherImGuiWindow{};

HookAction imgui_begin_pre(ModContext*, void* args, void*, void*) {
    const char* name = mods::arg<const char*>(args, 0);
    if (name != nullptr && std::strcmp(name, "Input Viewer") == 0) {
        s_sawInputViewer = true;
    } else if (name != nullptr) {
        // ImGui creates internal/default windows and passive Toast/Pipelines overlays that
        // must not turn the cursor back on. Count only Dusklight's user-facing tool windows.
        static constexpr const char* interactiveWindows[] = {
            "Actor Spawner", "Practice Tools", "Heaps", "Audio Debug", "Camera Debug",
            "Debug Overlay", "Player Info", "Input Macro", "Bloom", "TAS Movie",
            "State Manager", "Save Editor", "Processes", "Stub log",
        };
        for (const char* interactive : interactiveWindows) {
            if (std::strcmp(name, interactive) == 0) {
                s_sawOtherImGuiWindow = true;
                break;
            }
        }
    }
    return HOOK_CONTINUE;
}

void mouse_read_post(ModContext*, void*, void*, void*) {
    // ImGui windows are drawn after the mouse update, so retain one completed frame of
    // classification for set_cursor_visible() and then collect the next frame from scratch.
    s_sawInputViewer = false;
    s_sawOtherImGuiWindow = false;
}

bool menu_visible() {
    bool hostMenu = false;
    if (svc_ui && svc_ui->is_any_document_visible) {
        svc_ui->is_any_document_visible(mod_ctx, &hostMenu);
    }
    const bool onlyInputViewer = s_sawInputViewer && !s_sawOtherImGuiWindow;
    const bool imguiMenu = s_imguiWindowsVisible != nullptr && s_imguiWindowsVisible() &&
                           !onlyInputViewer;
    return hostMenu || imguiMenu || fapGmHIO_isMenu() || dMeter2Info_getWindowStatus() != 0;
}

HookAction set_cursor_visible_pre(ModContext*, void* args, void*, void*) {
    if (runtime_settings().hideGameplayCursor) {
        // Let the host perform the state change. Its implementation updates both SDL and
        // ImGuiConfigFlags_NoMouseCursorChange, so ImGui cannot show the cursor again later
        // in the frame. Menus explicitly request a visible cursor.
        mods::arg_ref<bool>(args, 0) = menu_visible();
    }
    return HOOK_CONTINUE;
}
}

ModResult initialize() {
    void* queryAddress = nullptr;
    HookSymbolFlags flags{};
    if (svc_hook != nullptr &&
        svc_hook->resolve(mod_ctx,
            "dusk::mouse::`anonymous namespace'::imgui_windows_visible",
            &queryAddress, &flags) == MOD_OK &&
        (static_cast<u32>(flags) & HOOK_SYMBOL_CODE) != 0) {
        s_imguiWindowsVisible = reinterpret_cast<VisibilityQuery>(queryAddress);
    }
    ModResult result = mods::hook::add_pre<ImGuiBegin>(imgui_begin_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<MouseRead>(mouse_read_post);
    if (result != MOD_OK) {
        mods::hook::uninstall<ImGuiBegin>();
        return result;
    }
    result = mods::hook::add_pre<SetCursorVisible>(set_cursor_visible_pre);
    if (result != MOD_OK) {
        mods::hook::uninstall<MouseRead>();
        mods::hook::uninstall<ImGuiBegin>();
    }
    return result;
}

void update() {}

void shutdown() {
    mods::hook::uninstall<SetCursorVisible>();
    mods::hook::uninstall<MouseRead>();
    mods::hook::uninstall<ImGuiBegin>();
    s_imguiWindowsVisible = nullptr;
    s_sawInputViewer = false;
    s_sawOtherImGuiWindow = false;
}
}
