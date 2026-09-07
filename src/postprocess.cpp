#include "postprocess.hpp"
#include "runtime.hpp"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "SSystem/SComponent/c_m3d.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace twilight_visuals::postprocess {
namespace {
DEFINE_HOOK(&mDoGph_gInf_c::bloom_c::draw, BloomDraw);

void draw_astral_chromatic_aberration() {
    const auto& cfg = runtime_settings();
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || cfg.style != Style::AstralPlane || stage == nullptr ||
        palace_excluded()) return;
    const f32 strength = std::clamp(cfg.chromaticAberration, 0, 200) / 100.0f;
    const u16 width = mDoGph_gInf_c::getWidth();
    const u16 height = mDoGph_gInf_c::getHeight();
    if (strength == 0.0f || width == 0 || height == 0) return;

    static std::vector<u8> pixels;
    pixels.resize(GXGetTexBufferSize(width, height, GX_TF_RGBA8, GX_FALSE, 0));
    GXSetTexCopySrc(0, 0, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(pixels.data(), GX_FALSE);
    GXPixModeSync();
    GXInvalidateTexAll();
    TGXTexObj scene;
    GXInitTexObj(&scene, pixels.data(), width, height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&scene, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    j3dSys.reinitGX();
    GXLoadTexObj(&scene, GX_TEXMAP0);
    GXSetViewport(0, 0, width, height, 0, 1);
    GXSetScissor(0, 0, width, height);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetZCompLoc(GX_TRUE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetColorUpdate(GX_TRUE);
    GXSetFog(GX_FOG_NONE, 0, 0, 0, 0, g_clearColor);
    GXSetCullMode(GX_CULL_NONE);
    Mtx44 ortho;
    C_MTXOrtho(ortho, 0, 1, 0, 1, 0, 10);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(cMtx_getIdentity(), 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    const GXColor masks[] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};
    for (int channel = 0; channel < 3; ++channel) {
        const f32 inset = (channel - 1) * strength * 0.006f;
        GXSetTevColor(GX_TEVREG0, masks[channel]);
        GXSetBlendMode(channel == 0 ? GX_BM_NONE : GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_COPY);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0, 0, 0); GXTexCoord2f32(inset, inset);
        GXPosition3f32(1, 0, 0); GXTexCoord2f32(1 - inset, inset);
        GXPosition3f32(1, 1, 0); GXTexCoord2f32(1 - inset, 1 - inset);
        GXPosition3f32(0, 1, 0); GXTexCoord2f32(inset, 1 - inset);
        GXEnd();
    }
    GXSetAlphaUpdate(GX_TRUE);
    j3dSys.reinitGX();
    J3DShape::resetVcdVatCache();
}

void bloom_draw_post(ModContext*, void*, void*, void*) { draw_astral_chromatic_aberration(); }
}

ModResult install_hooks() { return mods::hook::add_post<BloomDraw>(bloom_draw_post); }
void uninstall_hooks() { mods::hook::uninstall<BloomDraw>(); }
}
