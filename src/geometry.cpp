#include "geometry.hpp"
#include "monochrome.hpp"
#include "runtime.hpp"
#include "compat.hpp"
#include "d/d_com_inf_game.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "SSystem/SComponent/c_math.h"
#include <algorithm>
#include <vector>
#include <utility>
#include <cstddef>
#include <cstring>

namespace twilight_visuals::geometry {
namespace {
alignas(32) static u8 s_grassMonochromeKusaTexture[0x800];
alignas(32) static u8 s_grassMonochromeHijikiTexture[0x800];
static TGXTexObj s_grassMonochromeKusaTexObj;
static TGXTexObj s_grassMonochromeHijikiTexObj;


bool grass_active() {
    bool enabled = false;
    provide_grass(&enabled);
    return enabled;
}
static u8 grass_luminance(u8 r, u8 g, u8 b) {
    return static_cast<u8>((static_cast<u32>(r) * 77 + static_cast<u32>(g) * 150 +
                            static_cast<u32>(b) * 29) >> 8);
}

static void grass_monochrome_color(GXColor& color) {
    const u8 luminance = grass_luminance(color.r, color.g, color.b);
    color.r = luminance;
    color.g = luminance;
    color.b = luminance;
}

static void grass_monochrome_color(GXColorS10& color) {
    const s32 luminance = (static_cast<s32>(color.r) * 77 + static_cast<s32>(color.g) * 150 +
                           static_cast<s32>(color.b) * 29) >> 8;
    const s16 clamped = static_cast<s16>(std::clamp(luminance, -1024, 1023));
    color.r = clamped;
    color.g = clamped;
    color.b = clamped;
}

static void make_grass_monochrome_texture(u8* dst, const u8* src, size_t size) {
    for (size_t i = 0; i < size; i += 2) {
        const u16 pixel = static_cast<u16>((src[i] << 8) | src[i + 1]);
        u16 monochrome;
        if ((pixel & 0x8000) != 0) {
            const u8 r = static_cast<u8>(((pixel >> 10) & 0x1F) * 255 / 31);
            const u8 g = static_cast<u8>(((pixel >> 5) & 0x1F) * 255 / 31);
            const u8 b = static_cast<u8>((pixel & 0x1F) * 255 / 31);
            const u16 gray = static_cast<u16>(grass_luminance(r, g, b) * 31 / 255);
            monochrome = static_cast<u16>(0x8000 | (gray << 10) | (gray << 5) | gray);
        } else {
            const u16 alpha = pixel & 0x7000;
            const u8 r = static_cast<u8>(((pixel >> 8) & 0xF) * 17);
            const u8 g = static_cast<u8>(((pixel >> 4) & 0xF) * 17);
            const u8 b = static_cast<u8>((pixel & 0xF) * 17);
            const u16 gray = static_cast<u16>((grass_luminance(r, g, b) + 8) / 17);
            monochrome = static_cast<u16>(alpha | (gray << 8) | (gray << 4) | gray);
        }
        dst[i] = static_cast<u8>(monochrome >> 8);
        dst[i + 1] = static_cast<u8>(monochrome);
    }
}


void grass_color(void* raw, bool signedChannels) {
    if (!raw || !grass_active()) return;
    if (signedChannels) grass_monochrome_color(*static_cast<GXColorS10*>(raw));
    else grass_monochrome_color(*static_cast<GXColor*>(raw));
}
void* grass_texture(void* original, const u8* pixels, bool first) {
    if (!pixels || !grass_active()) return original;
    static bool ready[2]{};
    const unsigned slot = first ? 0 : 1;
    auto* bytes = first ? s_grassMonochromeKusaTexture : s_grassMonochromeHijikiTexture;
    auto* texture = first ? &s_grassMonochromeKusaTexObj : &s_grassMonochromeHijikiTexObj;
    if (!ready[slot]) {
        make_grass_monochrome_texture(bytes, pixels, 0x800);
        GXInitTexObj(texture, bytes, 32, 32, GX_TF_RGB5A3, GX_REPEAT, GX_CLAMP, GX_FALSE);
        ready[slot] = true;
    }
    return static_cast<GXTexObj*>(texture);
}
bool grass_lighting() {
    if (!grass_active()) return false;
    GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    return true;
}
using FogRestore = std::vector<std::pair<J3DFogInfo*, J3DFogInfo>>;
f32 bloom_gain() {
    return active() ? std::clamp(runtime_settings().brightness, 0.0f, 1.2f) : 1.0f;
}
void after_background(void* viewRaw, void* viewportRaw) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || runtime_settings().style != Style::BlackAndWhite ||
        !stage || palace_excluded()) return;
    auto* view = static_cast<view_class*>(viewRaw);
    auto* viewport = static_cast<view_port_class*>(viewportRaw);
    draw_monochrome_background();
    j3dSys.reinitGX();
    j3dSys.setViewMtx(view->viewMtx);
    GXSetProjection(view->projMtx, GX_PERSPECTIVE);
    GXSetViewport(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height,
                  viewport->near_z, viewport->far_z);
    GXSetScissor(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height);
}
void* before_model(void* modelRaw, void* lightingRaw) {
    if (!active() || runtime_settings().style != Style::AstralPlane ||
        !modelRaw || !lightingRaw) return nullptr;
    auto* model = static_cast<J3DModelData*>(modelRaw);
    auto* lighting = static_cast<dKy_tevstr_c*>(lightingRaw);
    auto* saved = new (std::nothrow) FogRestore;
    if (!saved) return nullptr;
    for (u16 i = 0; i < model->getMaterialNum(); ++i) {
        auto* material = model->getMaterialNodePointer(i);
        if (!material || !material->getFog()) continue;
        auto* fog = material->getFog()->getFogInfo();
        if (!fog || fog->mType != 0) continue;
        saved->emplace_back(fog, *fog);
        fog->mType = 2;
        fog->mStartZ = lighting->mFogStartZ;
        fog->mEndZ = lighting->mFogEndZ;
        fog->mColor = {static_cast<u8>(lighting->FogCol.r), static_cast<u8>(lighting->FogCol.g),
                       static_cast<u8>(lighting->FogCol.b), 255};
        if (auto* view = dComIfGd_getView()) {
            fog->mNearZ = view->near_;
            fog->mFarZ = view->far_;
        }
    }
    return saved;
}
void after_model(void* token) {
    auto* saved = static_cast<FogRestore*>(token);
    if (!saved) return;
    for (auto& entry : *saved) *entry.first = entry.second;
    delete saved;
}
void particle(cXyz* corners, cXyz* position, void* rawColor, u32 j, f32 presentationCounter) {
    if (!active() || runtime_settings().style != Style::AstralPlane) return;
    auto& color = *static_cast<GXColor*>(rawColor);
    const u32 seed = (j + 1u) * 2654435761u;
    const f32 width = 0.3f + (seed & 255u) / 255.0f;
    const f32 height = 0.4f + ((seed >> 8) & 255u) / 100.0f;
    for (int k = 0; k < 4; ++k) {
        corners[k].x *= width;
        corners[k].y *= height;
    }
    if (j % 3 == 0) corners[3] = corners[2];
    else corners[1].x *= 0.2f;
    const f32 phase = (seed & 65535u) * (6.2831853f / 65536.0f);
    const s16 angle = static_cast<s16>(32767.0f * cM_fsin(
        phase + presentationCounter * (0.0045f + (j % 89) * 0.0001f)));
    const f32 sine = cM_ssin(angle), cosine = cM_scos(angle);
    for (int k = 0; k < 4; ++k) {
        const f32 x = corners[k].x;
        corners[k].x = x * cosine - corners[k].y * sine;
        corners[k].y = x * sine + corners[k].y * cosine;
    }
    position->x += 22.0f * cM_fsin(phase + presentationCounter * (0.0037f + (j % 61) * 0.0001f));
    position->z += 18.0f * cM_fcos(phase + presentationCounter * (0.0029f + (j % 47) * 0.0001f));
    color.r = j % 7 == 0 ? 130 : 10;
    color.g = j % 7 == 0 ? 38 : 17;
    color.b = j % 7 == 0 ? 52 : 29;
}
}
void transform_particle(cXyz* corners, cXyz* position, void* color, u32 index, f32 time) {
    particle(corners, position, color, index, time);
}
bool celestial_visibility(DuskCelestialVisibility point, bool nativeValue) {
    if (!active() || runtime_settings().style != Style::DarkHour) return nativeValue;
    return point == DuskCelestial_ForceMoon || point == DuskCelestial_DrawMoon;
}
void celestial_parameter(DuskCelestialParameter point, void* value) {
    if (!active() || runtime_settings().style != Style::DarkHour) return;
    if (point == DuskCelestial_MoonAlpha) *static_cast<f32*>(value) = 1.0f;
    if (point == DuskCelestial_MoonPosition) {
        auto& position = *static_cast<DuskCelestialPositionV1*>(value);
        position.relative->set(-30000.0f, 45000.0f, -65000.0f);
        *position.world = *position.eye + *position.relative;
    }
    if (point == DuskCelestial_MoonPhase) *static_cast<int*>(value) = 0;
    if (point == DuskCelestial_MoonSize) *static_cast<f32*>(value) *= 4.0f;
}
bool initialize() {
    const auto* api = compat::host_api();
    if (!api || !(api->capabilities & DUSK_TWILIGHT_HOST_CAP_GEOMETRY_HOOKS) ||
        api->structSize < offsetof(DuskTwilightHostApiV1, setGeometryHooks) + sizeof(api->setGeometryHooks) ||
        !api->setGeometryHooks) return false;
    const DuskGeometryHooksV1 hooks{grass_color, grass_texture, grass_lighting,
        before_model, after_model, bloom_gain, after_background,
        celestial_visibility, celestial_parameter};
    api->setGeometryHooks(&hooks);
    return true;
}
void shutdown() {
    const auto* api = compat::host_api();
    if (api && (api->capabilities & DUSK_TWILIGHT_HOST_CAP_GEOMETRY_HOOKS) &&
        api->structSize >= offsetof(DuskTwilightHostApiV1, setGeometryHooks) + sizeof(api->setGeometryHooks) &&
        api->setGeometryHooks) api->setGeometryHooks(nullptr);
}
}
