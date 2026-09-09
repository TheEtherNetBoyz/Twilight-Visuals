#include "blood.hpp"
#include "runtime.hpp"
#include "d/d_kankyo_wether.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
#include <algorithm>
#include <chrono>
#include <cstring>
namespace twilight_visuals::blood {
namespace {
bool dark_hour_moon_enabled() { return active() && runtime_settings().style == Style::DarkHour; }
struct DarkHourBloodMark {
    cXyz position;
    f32 radius[32];
    f32 midGround[32];
    f32 rimGround[32];
    cXyz surfaceNormal;
    f32 surfaceD;
    f32 extent;
    f32 rotation;
    f32 sheenAngle;
    bool active;
};

struct DarkHourBloodFootprint {
    cXyz position;
    cXyz surfaceNormal;
    f32 surfaceD;
    f32 rotation;
    u16 life;
    bool active;
};

class DarkHourBloodPacket : public J3DPacket {
public:
    void draw() override {
        if (!dark_hour_moon_enabled()) return;

        j3dSys.reinitGX();
        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
                     GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_DISABLE);
        GXSetZCompLoc(GX_TRUE);
        GXSetCullMode(GX_CULL_NONE);
        GXSetAlphaCompare(GX_GREATER, 4, GX_AOP_AND, GX_ALWAYS, 0);
        GXSetNumIndStages(0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);

        // Reapply the scene fog after reinitializing GX. Without this, blood
        // remained fully saturated at long range and appeared on top of the
        // Dark Hour distance fog instead of receding with the floor beneath it.
        dKy_GxFog_set();

        for (const DarkHourBloodMark& mark : marks) {
            if (!mark.active) continue;
            const auto groundAt = [&mark](int sample, f32 radialScale,
                                          f32 offsetX, f32 offsetZ) {
                const f32 centerGround = mark.position.y;
                f32 grounded;
                if (radialScale <= 0.5f) {
                    grounded = centerGround +
                        (mark.midGround[sample] - centerGround) * (radialScale * 2.0f);
                } else {
                    grounded = mark.midGround[sample] +
                        (mark.rimGround[sample] - mark.midGround[sample]) *
                        ((radialScale - 0.5f) * 2.0f);
                }
                // Offset decorative lobes along the center collision plane while retaining
                // their sampled radial terrain profile.
                if (fabsf(mark.surfaceNormal.y) > 0.001f) {
                    grounded += (-mark.surfaceNormal.x * offsetX -
                                 mark.surfaceNormal.z * offsetZ) / mark.surfaceNormal.y;
                }
                return grounded + 0.8f;
            };
            const auto drawPool = [&mark, &groundAt](f32 scale, f32 offsetX, f32 offsetZ,
                                                     f32 height, GXColor center, GXColor rim) {
                const f32 centerX = mark.position.x + offsetX;
                const f32 centerZ = mark.position.z + offsetZ;
                f32 centerY = mark.position.y;
                if (fabsf(mark.surfaceNormal.y) > 0.001f) {
                    centerY += (-mark.surfaceNormal.x * offsetX -
                                mark.surfaceNormal.z * offsetZ) / mark.surfaceNormal.y;
                }
                const auto emit = [&](int sample, f32 radialScale, GXColor color) {
                    const f32 angle = mark.rotation + sample * 0.19634954f;
                    const f32 x = centerX + sinf(angle) * mark.radius[sample] * radialScale;
                    const f32 z = centerZ + cosf(angle) * mark.radius[sample] * radialScale;
                    GXPosition3f32(x, groundAt(sample, radialScale, offsetX, offsetZ) + height, z);
                    GXColor4u8(color.r, color.g, color.b, color.a);
                };
                // Two radial bands follow collision sampled halfway out and at the rim.
                // A single center fan bridges over angled triangles and clips into convex floors.
                GXBegin(GX_TRIANGLES, GX_VTXFMT0, 32 * 9);
                for (int sample = 0; sample < 32; ++sample) {
                    const int next = (sample + 1) & 31;
                    GXPosition3f32(centerX, centerY + 0.8f + height, centerZ);
                    GXColor4u8(center.r, center.g, center.b, center.a);
                    emit(sample, scale * 0.5f, center);
                    emit(next, scale * 0.5f, center);

                    emit(sample, scale * 0.5f, center);
                    emit(sample, scale, rim);
                    emit(next, scale, rim);

                    emit(sample, scale * 0.5f, center);
                    emit(next, scale, rim);
                    emit(next, scale * 0.5f, center);
                }
                GXEnd();
            };

            const auto surfaceY = [&mark, &groundAt](f32 x, f32 z) {
                const f32 dx = x - mark.position.x;
                const f32 dz = z - mark.position.z;
                const f32 angle = atan2f(dx, dz) - mark.rotation;
                const int sample = static_cast<int>(floorf(angle / 0.19634954f + 0.5f)) & 31;
                const f32 radial = sqrtf(dx * dx + dz * dz) /
                                   std::max(mark.radius[sample], 1.0f);
                return groundAt(sample, std::clamp(radial, 0.0f, 1.0f), 0.0f, 0.0f);
            };
            const auto drawMottle = [&mark, &surfaceY](f32 x, f32 z, f32 radiusX,
                                                       f32 radiusZ, f32 rotation,
                                                       GXColor color) {
                GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 11);
                GXPosition3f32(x, surfaceY(x, z) + 0.62f, z);
                GXColor4u8(color.r, color.g, color.b, color.a);
                for (int point = 0; point <= 9; ++point) {
                    const f32 angle = point * 0.6981317f;
                    const f32 localX = cosf(angle) * radiusX;
                    const f32 localZ = sinf(angle) * radiusZ;
                    const f32 edgeX = x + cosf(rotation) * localX - sinf(rotation) * localZ;
                    const f32 edgeZ = z + sinf(rotation) * localX + cosf(rotation) * localZ;
                    GXPosition3f32(edgeX, surfaceY(edgeX, edgeZ) + 0.6f, edgeZ);
                    GXColor4u8(color.r, color.g, color.b, 0);
                }
                GXEnd();
            };

