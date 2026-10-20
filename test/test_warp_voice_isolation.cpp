// test_warp_voice_isolation.cpp — CC_PROMPT_07: per-voice warp isolation + the delegation
// source contract (the Macro→East leak / V1-vs-V2+ asymmetry bug class).
//
// CONTEXT (the Rack symptom fingerprint this pins):
//   1. EAST, delegated lanes, V2+: warp has NO effect.
//   2. MACRO: warp has NO effect on V1; DOES affect V2+.
//   3. MACRO warp changes EAST's probabilities for V2+ — on delegated AND non-delegated lanes.
//
// ROOT CAUSE (verified by reading — NOT the slot-vs-polyBank bank off-by-one the prompt
// suspected; the bank/write/read indices are voiceSlot-consistent everywhere):
// MonsoonSandsManager::processDNA built its SandsTopology from fillPresence ONLY —
// presence flags, no ownership rows — so SandsTopology::owner() read the Inputs DEFAULTS
// (eastV1Owner = all-true ⇒ V1 never delegated; eastPolyOwner = all-false ⇒ EVERY poly
// (voice,lane) Macro-owned). warpDelegated_[v][l] therefore went true for ALL V2+ on ALL
// lanes: East's own-slot warp was bypassed (symptom 1), Macro's global reached every poly
// voice (symptoms 2b/3) but never V1 (symptom 2a). Two same-family display off-by-ones
// compounded the perception: Macro's macroOwnProbability added +1 to an already-voiceSlot
// index (V2's bars showed V3's row), and East's buildTopo live-override wrote
// eastPolyOwner[cv - 1] (the PREVIOUS voice's row).
//
// Sections:
//   B1. ENGINE PER-VOICE ISOLATION: distinct East curve+w on EVERY (slot,lane) cell + a
//       distinct Macro-global curve+w on every lane + a per-slot delegation matrix where
//       slot 0 (V1) and slot 1 (V2) carry OPPOSITE flags (the pair the symptoms
//       distinguish). Assert warpRow(s,l) returns EXACTLY the (F,w) its delegation
//       resolves to — own-slot curve for non-delegated, Macro's global for delegated —
//       for all 16×7×16 cells, byte-equal to the independently computed mix. Any
//       cross-voice leak or V1/V2 swap fails.
//   B2. THE DELEGATION-SOURCE CONTRACT: a store-faithful Inputs (hand-filled rows — the
//       MODEL of what the manager must feed owner()) routes warp per-voice correctly:
//       V1 ceded → Macro's curve; V2 East-owned → V2's OWN curve (Macro's global NEVER
//       appears); V3 ceded → Macro's; a fully East-owned lane shows NO Macro curve in ANY
//       slot; a fully ceded lane shows Macro's in every slot. Engine wiring mirrors the
//       manager's loop (warpDelegated_[v][l] = owner(v,l)==MACRO).
//   B3. STRUCTURAL (RED on HEAD — the A5 "grep in test" precedent):
//       - MonsoonSandsManager.cpp must fill the owner rows (fillOwners) between its
//         `Inputs topoIn` and `SandsTopology::build(topoIn)` — on HEAD it does NOT
//         (presence-only topology ⇒ the defaults ⇒ the leak).
//       - MonsoonExpanderManager.cpp routes its STEP 5b owner fill through the SAME
//         one implementation (fillOwners).
//       - The two display off-by-ones are gone: no "polyVoice + 1" in
//         StraitsSandsMacroVisual.cpp, no "eastPolyOwner[cv - 1]" in
//         StraitsEastSandsVisual.cpp.
//
// Companion sources: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp
// (the test_warp_all_lanes pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "WarpF.hpp"
#include "SandsTopology.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _msg; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _msg = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
           if (!_msg.empty()) std::cout << " — " << _msg; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)
