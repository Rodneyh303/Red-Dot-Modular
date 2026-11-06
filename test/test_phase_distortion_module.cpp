// test_phase_distortion_module.cpp — the STANDALONE PHASE-DISTORTION MODULE battery
// (PHASE_DISTRIBUTION_SPEC §13 + docs/design/CC_PROMPT_PHASE_DISTORTION_MODULE.md —
// the module-only comparison build, B, built alongside the expander model A).
//
// THE GATES, mapped 1:1 (the §13 prompt's "GATES"):
//   G1  IDENTITY (the ADJUSTED gate) — with all eight channels at identity, the
//       module's output equals the source phase delayed by EXACTLY ONE sample
//       (compared against the source SHIFTED one sample — the comparison doc's
//       gate note: "the test is not a false failure"). Unpatched (0 V) stays 0.
//   G2  CHANNEL SEPARATION — changing channel k's parameters changes only the
//       output of channel k (every other channel stays the bit-exact delayed raw).
//   G3  SAME OUTPUT, SAME RESULT — two consumers of output k produce identical
//       outputs, sample for sample (two cores, same set, same input stream).
//   G4  SHARED PRIMITIVE — the transform exists once: the module's channel output
//       IS dotModular::phaseDistort(normPhase(delayed), cell) × phaseInMax computed
//       externally, BIT-EXACT over the roster × signed-w grid (the core calls the
//       primitive; it cannot drift from it). The "grep confirms no second copy in
//       the module" half of the gate runs in the battery shell (run_all's smoke
//       covers the TU; the review grep is recorded in the commit).
//   G5  GAP/ATOM/DUAL semantics unchanged — the existing phase distribution
//       batteries (test_phase_distribution, test_phase_distortion, test_warp_*)
//       stay green in run_all; the bridge above pins the module onto the SAME
//       primitive those batteries pin.
//   G6  A/B READY — with matching parameters, the module's output equals the
//       EXPANDER build's output for the same input, delayed by one sample. The A
//       side here is Monsoon's Mode E apply lines VERBATIM (identity set ⟹ the raw
//       voltage through; non-identity ⟹ normPhase → phaseDistort → × phaseInMax),
//       and the comparison is B[n] == A[n−1] bit-exact — through the SAME
//       PhaseEngine::normPhase and phaseInMax both sides share.
//   G7  run_all green — both SDK legs (the battery itself; not a local pin).
//
// Plus the §13 model's own cadence pin: the module's per-block snapshot (the
// PhaseDistBlockReader at the module's kCtrlEvery = 32-frame cadence) — a store
// write between blocks lands at the NEXT boundary exactly (frame 31 old, frame 32
// new), never mid-block (the single-reader edition of the store mechanism).
//
// Header-only, Rack-free (the module's rack-free core — dsp/PhaseDistCore.hpp — is
// the unit under test; the Rack shell is covered by the plugin-TU smoke); jansson
// via the test/jansson.h shim; registered in test/run_all.sh.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>

#include "PhaseDistCore.hpp"            // the module's signal core (the unit)
#include "SandsModifierResolver.hpp"    // the ONE primitive (the bridge's authority)
#include "engines/PhaseEngine.hpp"      // the A-side oracle's normalisation (SHARED, not copied)

using dotModular::WarpF;
using redDot::PhaseDistSet;
using redDot::PhaseDistStore;