            // Larger spills hold more liquid and are slightly denser. A narrow,
            // irregular near-black perimeter reads as a drying/coagulated edge.
            const f32 sizeOpacity = std::clamp((mark.extent - 220.0f) / 430.0f, 0.0f, 1.0f);
            const u8 bodyAlpha = static_cast<u8>(135.0f + sizeOpacity * 34.0f);
            const u8 centerAlpha = static_cast<u8>(66.0f + sizeOpacity * 22.0f);
            drawPool(1.0f, 0.0f, 0.0f, 0.0f, {14, 0, 2, 205}, {14, 0, 2, 205});
            drawPool(0.925f, -mark.radius[4] * 0.008f, mark.radius[20] * 0.006f,
                     0.14f, {78, 2, 9, bodyAlpha}, {78, 2, 9, bodyAlpha});
            drawPool(0.52f, mark.radius[10] * 0.07f, -mark.radius[26] * 0.04f,
                     0.32f, {42, 0, 5, centerAlpha}, {42, 0, 5, centerAlpha});

            // Soft deterministic mottling replaces a tiled texture. It breaks up
            // the flat fill while keeping boundaries diffuse and line-free.
            for (int patch = 0; patch < 12; ++patch) {
                const f32 seed = mark.rotation * (patch + 1) + mark.sheenAngle * 1.7f;
                const f32 angle = seed + patch * 2.3999632f;
                const f32 distance = mark.extent * (0.10f + 0.35f *
                    (0.5f + 0.5f * sinf(seed * 2.31f)));
                const f32 x = mark.position.x + sinf(angle) * distance;
                const f32 z = mark.position.z + cosf(angle) * distance;
                const f32 patchSize = mark.extent * (0.065f + 0.035f *
                    (0.5f + 0.5f * cosf(seed * 1.63f)));
                const bool darkPatch = (patch & 1) == 0;
                drawMottle(x, z, patchSize * 1.7f, patchSize, angle,
                           darkPatch ? GXColor{18, 0, 2, 72} : GXColor{145, 8, 16, 46});
            }

