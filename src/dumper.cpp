// dumper.dll — inject into RobloxApp_client.exe, writes %TEMP%\rbx_dump.txt
// No STL, no ImGui. Minimal x86 DLL.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include "roblox.h"

static uintptr_t g_base = 0;
static FILE*     g_f    = nullptr;

// Simple seen-set: fixed array of visited class descriptor pointers
static uintptr_t g_seenCd[512];
static int       g_seenCdN = 0;
static uintptr_t g_seenInst[4096];
static int       g_seenInstN = 0;

static bool SeenCd(uintptr_t cd) {
    for (int i = 0; i < g_seenCdN; i++) if (g_seenCd[i] == cd) return true;
    if (g_seenCdN < 512) g_seenCd[g_seenCdN++] = cd;
    return false;
}
static bool SeenInst(uintptr_t inst) {
    for (int i = 0; i < g_seenInstN; i++) if (g_seenInst[i] == inst) return true;
    if (g_seenInstN < 4096) g_seenInst[g_seenInstN++] = inst;
    return false;
}

static void DumpClass(uintptr_t inst) {
    __try {
        uintptr_t cd = *reinterpret_cast<uintptr_t*>(inst + 0x1C);
        if (!cd || SeenCd(cd)) return;

        char className[64] = "<unknown>";
        ReadClassName(inst, className, sizeof(className));

        uintptr_t vecFirst = *reinterpret_cast<uintptr_t*>(cd + 0x14);
        uintptr_t vecLast  = *reinterpret_cast<uintptr_t*>(cd + 0x18);
        int n = (vecLast > vecFirst) ? (int)((vecLast - vecFirst) / 4) : 0;
        if (n > 512) n = 0;

        printf("=== %s  cd=0x%08X  %d props ===\n", className, (uint32_t)cd, n);
        fprintf(g_f, "\n=== %s  cd=0x%08X  %d props ===\n", className, (uint32_t)cd, n);

        for (int i = 0; i < n; i++) {
            __try {
                uintptr_t desc = *reinterpret_cast<uintptr_t*>(vecFirst + i * 4);
                if (!desc) continue;

                uintptr_t nameStr = *reinterpret_cast<uintptr_t*>(desc + 0x04);
                uintptr_t catStr  = *reinterpret_cast<uintptr_t*>(desc + 0x08);
                uintptr_t meta    = *reinterpret_cast<uintptr_t*>(desc + 0x18);

                char name[64] = "?", cat[32] = "";
                if (nameStr) ReadStdString(nameStr, name, sizeof(name));
                if (catStr)  ReadStdString(catStr,  cat,  sizeof(cat));

                uintptr_t getter = 0;
                if (meta) {
                    __try { getter = *reinterpret_cast<uintptr_t*>(meta + 0x08); }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                uint8_t raw[24] = {};
                if (getter) {
                    __try { memcpy(raw, reinterpret_cast<void*>(getter), 24); }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                uint8_t  kind = PK_UNKNOWN;
                uint32_t off1 = 0, off2 = 0;
                if (getter) DecodePropGetter(getter, kind, off1, off2);

                char line[256];
                snprintf(line, sizeof(line),
                    "  [%2d] %-32s cat=%-12s getter=0x%08X kind=%2d off=0x%04X off2=0x%04X\n"
                    "       %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                    "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                    i, name, cat, (uint32_t)getter, (int)kind, off1, off2,
                    raw[0],raw[1],raw[2],raw[3],raw[4],raw[5],raw[6],raw[7],
                    raw[8],raw[9],raw[10],raw[11],raw[12],raw[13],raw[14],raw[15],
                    raw[16],raw[17],raw[18],raw[19],raw[20],raw[21],raw[22],raw[23]);

                printf("%s", line);
                fprintf(g_f, "%s", line);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("  <exception in DumpClass>\n");
        fprintf(g_f, "  <exception in DumpClass>\n");
    }
}

static void WalkTree(uintptr_t inst, int depth) {
    if (depth > 8 || !inst || SeenInst(inst)) return;
    DumpClass(inst);

    uint32_t children[64];
    uint32_t n = GetChildren((uint32_t)inst, children, 64);
    for (uint32_t i = 0; i < n; i++)
        if (children[i]) WalkTree(children[i], depth + 1);
}

static DWORD WINAPI DumpThread(LPVOID) {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    printf("[dumper] thread started, waiting 6s...\n");

    Sleep(6000);

    g_base = (uintptr_t)GetModuleHandleA("RobloxApp_client.exe");
    printf("[dumper] base=0x%08X\n", (uint32_t)g_base);

    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strncat(path, "rbx_dump.txt", sizeof(path) - strlen(path) - 1);
    printf("[dumper] writing to %s\n", path);

    g_f = fopen(path, "w");
    if (!g_f) { printf("[dumper] fopen failed!\n"); return 0; }

    uintptr_t dm = GetDataModel(g_base);
    printf("[dumper] DataModel=0x%08X\n", (uint32_t)dm);
    fprintf(g_f, "=== Roblox 2009E Dump ===\nbase=0x%08X dm=0x%08X\n", (uint32_t)g_base, (uint32_t)dm);

    if (dm) WalkTree(dm, 0);

    fprintf(g_f, "\n=== Done: %d classes ===\n", g_seenCdN);
    fclose(g_f);

    printf("[dumper] done! %d classes. file: %s\n", g_seenCdN, path);
    printf("[dumper] press enter to close console\n");
    getchar();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, DumpThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
