#include "properties.h"
#include "roblox.h"
#include "vendor/imgui/imgui.h"
#include <cstdio>
#include <cstring>

namespace properties {

static char s_className[64] = {};
static PropEntry s_props[256];
static int       s_propCount = 0;
static int       s_decoded   = 0;
static uint32_t  s_lastInst  = 0;

// ── BrickColor name lookup ────────────────────────────────────────────────────
static const char* BrickColorName(int id) {
    struct Entry { int id; const char* name; };
    static const Entry table[] = {
        {1,"White"},{2,"Grey"},{3,"Light yellow"},{5,"Brick yellow"},
        {6,"Light green"},{9,"Light reddish violet"},{11,"Pastel Blue"},
        {12,"Light orange brown"},{18,"Nougat"},{21,"Bright red"},
        {22,"Med. reddish violet"},{23,"Bright blue"},{24,"Yellow"},
        {25,"Earth orange"},{26,"Black"},{27,"Dark grey"},{28,"Dark green"},
        {29,"Med. stone grey"},{36,"Br. yel. orange"},{37,"Bright green"},
        {38,"Dark orange"},{40,"Sand blue"},{41,"Sand violet"},{42,"Peach"},
        {43,"Cobalt"},{44,"Slime green"},{45,"Smoky grey"},{47,"Gold"},
        {48,"Dark curry"},{49,"Fire yellow"},{50,"Flame yel. orange"},
        {100,"Light orange"},{101,"Bright orange"},{102,"Bright bl.-green"},
        {103,"Earth yellow"},{104,"Bright violet"},{106,"Br. orange"},
        {107,"Bright bl. green"},{119,"Lime green"},{120,"Lt. yel. green"},
        {125,"Light orange"},{151,"Sand green"},{192,"Reddish brown"},
        {194,"Med. stone grey"},{195,"Smoky grey"},{199,"Dark blue"},
        {208,"Dark yellow"},{217,"Sand orange"},{226,"Cool yellow"},
        {0, nullptr}
    };
    for (const Entry* e = table; e->name; e++)
        if (e->id == id) return e->name;
    return nullptr;
}

// ── Value reader ─────────────────────────────────────────────────────────────
static const char* ReadValue(uint32_t inst, const PropEntry& e, char* buf, size_t sz) {
    buf[0] = '\0';
    __try {
        switch (e.kind) {

        case PK_FLOAT: {
            float v = *reinterpret_cast<float*>(inst + e.offset);
            snprintf(buf, sz, "%.4g", v);
            break;
        }
        case PK_INT32: {
            int32_t v = *reinterpret_cast<int32_t*>(inst + e.offset);
            // Show BrickColor / TeamColor by name when possible
            if (strstr(e.name, "Color") || strstr(e.name, "colour")) {
                const char* nm = BrickColorName(v);
                if (nm) snprintf(buf, sz, "%s (%d)", nm, v);
                else    snprintf(buf, sz, "BrickColor (%d)", v);
            } else {
                snprintf(buf, sz, "%d", v);
            }
            break;
        }
        case PK_BOOL: {
            uint8_t v = *reinterpret_cast<uint8_t*>(inst + e.offset);
            snprintf(buf, sz, "%s", v ? "true" : "false");
            break;
        }
        case PK_BITFIELD_BOOL: {
            uint8_t v = *reinterpret_cast<uint8_t*>(inst + e.offset);
            snprintf(buf, sz, "%s", (v & e.offset2) ? "true" : "false");
            break;
        }
        case PK_VECTOR3: {
            float* v = reinterpret_cast<float*>(inst + e.offset);
            snprintf(buf, sz, "%.3g, %.3g, %.3g", v[0], v[1], v[2]);
            break;
        }
        case PK_STRING:
            ReadStdString(inst + e.offset, buf, sz);
            break;
        case PK_CONST_BOOL:
            snprintf(buf, sz, "%s", e.offset ? "true" : "false");
            break;

        case PK_INDIRECT_FLOAT: {
            uintptr_t ptr = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!ptr) { snprintf(buf, sz, "nil"); break; }
            float v = *reinterpret_cast<float*>(ptr + e.offset2);
            snprintf(buf, sz, "%.4g", v);
            break;
        }
        case PK_INDIRECT_INT32: {
            uintptr_t ptr = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!ptr) { snprintf(buf, sz, "nil"); break; }
            int32_t v = *reinterpret_cast<int32_t*>(ptr + e.offset2);
            snprintf(buf, sz, "%d", v);
            break;
        }
        case PK_INDIRECT_BOOL: {
            uintptr_t ptr = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!ptr) { snprintf(buf, sz, "nil"); break; }
            uint8_t v = *reinterpret_cast<uint8_t*>(ptr + e.offset2);
            snprintf(buf, sz, "%s", v ? "true" : "false");
            break;
        }
        case PK_INDIRECT_VECTOR3: {
            uintptr_t ptr = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!ptr) { snprintf(buf, sz, "nil"); break; }
            float* v = reinterpret_cast<float*>(ptr + e.offset2);
            snprintf(buf, sz, "%.3g, %.3g, %.3g", v[0], v[1], v[2]);
            break;
        }

        case PK_DEREF_VECTOR3: {
            // body = *(inst+off1), vecPtr = *(body+off2), Vec3 at vecPtr+4
            uintptr_t body = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!body) { snprintf(buf, sz, "nil"); break; }
            uintptr_t vecPtr = *reinterpret_cast<uintptr_t*>(body + e.offset2);
            if (!vecPtr) { snprintf(buf, sz, "nil"); break; }
            float* v = reinterpret_cast<float*>(vecPtr + 4);
            snprintf(buf, sz, "%.3g, %.3g, %.3g", v[0], v[1], v[2]);
            break;
        }
        case PK_DEREF_VECTOR3_P4: {
            // Same chain, FPU copy variant — also starts at vecPtr+4
            uintptr_t body = *reinterpret_cast<uintptr_t*>(inst + e.offset);
            if (!body) { snprintf(buf, sz, "nil"); break; }
            uintptr_t vecPtr = *reinterpret_cast<uintptr_t*>(body + e.offset2);
            if (!vecPtr) { snprintf(buf, sz, "nil"); break; }
            float* v = reinterpret_cast<float*>(vecPtr + 4);
            snprintf(buf, sz, "%.3g, %.3g, %.3g", v[0], v[1], v[2]);
            break;
        }

        case PK_POSITION: {
            // World position via the known prim chain: *(*(inst+0x24)+0x164)
            uintptr_t prim = *reinterpret_cast<uintptr_t*>(inst + 0x24);
            if (!prim) { snprintf(buf, sz, "nil"); break; }
            float* v = reinterpret_cast<float*>(prim + 0x164);
            snprintf(buf, sz, "%.3g, %.3g, %.3g", v[0], v[1], v[2]);
            break;
        }

        case PK_VTABLE_RELAY: {
            // Read the real function from the instance's vtable at the stored slot offset,
            // decode it, then read the value — one level only (no relay-of-relay).
            uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(inst);
            if (!vtbl) break;
            uintptr_t fn = *reinterpret_cast<uintptr_t*>(vtbl + e.offset);
            if (!fn) break;
            PropEntry relay = e;
            relay.kind    = PK_UNKNOWN;
            relay.offset  = 0;
            relay.offset2 = 0;
            if (!DecodePropGetter(fn, relay.kind, relay.offset, relay.offset2)) break;
            if (relay.kind == PK_VTABLE_RELAY) break; // refuse to chain
            ReadValue(inst, relay, buf, sz);
            break;
        }

        default:
            break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snprintf(buf, sz, "err");
    }
    return buf;
}

