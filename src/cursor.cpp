#include "cursor.hpp"

#include "runtime.hpp"

#include "helpers/string.hpp"
#include "d/d_meter2_info.h"
#include "f_ap/f_ap_game.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/ui.h"

extern const UiService* svc_ui;

namespace twilight_visuals::cursor {
namespace {
DEFINE_HOOK_SYMBOL("dusk::mouse::`anonymous namespace'::set_cursor_visible", void(bool),
    SetCursorVisible);

bool menu_visible() {
    bool hostMenu = false;
    if (svc_ui && svc_ui->is_any_document_visible) {
        svc_ui->is_any_document_visible(mod_ctx, &hostMenu);
    }
    return hostMenu || fapGmHIO_isMenu() || dMeter2Info_getWindowStatus() != 0;
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

ModResult initialize() { return mods::hook::add_pre<SetCursorVisible>(set_cursor_visible_pre); }

void update() {}

void shutdown() { mods::hook::uninstall<SetCursorVisible>(); }
}
