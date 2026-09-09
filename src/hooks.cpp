#include "hooks.hpp"

#include "runtime.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_camera_mng.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "m_Do/m_Do_graphic.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"

#include <algorithm>
#include <cstring>

namespace twilight_visuals {
namespace {
DEFINE_HOOK_SYMBOL("dusk::speedrun::resetForSpeedrunMode", void(), SpeedrunReset);
DEFINE_HOOK_SYMBOL("dusk::speedrun::restoreFromSpeedrunMode", void(), SpeedrunRestore);
DEFINE_HOOK_SYMBOL("dKyr_evil_draw", void(Mtx, u8**), EvilFogDraw);

void speedrun_reset_post(ModContext*, void*, void*, void*) { set_speedrun_suppressed(true); }
void speedrun_restore_post(ModContext*, void*, void*, void*) { set_speedrun_suppressed(false); }

void evil_fog_draw_post(ModContext*, void*, void*, void*) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || runtime_settings().style != Style::DarkHour || palace_excluded() ||
        stage == nullptr || std::strcmp(stage, "D_MN08") != 0)
    {
        return;
    }

    dKankyo_evil_Packet* packet = g_env_light.mpEvilPacket;
    camera_process_class* camera = static_cast<camera_process_class*>(dComIfGp_getCamera(0));
    if (packet == nullptr || camera == nullptr || dComIfGd_getView() == nullptr) return;

    // Detect waterfalls by their authored downward particle motion instead of a
    // finite coordinate list. This catches every Palace stream while excluding
    // stationary floor fog. Keep classification for the lifetime of the packet
    // because individual particles periodically wrap back to the top.
    static dKankyo_evil_Packet* trackedPacket = nullptr;
    static f32 previousBaseY[2048] = {};
    static bool seenEffect[2048] = {};
    static bool fallingEffect[2048] = {};
    if (trackedPacket != packet) {
        trackedPacket = packet;
        std::fill(std::begin(seenEffect), std::end(seenEffect), false);
        std::fill(std::begin(fallingEffect), std::end(fallingEffect), false);
    }

    // dKyr_evil_draw() finishes with a secondary pass that may bind a different
    // texture. Load the primary dark-fog image explicitly for this overlay.
    static TGXTexObj fogTexture;
    static ResTIMG* loadedFogImage = nullptr;
    ResTIMG* fogImage = reinterpret_cast<ResTIMG*>(packet->mpMoyaRes);
    if (fogImage == nullptr) return;
    if (loadedFogImage != fogImage) {
        if (loadedFogImage != nullptr) fogTexture.reset();
        loadedFogImage = fogImage;
        GXInitTexObj(&fogTexture, (&fogImage->format + fogImage->imageOffset),
                     fogImage->width, fogImage->height,
                     static_cast<GXTexFmt>(fogImage->format),
                     static_cast<GXTexWrapMode>(fogImage->wrapS),
                     static_cast<GXTexWrapMode>(fogImage->wrapT),
                     static_cast<GXBool>(fogImage->mipmapCount > 1));
        GXInitTexObjLOD(&fogTexture,
                        static_cast<GXTexFilter>(fogImage->minFilter),
                        static_cast<GXTexFilter>(fogImage->magFilter),
                        fogImage->minLOD * 0.125f, fogImage->maxLOD * 0.125f,
                        fogImage->LODBias * 0.01f,
                        static_cast<GXBool>(fogImage->biasClamp),
                        static_cast<GXBool>(fogImage->doEdgeLOD),
                        static_cast<GXAnisotropy>(fogImage->maxAnisotropy));
    }
    GXLoadTexObj(&fogTexture, GX_TEXMAP0);

