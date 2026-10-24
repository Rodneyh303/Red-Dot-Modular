// test_sweepsig_gate.cpp — CC_PROMPT_restore_sweepsig_gate_START: the RESTORED prob-out
// sweep gate's contract suite. 70dd837 deleted the input-change fingerprint gate for a
// step-cache keyed on the step NUMBER (fired at every step edge, could skip nothing
// else); Rodney's bisect put Macro-only CPU back at ~3% (the gate's idle skip was the
// 0.4%). This suite pins the restored mechanism at the layer the rig can see:
//
//   G1 FIRES/SKIPS: the gate fires on EVERY fingerprinted input (step, pub cell at the
//      current step, the LIVE macroOwnSpread store, nV, bank pointer, each global warp
//      cell incl. tilt/atom + poolUsed) and skips when NOTHING changed — the core
//      skip-on-unchanged property, plus a 10k-iteration idle run with ZERO fires.
//   G2 THE DEFENSE (Rodney's ask — the exact fingerprint's load-bearing assumption):
//      the MacroOwn lens (VoiceResolver::laneProbabilityAtStep → effectiveProbability,
//      MacroOwn) does NOT read the East-authorable own-slot warp stores nor the
//      warpDelegated_ matrix (warpDisplayMixMacroOwn is delegated=TRUE pure selection —
//      test_warp_f_bank S4c pins the selection; this pins it END-TO-END through the
//      accessor the widget drives): perturbing them leaves the output bit-unchanged AND
//      the gate cold. With the counter-proof that a GLOBAL cell perturbation DOES move
//      the output (non-vacuous). This is why those fields are deliberately NOT in the
//      fingerprint: fingerprinting an input the resolve cannot read only re-runs the
//      resolve spuriously (the CPU cost) for zero correctness.
//   G3 BIT-IDENTITY: the gate only skips unchanged recomputes — over a randomized
//      change script (step/spread/pub/nV/global-cell mutations), whenever the gate
//      skips, an UNGATED re-resolve through the REAL accessor is bit-identical to the
//      held values. A missed input = stale output = this fails.
//   G4 STRUCTURAL (the B3 grep precedent — the module is rig-invisible): the restored
//      wiring in the widget: /32 probOutDiv + the gate decision + the resolve inside
//      the run + the out-epoch/write-elimination kept + the 70dd837 step-NUMBER
//      trigger gone + the deliberately-dropped fingerprint fields absent.
//
// Companion sources: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp (the
// test_ui_to_resolver pattern). The gate header IS the shipped logic (included by
// StraitsSandsMacroVisual.hpp) — no reimplementation beside it.
#include "test_stubs.hpp"
#include "SweepSigGate.hpp"     // SweepSig + sweepSigChanged/sweepSigCapture + pubSlewedLane_ (the REAL gate)
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "SpreadInterp.hpp"
#include "VoiceResolver.hpp"    // the prob-out seam the widget's resolve drives
#include "WarpF.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
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

// Deterministic, well-spread [0,1) values (golden-ratio scramble — the same basis as
// the warp suites), applied to the PRE-spread slewed buffers and published. pubSlewed*
// is the accessor's base. (The test_ui_to_resolver basis, reused verbatim.)
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

// The widget's exact resolve shape: ch 0..nV carry voices 1..nV+1 (ch0 → the mono
// voice), every channel Macro-OWN at the lane's step. [out lane][ch].
static void resolveAll(SequencerEngine& se, const int stepArr[7], int nV, float out[7][16]) {
    dotModular::VoiceResolver rr(se);
    for (int l = 0; l < 7; ++l)
        for (int ch = 0; ch <= nV; ++ch)
            out[l][ch] = rr.laneProbabilityAtStep(ch + 1, l, stepArr[l]);
}
static bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

// pubSlewed cell writer, by PL lane (the same switch polySlewedPub/pubSlewedLane_ use).
static void setPubCell(PatternEngine& pe, int lane, int v, int step, float val) {
    switch (lane) {
        case PatternEngine::PL_MELODY:    pe.pubSlewedPolyMelody[v][step] = val; break;
        case PatternEngine::PL_OCTAVE:    pe.pubSlewedPolyOctave[v][step] = val; break;
        case PatternEngine::PL_QMIX:      pe.pubSlewedPolyQmix[v][step] = val; break;
        case PatternEngine::PL_REST:      pe.pubSlewedPolyRhythm[v][step] = val; break;
        case PatternEngine::PL_ACCENT:    pe.pubSlewedPolyAccent[v][step] = val; break;
        case PatternEngine::PL_VARIATION: pe.pubSlewedPolyVariation[v][step] = val; break;
        default:                          pe.pubSlewedPolyLegato[v][step] = val; break;
    }
}

