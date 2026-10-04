// Tests for SandsTopology. SANDS CONSOLIDATION Step 7: simplified to 2-module (East+Macro).
// Mono is dead — all MONO configs/roles removed. Tests cover the 4 remaining configs.
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

static SandsTopology::Inputs base(bool e, bool x) {
    SandsTopology::Inputs i;
    i.eastPresent = e; i.macroPresent = x;
    i.polyBaseActive = (e || x); i.polyVoiceCount = (e || x) ? 4 : 0;
    return i;   // eastV1Owner defaults all-true (local owns)
}

int main() {
    std::printf("== config classification (4 reachable, Step 7: 2-module) ==\n");
    CHECK(SandsTopology::build(base(false,false)).config == Config::EMPTY,           "none -> EMPTY");
    CHECK(SandsTopology::build(base(true, false)).config == Config::EAST,            "east -> EAST");
    CHECK(SandsTopology::build(base(false,true )).config == Config::MACRO_SOLE,      "macro -> MACRO_SOLE");
    CHECK(SandsTopology::build(base(true, true )).config == Config::EAST_PLUS_MACRO, "east+macro -> EAST_PLUS_MACRO");

    std::printf("== EAST_PLUS_MACRO V1 ownership (no lanes ceded) ==\n");
    {
        auto t = SandsTopology::build(base(true,true));
        CHECK(t.owner(0,0) == Role::EAST,            "V1 melody owned by EAST");
        CHECK(t.editableOn(Role::EAST,  0,0),        "editable on East panel");
        CHECK(t.lockedOn  (Role::MACRO, 0,0),        "locked on Macro panel");
        CHECK(!t.lockedOn (Role::EAST,  0,0),        "NOT locked on East panel");
        CHECK(t.writesEngine(Role::EAST, 0,0),       "EAST writes engine V1");
        CHECK(!t.writesEngine(Role::MACRO,0,0),      "MACRO does NOT write engine V1 (the fix)");
        CHECK(t.owner(0,2) == Role::EAST,            "QMIX (lane2) delegable, not ceded -> EAST");
        CHECK(t.owner(0,4) == Role::EAST,            "ACCENT (lane4) delegable, not ceded -> EAST");
        CHECK(t.owner(0,5) == Role::EAST,            "VAR (lane5) V1 -> EAST (poly lane now)");
        CHECK(t.owner(0,6) == Role::EAST,            "LEG (lane6) V1 -> EAST (poly lane now)");
    }

    std::printf("== EAST_PLUS_MACRO with melody lane CEDED to Macro ==\n");
    {
        auto i = base(true,true);
        i.eastV1Owner[0] = false;   // cede melody (editor lane 0) to Macro
        auto t = SandsTopology::build(i);
        CHECK(t.owner(0,0) == Role::MACRO,           "ceded melody owned by MACRO");
        CHECK(t.editableOn(Role::MACRO, 0,0),        "editable on Macro panel");
        CHECK(t.lockedOn  (Role::EAST,  0,0),        "locked on East panel");
        CHECK(t.owner(0,1) == Role::EAST,            "octave still EAST (per-lane)");
    }

    std::printf("== MACRO_SOLE owns V1 ==\n");
    {
        auto t = SandsTopology::build(base(false,true));
        CHECK(t.owner(0,0) == Role::MACRO,           "MACRO_SOLE owns V1 melody");
        CHECK(t.writesEngine(Role::MACRO, 0,0),      "MACRO writes engine V1 here (correct in MACRO_SOLE)");
    }

    std::printf("== EAST_PLUS_MACRO poly ownership (per voice/lane) ==\n");
    {
        auto i = base(true,true);
        for (int v=0; v<15; ++v) for (int l=0;l<7;++l) i.eastPolyOwner[v][l] = true; // East owns all 7 poly lanes
        i.eastPolyOwner[2][1] = false;   // voice index 2 (=V4), octave ceded to Macro
        auto t = SandsTopology::build(i);
        CHECK(t.owner(3,1) == Role::MACRO,           "V4 octave ceded -> MACRO (voice 3 = poly idx 2)");
        CHECK(t.owner(3,0) == Role::EAST,            "V4 melody still EAST");
        CHECK(t.owner(1,0) == Role::EAST,            "V2 melody EAST");
        CHECK(t.owner(1,4) == Role::EAST,            "V2 ACCENT (lane4) is a poly lane -> EAST");
        CHECK(t.owner(1,5) == Role::EAST,            "V2 VAR (lane5) is a poly lane -> EAST (SANDS CONSOLIDATION Step 1)");
    }

    std::printf("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