static int g_pass = 0, g_fail = 0;
static void ok(bool cond, const char* what) {
    if (cond) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static bool sameBitsF(float a, float b) {
    uint32_t ra, rb;
    std::memcpy(&ra, &a, sizeof(ra));
    std::memcpy(&rb, &b, sizeof(rb));
    return ra == rb;
}

// A roster of parametric members (the palette minus drawn/identity — drawn cannot
// enter the store, the store codec lowers it; identity is pinned separately).
struct RosterEntry { WarpF f; const char* name; };
static const RosterEntry g_roster[] = {
    { dotModular::TriangularWarp{0.5},         "triangular" },
    { dotModular::PowerWarp{2.0},              "power" },
    { dotModular::KumaraswamyWarp{0.55, 2.2},  "kumaraswamy" },
    { dotModular::MobiusWarp{1.5},             "mobius" },
    { dotModular::LogitSigmoidWarp{1.5, 0.0},  "logit-sigmoid" },
    { dotModular::ReversedPowerWarp{2.0},      "reversed-power" },
};

// THE SOURCE WAVE (in volts, the Monsoon Mode E convention: 0..10 V wraps): a ramp
// with a wrap, a JUMP (a scrub — a discontinuity far beyond a sample's motion), and
// a STALL (a flat stretch — the atom-plateau shape's input twin).
static float srcAt(int n) {
    float v = 10.0f * std::fmod(0.0031f * (float)n, 1.0f);
    if (n >= 500 && n < 550) v = std::fmod(v + 4.7f, 10.0f);   // the jump
    if (n >= 800 && n < 830) v = 3.14159f;                     // the stall
    return v;
}

// THE RIG — the module's process loop verbatim, rack-free (PhaseDistortion's
// structure: the store, eight cores, the block reader, the kCtrlEvery = 32 cadence;
// the constant here MUST match PhaseDistortion::kCtrlEvery — pinned by the cadence
// test's frame numbers).
struct ModuleRig {
    static constexpr int kN = PhaseDistStore::kSets;
    static constexpr int kCtrlEvery = 32;

    PhaseDistStore store;
    std::array<redDot::PhaseDistChannel, kN> ch_{};
    redDot::PhaseDistBlockReader reader_;
    int ctrlCount_ = 0;
    std::array<float, kN> in_{};    // this frame's input voltages (the cables)
    std::array<float, kN> out_{};   // this frame's output voltages

    void step() {   // ONE audio frame — PhaseDistortion::process verbatim
        if (ctrlCount_ == 0) reader_.beginBlock(store);
        if (++ctrlCount_ >= kCtrlEvery) ctrlCount_ = 0;
        for (int k = 0; k < kN; ++k)
            out_[k] = ch_[k].process(in_[k], reader_.snap_[k]);
    }
};

// THE A-SIDE ORACLE — Monsoon's Mode E apply lines VERBATIM (src/Monsoon.cpp, the
// §14 TASK 3 block): an identity set (or no expander) hands the RAW voltage to the
// engine; a non-identity set distorts in the normalised domain and hands the engine
// the distorted voltage. `pe` is a default PhaseEngine — the SAME normPhase /
// phaseInMax the module's core holds (one normalisation, not two).
static float applyA(const redDot::PhaseEngine& pe, float phaseVolt, const PhaseDistSet& s, bool identityFlag) {
    if (identityFlag) return phaseVolt;   // the RAW path verbatim
    const double uT = pe.normPhase(phaseVolt);
    return (float)dotModular::phaseDistort(uT, s.cell()) * pe.phaseInMax;
}

// A non-identity set with every feature LIVE (tilt + gap + atom), the signed depth
// the caller picks — the reading follows the sign.
static PhaseDistSet liveSet(const WarpF& f, float w) {
    PhaseDistSet s{};
    s.f = f;
    s.w = w;
    s.tilt = 0.3f;
    s.atomLevel = 0.42f;
    s.atomMass = 0.25f;
    s.gapLoc = 0.55f;
    s.gapSize = 0.12f;
    return s;
}

int main() {
    std::printf("PHASE-DISTORTION MODULE (§13, build B) — the gates\n");

    // ── G1 IDENTITY (the ADJUSTED gate): output == source SHIFTED ONE SAMPLE ────
    {
        std::printf("G1 IDENTITY (adjusted) — all-identity channels: out == source delayed exactly one sample\n");
        ModuleRig rig;
        const int N = 1200;
        std::array<float, ModuleRig::kN> prevIn{};
        bool bitExact = true, startZero = true, allIdent = true;
        for (int k = 0; k < ModuleRig::kN; ++k) {
            PhaseDistSet snap{};
            rig.store.snapshotSet(k, snap);
            if (!snap.isIdentity()) allIdent = false;
        }
        ok(allIdent, "G1: a fresh module's store is EIGHT identity sets");
        for (int n = 0; n < N; ++n) {
            const float v = srcAt(n);
            for (int k = 0; k < ModuleRig::kN; ++k) rig.in_[k] = v;   // every channel patched, same source
            rig.step();
            if (n == 0) {
                for (int k = 0; k < ModuleRig::kN; ++k)
                    if (rig.out_[k] != 0.f) startZero = false;        // in(−1) = 0
            } else {
                for (int k = 0; k < ModuleRig::kN; ++k)
                    if (!sameBitsF(rig.out_[k], prevIn[k])) { bitExact = false; break; }
            }
            for (int k = 0; k < ModuleRig::kN; ++k) prevIn[k] = rig.in_[k];
            if (!bitExact) break;
        }
        ok(startZero, "G1: out(0) == 0 == the source's pre-run level (the delay line's seed)");
        ok(bitExact, "G1: out(n) == in(n-1) BIT-EXACT on all eight channels over the sweep (ramp + wrap + jump + stall)");
        // UNPATCHED channel: 0 V in stays 0 V out (plus the same fixed hop).
        {
            ModuleRig r2;
            for (int n = 0; n < 64; ++n) {
                for (int k = 0; k < ModuleRig::kN; ++k) r2.in_[k] = 0.f;
                r2.step();
            }
            bool zero = true;
            for (int k = 0; k < ModuleRig::kN; ++k) if (r2.out_[k] != 0.f) zero = false;
            ok(zero, "G1: an unpatched (0 V) channel passes through unchanged (0 V), plus the fixed hop");
        }
        // delegation alone does not break the raw path (engine-inert, the store's law)
        {
            ModuleRig r3;
            redDot::PhaseDistSet s{};
            s.delegated = true;
            r3.store.writeSet(5, s);
            r3.reader_.beginBlock(r3.store);
            bool raw = true;
            float prev = 0.f;
            for (int n = 0; n < 300; ++n) {
                r3.in_[5] = srcAt(n);
                r3.out_[5] = r3.ch_[5].process(r3.in_[5], r3.reader_.snap_[5]);
                if (n > 0 && !sameBitsF(r3.out_[5], prev)) raw = false;
                prev = r3.in_[5];
            }
            ok(raw, "G1: delegation alone does not break the raw pass-through (engine-inert)");
        }
    }

    // ── G2 CHANNEL SEPARATION ───────────────────────────────────────────────────
    {
        std::printf("G2 CHANNEL SEPARATION — changing channel 3's set changes ONLY channel 3\n");
        ModuleRig rig;
        const int K = 2;   // channel 3, zero-based
        redDot::PhaseDistSet s = liveSet(g_roster[0].f, 0.7f);
        rig.store.writeSet(K, s);
        rig.reader_.beginBlock(rig.store);   // a UI write lands at the next boundary
        const int N = 600;
        std::array<float, ModuleRig::kN> prevIn{};
        bool othersRaw = true, changed = false, inRange = true;
        for (int n = 0; n < N; ++n) {
            const float v = srcAt(n);
            for (int k = 0; k < ModuleRig::kN; ++k) rig.in_[k] = v;
            rig.step();
            if (n > 0) {
                for (int k = 0; k < ModuleRig::kN; ++k) {
                    if (k == K) continue;
                    if (!sameBitsF(rig.out_[k], prevIn[k])) othersRaw = false;   // the identity raw path
                }
                if (!sameBitsF(rig.out_[K], applyA(rig.ch_[K].pe_, prevIn[K], s, false))) inRange = false;
                if (!sameBitsF(rig.out_[K], prevIn[K])) changed = true;          // its own set decides
            }
            for (int k = 0; k < ModuleRig::kN; ++k) prevIn[k] = rig.in_[k];
        }
        ok(othersRaw, "G2: every OTHER channel stays the bit-exact delayed raw");
        ok(changed,   "G2: channel 3's output follows ITS set (differs from the delayed raw)");
        ok(inRange,   "G2: channel 3's output == the primitive on its own set (bit-exact)");
    }

    // ── G3 SAME OUTPUT, SAME RESULT ─────────────────────────────────────────────
    {
        std::printf("G3 SAME OUTPUT SAME RESULT — two consumers of output k see identical samples\n");
        // Two Monsoons patched to output k read the SAME cable voltage — one write,
        // two reads. Modelled honestly: ONE channel core writes the output; two
        // reader cores with the same set + the same input stream must agree
        // bit-for-bit (the module's determinism: same state + same input ⟹ same out).
        redDot::PhaseDistChannel writer, reader1, reader2;
        PhaseDistSet s = liveSet(g_roster[2].f, -0.6f);
        bool identical = true;
        for (int n = 0; n < 700; ++n) {
            const float v = srcAt(n);
            const float outWritten = writer.process(v, s);   // the module writes ONCE
            (void)outWritten;
            const float r1 = reader1.process(v, s);          // consumer 1's world
            const float r2 = reader2.process(v, s);          // consumer 2's world
            if (!sameBitsF(r1, r2)) { identical = false; break; }
        }
        ok(identical, "G3: two cores on the same set + input stream agree sample for sample (bit-exact)");
    }

    // ── G4 SHARED PRIMITIVE (the bridge) ────────────────────────────────────────
    {
        std::printf("G4 SHARED PRIMITIVE — the channel output IS the primitive's output (no second copy)\n");
        redDot::PhaseEngine pe;   // the shared normalisation authority
        bool bridge = true;
        const char* badName = "";
        float badW = 0.f;
        for (const auto& e : g_roster) {
            for (float w : { -1.0f, -0.4f, 0.3f, 1.0f }) {
                PhaseDistSet s = liveSet(e.f, w);
                redDot::PhaseDistChannel core;
                core.process(0.f, s);   // warm the delay line (its payload becomes 0 — compared below)
                float prev = 0.f;
                for (int i = 0; i <= 50; ++i) {
                    const float v = 10.0f * (float)i / 50.0f * 0.98f;
                    const float got = core.process(v, s);      // transform(norm(delayed)) — the core's ONE call
                    const float want = applyA(pe, prev, s, false);  // the primitive, evaluated EXTERNALLY
                    if (!sameBitsF(got, want)) { bridge = false; badName = e.name; badW = w; break; }
                    prev = v;
                }
                if (!bridge) break;
            }
            if (!bridge) break;
        }
        ok(bridge, "G4: core out == phaseDistort(normPhase(delayed), cell) * phaseInMax BIT-EXACT over the roster x signed-w x u-grid");
        if (!bridge) std::printf("       (first mismatch: %s w=%+.2f)\n", badName, badW);
        // The identity set's bridge is the RAW delayed sample (the adjusted gate's law).
        {
            PhaseDistSet ident{};
            redDot::PhaseDistChannel core;
            const float v0 = 3.7f, v1 = 8.2f;
            (void)core.process(v0, ident);
            const float got = core.process(v1, ident);
            ok(sameBitsF(got, v0), "G4: identity set — the core hands the RAW delayed voltage through (no round-trip)");
        }
    }

    // ── G6 A/B READY (module B == expander build A, delayed one sample) ─────────
    {
        std::printf("G6 A/B READY — B's stream == A's Mode E stream shifted one sample, matching params\n");
        redDot::PhaseEngine pe;
        bool ab = true;
        const char* badName = "";
        float badW = 0.f;
        for (const auto& e : g_roster) {
            for (float w : { -1.0f, -0.35f, 0.0f, 0.35f, 1.0f }) {
                // w = 0 with a LIVE member: the store's isIdentity() is still false (the
                // predicate requires the identity MEMBER), so BOTH sides run the primitive
                // path — and agree, delayed one sample. The identity-set case is G1/G4's.
                PhaseDistSet s = liveSet(e.f, w);
                ModuleRig rig;
                rig.store.writeSet(0, s);
                rig.reader_.beginBlock(rig.store);
                const bool ident = s.isIdentity();
                std::array<float, ModuleRig::kN> prevIn{};
                for (int n = 0; n < 400; ++n) {
                    const float v = srcAt(n);
                    for (int k = 0; k < ModuleRig::kN; ++k) rig.in_[k] = v;
                    rig.step();
                    if (n > 0) {
                        const float aPrev = applyA(pe, prevIn[0], s, ident);
                        if (!sameBitsF(rig.out_[0], aPrev)) {      // B[n] == A[n-1]
                            ab = false; badName = e.name; badW = w; break;
                        }
                    }
                    for (int k = 0; k < ModuleRig::kN; ++k) prevIn[k] = rig.in_[k];
                }
                if (!ab) break;
            }
            if (!ab) break;
        }
        ok(ab, "G6: module B out == expander build A out DELAYED ONE SAMPLE, bit-exact (roster x signed-w incl. 0)");
        if (!ab) std::printf("       (first mismatch: %s w=%+.2f)\n", badName, badW);
    }

    // ── the §13 BLOCK CADENCE — a write between blocks lands at the NEXT boundary ─
    {
        std::printf("BLOCK CADENCE — the module's per-block snapshot: a write lands at the next boundary, never mid-block\n");
        ModuleRig rig;
        const int K = 4;
        PhaseDistSet s = liveSet(g_roster[1].f, 0.5f);
        auto feed = [&](int n) {
            for (int k = 0; k < ModuleRig::kN; ++k) rig.in_[k] = srcAt(n);
            rig.step();
        };
        bool oldSnap = true;
        for (int n = 0; n < 31; ++n) {
            feed(n);
            if (rig.reader_.snap_[K].w != 0.f) oldSnap = false;   // the identity snapshot
        }
        ok(oldSnap, "cadence: frames 0..30 run the identity snapshot");
        rig.store.writeSet(K, s);   // the UI write, between frames 30 and 31
        feed(31);
        ok(rig.reader_.snap_[K].w == 0.f,
           "cadence: a write just before the boundary does NOT land mid-block (frame 31 still old)");
        feed(32);   // ctrlCount_ wraps to 0 exactly here — the boundary's beginBlock
        ok(rig.reader_.snap_[K].w == 0.5f,
           "cadence: frame 32's boundary picks the write up (the new set is live)");
        {
            redDot::PhaseEngine pe;
            const float prev = srcAt(31);
            const float want = applyA(pe, prev, s, false);
            ok(sameBitsF(rig.out_[K], want) && !sameBitsF(rig.out_[K], prev),
               "cadence: the output follows the new snapshot at the SAME boundary frame");
        }
    }

    std::printf("\n%d checks, %d pass, %d fail\n", g_pass + g_fail, g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