            // Uneven satellite droplets make large spills look naturally scattered.
            drawPool(0.105f, mark.extent * 0.73f, mark.extent * 0.16f,
                     0.08f, {72, 0, 6, 168}, {24, 0, 2, 126});
            drawPool(0.075f, -mark.extent * 0.62f, mark.extent * 0.43f,
                     0.08f, {80, 1, 7, 158}, {25, 0, 2, 118});
            drawPool(0.045f, mark.extent * 0.34f, -mark.extent * 0.71f,
                     0.07f, {66, 0, 5, 142}, {21, 0, 2, 102});
        }

        // Wet footprints use the floor plane at each step, so they remain flush on
        // slopes instead of hovering or sinking like a horizontal decal.
        for (const DarkHourBloodFootprint& print : footprints) {
            if (!print.active || print.life == 0 || fabsf(print.surfaceNormal.y) < 0.001f) continue;
            const f32 fade = std::min(1.0f, static_cast<f32>(print.life) / 180.0f);
            const f32 forwardX = sinf(print.rotation);
            const f32 forwardZ = cosf(print.rotation);
            const f32 sideX = cosf(print.rotation);
            const f32 sideZ = -sinf(print.rotation);
            const auto point = [&](f32 along, f32 across, f32 lift) {
                const f32 x = print.position.x + forwardX * along + sideX * across;
                const f32 z = print.position.z + forwardZ * along + sideZ * across;
                const f32 y = (-print.surfaceNormal.x * x - print.surfaceNormal.z * z -
                               print.surfaceD) / print.surfaceNormal.y;
                return cXyz(x, y + lift, z);
            };
            const auto drawLobe = [&](f32 along, f32 length, f32 width, GXColor color) {
                GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 13);
                cXyz center = point(along, 0.0f, 1.15f);
                GXPosition3f32(center.x, center.y, center.z);
                GXColor4u8(color.r, color.g, color.b, static_cast<u8>(color.a * fade));
                for (int i = 0; i <= 11; ++i) {
                    const f32 angle = i * 0.57119866f;
                    cXyz edge = point(along + cosf(angle) * length,
                                      sinf(angle) * width, 1.1f);
                    GXPosition3f32(edge.x, edge.y, edge.z);
                    GXColor4u8(color.r, color.g, color.b,
                               static_cast<u8>(color.a * fade * 0.45f));
                }
                GXEnd();
            };
            drawLobe(7.0f, 15.0f, 8.0f, {62, 0, 5, 175});
            drawLobe(-9.0f, 9.0f, 6.5f, {45, 0, 4, 150});
            drawLobe(8.5f, 6.5f, 3.0f, {205, 112, 116, 48});
        }
        J3DShape::resetVcdVatCache();
    }

    DarkHourBloodMark marks[160] = {};
    u32 nextMark = 0;
    DarkHourBloodFootprint footprints[192] = {};
    u32 nextFootprint = 0;
};

static DarkHourBloodPacket s_darkHourBloodPacket;
static cXyz s_lastFootprintPosition;
static bool s_haveLastFootprintPosition = false;
static bool s_nextFootIsLeft = true;
static int s_wetStepsRemaining = 0;

static void dark_hour_blood_clear_footprints() {
    for (DarkHourBloodFootprint& print : s_darkHourBloodPacket.footprints) print.active = false;
    s_darkHourBloodPacket.nextFootprint = 0;
    s_haveLastFootprintPosition = false;
    s_nextFootIsLeft = true;
    s_wetStepsRemaining = 0;
}