// ── Draw ─────────────────────────────────────────────────────────────────────
void Draw(uint32_t inst) {
    if (!inst) {
        ImGui::TextDisabled("No selection");
        return;
    }

    if (inst != s_lastInst) {
        s_lastInst  = inst;
        s_propCount = EnumerateProperties(inst, s_props, 256);
        s_decoded   = 0;
        for (int i = 0; i < s_propCount; i++)
            if (s_props[i].kind != PK_UNKNOWN) s_decoded++;
        if (!ReadClassName(inst, s_className, sizeof(s_className)))
            snprintf(s_className, sizeof(s_className), "Instance");
    }

    ImGui::TextUnformatted(s_className);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d / %d)", s_decoded, s_propCount);
    ImGui::Separator();

    if (s_decoded == 0) {
        ImGui::TextDisabled("No readable properties");
        return;
    }

    ImGui::BeginChild("##props_scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("##props", 2,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp)) {

        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableHeadersRow();

        const char* lastCat = nullptr;
        char vbuf[128];

        for (int i = 0; i < s_propCount; i++) {
            const PropEntry& e = s_props[i];
            if (e.kind == PK_UNKNOWN) continue;

            if (e.category[0] && (lastCat == nullptr || strcmp(e.category, lastCat) != 0)) {
                lastCat = e.category;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 100, 255));
                ImGui::TextUnformatted(e.category);
                ImGui::PopStyleColor();
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(e.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(ReadValue(inst, e, vbuf, sizeof(vbuf)));
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace properties
