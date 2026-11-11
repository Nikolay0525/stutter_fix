// stutter_fix: Kyubu port of coremaze's Cube-World-Stutter-Fix-Mod.
//
// Runs `PRAGMA synchronous=OFF;` on every SQLite database the game opens, so that heavy queries stop waiting for the
// disk. The old mod did it with a jmp stub at 0x449A1C; here it is a mid-function hook at the same instruction:
// EDI+4 holds the sqlite3* handle, and sqlite3_exec (0x520760) is called with it.
#include "../common.hpp"

namespace {
using SqliteExecFn = int(__cdecl*)(void* db, const char* sql, void* callback, void* arg, char** errmsg);
SqliteExecFn g_exec = nullptr;
const KyubuApi* g_api = nullptr;

void KYUBU_CALL on_open(KyubuCpuContext* ctx, void*) {
    uint32_t db = 0;
    if (g_api->mem_read(ctx->edi + 4, &db, sizeof db) != KY_OK || !db) return;
    g_exec(reinterpret_cast<void*>(db), "PRAGMA synchronous=OFF;", nullptr, nullptr, nullptr);
}
}  // namespace

struct StutterFix {
    explicit StutterFix(kyubu::Mod& m) {
        g_api = m.api();
        const uintptr_t site = classic::at(m, 0x49A1C);
        const uintptr_t exec = classic::at(m, 0x120760);
        static const uint8_t want_site[] = {0x56, 0x8B, 0x75, 0x08, 0x83, 0x7E, 0x14, 0x10};
        static const uint8_t want_exec[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x53, 0x57};
        if (!classic::expect(m, site, want_site, sizeof want_site, "stutter_fix (hook site)") ||
            !classic::expect(m, exec, want_exec, sizeof want_exec, "stutter_fix (sqlite3_exec)"))
            return;
        g_exec = reinterpret_cast<SqliteExecFn>(exec);
        KyubuId id = 0;
        const KyubuResult r = m.api()->hook_mid(m.handle(), site, &on_open, nullptr, &id);
        if (r != KY_OK) { m.error("stutter_fix: could not hook the database open (result %d)", static_cast<int>(r)); return; }
        m.info("stutter_fix: databases will be opened with synchronous=OFF");
    }
};

KYUBU_MOD("stutter_fix", KY_SIDE_CLIENT, StutterFix)
