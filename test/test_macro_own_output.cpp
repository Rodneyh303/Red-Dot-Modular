// test_macro_own_output.cpp — CC_PROMPT_macro_output_east_leak: the BACKWARDS-delegation
// contract — East's spread on an East-DELEGATED lane must NOT reach Macro's probability
// OUTPUT (the bars were already one-way-fixed; the out was not).
//
// CONTEXT (Rodney's repro): lane delegated to EAST → moving East's SPREAD moves East's
// bars (correct, East owns it) AND Macro's probability CV OUTPUT on the scope (WRONG),
// but NOT Macro's bars (already fixed). Root cause: the bars read macroOwnProbability
// (Macro's OWN base + Macro's OWN spread) but the out read the SHARED finalRandomByStrand
// → warpRow → random_, which East's spread WRITES on an East-owned lane. Bars = Macro-own,
// out = East-contaminated → they disagree, and the output leaks East backwards.
//
// THE CONTRACT (who-responds, at the SOURCE level — NOT bars==output values; the module's
// S&H latch + scale are rig-invisible and intentionally NOT asserted here. Bars stay
// continuous, out stays latched — the fix is the SOURCE, not the timing):
//   C1. Lane delegated to EAST: the East-spread write (simulated DIRECTLY — the manager is
//       rig-invisible: applyMono on the mono row + cachedPolySpread/refreshPolyRandomCell
//       on the poly rows, exactly what MonsoonSandsManager's spread pass does) changes
//       EAST's/shared consumers (masterLaneProbability, finalRandomByStrand, warpRow) but
//       changes NEITHER Macro's bars-source NOR Macro's OUTPUT. The OUT is asserted through
//       VoiceResolver::laneProbabilityAtStep — the exact call StraitsSandsMacroVisual::
//       process() makes — so this section is RED on HEAD (the out moved) and GREEN after.
//   C2. Lane delegated to MACRO: East's own-slot edit (the same raw write — the value
//       East's spread pass WOULD land) still moves the shared rows but changes NEITHER
//       Macro's bars-source NOR Macro's output (Macro overrides; East inert). The
//       "Macro drives" half of this case is pinned in C3 (the accessor + the store the
//       module publishes — on a Macro-delegated lane the manager's write IS Macro's spread).
//   C3. (Post-fix — a function TEMPLATE gated on the accessor existing via if-constexpr +
//       requires, so this file still COMPILES+RUNS on a pre-fix tree, where it reports
//       "absent" and C1/C2 are already RED.) ONE accessor, both consumers: the out IS
//       eng.macroOwnLaneProbability (bit-exact routing identity); Macro's own spread
//       drives it for the poly voices (the delegating voices' out) and leaves V1
//       spread-invariant (the anchor self-target no-op); the accessor is invariant under
//       the East write; and it reproduces the OLD bars formula bit-exactly (the bars'
//       values do not change — only the out's source is fixed).
//   C4. STRUCTURAL (the B3 "grep in test" precedent from test_warp_voice_isolation): the
//       routing pins — the resolver's laneProbabilityAtStep routes to macroOwnLaneProbability
//       (NOT the shared masterLaneProbability/polyLaneProbabilityAtStep), the widget's
//       macroOwnProbability delegates to the SAME engine accessor, and the module
//       publishes macroOwnSpread (the one spread source).
//
// Companion sources: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp
// (the test_warp_voice_isolation pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "SpreadInterp.hpp"
#include "VoiceResolver.hpp"
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

// Deterministic, well-spread [0,1) values (golden-ratio scramble — same basis as the warp
// suites), applied to the PRE-spread slewed buffers and published. pubSlewed* is the
// Macro-own base; random_ starts as the spread-0 shared final (== the base). Distinct
// per (voice,lane,step) so any spread/cross-voice mixture is maximally distinguishable.
static void fillSlewedGolden(PatternEngine& pe) {
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s) {
                const long idx = (long)v * 7 * 16 + l * 16 + s;
                const float u = (float)std::fmod(0.125 + idx * 0.6180339887498949, 1.0);
                switch (l) {
                    case PatternEngine::PL_MELODY:    pe.slewedMelody[v][s] = u; break;
                    case PatternEngine::PL_OCTAVE:    pe.slewedOctave[v][s] = u; break;
                    case PatternEngine::PL_QMIX:      pe.slewedQmix[v][s] = u; break;
                    case PatternEngine::PL_REST:      pe.slewedRhythm[v][s] = u; break;
                    case PatternEngine::PL_ACCENT:    pe.slewedAccent[v][s] = u; break;
                    case PatternEngine::PL_VARIATION: pe.slewedVariation[v][s] = u; break;
                    default:                          pe.slewedLegato[v][s] = u; break;
                }
                pe.random_[v][l][s] = u;   // spread-0 shared final == the base
            }
    pe.publishSlewedRhythm();   // rhythm + accent + variation + legato
    pe.publishSlewedMelody();   // melody + octave
    pe.publishSlewedQmix();
}

