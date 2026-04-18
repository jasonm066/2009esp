#include "esp.h"
#include "draw.h"
#include "roblox.h"
#include <cstdio>

namespace esp {

struct PlayerEntry {
    uint32_t ptr      = 0;
    uint32_t torso    = 0;
    uint32_t humanoid = 0;
    char     name[32] = {};
};
static PlayerEntry s_cache[100];

static uintptr_t ModuleBase() {
    static uintptr_t cached = 0;
    if (!cached) cached = reinterpret_cast<uintptr_t>(GetModuleHandleA("RobloxApp_client.exe"));
    return cached;
}

void Draw(int sw, int sh, const Config& cfg) {
    uintptr_t modBase = ModuleBase();
    if (!modBase) return;

    uintptr_t players    = GetPlayers(modBase);
    uintptr_t cameraInst = GetCameraInst(modBase);
    if (!players || !cameraInst) return;

    Camera cam;
    if (!ReadCamera(cameraInst, cam)) return;

    uint32_t A = SafeDeref<uint32_t>(players + offsets::Children, 0); if (!A) return;
    uint32_t B = SafeDeref<uint32_t>(A + 0xC, 0);                     if (!B) return;
    uint32_t childStart = SafeDeref<uint32_t>(B + 0xC,  0);
    uint32_t childEnd   = SafeDeref<uint32_t>(B + 0x10, 0);
    if (childEnd < childStart) return;

    uint32_t count = (childEnd - childStart) / 8;
    if (count == 0 || count > 100) return;

    for (uint32_t i = 1; i < count; i++) {
        uint32_t playerPtr = SafeDeref<uint32_t>(childStart + i * 8, 0);
        if (!playerPtr) continue;

        PlayerEntry& pe = s_cache[i];
        if (pe.ptr != playerPtr) {
            pe.ptr = playerPtr;
            pe.torso = 0; pe.humanoid = 0;
            ReadString(playerPtr + offsets::Name, pe.name, sizeof(pe.name));
            uint32_t character = SafeDeref<uint32_t>(playerPtr + offsets::Character, 0);
            if (character) {
                pe.torso    = FindChild(character, "Torso");
                pe.humanoid = FindChild(character, "Humanoid");
            }
        }
        if (!pe.torso) continue;

        Vec3 pos{};
        if (!GetPartPosition(pe.torso, pos)) continue;

        float hp = 100.f, maxHP = 100.f;
        if (pe.humanoid) ReadHealth(pe.humanoid, hp, maxHP);
        float hpFrac = (maxHP > 0.f) ? (hp / maxHP) : 1.f;
        if (hpFrac < 0.f) hpFrac = 0.f;
        if (hpFrac > 1.f) hpFrac = 1.f;

        Vec3 headTop = {pos.x, pos.y + 2.5f, pos.z};
        Vec3 feet    = {pos.x, pos.y - 3.0f, pos.z};

        float topX, topY, botX, botY;
        bool projected = WorldToScreen(cam, headTop, sw, sh, topX, topY) &&
                         WorldToScreen(cam, feet,    sw, sh, botX, botY);
        float midX = (topX + botX) * 0.5f, midY = (topY + botY) * 0.5f;
        bool onScreen = projected && midX > 0 && midX < sw && midY > 0 && midY < sh;

        if (!onScreen) {
            if (cfg.offscreen) {
                float dx = pos.x - cam.pos.x;
                float dz = pos.z - cam.pos.z;
                float lx  = cam.rot[0] * dx + cam.rot[6] * dz;
                float ang  = atan2f(lx, -(cam.rot[2] * dx + cam.rot[8] * dz));
                float cx2  = sw * 0.5f, cy2 = sh * 0.5f;
                float ax   = cx2 + cosf(ang - 1.5708f) * (cx2 - 40.f);
                float ay   = cy2 + sinf(ang - 1.5708f) * (cy2 - 40.f);
                float cs   = cosf(ang), sn = sinf(ang);
                float sz   = cfg.arrowScale;
                DrawTriangleFilled(
                    ax + sn * sz,          ay - cs * sz,
                    ax - cs * sz * 0.5f,   ay - sn * sz * 0.5f,
                    ax + cs * sz * 0.5f,   ay + sn * sz * 0.5f,
                    cfg.ofsColor());
            }
            continue;
        }

        float boxH = botY - topY;
        float boxW = boxH * 0.45f;
        float cx   = (topX + botX) * 0.5f;
        float boxL = cx - boxW * 0.5f;

        if (cfg.box) {
            if (cfg.boxStyle == 2) {
                constexpr float hw = 1.5f, hd = 1.0f, top3d = 2.5f, bot3d = -3.0f;
                Vec3 corners[8] = {
                    {pos.x - hw, pos.y + top3d, pos.z - hd},
                    {pos.x + hw, pos.y + top3d, pos.z - hd},
                    {pos.x + hw, pos.y + top3d, pos.z + hd},
                    {pos.x - hw, pos.y + top3d, pos.z + hd},
                    {pos.x - hw, pos.y + bot3d, pos.z - hd},
                    {pos.x + hw, pos.y + bot3d, pos.z - hd},
                    {pos.x + hw, pos.y + bot3d, pos.z + hd},
                    {pos.x - hw, pos.y + bot3d, pos.z + hd},
                };
                float sx[8], sy[8]; bool ok[8]; int valid = 0;
                for (int j = 0; j < 8; j++) {
                    ok[j] = WorldToScreen(cam, corners[j], sw, sh, sx[j], sy[j]);
                    if (ok[j]) valid++;
                }
                if (valid >= 2) {
                    static const int EDGES[][2] = {
                        {0,1},{1,2},{2,3},{3,0},
                        {4,5},{5,6},{6,7},{7,4},
                        {0,4},{1,5},{2,6},{3,7},
                    };
                    Color c = cfg.boxColor();
                    for (auto& e : EDGES)
                        if (ok[e[0]] && ok[e[1]])
                            DrawLine(sx[e[0]], sy[e[0]], sx[e[1]], sy[e[1]], 1.5f, c);
                }
            } else {
                if (cfg.boxFilled)
                    DrawRectFilled(boxL, topY, boxW, boxH, cfg.fillColor());
                if (cfg.boxGradient && cfg.boxStyle == 0)
                    DrawGradientOutline(boxL, topY, boxW, boxH, 1.5f, cfg.boxColor(), cfg.boxColor2());
                else if (cfg.boxStyle == 1)
                    DrawCornerBox(boxL, topY, boxW, boxH, 1.5f, cfg.boxColor());
                else
                    DrawRect(boxL, topY, boxW, boxH, 1.5f, cfg.boxColor());
            }
        }

        if (cfg.health) {
            constexpr float barW = 3.f, barGap = 3.f;
            float barX = boxL - barGap - barW;
            float barH = boxH * hpFrac;
            float barY = topY + boxH - barH;
            DrawRectFilled(barX, topY, barW, boxH, {0.15f, 0.15f, 0.15f, 1.f});
            if (cfg.hpGradient) {
                Color cF = cfg.hpFull(), cE = cfg.hpEmpty();
                Color cT = {cF.r * hpFrac + cE.r * (1 - hpFrac),
                            cF.g * hpFrac + cE.g * (1 - hpFrac),
                            cF.b * hpFrac + cE.b * (1 - hpFrac), 1.f};
                DrawGradientV(barX, barY, barW, barH, cT, cE);
            } else {
                DrawRectFilled(barX, barY, barW, barH, {1.f - hpFrac, hpFrac, 0.f, 1.f});
            }
        }

        if (cfg.name)
            DrawOutlinedText(cx - 20, topY - 16, pe.name, cfg.nameColor());

        if (cfg.distance) {
            float dx = pos.x - cam.pos.x, dy = pos.y - cam.pos.y, dz = pos.z - cam.pos.z;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0fm", sqrtf(dx*dx + dy*dy + dz*dz));
            DrawOutlinedText(cx - 12, topY - 2, buf, cfg.distColor());
        }

        if (cfg.snaplines)
            DrawLine(sw * 0.5f, (float)sh, botX, botY, 1.f, cfg.snapColor());
    }
}

} // namespace esp