#define EXPECTF(e, ...) do { if (!(e)) { char _b[256]; std::snprintf(_b, sizeof _b, __VA_ARGS__); \
    throw std::runtime_error(std::string("EXPECT(" #e ") failed: ") + _b); } } while (0)

// Deterministic, well-spread [0,1) values across the whole store (golden-ratio
// scramble) — same basis as test_warp_all_lanes so the suites share a foundation.
static void fillRandomGolden(PatternEngine& pe) {
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s) {
                const long idx = (long)v * 7 * 16 + l * 16 + s;
                pe.random_[v][l][s] = (float)std::fmod(0.125 + idx * 0.6180339887498949, 1.0);
            }
    pe.random_[0][0][0] = 0.f;
    pe.random_[0][0][1] = 1e-7f;
    pe.random_[15][6][15] = 1.f - 1e-7f;
}

// ── B3 helpers: the structural "grep in test" ─────────────────────────────────
static std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}
static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
static std::string repoRoot() {
    static const char* candidates[] = { ".", "..", "../.." };
    for (const char* m : candidates) {
        std::ifstream f(std::string(m) + "/src/dsp/engines/SequencerEngine.cpp", std::ios::binary);
        if (f) return m;
    }
    throw std::runtime_error("repo root not found — run via test/run_all.sh from the repo");
}