// Simulate the manager's East-spread pass on `lane` (the manager is rig-invisible — these
// are the exact writes it performs, at the engine's own seams):
//   • the CONTROL-RATE mono write: polyRandom(0,lane)[i] = applyMono(pe, lane, i, spread)
//     with the OWNER's spread from the engine spread store (MonsoonSandsManager's pass);
//   • the STEP-RATE poly writes: cachedPolySpread[v][lane] + refreshPolyRandomCell (which
//     also refreshes the warp mirror cell — the full rig-visible spread path).
// NEGATIVE spread so the mono row actually moves (applyMono self-targets under anchor-V1:
// positive spread is a no-op on V1, negative inverts toward the complement) while the poly
// rows move toward V1 either way.
static void eastSpreadWrite(SequencerEngine& se, int lane, float eastSpread) {
    PatternEngine& pe = se.pe;
    pe.cachedSpreadInitialized = true;   // refreshPolyRandomCell's gate (set in sync())
    se.spreadERef(0, lane) = eastSpread; // the store holds the OWNER's spread on this lane
    for (int i = 0; i < 16; ++i)
        pe.polyRandom(0, lane)[i] = redDot::SpreadInterp::applyMono(pe, lane, i, eastSpread);
    for (int v = 1; v < 16; ++v) {
        pe.cachedPolySpread[v][lane] = eastSpread;
        for (int i = 0; i < 16; ++i) se.refreshPolyRandomCell(v, lane, i);
    }
}

// ── C4 helpers: the structural "grep in test" ─────────────────────────────────
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

// ── C3: the ONE accessor, both consumers ──────────────────────────────────────
// A function TEMPLATE on purpose: the if-constexpr/requires guard makes the dependent
// branch NOT INSTANTIATE when the accessor is absent, so this file compiles on a pre-fix
// tree (where C1/C2 already fail — the RED). On the fixed tree the branch runs.
template <typename Eng>
static void macroAccessorContract(Eng& se, int& absent) {
    if constexpr (requires { se.macroOwnLaneProbability(PatternEngine::PL_MELODY, 0, true, 0); }) {
        const int LANE = PatternEngine::PL_MELODY;
        dotModular::VoiceResolver resolver(se);
        // Baseline (Macro's own spread = 0 — the store default before the module publishes).
        float before[16][16];
        for (int s = 0; s < 16; ++s) {
            before[0][s] = se.macroOwnLaneProbability(LANE, s, /*mono=*/true,  /*polyVoice=*/0);
            for (int v = 1; v < 16; ++v)
                before[v][s] = se.macroOwnLaneProbability(LANE, s, /*mono=*/false, /*polyVoice=*/v);
        }
        // (a) East-write invariance at the accessor — the BARS' property (Rodney: the bars
        // don't move), now the out's too: the accessor reads pubSlewed* + macroOwnSpread,
        // never the East-written shared random_/warpRow.
        eastSpreadWrite(se, LANE, -0.8f);
        for (int s = 0; s < 16; ++s) {
            EXPECTF(se.macroOwnLaneProbability(LANE, s, true, 0) == before[0][s],
                    "V1 accessor moved under the East write (step %d: %.9g -> %.9g)", s,
                    (double)before[0][s], (double)se.macroOwnLaneProbability(LANE, s, true, 0));
            for (int v = 1; v < 16; ++v)
                EXPECTF(se.macroOwnLaneProbability(LANE, s, false, v) == before[v][s],
                        "poly accessor moved under the East write (v=%d step %d: %.9g -> %.9g)",
                        v, s, (double)before[v][s],
                        (double)se.macroOwnLaneProbability(LANE, s, false, v));
        }
        // (b) Macro's OWN spread drives the accessor for the POLY voices (the delegating
        // voices' bars + out on a Macro-delegated lane); V1 stays spread-invariant (the
        // anchor self-target no-op — matches East's V1 display). The store write stands in
        // for the module's publish (macroBase[l][3] + macroPostDelta[l][3], clamped — the
        // SAME expression the manager's MACRO_SOLE branch uses).
        se.macroOwnSpread[LANE] = 0.6f;
        int moved = 0;
        for (int v = 1; v < 16; ++v)
            for (int s = 0; s < 16; ++s)
                if (se.macroOwnLaneProbability(LANE, s, false, v) != before[v][s]) ++moved;
        EXPECTF(moved == 15 * 16, "Macro's own spread moved only %d/240 poly cells", moved);
        for (int s = 0; s < 16; ++s)
            EXPECTF(se.macroOwnLaneProbability(LANE, s, true, 0) == before[0][s],
                    "V1 (anchor) moved under Macro's own spread (step %d)", s);
        // (c) ROUTING IDENTITY — the OUT is the SAME source the bars read: the resolver's
        // call (what the module's process() invokes) is bit-exactly the accessor. This
        // asserts ONE SOURCE, not bars==latched-out values (the module's S&H latch and
        // scale live downstream, rig-invisible, and are untouched by design).
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s) {
                const bool mono = dotModular::VoiceResolver::isMono(v);
                const float want = mono
                    ? se.macroOwnLaneProbability(LANE, s, true, 0)
                    : se.macroOwnLaneProbability(LANE, s, false,
                                                  dotModular::VoiceResolver::polyVoiceSlot(v));
                EXPECTF(resolver.laneProbabilityAtStep(v, LANE, s) == want,
                        "out != accessor (voice=%d step=%d: %.9g vs %.9g) — two sources",
                        v, s, (double)resolver.laneProbabilityAtStep(v, LANE, s), (double)want);
            }
        // (d) Regression guard — the accessor reproduces the OLD bars formula bit-exactly
        // (pubSlewed base + Macro spread via SpreadInterp + warpDisplayMix): the BARS'
        // values do not change; only the out's source is fixed.
        for (int s = 0; s < 16; ++s)
            for (int v = 0; v < 16; ++v) {
                const float sp = se.macroOwnSpread[LANE];
                const float base = redDot::SpreadInterp::polySlewedPub(se.pe, LANE, v, s);
                const float disp = (v == 0)
                    ? redDot::SpreadInterp::interpolate(base, base, sp)
                    : redDot::SpreadInterp::applyAnchorV1Only(se.pe, LANE, s, base, sp);
                const float want = se.pe.warpDisplayMix(v, LANE, (double)disp);
                const float got = (v == 0) ? se.macroOwnLaneProbability(LANE, s, true, 0)
                                           : se.macroOwnLaneProbability(LANE, s, false, v);
                EXPECTF(got == want, "old-bars-formula mismatch (v=%d step=%d: %.9g vs %.9g)",
                        v, s, (double)got, (double)want);
            }
    } else {
        ++absent;
        std::cout << "  \033[33m?\033[0m (engine accessor macroOwnLaneProbability absent — "
                     "pre-fix build; C1/C2 carry the RED)\n";
    }
}