static u32 dark_hour_blood_random(u32& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static f32 dark_hour_blood_random_unit(u32& state) {
    return static_cast<f32>(dark_hour_blood_random(state) & 0x00FFFFFF) /
           static_cast<f32>(0x01000000);
}

static bool dark_hour_blood_footprint_is_walkable(const cXyz& center, f32 ground,
                                                   const f32 (&radius)[32], f32 rotation,
                                                   const cM3dGPla& centerSurface,
                                                   f32 (&midGroundOut)[32],
                                                   f32 (&rimGroundOut)[32]) {
    // Probe 32 directions at quarter-radius intervals. Besides finding holes, require every
    // point to remain on the center collision plane. A single mesh cannot represent a puddle
    // crossing a curb/crease without visibly cutting into one of the surfaces.
    for (int probeSample = 0; probeSample < 32; ++probeSample) {
        const int sample0 = probeSample;
        const f32 probeRadius = radius[sample0];
        const f32 angle = rotation + probeSample * 0.19634954f;
        f32 outerGround = ground;
        for (int ring = 1; ring <= 4; ++ring) {
            const f32 scale = ring * 0.25f;
            const f32 x = center.x + sinf(angle) * probeRadius * scale;
            const f32 z = center.z + cosf(angle) * probeRadius * scale;
            cXyz probe(x, ground + 120.0f, z);
            dBgS_GndChk check;
            check.SetPos(&probe);
            const f32 sampledGround = dComIfG_Bgsp().GroundCross(&check);
            cM3dGPla sampledSurface;
            const bool hasSurface = sampledGround != -G_CM3D_F_INF &&
                dComIfG_Bgsp().GetTriPla(check, &sampledSurface);
            const f32 expectedGround = (-centerSurface.mNormal.x * x -
                centerSurface.mNormal.z * z - centerSurface.mD) /
                centerSurface.mNormal.y;
            const f32 normalAgreement = hasSurface ?
                centerSurface.mNormal.x * sampledSurface.mNormal.x +
                    centerSurface.mNormal.y * sampledSurface.mNormal.y +
                    centerSurface.mNormal.z * sampledSurface.mNormal.z : -1.0f;
            if (sampledGround == -G_CM3D_F_INF || fabsf(sampledGround - ground) > 28.0f ||
                !hasSurface || sampledSurface.mNormal.y < 0.78f ||
                fabsf(sampledGround - expectedGround) > 4.0f || normalAgreement < 0.985f) {
                return false;
            }
            if (ring == 2) midGroundOut[sample0] = sampledGround;
            if (ring == 4) rimGroundOut[sample0] = sampledGround;
            if (ring == 4) outerGround = sampledGround;
        }

        // GroundCross can see the same broad floor plane on both sides of a curb. Trace just
        // above the surface to reject any vertical collision face cutting through the puddle.
        cXyz lineStart(center.x, ground + 3.0f, center.z);
        cXyz lineEnd(center.x + sinf(angle) * probeRadius, outerGround + 3.0f,
                     center.z + cosf(angle) * probeRadius);
        dBgS_LinChk lineCheck;
        lineCheck.Set(&lineStart, &lineEnd, nullptr);
        if (dComIfG_Bgsp().LineCross(&lineCheck)) {
            return false;
        }
    }
    return true;
}

static void dark_hour_blood_move() {
    static u32 randomState = 0xD44B100Du;
    static s8 previousRoom = -128;
    static char previousStage[16] = {};

    if (!dark_hour_moon_enabled()) {
        previousRoom = -128;
        previousStage[0] = '\0';
        for (DarkHourBloodMark& mark : s_darkHourBloodPacket.marks) mark.active = false;
        s_darkHourBloodPacket.nextMark = 0;
        dark_hour_blood_clear_footprints();
        return;
    }

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const char* stage = dComIfGp_getStartStageName();
    if (player == NULL || stage == NULL) return;

    const s8 room = dComIfGp_roomControl_getStayNo();
    if (room != previousRoom || strncmp(previousStage, stage, sizeof(previousStage) - 1) != 0) {
        previousRoom = room;
        strncpy(previousStage, stage, sizeof(previousStage) - 1);
        previousStage[sizeof(previousStage) - 1] = '\0';
        const u64 timeSeed = static_cast<u64>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        randomState = 0xD44B100Du ^ static_cast<u32>(room + 128) ^
                      static_cast<u32>(timeSeed) ^ static_cast<u32>(timeSeed >> 32);
        for (const char* it = stage; *it != '\0'; ++it) {
            randomState = randomState * 33u + static_cast<u8>(*it);
        }
        for (DarkHourBloodMark& mark : s_darkHourBloodPacket.marks) mark.active = false;
        s_darkHourBloodPacket.nextMark = 0;
        dark_hour_blood_clear_footprints();

        // Populate the complete loaded room in this first map frame. A
        // low-discrepancy disk covers distant geometry evenly instead of
        // relying on a small random circle around Link. Accepted marks remain
        // resident and are rendered regardless of their distance from Link.
        constexpr int MaxAttempts = 12000;
        constexpr int MaxMarks = 72;
        constexpr int MaxFloorLayersPerColumn = 8;
        constexpr f32 CoverageRadius = 40000.0f;
        constexpr f32 FloorSearchHeight = 30000.0f;
        constexpr f32 FloorSearchDepth = 30000.0f;
        for (int attempt = 0;
             attempt < MaxAttempts && s_darkHourBloodPacket.nextMark < MaxMarks; ++attempt) {
            // Fresh random polar coordinates make every room load different. Square-root
            // radius keeps the distribution uniform by area; overlap rejection maintains
            // the requested spacing without falling back to a repeated fixed pattern.
            const f32 angle = dark_hour_blood_random_unit(randomState) * 6.2831853f;
            const f32 distance = sqrtf(dark_hour_blood_random_unit(randomState)) * CoverageRadius;
            cXyz position(player->current.pos.x + sinf(angle) * distance,
                          player->current.pos.y + FloorSearchHeight,
                          player->current.pos.z + cosf(angle) * distance);

            // Walk down every collision layer in this X/Z column. The old
            // player-height restriction omitted upper stories, basements and
            // disconnected platforms even though their collision was loaded.
            for (int floorLayer = 0;
                 floorLayer < MaxFloorLayersPerColumn &&
                 position.y >= player->current.pos.y - FloorSearchDepth &&
                 s_darkHourBloodPacket.nextMark < MaxMarks;
                 ++floorLayer) {
                dBgS_GndChk groundCheck;
                groundCheck.SetPos(&position);
                const f32 ground = dComIfG_Bgsp().GroundCross(&groundCheck);
                if (ground == -G_CM3D_F_INF || ground < player->current.pos.y - FloorSearchDepth) {
                    break;
                }

                // Continue below this surface on the next pass even when it
                // is unsuitable, allowing a valid walkable floor beneath it.
                position.y = ground - 80.0f;

                cM3dGPla surface;
                if (!dComIfG_Bgsp().GetTriPla(groundCheck, &surface) ||
                    surface.mNormal.y < 0.78f) {
                    continue;
                }

                // Give each puddle a noticeably different footprint. The
                // larger range makes broad floor spills possible without
                // making every mark the same oversized shape.
                const f32 size = 220.0f + dark_hour_blood_random_unit(randomState) * 360.0f;
                const f32 rotation = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                const f32 phaseA = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                const f32 phaseB = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                f32 roundedRadius[32];
                f32 extent = 0.0f;
                for (int sample = 0; sample < 32; ++sample) {
                    const f32 radiusAngle = sample * 0.19634954f;
                    // Low-frequency waves create broad organic curves with a
                    // slightly offset lobe, like a spill spreading across a
                    // floor. Avoid independent per-vertex noise so the edge
                    // stays soft instead of becoming star-shaped.
                    const f32 shape = 1.0f + 0.15f * sinf(radiusAngle * 2.0f + phaseA) +
                                      0.075f * sinf(radiusAngle * 3.0f + phaseB) +
                                      0.045f * sinf(radiusAngle + phaseA * 0.55f);
                    roundedRadius[sample] = size * shape;
                    if (roundedRadius[sample] > extent) extent = roundedRadius[sample];
                }

                cXyz floorPosition(position.x, ground, position.z);
                f32 midGround[32];
                f32 rimGround[32];
                if (!dark_hour_blood_footprint_is_walkable(floorPosition, ground,
                                                           roundedRadius, rotation,
                                                           surface,
                                                           midGround, rimGround)) {
                    continue;
                }

                bool overlaps = false;
                for (u32 i = 0; i < s_darkHourBloodPacket.nextMark; ++i) {
                    const DarkHourBloodMark& existing = s_darkHourBloodPacket.marks[i];
                    const f32 dx = existing.position.x - floorPosition.x;
                    const f32 dz = existing.position.z - floorPosition.z;
                    const f32 separation = existing.extent + extent + 600.0f;
                    if (dx * dx + dz * dz < separation * separation &&
                        fabsf(existing.position.y - ground) < 120.0f) {
                        overlaps = true;
                        break;
                    }
                }
                if (overlaps) continue;

                DarkHourBloodMark& mark =
                    s_darkHourBloodPacket.marks[s_darkHourBloodPacket.nextMark++];
                mark.position = floorPosition;
                mark.position.y = ground + 0.8f;
                mark.surfaceNormal = surface.mNormal;
                mark.surfaceD = surface.mD;
                mark.extent = extent;
                mark.rotation = rotation;
                mark.sheenAngle = dark_hour_blood_random_unit(randomState) * 6.2831853f;
                for (int sample = 0; sample < 32; ++sample) {
                    mark.radius[sample] = roundedRadius[sample];
                    mark.midGround[sample] = midGround[sample];
                    mark.rimGround[sample] = rimGround[sample];
                }
                mark.active = true;
            }
        }
    }

    for (DarkHourBloodFootprint& print : s_darkHourBloodPacket.footprints) {
        if (print.active && print.life > 0 && --print.life == 0) print.active = false;
    }

    bool touchingBlood = false;
    for (const DarkHourBloodMark& mark : s_darkHourBloodPacket.marks) {
        if (!mark.active || fabsf(player->current.pos.y - mark.position.y) > 70.0f) continue;
        const f32 dx = player->current.pos.x - mark.position.x;
        const f32 dz = player->current.pos.z - mark.position.z;
        if (dx * dx + dz * dz <= mark.extent * mark.extent * 0.72f) {
            touchingBlood = true;
            s_wetStepsRemaining = 128;
            break;
        }
    }

    if (!s_haveLastFootprintPosition) {
        s_lastFootprintPosition = player->current.pos;
        s_haveLastFootprintPosition = true;
        return;
    }

    const f32 moveX = player->current.pos.x - s_lastFootprintPosition.x;
    const f32 moveZ = player->current.pos.z - s_lastFootprintPosition.z;
    const f32 moved = sqrtf(moveX * moveX + moveZ * moveZ);
    if (moved > 300.0f) {
        s_lastFootprintPosition = player->current.pos;
        return;
    }
    if (s_wetStepsRemaining <= 0 || moved < 38.0f) return;

    const f32 directionX = moveX / moved;
    const f32 directionZ = moveZ / moved;
    const f32 side = s_nextFootIsLeft ? -9.5f : 9.5f;
    cXyz footPosition(player->current.pos.x + directionZ * side,
                      player->current.pos.y + 90.0f,
                      player->current.pos.z - directionX * side);
    dBgS_GndChk groundCheck;
    groundCheck.SetPos(&footPosition);
    const f32 ground = dComIfG_Bgsp().GroundCross(&groundCheck);
    cM3dGPla surface;
    if (ground != -G_CM3D_F_INF && dComIfG_Bgsp().GetTriPla(groundCheck, &surface) &&
        surface.mNormal.y >= 0.65f && fabsf(ground - player->current.pos.y) < 85.0f) {
        DarkHourBloodFootprint& print = s_darkHourBloodPacket.footprints[
            s_darkHourBloodPacket.nextFootprint++ % 192];
        print.position = cXyz(footPosition.x, ground, footPosition.z);
        print.surfaceNormal = surface.mNormal;
        print.surfaceD = surface.mD;
        print.rotation = atan2f(directionX, directionZ);
        print.life = 1200;
        print.active = true;
        s_nextFootIsLeft = !s_nextFootIsLeft;
        if (!touchingBlood) --s_wetStepsRemaining;
    }
    s_lastFootprintPosition = player->current.pos;
}

}
void move() { dark_hour_blood_move(); }
void draw() {
    if (!dark_hour_moon_enabled() || g_env_light.camera_water_in_status != 0) return;
    dComIfGd_setXluListBG();
    j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(&s_darkHourBloodPacket, 0);
    dComIfGd_setList();
}
}
