#include "settings.hpp"
#include "runtime.hpp"
#include "hooks.hpp"
#include "environment.hpp"
#include "postprocess.hpp"
#include "particles.hpp"
#include "music.hpp"
#include "compat.hpp"
#include "geometry.hpp"
#include "boundary.hpp"
#include "running.hpp"
#include "sequencing.hpp"
#include "sky.hpp"
#include "cursor.hpp"
#include <cstdio>

#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ResourceService, svc_resource);

extern "C" {
MOD_EXPORT ModResult mod_shutdown(ModError*);

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    const auto* host = twilight_visuals::compat::host_api();
    if (!host || host->structSize < sizeof(DuskTwilightHostApiV1) ||
        !(host->capabilities & DUSK_TWILIGHT_HOST_CAP_GEOMETRY_HOOKS) ||
        !(host->capabilities & DUSK_TWILIGHT_HOST_CAP_AUDIO_HOOKS) ||
        !host->setGeometryHooks || !host->setAudioHooks || !host->setEnvironmentHooks ||
        !host->setPlayerHooks || !host->setSequenceHooks || !host->interpolationStep ||
        !host->interpolationEnabled || !host->simulationFrame || !host->recordMatrix || !host->lookupMatrix || !host->audioManager) {
        if (error) {
            error->code = MOD_UNSUPPORTED;
            std::snprintf(error->message, sizeof(error->message),
                "Twilight Visuals requires MFB hook ABI revision 4. Use the matching rebuilt host.");
        }
        return MOD_UNSUPPORTED;
    }
    ModResult result = twilight_visuals::register_settings(error);
    if (result != MOD_OK) return result;

    result = twilight_visuals::register_quick_menu_tab(error);
    if (result != MOD_OK) return result;

    twilight_visuals::compat::set_enemy_proc_provider(&twilight_visuals::provide_enemy_proc);
    twilight_visuals::compat::set_bloom_provider(&twilight_visuals::provide_bloom_profile);
    twilight_visuals::compat::set_scene_music_provider(&twilight_visuals::provide_scene_music);
    twilight_visuals::refresh_runtime_settings();

    twilight_visuals::geometry::initialize();
    twilight_visuals::boundary::initialize();
    twilight_visuals::running::initialize();
    twilight_visuals::sequencing::initialize();

    result = twilight_visuals::cursor::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Mouse cursor hook unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::music::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Astral music resources unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Speedrun integration hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::environment::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Environment hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::postprocess::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Post-processing hook unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::particles::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Particle hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    svc_log->info(mod_ctx, "Twilight Visuals mod initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    twilight_visuals::refresh_runtime_settings();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    twilight_visuals::cursor::shutdown();
    twilight_visuals::sky::shutdown();
    twilight_visuals::running::shutdown();
    twilight_visuals::sequencing::shutdown();
    twilight_visuals::geometry::shutdown();
    twilight_visuals::compat::set_enemy_proc_provider(nullptr);
    twilight_visuals::compat::set_bloom_provider(nullptr);
    twilight_visuals::compat::set_scene_music_provider(nullptr);
    twilight_visuals::close_settings_window();
    twilight_visuals::unregister_quick_menu_tab();
    twilight_visuals::particles::uninstall_hooks();
    twilight_visuals::postprocess::uninstall_hooks();
    twilight_visuals::environment::uninstall_hooks();
    twilight_visuals::boundary::shutdown();
    twilight_visuals::uninstall_hooks();
    twilight_visuals::music::shutdown();
    svc_log->info(mod_ctx, "Twilight Visuals mod unloaded");
    return MOD_OK;
}

}