    Mtx cameraBillboard;
    MTXInverse(dComIfGd_getView()->viewMtxNoTrans, cameraBillboard);

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_RGBA4, 8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX,
                 GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevColor(GX_TEVREG1, {34, 0, 3, 255});
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                    GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                    GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_GREATER, 0);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_DISABLE);
    GXSetCullMode(GX_CULL_NONE);

    const int effectCount = std::min<int>(g_env_light.field_0x1054, 2048);
    for (int i = 0; i < effectCount; ++i) {
        const EF_EVIL_EFF& effect = packet->mEffect[i];
        if (seenEffect[i] && effect.mBasePos.y < previousBaseY[i] - 0.5f) {
            fallingEffect[i] = true;
        }
        previousBaseY[i] = effect.mBasePos.y;
        seenEffect[i] = true;
        if (effect.mStatus == 0 || effect.field_0x38 >= 9000.0f || effect.field_0x24 <= 0.001f)
            continue;

        const cXyz center = effect.mBasePos + effect.mPosition;
        const bool roomOneWaterfall =
            (fabsf(effect.mBasePos.x + 5200.0f) < 400.0f ||
             fabsf(effect.mBasePos.x + 2700.0f) < 400.0f) &&
            (fabsf(effect.mBasePos.z - 5400.0f) < 400.0f ||
             fabsf(effect.mBasePos.z - 3200.0f) < 400.0f);
        const bool roomElevenWaterfall = fabsf(effect.mBasePos.x) < 1300.0f &&
                                             fabsf(effect.mBasePos.z + 2828.0f) < 400.0f;
        if (!fallingEffect[i] && !roomOneWaterfall && !roomElevenWaterfall) continue;
        const f32 distance = camera->view.lookat.eye.abs(center);
        const f32 distanceFade = std::clamp((distance - 50.0f) / 750.0f, 0.0f, 1.0f);
        const f32 size = effect.field_0x38 * 1.8f * distanceFade;
        if (size <= 0.01f) continue;

        Vec corners[4] = {
            {-size, size, 0.0f}, {size, size, 0.0f},
            {size, -size, 0.0f}, {-size, -size, 0.0f},
        };
        cXyz world[4];
        for (int corner = 0; corner < 4; ++corner) {
            Vec rotated;
            MTXMultVec(cameraBillboard, &corners[corner], &rotated);
            world[corner] = cXyz(center.x + rotated.x, center.y + rotated.y,
                                 center.z + rotated.z);
        }

        const u8 alpha = static_cast<u8>(std::clamp(effect.field_0x24 * distanceFade * 150.0f,
                                                     0.0f, 150.0f));
        const GXColor bloodFog = {76, 0, 7, alpha};
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(world[0].x, world[0].y, world[0].z); GXColor4u8(bloodFog.r, bloodFog.g, bloodFog.b, bloodFog.a); GXTexCoord2s16(0, 0);
        GXPosition3f32(world[1].x, world[1].y, world[1].z); GXColor4u8(bloodFog.r, bloodFog.g, bloodFog.b, bloodFog.a); GXTexCoord2s16(0xFF, 0);
        GXPosition3f32(world[2].x, world[2].y, world[2].z); GXColor4u8(bloodFog.r, bloodFog.g, bloodFog.b, bloodFog.a); GXTexCoord2s16(0xFF, 0xFF);
        GXPosition3f32(world[3].x, world[3].y, world[3].z); GXColor4u8(bloodFog.r, bloodFog.g, bloodFog.b, bloodFog.a); GXTexCoord2s16(0, 0xFF);
        GXEnd();
    }
    J3DShape::resetVcdVatCache();
}
}  // namespace

ModResult install_hooks() {
    ModResult result = mods::hook::add_post<SpeedrunReset>(speedrun_reset_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<SpeedrunRestore>(speedrun_restore_post);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<EvilFogDraw>(evil_fog_draw_post);
}

void uninstall_hooks() {
    mods::hook::uninstall<EvilFogDraw>();
    mods::hook::uninstall<SpeedrunRestore>();
    mods::hook::uninstall<SpeedrunReset>();
}

}  // namespace twilight_visuals