// Deterministic LCG (the test_monotone_curve pattern).
static uint32_t rngState = 0x12345678u;
static uint32_t rng32() { rngState = rngState * 1664525u + 1013904223u; return rngState >> 8; }

// ── Structural-pin helpers (the B3 precedent, test_ui_to_resolver's) ──────────────
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
    std::cout << "\033[1mSweepSig gate (CC_PROMPT_restore_sweepsig_gate_START) Contract Tests\033[0m\n"
              << std::string(70, '=') << "\n";

    // ── G1: fires on every fingerprinted input; skips when nothing changed ─────────
    TEST("G1 FIRES/SKIPS: every fingerprinted input trips the gate; identical inputs skip; 10k idle iterations = 0 fires", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        for (int l = 0; l < 7; ++l) se.macroOwnSpread[l] = 0.1f * l - 0.3f;
        int stepArr[7] = {0, 1, 2, 3, 4, 5, 6};
        const int nV = 3;
        // The flat-walk helper's indexing IS the 2D array (restored verbatim — pin it).
        EXPECT(sameBits(pubSlewedLane_(se.pe, PatternEngine::PL_MELODY)[1 * 16 + 3],
                        se.pe.pubSlewedPolyMelody[1][3]));
        SweepSig sig{};
        EXPECT(sweepSigChanged(sig, se, stepArr, nV));          // first run (!valid)
        sweepSigCapture(sig, se, stepArr, nV);
        EXPECT(!sweepSigChanged(sig, se, stepArr, nV));         // THE core: unchanged → skip
        bool idleFired = false;
        for (int i = 0; i < 10000; ++i)
            if (sweepSigChanged(sig, se, stepArr, nV)) { idleFired = true; break; }
        EXPECT(!idleFired);
        // nV (the channel count the resolve covers)
        EXPECT(sweepSigChanged(sig, se, stepArr, 4));           sweepSigCapture(sig, se, stepArr, 4);
        // a lane's step
        int stepArr2[7] = {0, 1, 2, 3, 4, 5, 7};
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        // the LIVE macroOwnSpread store (the manager's publishGlobal is an external
        // writer from the gate's view — the live compare catches it)
        se.macroOwnSpread[2] += 0.25f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        // a pub cell AT the current step (the cells the resolve reads)
        se.pe.pubSlewedPolyMelody[1][stepArr2[PatternEngine::PL_MELODY]] = 0.777f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        // the bank pointer (null → wired)
        dotModular::WarpBank bank;
        se.pe.setWarpBank(&bank);
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        // each global warp cell, one at a time (the FEEL-TEST tilt/atom included)
        bank.globalWarpW[3] = 0.5f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        bank.globalWarpF[PatternEngine::PL_OCTAVE] = dotModular::PowerWarp{3.0};
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        bank.globalWarpTilt[5] = 0.4f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        bank.globalWarpAtomLevel[0] = 0.9f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        bank.globalWarpAtomMass[6] = 0.3f;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        bank.poolUsed = 5;
        EXPECT(sweepSigChanged(sig, se, stepArr2, 4));          sweepSigCapture(sig, se, stepArr2, 4);
        // and identical again
        EXPECT(!sweepSigChanged(sig, se, stepArr2, 4));
    });

    // ── G2: THE DEFENSE — own-slot + delegation perturbations are inert on MacroOwn ─
    TEST("G2 DEFENSE: own-slot warp stores + warpDelegated_ + off-step pub edits leave the MacroOwn output bit-unchanged AND the gate cold (a GLOBAL edit moves it — non-vacuous)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        for (int l = 0; l < 7; ++l) se.macroOwnSpread[l] = (l & 1) ? 0.35f : -0.45f;
        int stepArr[7] = {2, 3, 5, 7, 11, 13, 1};
        const int nV = 2;
        // Non-trivial globals: w=0.6 with a non-identity curve, tilt + atom live —
        // a leak through ANY layer would move the output.
        dotModular::WarpBank bank;
        for (int l = 0; l < 7; ++l) {
            bank.globalWarpW[l] = 0.6f;
            bank.globalWarpF[l] = dotModular::PowerWarp{2.5};
            bank.globalWarpTilt[l] = 0.2f;
            bank.globalWarpAtomLevel[l] = 0.7f;
            bank.globalWarpAtomMass[l] = 0.4f;
        }
        se.pe.setWarpBank(&bank);
        SweepSig sig{};
        sweepSigCapture(sig, se, stepArr, nV);
        EXPECT(!sweepSigChanged(sig, se, stepArr, nV));   // settled

        float before[7][16];
        resolveAll(se, stepArr, nV, before);

        // Perturb EVERY own-slot store the pre-70dd837 gate fingerprinted (the
        // East-authorable layer) + flip the whole delegation matrix + a pub cell at a
        // step the resolve does NOT read.
        for (int v = 0; v <= nV; ++v)
            for (int l = 0; l < 7; ++l) {
                const int i = dotModular::WarpBank::index(v, l);
                bank.warpW[i] = 0.95f;
                bank.warpF[i] = dotModular::ReversedPowerWarp{4.0};
                bank.warpTilt[i] = -0.5f;
                bank.warpAtomLevel[i] = 0.1f;
                bank.warpAtomMass[i] = 0.8f;
                se.pe.setWarpDelegated(v, l, (v + l) & 1);   // flip the matrix rows
            }
        setPubCell(se.pe, PatternEngine::PL_OCTAVE, 1,
                   (stepArr[PatternEngine::PL_OCTAVE] + 3) & 0x0F, 0.333f);

        float after[7][16];
        resolveAll(se, stepArr, nV, after);
        for (int l = 0; l < 7; ++l)
            for (int ch = 0; ch <= nV; ++ch)
                EXPECTF(sameBits(before[l][ch], after[l][ch]),
                        "lane %d ch %d moved: %.9g → %.9g — the MacroOwn lens read an "
                        "own-slot/delegation input it must not (pure selection broken)",
                        l, ch, (double)before[l][ch], (double)after[l][ch]);
        // The gate stays cold through ALL of it — the exact fingerprint's design.
        EXPECT(!sweepSigChanged(sig, se, stepArr, nV));

        // Non-vacuity counter-proof: a GLOBAL cell (a fingerprinted input) DOES move
        // the output — the probe is live, the equality above is meaningful.
        bank.globalWarpW[0] = 0.61f;
        float moved[7][16];
        resolveAll(se, stepArr, nV, moved);
        bool anyMoved = false;
        for (int ch = 0; ch <= nV; ++ch)
            if (!sameBits(before[0][ch], moved[0][ch])) { anyMoved = true; break; }
        EXPECT(anyMoved);
    });

    // ── G3: bit-identity — the gate only skips UNCHANGED recomputes ────────────────
    TEST("G3 BIT-IDENTITY: whenever the gate skips, an ungated re-resolve through the REAL accessor is bit-identical to the held values (randomized script)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        for (int l = 0; l < 7; ++l) se.macroOwnSpread[l] = 0.05f * l - 0.15f;
        dotModular::WarpBank bank;
        for (int l = 0; l < 7; ++l) {
            bank.globalWarpW[l] = 0.25f + 0.05f * l;
            bank.globalWarpF[l] = dotModular::PowerWarp{1.5 + 0.5 * l};
            bank.globalWarpTilt[l] = 0.1f * (l - 3);
            bank.globalWarpAtomMass[l] = 0.2f;
        }
        se.pe.setWarpBank(&bank);
        int stepArr[7] = {0, 2, 4, 6, 8, 10, 12};
        int nV = 2;
        SweepSig sig{};
        float held[7][16] = {};
        bool have = false;
        int runs = 0, skips = 0;
        for (int tick = 0; tick < 4000; ++tick) {
            if (rng32() % 100 < 15) {   // ~15% of ticks mutate a fingerprinted input
                switch (rng32() % 7) {
                    case 0: { const int l = rng32() % 7;
                              stepArr[l] = (stepArr[l] + 1 + (int)(rng32() % 15)) & 0x0F; break; }
                    case 1: se.macroOwnSpread[rng32() % 7] = (float)((int)(rng32() % 1000) - 500) / 1000.f; break;
                    case 2: setPubCell(se.pe, (int)(rng32() % 7), (int)(rng32() % 16),
                                       (int)(rng32() % 16), (float)(rng32() % 1000) / 1000.f); break;
                    case 3: nV = (int)(rng32() % 16); break;
                    case 4: bank.globalWarpW[rng32() % 7] = (float)(rng32() % 100) / 100.f; break;
                    case 5: bank.globalWarpTilt[rng32() % 7] = (float)((int)(rng32() % 191) - 95) / 100.f; break;
                    default: bank.globalWarpAtomMass[rng32() % 7] = (float)(rng32() % 90) / 100.f; break;
                }
            }
            float ref[7][16];   // the UNGATED behaviour: always re-resolve
            resolveAll(se, stepArr, nV, ref);
            if (sweepSigChanged(sig, se, stepArr, nV)) {
                sweepSigCapture(sig, se, stepArr, nV);
                for (int l = 0; l < 7; ++l)
                    for (int ch = 0; ch < 16; ++ch) held[l][ch] = ref[l][ch];
                have = true; ++runs;
            } else {
                ++skips;
                if (have)
                    for (int l = 0; l < 7; ++l)
                        for (int ch = 0; ch <= nV; ++ch)
                            EXPECTF(sameBits(held[l][ch], ref[l][ch]),
                                    "tick %d lane %d ch %d: the gate skipped but the "
                                    "ungated re-resolve moved %.9g → %.9g — an input "
                                    "the accessor reads is NOT fingerprinted",
                                    tick, l, ch, (double)held[l][ch], (double)ref[l][ch]);
            }
        }
        EXPECTF(runs > 20, "vacuous: only %d gate runs in 4000 ticks", runs);
        EXPECTF(skips > 1000, "vacuous: only %d gate skips in 4000 ticks", skips);
    });

    // ── G4: the restored wiring, pinned in source (the module is rig-invisible) ────
    TEST("G4 STRUCTURAL: the widget carries the /32 probOutDiv + the SweepSig gate + the resolve inside the run; the 70dd837 step-NUMBER trigger is gone; out-epoch + write-elimination kept; the dropped fingerprint fields absent", {
        const std::string macro = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        const std::string hpp   = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.hpp");
        const std::string gate  = slurp(repoRoot() + "/src/ui/SweepSigGate.hpp");
        EXPECTF(contains(hpp, "probOutDiv.setDivision(32)"),
                "StraitsSandsMacroVisual.hpp: the /32 division is not restored");
        EXPECTF(contains(macro, "probOutDiv.process()"),
                "StraitsSandsMacroVisual.cpp: the gate has no /32 cadence");
        EXPECTF(contains(macro, "sweepSigChanged(sweepSig_, eng, stepArr, nV)"),
                "StraitsSandsMacroVisual.cpp: the fingerprint gate decision is missing");
        EXPECTF(contains(macro, "sweepSigCapture(sweepSig_, eng, stepArr, nV)"),
                "StraitsSandsMacroVisual.cpp: the signature capture is missing");
        EXPECTF(contains(macro, "laneProbabilityAtStep(ch + 1, l, stepArr[l])"),
                "StraitsSandsMacroVisual.cpp: the resolve no longer reads the ONE "
                "accessor through the resolver seam (ch v → voice v+1)");
        EXPECTF(!contains(macro, "step != row.step"),
                "the per-row step-NUMBER trigger survives — keyed on the step edge it "
                "cannot skip anything else (Rodney's bisect: the 3% regression)");
        EXPECTF(!contains(hpp, "probHeld") && !contains(hpp, "probLastStep"),
                "the S&H latch state is back — the cache holds between runs by design");
        EXPECTF(contains(macro, "memcmp(&out.voltages[ch], &v"),
                "the output write-elimination was lost — steady-state writes are back");
        EXPECTF(contains(macro, "row.outFresh"),
                "the out-write epoch (FINISH2 CPU fix) was lost");
        // The deliberately-dropped fingerprint fields stay dropped (G2 defends the
        // behaviour; this pins the shape): the East-authorable own-slot stores and the
        // delegation matrix are NOT inputs of the MacroOwn lens.
        static const char* const dropped[] = {
            "warpW[112]", "warpF[112]", "warpTilt[112]", "warpAtomLevel[112]",
            "warpAtomMass[112]", "deleg[16][7]"
        };
        for (const char* f : dropped)
            EXPECTF(!contains(gate, f) && !contains(hpp, f),
                    "the unread fingerprint field %s is back in the sig — a "
                    "fingerprinted-but-unread input only re-runs the resolve "
                    "spuriously (the CPU cost) for zero correctness", f);
    });

    std::cout << std::string(70, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