int main() {
    std::cout << "\033[1mWarp voice isolation (CC_PROMPT_07) Tests\033[0m\n" << std::string(58, '=') << "\n";
    using dotModular::WarpBank;
    using dotModular::SandsTopology;
    const int MEL = PatternEngine::PL_MELODY, REST = PatternEngine::PL_REST;

    // ── B1: per-voice isolation across the FULL 16×7 matrix, V1/V2 opposite flags ──
    TEST("B1 full-matrix isolation: every (slot,lane) row returns ITS delegation-resolved mix — own curve when non-delegated, Macro-global when delegated (slot 0 and slot 1 carry OPPOSITE flags)", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        // Delegation matrix per (slot,lane): alternating per lane phase so slot 0 (V1)
        // and slot 1 (V2) always differ; lanes 1/2 pin the all-East / all-Macro extremes.
        // delegated[s][l]:
        bool delegated[16][7];
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s) {
                if (l == 1) delegated[s][l] = false;        // fully East-owned lane
                else if (l == 2) delegated[s][l] = true;    // fully ceded lane
                else delegated[s][l] = ((s + l) % 2) == 0;  // alternating; s=0 vs s=1 differ
            }
        for (int s = 0; s < 16; ++s) {
            EXPECT(delegated[0][0] != delegated[1][0]);     // V1/V2 opposite, lane 0
            EXPECT(delegated[0][3] != delegated[1][3]);     // ... and lane 3
        }
        // Distinct East curve+w per (slot,lane); distinct Macro-global curve+w per lane.
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l) {
                bank.warpF[WarpBank::index(s, l)] = dotModular::TriangularWarp{0.05 * s + 0.05 * l + 0.02};
                bank.warpW[WarpBank::index(s, l)] = 1.f;    // full curve: maximally distinguishable
            }
        for (int l = 0; l < 7; ++l) {
            bank.globalWarpF[l] = dotModular::PowerWarp{2.0 + 0.25 * l};
            bank.globalWarpW[l] = 1.f;
        }
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l)
                se.pe.setWarpDelegated(s, l, delegated[s][l]);
        se.pe.rebuildWarpMirrorAll();
        // Sweep: each cell must equal ITS independently computed effective mix.
        int ownCells = 0, delCells = 0;
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l)
                for (int st = 0; st < 16; ++st) {
                    const float u = se.pe.random_[s][l][st];
                    const float expected = (float)bank.warpedValue(s, l, (double)u, delegated[s][l]);
                    const float got = se.pe.warpRow(s, l)[st];
                    EXPECTF(got == expected,
                            "cell (s=%d,l=%d,st=%d): warpRow %.9g != expected %.9g (raw %.9g, delegated=%d)",
                            s, l, st, (double)got, (double)expected, (double)u, (int)delegated[s][l]);
                    delegated[s][l] ? ++delCells : ++ownCells;
                }
        EXPECTF(ownCells > 400 && delCells > 400, "vacuous matrix (own=%d del=%d)", ownCells, delCells);
        // And the delegation mix genuinely differs from the own-slot mix in these rows
        // (non-vacuous distinction): at least one step where own != global mixture.
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l) {
                if (delegated[s][l]) continue;
                int diffs = 0;
                for (int st = 0; st < 16; ++st) {
                    const double u = (double)se.pe.random_[s][l][st];
                    if (bank.warpedValue(s, l, u, true) != bank.warpedValue(s, l, u, false)) ++diffs;
                }
                EXPECTF(diffs > 0, "vacuous: own vs global mix identical for (s=%d,l=%d) — curves not distinguishable", s, l);
            }
    });

    TEST("B1 V1/V2 focus pair (lane MELODY): V1 delegated → Macro's curve; V2 East-owned → V2's OWN curve, never Macro's (CV-out funnel agrees with warpRow)", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        se.pe.setWarpDelegated(0, MEL, true);    // V1 (slot 0) ceded to Macro
        se.pe.setWarpDelegated(1, MEL, false);   // V2 (slot 1) East-owned
        bank.warpF[WarpBank::index(1, MEL)] = dotModular::TriangularWarp{0.3};   // V2's own curve
        bank.warpW[WarpBank::index(1, MEL)] = 1.f;
        bank.globalWarpF[MEL] = dotModular::PowerWarp{3.0};                      // Macro's global
        bank.globalWarpW[MEL] = 1.f;
        se.pe.rebuildWarpMirrorAll();
        for (int st = 0; st < 16; ++st) {
            const double u0 = (double)se.pe.random_[0][MEL][st];
            const double u1 = (double)se.pe.random_[1][MEL][st];
            const float macroMix0 = (float)bank.warpedValue(0, MEL, u0, true);
            const float eastMix1   = (float)bank.warpedValue(1, MEL, u1, false);
            const float macroMix1  = (float)bank.warpedValue(1, MEL, u1, true);
            EXPECTF(se.pe.warpRow(0, MEL)[st] == macroMix0,
                    "V1 step %d: warpRow %.9g != Macro-global mix %.9g", st,
                    (double)se.pe.warpRow(0, MEL)[st], (double)macroMix0);
            EXPECTF(se.pe.warpRow(1, MEL)[st] == eastMix1,
                    "V2 step %d: warpRow %.9g != V2's OWN mix %.9g", st,
                    (double)se.pe.warpRow(1, MEL)[st], (double)eastMix1);
            if (eastMix1 != macroMix1)
                EXPECTF(se.pe.warpRow(1, MEL)[st] != macroMix1,
                        "V2 step %d: LEAK — warpRow equals Macro's global mix (%.9g)", st, (double)macroMix1);
            // The CV-out funnel (what East's panel meters report) reads the same rows.
            EXPECTF(se.polyLaneProbabilityAtStep(MEL, 1, st) == eastMix1,
                    "V2 step %d: CV-out funnel %.9g != own mix %.9g", st,
                    (double)se.polyLaneProbabilityAtStep(MEL, 1, st), (double)eastMix1);
        }
    });

    // ── B2: the delegation-SOURCE contract (store-faithful topology ⇒ correct routing) ──
    TEST("B2 store-faithful Inputs: owner() answers per the store (V1 ceded / V2 East / V3 ceded); engine wiring routes warp accordingly — Macro's curve appears ONLY in ceded cells, NEVER in an East-owned cell", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        // The owner STORE model (what getMacroOwn(v,l) > 0.5 returns; voiceSlot rows).
        bool store[16][7];
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) store[v][l] = true;     // default East owns
        store[0][0] = false;   // V1 cedes lane 0
        store[2][0] = false;   // V3 cedes lane 0
        store[5][0] = false;   // V6 cedes lane 0
        store[15][0] = false;  // V16 cedes lane 0
        // lane 1 stays fully East-owned; lane 2 fully ceded; others mixed.
        for (int v = 0; v < 16; ++v) { store[v][2] = false; store[v][3] = (v % 2) == 1; }
        store[1][0] = true;    // V2 owns lane 0 (the symptom-distinguishing pair: V1 ceded, V2 not)
        // Hand-fill the Inputs EXACTLY as the (fixed) manager must: presence + owner rows.
        SandsTopology::Inputs in;
        in.eastPresent = true; in.macroPresent = true;
        in.polyBaseActive = true; in.polyVoiceCount = 15;
        for (int l = 0; l < 7; ++l) in.eastV1Owner[l] = store[0][l];
        for (int v = 1; v < 16; ++v)
            for (int l = 0; l < 7; ++l) in.eastPolyOwner[v][l] = store[v][l];
        const SandsTopology topo = SandsTopology::build(in);
        // owner() must answer per the store — the contract the manager's topology broke
        // (presence-only build answered MACRO for EVERY poly cell regardless of store).
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                const SandsTopology::Role want = store[v][l] ? SandsTopology::Role::EAST
                                                             : SandsTopology::Role::MACRO;
                EXPECTF(topo.owner(v, l) == want, "owner(%d,%d): store says %s but topology says %d",
                        v, l, store[v][l] ? "EAST" : "MACRO", (int)topo.owner(v, l));
            }
        // Wire the engine exactly like the manager's population loop.
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                se.pe.setWarpDelegated(v, l, topo.owner(v, l) == SandsTopology::Role::MACRO);
        // Author: East's own distinct curve on EVERY slot; Macro-global distinct per lane.
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l) {
                bank.warpF[WarpBank::index(s, l)] = dotModular::TriangularWarp{0.03 * s + 0.04 * l + 0.01};
                bank.warpW[WarpBank::index(s, l)] = 1.f;
            }
        for (int l = 0; l < 7; ++l) {
            bank.globalWarpF[l] = dotModular::PowerWarp{1.5 + 0.5 * l};
            bank.globalWarpW[l] = 1.f;
        }
        se.pe.rebuildWarpMirrorAll();
        // Sweep: every cell matches its store-resolved mix; Macro's curve NEVER appears
        // in an East-owned cell (the leak); it DOES appear in every ceded cell.
        for (int s = 0; s < 16; ++s)
            for (int l = 0; l < 7; ++l)
                for (int st = 0; st < 16; ++st) {
                    const double u = (double)se.pe.random_[s][l][st];
                    const bool deleg = !store[s][l];
                    const float expected = (float)bank.warpedValue(s, l, u, deleg);
                    const float got = se.pe.warpRow(s, l)[st];
                    EXPECTF(got == expected,
                            "cell (s=%d,l=%d,st=%d): warpRow %.9g != store-resolved %.9g (store=%s)",
                            s, l, st, (double)got, (double)expected, store[s][l] ? "EAST" : "MACRO");
                    if (!store[s][l]) {
                        const float own = (float)bank.warpedValue(s, l, u, false);
                        if (own != expected)
                            EXPECTF(got != own, "LEAK (s=%d,l=%d,st=%d): ceded cell equals the OWN-slot mix", s, l, st);
                    } else {
                        const float glob = (float)bank.warpedValue(s, l, u, true);
                        if (glob != expected)
                            EXPECTF(got != glob, "LEAK (s=%d,l=%d,st=%d): East-owned cell equals Macro's GLOBAL mix", s, l, st);
                    }
                }
        // The lane-1 (fully East-owned) leak pin, stated directly: Macro's global mix
        // appears in NO slot's warpRow on that lane.
        for (int s = 0; s < 16; ++s) {
            int leaks = 0;
            for (int st = 0; st < 16; ++st) {
                const double u = (double)se.pe.random_[s][1][st];
                if (se.pe.warpRow(s, 1)[st] == (float)bank.warpedValue(s, 1, u, true)) ++leaks;
            }
            EXPECTF(leaks == 0, "lane 1 (all East-owned): slot %d shows Macro's global mix at %d steps", s, leaks);
        }
    });

    TEST("B2 MACRO_SOLE twin: no East present ⇒ Macro owns every voice (delegation all-true) — V1 and V2+ BOTH track Macro's global", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        SandsTopology::Inputs in;
        in.eastPresent = false; in.macroPresent = true;   // MACRO_SOLE: owner rows stay defaulted
        const SandsTopology topo = SandsTopology::build(in);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                EXPECT(topo.owner(v, l) == SandsTopology::Role::MACRO);
                se.pe.setWarpDelegated(v, l, true);
            }
        for (int l = 0; l < 7; ++l) {
            bank.globalWarpF[l] = dotModular::PowerWarp{2.5};
            bank.globalWarpW[l] = 1.f;
        }
        se.pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int st = 0; st < 16; ++st) {
                    const double u = (double)se.pe.random_[v][l][st];
                    EXPECTF(se.pe.warpRow(v, l)[st] == (float)bank.warpedValue(v, l, u, true),
                            "MACRO_SOLE cell (v=%d,l=%d,st=%d) does not track the global curve", v, l, st);
                }
    });

    // ── B3: STRUCTURAL — the RED-on-HEAD pins (the test_warp_all_lanes A5 precedent) ──
    TEST("B3 manager topology: MonsoonSandsManager fills the OWNER rows (fillOwners) between `Inputs topoIn` and SandsTopology::build — a presence-only build is the warp leak (defaults ⇒ all-poly Macro-owned, V1 never)", {
        const std::string src = slurp(repoRoot() + "/src/dsp/managers/MonsoonSandsManager.cpp");
        const size_t decl = src.find("SandsTopology::Inputs topoIn");
        EXPECTF(decl != std::string::npos, "MonsoonSandsManager.cpp: `SandsTopology::Inputs topoIn` not found");
        const size_t build = src.find("SandsTopology::build(topoIn)", decl);
        EXPECTF(build != std::string::npos, "MonsoonSandsManager.cpp: SandsTopology::build(topoIn) after the Inputs decl not found");
        const std::string between = src.substr(decl, build - decl);
        EXPECTF(contains(between, "fillOwners("),
                "presence-only topology: no fillOwners( between Inputs topoIn and build(topoIn) — "
                "owner() reads the struct defaults (eastV1Owner=all-true, eastPolyOwner=all-false) "
                "⇒ warpDelegated_ all-true for V2+ on every lane, V1 never delegated");
    });

    TEST("B3 one implementation: MonsoonExpanderManager's STEP 5b owner fill routes through the SAME fillOwners helper", {
        const std::string src = slurp(repoRoot() + "/src/dsp/managers/MonsoonExpanderManager.cpp");
        EXPECTF(contains(src, "fillOwners("),
                "MonsoonExpanderManager.cpp does not call fillOwners( — the owner fill is a second, "
                "hand-rolled implementation that can drift from the manager's");
    });

    TEST("B3 Macro bars off-by-one gone: no `polyVoice + 1` in StraitsSandsMacroVisual.cpp (macroOwnProbability receives voiceSlot; +1 double-shifted V2's bars onto V3's row)", {
        const std::string src = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        EXPECTF(!contains(src, "polyVoice + 1"),
                "StraitsSandsMacroVisual.cpp still shifts an already-voiceSlot index (`polyVoice + 1`) — "
                "Macro's V2 tab bars display V3's row");
    });

    TEST("B3 East lock-topo off-by-one gone: no `eastPolyOwner[cv - 1]` in StraitsEastSandsVisual.cpp (cv is already the voiceSlot; -1 wrote the PREVIOUS voice's owner row)", {
        const std::string src = slurp(repoRoot() + "/src/StraitsEastSandsVisual.cpp");
        EXPECTF(!contains(src, "eastPolyOwner[cv - 1]"),
                "StraitsEastSandsVisual.cpp still writes eastPolyOwner[cv - 1] — the live owner of the "
                "viewed voice lands on the previous voice's row in East's local lock topology");
    });

    std::cout << std::string(58, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
