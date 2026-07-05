// Tests for SandsTopology (step 2 skeleton). Pins config classification + ownership,
// especially the cases tied to this session's bugs:
//   - MACRO_SOLE vs MONO_PLUS_MACRO must be DISTINCT configs (the clobber bug).
//   - In MONO_PLUS_MACRO, a Mono-owned V1 lane → owner MONO (editable on Mono, locked
//     on Macro); a ceded lane → owner MACRO (the inverse).
//
// Build/run:
//   cd test && g++ -std=c++17 -I. -I../src/dsp test_SandsTopology.cpp -o /tmp/tst && /tmp/tst
#include "SandsTopology.hpp"
#include <cstdio>

using dotModular::SandsTopology;
using Role   = SandsTopology::Role;
using Config = SandsTopology::Config;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } \
                              else { std::printf("  pass: %s\n", msg); } } while(0)

static SandsTopology::Inputs base(bool m, bool e, bool x) {
    SandsTopology::Inputs i;
    i.monoPresent = m; i.eastPresent = e; i.macroPresent = x;
    i.polyBaseActive = (e || x); i.polyVoiceCount = (e || x) ? 4 : 0;
    return i;   // monoV1Owner/eastV1Owner default all-true (local owns)
}

int main() {
    std::printf("== config classification (all 8 reachable) ==\n");
    CHECK(SandsTopology::build(base(false,false,false)).config == Config::EMPTY,           "none -> EMPTY");
    CHECK(SandsTopology::build(base(true, false,false)).config == Config::MONO,            "mono -> MONO");
    CHECK(SandsTopology::build(base(false,true, false)).config == Config::EAST,            "east -> EAST");
    CHECK(SandsTopology::build(base(false,false,true )).config == Config::MACRO_SOLE,      "macro -> MACRO_SOLE");
    CHECK(SandsTopology::build(base(true, true, false)).config == Config::MONO_PLUS_EAST,  "mono+east -> MONO_PLUS_EAST");
    CHECK(SandsTopology::build(base(true, false,true )).config == Config::MONO_PLUS_MACRO, "mono+macro -> MONO_PLUS_MACRO");
    CHECK(SandsTopology::build(base(false,true, true )).config == Config::EAST_PLUS_MACRO, "east+macro -> EAST_PLUS_MACRO");
    CHECK(SandsTopology::build(base(true, true, true )).config == Config::MONO_EAST_MACRO, "all -> MONO_EAST_MACRO");

    std::printf("== THE bug case: MACRO_SOLE != MONO_PLUS_MACRO ==\n");
    CHECK(Config::MACRO_SOLE != Config::MONO_PLUS_MACRO, "distinct configs (clobber guard can't match both)");

    std::printf("== MONO_PLUS_MACRO V1 ownership (no lanes ceded) ==\n");
    {
        auto t = SandsTopology::build(base(true,false,true));
        CHECK(t.owner(0,0) == Role::MONO,           "V1 melody owned by MONO");
        CHECK(t.editableOn(Role::MONO,  0,0),       "editable on Mono panel");
        CHECK(t.lockedOn  (Role::MACRO, 0,0),       "locked on Macro panel");
        CHECK(!t.lockedOn (Role::MONO,  0,0),       "NOT locked on Mono panel");
        CHECK(t.writesEngine(Role::MONO, 0,0),      "MONO writes engine V1");
        CHECK(!t.writesEngine(Role::MACRO,0,0),     "MACRO does NOT write engine V1 (the fix)");
        CHECK(t.owner(0,4) == Role::MONO,           "VAR (lane4) mono-only -> MONO");
        CHECK(t.owner(0,5) == Role::MONO,           "LEG (lane5) mono-only -> MONO");
    }

    std::printf("== MONO_PLUS_MACRO with melody lane CEDED to Macro ==\n");
    {
        auto i = base(true,false,true);
        i.monoV1Owner[0] = false;   // cede melody (editor lane 0) to Macro
        auto t = SandsTopology::build(i);
        CHECK(t.owner(0,0) == Role::MACRO,          "ceded melody owned by MACRO");
        CHECK(t.editableOn(Role::MACRO, 0,0),       "editable on Macro panel");
        CHECK(t.lockedOn  (Role::MONO,  0,0),       "locked on Mono panel");
        CHECK(t.owner(0,1) == Role::MONO,           "octave still MONO (per-lane)");
    }

    std::printf("== MACRO_SOLE owns V1 (no Mono to clobber) ==\n");
    {
        auto t = SandsTopology::build(base(false,false,true));
        CHECK(t.owner(0,0) == Role::MACRO,          "MACRO_SOLE owns V1 melody");
        CHECK(t.writesEngine(Role::MACRO, 0,0),     "MACRO writes engine V1 here (correct in MACRO_SOLE)");
    }

    std::printf("== EAST_PLUS_MACRO poly ownership (per voice/lane) ==\n");
    {
        auto i = base(false,true,true);
        for (int v=0; v<15; ++v) for (int l=0;l<4;++l) i.eastPolyOwner[v][l] = true; // East owns all
        i.eastPolyOwner[2][1] = false;   // voice index 2 (=V4), octave ceded to Macro
        auto t = SandsTopology::build(i);
        CHECK(t.owner(3,1) == Role::MACRO,          "V4 octave ceded -> MACRO (voice 3 = poly idx 2)");
        CHECK(t.owner(3,0) == Role::EAST,           "V4 melody still EAST");
        CHECK(t.owner(1,0) == Role::EAST,           "V2 melody EAST");
        CHECK(t.owner(1,4) == Role::NONE,           "poly has no VAR lane -> NONE");
    }

    std::printf("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
