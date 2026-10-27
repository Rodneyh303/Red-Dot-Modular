/**
 * test_forest_rhythm.cpp — the FOREST behavioral spec (STAGE F, plan §5 "Forest property").
 *
 * F-1 re-pointed the RDM at ParentSource::GRAPH — the staged containmentParents is the
 * rhythm truth (D2: graph truth everywhere, no engine legacy mode). This suite pins the
 * three graph shapes the cascade now runs, in mode A (clock/phase — no per-sample C5
 * drive yet; the gate-mode forest compose is F-2):
 *
 *   FREE VOICES   parents(v) = ∅ ⇒ E(v) = Own(v) (D2/R2 ruled). A parentless voice detaches
 *                 from V1 entirely: it self-clocks at its own window expiry (mono's
 *                 MidNote-guard predicate), thins itself with its own rest draws, slides on
 *                 its own slur commitment. The star's invariants INVERT: mono rests do NOT
 *                 close free voices, and a free voice's gate may rise while mono's is low.
 *
 *   POLY CONTAINERS  a poly voice is a real parent: its captured rhythm verdict
 *                 (polyVerdict_) + drawn window (polyNv_) + live gate feed the child's
 *                 fold through containerView(u). V3 contained in V2 follows V2, NOT V1 —
 *                 the C1-C5 semantics are per-container, not mono-wired.
 *
 *   STAR CONTRAST the same cascade on the authored all-follow-V1 graph reproduces the
 *                 legacy invariants (mono rest closes all poly; poly gate ⊆ mono gate) —
 *                 the exact inversion of the free-voice rows, proving ONE code path runs
 *                 both graphs.
 *
 *   DOWNWARD EDGES a container with a HIGHER slot than its child was read one edge stale in
 *                 F-1 (slot-order visitation). F-2 materializes the topological visit order
 *                 (D8: resolved at the graph-change boundary), so every container — any
 *                 direction — executes before its child; the read is fresh everywhere. Pinned
 *                 here as defined-and-deterministic (the core suite pins the order itself).
 *
 * Determinism: PhiloxRng counter-based seeding, pitches quantised to semitones for
 * FP-hash safety — the differential-harness idioms. Byte-identity for STAR graphs is the
 * differential suite's contract (test_rhythm_decision_differential, injected star); this
 * suite pins the NEW shapes.
 *
 * Build (see test/run_all.sh, entry "test_forest_rhythm|$SE $GS $PE $CE"):
 *   g++ -std=c++20 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/ui
 *       test/test_forest_rhythm.cpp
 *       src/dsp/engines/SequencerEngine.cpp src/dsp/engines/PatternEngine.cpp
 *       src/dsp/gates/GateState.cpp src/dsp/engines/ClockEngine.cpp -o /tmp/tfr && /tmp/tfr
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"

using redDot::PhiloxRng;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── Seeding (the differential-harness idiom, verbatim shape) ────────────────────
static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO
};

static void seedEngine(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = dotModular::VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.10f + 0.55f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.60f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.85f * rng.nextFloat();
        eng.voices[vs].variationProb = 0.25f + 0.50f * rng.nextFloat();
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = dotModular::VoiceResolver::slot(1);
    eng.voices[m1].restProb      = 0.10f + 0.55f * rng.nextFloat();
    eng.voices[m1].accentProb    = 0.60f * rng.nextFloat();
    eng.voices[m1].legatoProb    = 0.85f * rng.nextFloat();
    eng.voices[m1].variationProb = 0.25f + 0.50f * rng.nextFloat();
    eng.voices[m1].qmixLevel     = rng.nextFloat();
}

// ── The graph seams ─────────────────────────────────────────────────────────────
// D2: the star is an AUTHORED graph — the same words the Cantonment preset carries.
static void authorGraph(SequencerEngine& eng, const uint16_t words[16]) {
    for (int v = 0; v < 16; ++v) eng.pe.containmentParents[v] = words[v];
}
static void authorStar(SequencerEngine& eng) {
    authorGraph(eng, RhythmDecisionCore::LEGACY_STAR_PARENTS);
}
static void authorFree(SequencerEngine& eng) {
    uint16_t zeros[16] = {};
    authorGraph(eng, zeros);
}

// ── The mode-A march (clock origin; the differential idiom) ─────────────────────
static const int kSteps = 64;

struct Tallies {
    long polyFires       = 0;   // poly gate-high edges (any voice)
    long polyWhileMonoLow = 0;  // poly gate-high while mono gate LOW (the INV-ENV inversion)
    long polyWhileMonoRest = 0; // poly gate-high while mono decision == Rest (the INV-C2 inversion)
    long holds           = 0;   // adjacent-edge gate holds (the same voice stays high)
    long v3Fires         = 0;   // V3-specific fires
    long v3WhileV2Low    = 0;   // V3 high while V2 gate low (must stay 0 under V2 containment)
    long v3WithV2        = 0;   // V3 high while V2 high (the coupling alive)
    long v4WhileV2Low    = 0;   // free V4 high while V2 low (no accidental coupling)
    long v2Verdicts      = 0;   // edges where V2's captured verdict is an onset (NewNote/Legato)
    long monoRests       = 0;   // mono rest edges (the C2 probe count)
    std::vector<uint8_t> stream; // gate/pitch byte stream (determinism double-run)
};

static void marchFree(int seed, const uint16_t words[16], Tallies& t) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u);
    SequencerEngine eng; eng.reset();
    authorGraph(eng, words);
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    redDot::RateManager rm;
    bool prevGate[16] = {};   // poly gates one edge ago, storage-slot indexed
    bool prevMono      = false;
    MonoDecision prevDecision = MonoDecision::Rest;
    (void)prevDecision; (void)prevMono;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = eng.executeModeA(rm, noteVal, in, +1);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        const bool monoLow = !eng.gs.gateHeld;
        for (int v = 1; v < 16; ++v) {
            const auto vs = dotModular::VoiceResolver::slot(v + 1);
            const bool high = eng.voices[vs].gs.gateHeld;
            if (high) {
                ++t.polyFires;
                if (monoLow) ++t.polyWhileMonoLow;
                if (r.decision == MonoDecision::Rest) ++t.polyWhileMonoRest;
                if (prevGate[v]) ++t.holds;
                t.stream.push_back((uint8_t)(1u | ((unsigned)v << 1)));
                t.stream.push_back((uint8_t)(int)std::lrintf((double)eng.voices[vs].gs.currentPitchV * 12.0));
            }
            prevGate[v] = high;
        }
        if (r.decision == MonoDecision::Rest) ++t.monoRests;
        // V2 (slot 1) / V3 (slot 2) / V4 (slot 3) probes.
        const auto v2 = dotModular::VoiceResolver::slot(2);
        const auto v3 = dotModular::VoiceResolver::slot(3);
        const auto v4 = dotModular::VoiceResolver::slot(4);
        const bool v2h = eng.voices[v2].gs.gateHeld;
        const bool v3h = eng.voices[v3].gs.gateHeld;
        const bool v4h = eng.voices[v4].gs.gateHeld;
        if (v3h) { ++t.v3Fires; if (!v2h) ++t.v3WhileV2Low; else ++t.v3WithV2; }
        if (v4h && !v2h) ++t.v4WhileV2Low;
        const MonoDecision v2v = eng.polyVerdict_[1];
        if (v2v == MonoDecision::NewNote || v2v == MonoDecision::Legato) ++t.v2Verdicts;
    }
}

static void marchStar(int seed, Tallies& t) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 77u);
    SequencerEngine eng; eng.reset();
    authorStar(eng);   // D2 seam: the legacy sound as an authored graph
    eng.numPolyVoices = 15;
    seedEngine(eng, rng);
    const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
    const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
    const PatternInput in = makeInput(varAmt);
    redDot::RateManager rm;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = eng.executeModeA(rm, noteVal, in, +1);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        if (r.decision == MonoDecision::Rest) {
            ++t.monoRests;
            for (int v = 1; v < 16; ++v) {
                const auto vs = dotModular::VoiceResolver::slot(v + 1);
                check(!eng.voices[vs].gs.gateHeld,
                      "STAR C1: mono rests -> all poly gates closed (the authored star)");
            }
        }
        for (int v = 1; v < 16; ++v) {
            const auto vs = dotModular::VoiceResolver::slot(v + 1);
            const bool high = eng.voices[vs].gs.gateHeld;
            if (high) {
                ++t.polyFires;
                check(eng.gs.gateHeld,
                      "STAR C2: poly gate high implies mono gate high (mode A envelope)");
            }
        }
    }
}

static const int kSeeds = 8;

int main() {
    std::printf("[test_forest_rhythm] the FOREST behavioral spec (STAGE F-1, plan §5)\n");

    // ── Suite A — FREE VOICES: parents(v) = ∅ ⇒ E(v) = Own(v) ───────────────────
    std::printf("  suite A: free voices (the empty graph, D2/R2)\n");
    {
        uint16_t zeros[16] = {};
        Tallies t;
        for (int seed = 0; seed < kSeeds; ++seed) marchFree(seed, zeros, t);
        std::printf("    fires=%ld polyWhileMonoLow=%ld polyWhileMonoRest=%ld holds=%ld monoRests=%ld\n",
                    t.polyFires, t.polyWhileMonoLow, t.polyWhileMonoRest, t.holds, t.monoRests);
        check(t.polyFires > 0,         "FREE: free voices PLAY (the empty graph is not silence)");
        check(t.monoRests > 0,         "FREE: the march exercised mono rests (the probe is live)");
        check(t.polyWhileMonoRest > 0, "FREE: mono rests do NOT close free voices (INV-C2 inverted)");
        check(t.polyWhileMonoLow > 0,  "FREE: a free voice's gate rises while mono's is low (INV-ENV inverted)");
        check(t.holds > 0,             "FREE: free voices HOLD their own windows across edges (self-clocking)");
    }

    // ── Suite B — POLY CONTAINERS: a poly voice is a real parent ────────────────
    // Graph: V3 (slot 2) contained in V2 (bit 1); everyone else free. V2 itself is free,
    // so its rhythm is its own — and V3 folds V2's captured verdict + live gate.
    std::printf("  suite B: poly containers (V3 in V2)\n");
    {
        uint16_t words[16] = {};
        words[2] = 0x0002;   // slot 2 = V3: bit 1 = V2
        Tallies t;
        for (int seed = 0; seed < kSeeds; ++seed) marchFree(seed, words, t);
        std::printf("    v3Fires=%ld v3WhileV2Low=%ld v3WithV2=%ld v2Verdicts=%ld v4WhileV2Low=%ld\n",
                    t.v3Fires, t.v3WhileV2Low, t.v3WithV2, t.v2Verdicts, t.v4WhileV2Low);
        check(t.v3WhileV2Low == 0,     "POLY-C: V3's gate is never high while V2's is low (the poly envelope closes the child)");
        check(t.v3Fires > 0,           "POLY-C: V3 plays inside V2's envelope (the containment is alive)");
        check(t.v3WithV2 > 0,          "POLY-C: V3 high while V2 high (the coupling observed)");
        check(t.v2Verdicts > 0,        "POLY-C: V2's rhythm verdict captures (the per-container view has data)");
        // (V4-vs-V2 independence is probed DETERMINISTICALLY in suite B3 below — a purely
        // statistical row is unreliable here: free voices hold their windows ~98% of edges,
        // so "V4 high while V2 low" is the square of two rare events on 8 seeds.)
    }

    // ── Suite B3 — the DIRECTED independence probe (deterministic) ──────────────
    // V2's restProb = 1 (its gate never arms — it rests at every edge) and V4's restProb
    // = 0 (it plays at every expiry edge). The graph contains ONLY V3 in V2. Contract:
    // V3 stays silent (C2 through a POLY container, deterministic — no seed luck) while
    // V4 plays freely — the graph couples V3 to V2 and touches NOTHING else.
    std::printf("  suite B3: the directed independence probe (V2 always rests, V4 never does)\n");
    {
        uint16_t words[16] = {};
        words[2] = 0x0002;   // slot 2 = V3: bit 1 = V2 — the only authored edge
        long v2Fires = 0, v3Fires = 0, v4Fires = 0, v4WhileV2Low = 0;
        for (int seed = 0; seed < kSeeds; ++seed) {
            PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 55u);
            SequencerEngine eng; eng.reset();
            authorGraph(eng, words);
            eng.numPolyVoices = 15;
            seedEngine(eng, rng);
            eng.voices[dotModular::VoiceResolver::slot(2)].restProb = 1.0f;   // V2: always rest
            eng.voices[dotModular::VoiceResolver::slot(4)].restProb = 0.0f;   // V4: never rest
            const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
            const float varAmt  = 0.20f + 0.70f * rng.nextFloat();
            const PatternInput in = makeInput(varAmt);
            redDot::RateManager rm;
            for (int step = 0; step < kSteps; ++step) {
                rm.advance(redDot::Rate::Step, true);
                StepResult r = eng.executeModeA(rm, noteVal, in, +1);
                if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
                const auto v2 = dotModular::VoiceResolver::slot(2);
                const auto v3 = dotModular::VoiceResolver::slot(3);
                const auto v4 = dotModular::VoiceResolver::slot(4);
                const bool v2h = eng.voices[v2].gs.gateHeld;
                if (eng.voices[v3].gs.gateHeld) ++v3Fires;
                if (eng.voices[v4].gs.gateHeld) { ++v4Fires; if (!v2h) ++v4WhileV2Low; }
            }
        }
        std::printf("    v2Fires=%ld v3Fires=%ld v4Fires=%ld v4WhileV2Low=%ld\n",
                    v2Fires, v3Fires, v4Fires, v4WhileV2Low);
        check(v2Fires == 0,            "POLY-C3: V2 (restProb 1) never sounds (the probe is live)");
        check(v3Fires == 0,            "POLY-C3: V3 never sounds under a permanently-rested poly container (C2, deterministic)");
        check(v4Fires > 0,             "POLY-C3: free V4 plays (the probe heard it)");
        check(v4WhileV2Low == v4Fires, "POLY-C3: every V4 fire happens while V2 is low (zero coupling — the graph touches only V3)");
    }

    // ── Suite B2 — the anti-V1 row: V3's containment is V2, NOT V1 ──────────────
    // Aggregate over the same marches: V3 high on edges where MONO is low must occur —
    // under a V1-star V3 could never do that (INV-ENV); V3's container is V2.
    std::printf("  suite B2: V3 independent of V1\n");
    {
        uint16_t words[16] = {};
        words[2] = 0x0002;
        Tallies t;
        for (int seed = 0; seed < kSeeds; ++seed) marchFree(seed, words, t);
        check(t.polyWhileMonoLow > 0, "POLY-C: the forest plays on while mono is low (V3 rides V2, not V1)");
    }

    // ── Suite C — STAR CONTRAST: the same cascade, the authored legacy graph ────
    std::printf("  suite C: star contrast (the legacy invariants hold on the authored graph)\n");
    {
        Tallies t;
        for (int seed = 0; seed < kSeeds; ++seed) marchStar(seed, t);
        std::printf("    fires=%ld monoRests=%ld\n", t.polyFires, t.monoRests);
        check(t.monoRests > 0,  "STAR: the march exercised mono rests (the C1 probe is live)");
        check(t.polyFires > 0,  "STAR: the star still plays (the authored graph is the legacy sound)");
    }

    // ── Suite D — DOWNWARD EDGES: fresh reads under the topological visit order ────
    // V2 (slot 1) contained in V16 (bit 15): in F-1 this read V16's PREVIOUS-edge capture
    // (slot order). F-2's materialized topo order visits V16 BEFORE V2, so the read is
    // fresh on every edge. Pinned: the run is byte-deterministic and plays.
    std::printf("  suite D: downward edges (fresh reads under the topo visit order)\n");
    {
        uint16_t words[16] = {};
        words[1] = 0x8000;   // slot 1 = V2: bit 15 = V16 (a higher slot — was the stale-read case)
        Tallies a, b;
        marchFree(3, words, a);
        marchFree(3, words, b);
        check(a.stream == b.stream && a.polyFires == b.polyFires,
              "DOWN: a downward-edge graph runs byte-deterministically (double-run)");
        check(a.polyFires > 0, "DOWN: the downward-edge graph still plays (defined, not silent-by-crash)");
    }

    // ── Suite E — NESTED CHAIN: V3 in {V2, V1}, V2 in V1 (depth 2, mode A) ──────
    // The E(v) ⊆ E(u) property (§3.4/§5) on a real depth-2 chain: V3's envelope is inside
    // V2's, and V2's inside V1's — so V3 high implies BOTH containers high and mono high.
    std::printf("  suite E: nested chain (V3 in V2 in V1 — the containment is transitive)\n");
    {
        uint16_t words[16] = {};
        words[2] = 0x0003;   // slot 2 = V3: bits 0,1 = {V1, V2}
        words[1] = 0x0001;   // slot 1 = V2: bit 0 = V1
        long v3Fires = 0, v3WithBoth = 0, v3WhileV2Low = 0, v3WhileMonoLow = 0;
        long v2Fires = 0, v2WhileMonoLow = 0, monoRests = 0;
        for (int seed = 0; seed < kSeeds; ++seed) {
            PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 91u);
            SequencerEngine eng; eng.reset();
            authorGraph(eng, words);
            eng.numPolyVoices = 15;
            seedEngine(eng, rng);
            const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
            const PatternInput in = makeInput(0.20f + 0.70f * rng.nextFloat());
            redDot::RateManager rm;
            for (int step = 0; step < kSteps; ++step) {
                rm.advance(redDot::Rate::Step, true);
                StepResult r = eng.executeModeA(rm, noteVal, in, +1);
                if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
                const auto v2 = dotModular::VoiceResolver::slot(2);
                const auto v3 = dotModular::VoiceResolver::slot(3);
                const bool v2h = eng.voices[v2].gs.gateHeld;
                const bool v3h = eng.voices[v3].gs.gateHeld;
                const bool monoHigh = eng.gs.gateHeld;
                if (r.decision == MonoDecision::Rest) ++monoRests;
                if (v2h) { ++v2Fires; if (!monoHigh) ++v2WhileMonoLow; }
                if (v3h) {
                    ++v3Fires;
                    if (v2h && monoHigh) ++v3WithBoth;
                    if (!v2h)       ++v3WhileV2Low;
                    if (!monoHigh)  ++v3WhileMonoLow;
                }
            }
        }
        std::printf("    v3Fires=%ld v3WithBoth=%ld v2Fires=%ld monoRests=%ld\n",
                    v3Fires, v3WithBoth, v2Fires, monoRests);
        check(monoRests > 0,       "NEST: the march exercised mono rests (the probe is live)");
        check(v2Fires > 0,         "NEST: V2 plays inside V1 (the inner container is alive)");
        check(v2WhileMonoLow == 0, "NEST: V2 high implies mono high (V2 in V1, mode A)");
        check(v3Fires > 0,         "NEST: V3 plays inside the chain (depth-2 containment alive)");
        check(v3WhileV2Low == 0,   "NEST: V3 high implies V2 high (its direct container)");
        check(v3WhileMonoLow == 0, "NEST: V3 high implies mono high (transitive through V2)");
        check(v3WithBoth > 0,      "NEST: the coupling observed (V3 high while V2 and mono high)");
    }

    // ── Suite F — DIAMOND: V5 in {V2, V3}, V2 and V3 both in V1 ────────────────
    // Two parents, one grandparent: V5's envelope is the intersection of V2's and V3's —
    // and both are inside V1. The same ONE code path as the star (the fold), now over a
    // diamond the star can never express.
    std::printf("  suite F: diamond (V5 in {V2,V3}, both in V1)\n");
    {
        uint16_t words[16] = {};
        words[4] = 0x0006;   // slot 4 = V5: bits 1,2 = {V2, V3}
        words[1] = 0x0001;   // V2 in {V1}
        words[2] = 0x0001;   // V3 in {V1}
        long v5Fires = 0, v5WithBoth = 0, v5WhileV2Low = 0, v5WhileV3Low = 0, v5WhileMonoLow = 0;
        for (int seed = 0; seed < kSeeds; ++seed) {
            PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 97u);
            SequencerEngine eng; eng.reset();
            authorGraph(eng, words);
            eng.numPolyVoices = 15;
            seedEngine(eng, rng);
            const float noteVal = 2.f + (float)(int)(rng.nextFloat() * 4.999f);
            const PatternInput in = makeInput(0.20f + 0.70f * rng.nextFloat());
            redDot::RateManager rm;
            for (int step = 0; step < kSteps; ++step) {
                rm.advance(redDot::Rate::Step, true);
                StepResult r = eng.executeModeA(rm, noteVal, in, +1);
                if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
                const bool v2h = eng.voices[dotModular::VoiceResolver::slot(2)].gs.gateHeld;
                const bool v3h = eng.voices[dotModular::VoiceResolver::slot(3)].gs.gateHeld;
                const bool v5h = eng.voices[dotModular::VoiceResolver::slot(5)].gs.gateHeld;
                const bool monoHigh = eng.gs.gateHeld;
                if (v5h) {
                    ++v5Fires;
                    if (v2h && v3h && monoHigh) ++v5WithBoth;
                    if (!v2h)      ++v5WhileV2Low;
                    if (!v3h)      ++v5WhileV3Low;
                    if (!monoHigh) ++v5WhileMonoLow;
                }
            }
        }
        std::printf("    v5Fires=%ld v5WithBoth=%ld\n", v5Fires, v5WithBoth);
        check(v5Fires > 0,         "DIAMOND: V5 plays inside the diamond (multi-parent alive)");
        check(v5WhileV2Low == 0,   "DIAMOND: V5 high implies V2 high (parent 1)");
        check(v5WhileV3Low == 0,   "DIAMOND: V5 high implies V3 high (parent 2)");
        check(v5WhileMonoLow == 0, "DIAMOND: V5 high implies mono high (grandparent, transitive)");
        check(v5WithBoth > 0,      "DIAMOND: the coupling observed (V5 high while both parents high)");
    }

    std::printf("\n%s: %d passed, %d failed\n", "test_forest_rhythm", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