int main() {
    std::cout << "\033[1mMacro-own output (CC_PROMPT_macro_output_east_leak) Tests\033[0m\n"
              << std::string(62, '=') << "\n";
    const int MEL = PatternEngine::PL_MELODY, OCT = PatternEngine::PL_OCTAVE;

    // ── C1: EAST-delegated lane — the Rodney repro, at the source level ──────────
    TEST("C1 EAST-delegated lane: the East-spread write moves the SHARED/East consumers but NOT Macro's prob-out (any voice, any step)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        dotModular::VoiceResolver resolver(se);
        // Baseline: Macro's OUT for every voice at every step — the exact call the module's
        // process() makes (channel ch carries voice ch+1; ch0 = V1/mono).
        float outBefore[16][16];
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s)
                outBefore[v - 1][s] = resolver.laneProbabilityAtStep(v, MEL, s);
        float frBefore[16], mpBefore = se.masterLaneProbability(MEL);
        for (int s = 0; s < 16; ++s) frBefore[s] = se.pe.finalRandomByStrand(MEL, s);
        // ── East's SPREAD edit on the East-owned lane (the manager's pass, simulated) ──
        eastSpreadWrite(se, MEL, -0.8f);
        // Sanity: the write TOOK — the shared final (what the engine consumes and East's
        // own V1 out reports) moved. If this fails the simulation is vacuous.
        int moved = 0;
        for (int s = 0; s < 16; ++s)
            if (se.pe.finalRandomByStrand(MEL, s) != frBefore[s]) ++moved;
        EXPECTF(moved == 16, "vacuous simulation: finalRandomByStrand moved only %d/16 steps", moved);
        EXPECTF(se.masterLaneProbability(MEL) != mpBefore,
                "vacuous simulation: masterLaneProbability did not move");
        // THE CONTRACT: Macro's OUT is UNCHANGED — East never reaches it (one-way
        // borrowing: Macro never borrows East). RED on HEAD: the out read the shared.
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s)
                EXPECTF(resolver.laneProbabilityAtStep(v, MEL, s) == outBefore[v - 1][s],
                        "LEAK: Macro's prob-out moved under East's spread write (voice=%d "
                        "step=%d: %.9g -> %.9g)",
                        v, s, (double)outBefore[v - 1][s],
                        (double)resolver.laneProbabilityAtStep(v, MEL, s));
    });

    // ── C2: MACRO-delegated lane — East's own-slot edit is inert on Macro ────────
    TEST("C2 MACRO-delegated lane: East's own-slot edit moves East's shared rows but NOT Macro's prob-out (Macro overrides; East inert)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        dotModular::VoiceResolver resolver(se);
        float outBefore[16][16], rowBefore[16][16];
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s) {
                outBefore[v - 1][s] = resolver.laneProbabilityAtStep(v, OCT, s);
                rowBefore[v - 1][s] = se.pe.warpRow(v - 1, OCT)[s];   // East's per-slot rows
            }
        // East's own-slot edit on the Macro-delegated lane: the raw write East's spread
        // pass WOULD land (the manager must NOT apply it — Macro owns the lane — but the
        // contract is at the SOURCE: even a raw shared-row write must not move Macro).
        eastSpreadWrite(se, OCT, -0.9f);
        // Sanity: East's own-slot rows DID move (the edit landed in the shared storage).
        int rowMoved = 0;
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s)
                if (se.pe.warpRow(v - 1, OCT)[s] != rowBefore[v - 1][s]) ++rowMoved;
        EXPECTF(rowMoved == 16 * 16, "vacuous simulation: only %d/256 shared cells moved", rowMoved);
        // THE CONTRACT: Macro's OUT is UNCHANGED under East's own-slot edit.
        for (int v = 1; v <= 16; ++v)
            for (int s = 0; s < 16; ++s)
                EXPECTF(resolver.laneProbabilityAtStep(v, OCT, s) == outBefore[v - 1][s],
                        "LEAK: Macro's prob-out moved under East's own-slot edit (voice=%d "
                        "step=%d: %.9g -> %.9g)",
                        v, s, (double)outBefore[v - 1][s],
                        (double)resolver.laneProbabilityAtStep(v, OCT, s));
    });

    // ── C3: ONE accessor, both consumers (post-fix; gated — see above) ───────────
    TEST("C3 ONE accessor both consumers: out == macroOwnLaneProbability bit-exact; Macro's spread drives poly voices, V1 anchor-invariant; East-write invariant; old-bars-formula bit-exact", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        int absent = 0;
        macroAccessorContract(se, absent);
        EXPECTF(absent == 0,
                "engine accessor macroOwnLaneProbability absent (pre-fix build)");
    });

    // ── C4: STRUCTURAL routing pins (the B3 "grep in test" precedent) ────────────
    TEST("C4 resolver routing: laneProbabilityAtStep (Macro's prob-out seam) routes to the ONE Macro-own accessor, not the shared master/poly funnels", {
        const std::string src = slurp(repoRoot() + "/src/dsp/VoiceResolver.hpp");
        const size_t fn = src.find("float laneProbabilityAtStep(");
        EXPECTF(fn != std::string::npos, "VoiceResolver.hpp: laneProbabilityAtStep not found");
        const size_t end = src.find("}", fn);
        EXPECTF(end != std::string::npos, "VoiceResolver.hpp: laneProbabilityAtStep body not found");
        const std::string body = src.substr(fn, end - fn);
        EXPECTF(contains(body, "macroOwnLaneProbability("),
                "the prob-out still routes to the SHARED final — no macroOwnLaneProbability "
                "call in laneProbabilityAtStep, so East's spread on an East-owned lane "
                "reaches Macro's out (the backwards-delegation leak)");
        EXPECTF(!contains(body, "masterLaneProbability(") && !contains(body, "polyLaneProbabilityAtStep("),
                "laneProbabilityAtStep still reads the shared master/poly funnels");
    });

    TEST("C4 bars + publish: the widget's macroOwnProbability delegates to the SAME engine accessor; the module publishes macroOwnSpread (the one spread source)", {
        const std::string src = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        const size_t fn = src.find("float macroOwnProbability(");
        EXPECTF(fn != std::string::npos, "StraitsSandsMacroVisual.cpp: macroOwnProbability not found");
        const size_t end = src.find("}", fn);
        EXPECTF(end != std::string::npos, "StraitsSandsMacroVisual.cpp: macroOwnProbability body not found");
        const std::string body = src.substr(fn, end - fn);
        EXPECTF(contains(body, "macroOwnLaneProbability("),
                "the bars do not route through the ONE engine accessor — bars and out are "
                "two resolutions that can diverge");
        EXPECTF(contains(src, "macroOwnSpread["),
                "the module does not publish macroOwnSpread — the engine store has no source");
    });

    std::cout << std::string(62, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
