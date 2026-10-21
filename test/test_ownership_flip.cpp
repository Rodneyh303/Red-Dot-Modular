// test_ownership_flip.cpp — layer-2 rig pin (ZAI_PROMPT_delegation_decision.md §4):
// the 72nd suite. THE RULE (Rodney, 2026-10-21, verbatim) for everything resolver-routed:
//   1. East only → then east plays.
//   2. Macro only → macro plays.
//   3. Both → east plays for voice and lane delegated to east, otherwise macro plays
//      for the voice and lane.
// Everything here runs through the ONE store→topo fill,
// SandsTopology::Inputs::fillOwnersFromStore (plain-data, rig-visible):
//   F1. Owner-flip grid: 16 voices × 7 lanes × {eastPresent × macroPresent} × store
//       {0,1} — store 0→1 flips topo.owner() MACRO↔EAST (rule case 3); no-Macro ⇒
//       all-EAST (case 1); Macro-present ∧ store-0 ⇒ MACRO (case 2); MACRO_SOLE (no
//       East) ⇒ MACRO regardless of store (case 2's no-East form); neither ⇒ NONE.
//   F2. Store-domain pin: only 0.0/1.0 are meaningful (the ownership brief §3 — every
//       writer is 0/1-valued); the two domain values resolve OPPOSITE owners cellwise.
//   F3. Warp-derivation pin at pattern level: warpDelegated_[v][l] ==
//       (topo.owner(v,l)==MACRO) given the same Inputs — the manager's derivation rule
//       as an executable contract, so a polarity flip fails THIS suite, not ears. The
//       store flip propagates through re-derivation; counts are non-vacuous.
//   F4. No-Macro warp pin (rule case 1 at pattern level): macroPresent=false derives
//       delegated=false on ALL 112 cells even with a fully-Macro store — the state
//       direction used to read from the raw store and get wrong (layer-2 §3 option B
//       fixed the manager side; this pin holds the RULE).
// Honesty clause (brief §4): if a rule case were unreachable in the plain-data fill,
// pin the reachable subset and document — none was: all four presence states are
// expressible with plain-data Inputs.
//
// Companions: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp (the warp_*
// pattern — PatternEngine hosts warpDelegated_).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "SandsTopology.hpp"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

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

using dotModular::SandsTopology;
using Role = SandsTopology::Role;

// Expected owner under the 3-case rule for a cell — pure function of presence + store.
// This is the SPEC the fill+owner() pair must satisfy (ruleOwner is the suite's oracle).
static Role ruleOwner(bool east, bool macro, float storeCell) {
    if (!east && !macro) return Role::NONE;
    if (!east)           return Role::MACRO;   // case 2 (no East)
    if (!macro)          return Role::EAST;    // case 1
    return storeCell > 0.5f ? Role::EAST : Role::MACRO;  // case 3
}

int main() {
    std::cout << "\033[1mOwnership flip (layer-2 §4) Tests\033[0m\n" << std::string(58, '=') << "\n";

    // ── F1: the owner-flip grid ──────────────────────────────────────────────
    TEST("F1 owner-flip grid: 16x7 x {eastPresent x macroPresent} x store {0,1} — owner() follows the 3-case rule on every cell through fillOwnersFromStore", {
        for (int east = 0; east <= 1; ++east)
        for (int macro = 0; macro <= 1; ++macro)
        for (int sv = 0; sv <= 1; ++sv) {
            float store[112];
            for (int i = 0; i < 112; ++i) store[i] = (float)sv;   // F2: domain values only
            SandsTopology::Inputs in;
            in.eastPresent = east != 0; in.macroPresent = macro != 0;
            in.fillOwnersFromStore(store, east != 0, macro != 0);
            const SandsTopology topo = SandsTopology::build(in);
            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l)
                    EXPECTF(topo.owner(v, l) == ruleOwner(east != 0, macro != 0, store[v * 7 + l]),
                            "grid (east=%d macro=%d store=%d) cell (v=%d,l=%d): owner=%d",
                            east, macro, sv, v, l, (int)topo.owner(v, l));
        }
    });

    TEST("F1 store 0→1 flip: with East+Macro attached the flip turns MACRO↔EAST on every one of the 112 cells (the delegation toggle the whole feature rides on)", {
        float store0[112] = {0}, store1[112];
        for (int i = 0; i < 112; ++i) store1[i] = 1.0f;
        SandsTopology::Inputs in0; in0.eastPresent = true; in0.macroPresent = true;
        in0.fillOwnersFromStore(store0, true, true);
        SandsTopology::Inputs in1; in1.eastPresent = true; in1.macroPresent = true;
        in1.fillOwnersFromStore(store1, true, true);
        const SandsTopology t0 = SandsTopology::build(in0);
        const SandsTopology t1 = SandsTopology::build(in1);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                EXPECTF(t0.owner(v, l) == Role::MACRO && t1.owner(v, l) == Role::EAST,
                        "flip cell (v=%d,l=%d): store0→%d store1→%d (want MACRO then EAST)",
                        v, l, (int)t0.owner(v, l), (int)t1.owner(v, l));
    });

    // ── F2: the store-domain pin ─────────────────────────────────────────────
    TEST("F2 store-domain pin: the writable domain is {0.0, 1.0} (every writer is 0/1-valued) — the two domain values resolve OPPOSITE owners on all 112 cells, east+macro", {
        float store0[112] = {0}, store1[112];
        for (int i = 0; i < 112; ++i) store1[i] = 1.0f;
        SandsTopology::Inputs a; a.eastPresent = true; a.macroPresent = true;
        a.fillOwnersFromStore(store0, true, true);
        SandsTopology::Inputs b; b.eastPresent = true; b.macroPresent = true;
        b.fillOwnersFromStore(store1, true, true);
        const SandsTopology ta = SandsTopology::build(a);
        const SandsTopology tb = SandsTopology::build(b);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                EXPECTF((ta.owner(v, l) == Role::MACRO) != (tb.owner(v, l) == Role::MACRO),
                        "domain cell (v=%d,l=%d): 0.0 and 1.0 must resolve opposite", v, l);
    });

    // ── F3: the warp-derivation pin (pattern level) ──────────────────────────
    TEST("F3 warp-derivation pin: warpDelegated_[v][l] == (topo.owner(v,l)==MACRO) for all 112 cells given the same Inputs — and the store flip propagates through re-derivation (non-vacuous: both flags occur)", {
        SequencerEngine se;
        // Store model: East owns everything except three ceded cells (V1 melody, V4
        // octave, V16 legato — the V1-vs-poly edge rows the old bugs lived on).
        float store[112];
        for (int i = 0; i < 112; ++i) store[i] = 1.0f;
        store[0 * 7 + 0] = 0.0f;
        store[3 * 7 + 1] = 0.0f;
        store[15 * 7 + 6] = 0.0f;
        auto derive = [&se](const SandsTopology& topo) {
            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l)
                    se.pe.setWarpDelegated(v, l, topo.owner(v, l) == Role::MACRO);
        };
        SandsTopology::Inputs in; in.eastPresent = true; in.macroPresent = true;
        in.fillOwnersFromStore(store, true, true);
        const SandsTopology topo = SandsTopology::build(in);
        derive(topo);
        int deleg = 0;
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                const bool want = (topo.owner(v, l) == Role::MACRO);
                EXPECTF(se.pe.warpDelegated_[v][l] == want,
                        "derivation cell (v=%d,l=%d): warpDelegated_=%d want %d",
                        v, l, (int)se.pe.warpDelegated_[v][l], (int)want);
                deleg += want ? 1 : 0;
            }
        EXPECTF(deleg == 3, "non-vacuous: want exactly the 3 ceded cells delegated, got %d", deleg);
        // Flip: V4 octave reclaimed by East, V2 QMIX ceded — re-derive, matrix follows.
        store[3 * 7 + 1] = 1.0f;
        store[1 * 7 + 2] = 0.0f;
        SandsTopology::Inputs in2; in2.eastPresent = true; in2.macroPresent = true;
        in2.fillOwnersFromStore(store, true, true);
        const SandsTopology topo2 = SandsTopology::build(in2);
        derive(topo2);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                EXPECTF(se.pe.warpDelegated_[v][l] == (topo2.owner(v, l) == Role::MACRO),
                        "post-flip cell (v=%d,l=%d): warpDelegated_=%d want %d",
                        v, l, (int)se.pe.warpDelegated_[v][l],
                        (int)(topo2.owner(v, l) == Role::MACRO));
        EXPECT(se.pe.warpDelegated_[3][1] == false);   // reclaimed: flips back to East
        EXPECT(se.pe.warpDelegated_[1][2] == true);    // newly ceded: flips to Macro
    });

    // ── F4: no-Macro warp pin (rule case 1 at pattern level) ─────────────────
    TEST("F4 no-Macro warp (rule case 1): macroPresent=false derives delegated=false on ALL 112 cells even with a fully-Macro store — warp (and, post-layer-2 §3 option B, direction) play East's own in the no-Macro state", {
        SequencerEngine se;
        float store[112];
        for (int i = 0; i < 112; ++i) store[i] = 0.0f;   // "Macro owns everything" per raw store
        SandsTopology::Inputs in; in.eastPresent = true; in.macroPresent = false;
        in.fillOwnersFromStore(store, true, false);      // store is UNREAD (short-circuit)
        const SandsTopology topo = SandsTopology::build(in);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                EXPECT(topo.owner(v, l) == Role::EAST);
                se.pe.setWarpDelegated(v, l, topo.owner(v, l) == Role::MACRO);
                EXPECTF(se.pe.warpDelegated_[v][l] == false,
                        "no-Macro cell (v=%d,l=%d): warpDelegated_ must be false (rule case 1)",
                        v, l);
            }
    });

    std::cout << std::string(58, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
