// test_warp_mirror.cpp — CC_PROMPT_03 (Phase 3 step 2): the warp MIRROR + consumer wiring.
//
// The mirror holds the EFFECTIVE w-mixture over random_ (one array, no toggle — -18b §2):
//     warped_[v][lane][step] = (1-w)·u + w·F⁻¹(u),  u = random_[v][lane][step]
// computed in DOUBLE via WarpBank::warpedValue (exact invCDF, no new LUT — the mirror IS
// the cache) and stored FLOAT. Built from random_ AFTER the slew write-back, so random_
// stays the single w=0 basis for BOTH the sounding and display consumers.
//
// Sections (mirroring the prompt's STEP 5 list; later steps append S5+):
//   S1. w=0 ⟹ warped_ == random_ BIT-EXACTLY, for every warp kind (the no-op guarantee),
//       incl. a drawn pool curve; plus the null-bank fallback (warpRow → raw row).
//   S1b. The rebuild fires INSIDE recomputeEffectiveMelody (after the slew write-back) —
//       the real engine path, not just direct rebuild calls; random_ stays UNWARPED.
//   S2. w=1 + IdentityWarp ⟹ still identity (warpedValue == u even at w=1).
//   S3. w=1 parametric ⟹ warped == (float)invCDF(F, u) exactly; mixed w ⟹ exactly
//       (float)((1-w)u + w·invCDF) — the DOUBLE compute / FLOAT store contract.
//   S4. MONOTONE (Sklar-safety): dense ascending sweep ⟹ stored float never decreases,
//       strictly increasing for meaningful u-gaps (ties-at-eps tolerated) — incl. the
//       steep kuma (5, 25.8) case from test_warp_f_bank S7.
//   S8. DISPLAY COMPOSITION (CC_PROMPT_04 feel-test bug 1): warpDisplayMix(v,lane,u)
//       composes the SAME per-(voice,lane) (F,w) the mirror resolves (WarpBank::
//       warpedValue(v,lane,u,delegated) — Macro-global for delegated cells) over a
//       caller-supplied DISPLAY value; no bank ⇒ bit-identical passthrough. (CC_PROMPT_05:
//       callers compose EVERY lane — the per-lane MEL/OCT gate died with the uniform plumbing.)
//   S9. DRAW-PATH WARP CONTRACT (CC_PROMPT_04-FIX-2): the redrawMelody DRAW feeds
//       genPitchLive the MIRROR — warpRow(displayedVoice_, MEL/OCT), the SAME
//       accessor + voice key the refreshVisualCache display writer uses — so a draw
//       with warp set produces a DIFFERENT pattern than the same draw at w=0, the
//       two melodyPitchV writers agree bit-exactly, and the POLY landing
//       (refreshPolyRandomCell → warpRow) reads the FRESH mirror cell.
//
// Companion source: PatternEngine.cpp (the run_all.sh:59 test_slew_copula pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "WarpF.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>
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

// Deterministic, well-spread [0,1) values across the whole store (golden-ratio scramble),
// with a couple of pinned edge values so the endpoints are exercised.
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

static const std::vector<dotModular::WarpF> allParametricKinds() {
    using namespace dotModular;
    return { IdentityWarp{}, TriangularWarp{0.3}, PowerWarp{2.5}, ReversedPowerWarp{1.7},
             KumaraswamyWarp{2, 3}, MobiusWarp{0.8}, LogitSigmoidWarp{2, 0.3},
             LogitNormalWarp{-0.5, 1.2}, TruncNormalWarp{0.4, 0.15} };
}

int main() {
    std::cout << "\033[1mWarp mirror (CC_PROMPT_03 STEP 2) Tests\033[0m\n" << std::string(56, '=') << "\n";
    using dotModular::WarpBank;
    const int MEL = PatternEngine::PL_MELODY, OCT = PatternEngine::PL_OCTAVE;

    // ── S1: w=0 is a bit-exact no-op for EVERY kind ─────────────────────────────
    TEST("S1 w=0: warped_ == random_ bit-exact for every kind (incl. drawn pool curve)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank;
        const auto kinds = allParametricKinds();
        for (int i = 0; i < WarpBank::N; ++i) bank.warpF[i] = kinds[(size_t)i % kinds.size()];
        // A drawn curve in the pool, referenced from a slot (the pool-threading path).
        const int poolIdx = bank.putCurve(
            dotModular::MonotoneCurve::fromDistribution(dotModular::KumaraswamyDistribution(2, 3), 9));
        EXPECT(poolIdx >= 0);
        bank.warpF[WarpBank::index(5, MEL)] = dotModular::DrawnWarp{(uint16_t)poolIdx};
        // w stays 0 everywhere (bank default) — the no-op contract must hold regardless of F.
        pe.setWarpBank(&bank);
        pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    EXPECTF(std::memcmp(&pe.warped_[v][l][s], &pe.random_[v][l][s], sizeof(float)) == 0,
                            "cell (%d,%d,%d): got %.9g want %.9g", v, l, s,
                            (double)pe.warped_[v][l][s], (double)pe.random_[v][l][s]);
    });

    TEST("S1 null-bank fallback: warpRow(v,lane) IS the raw random_ row", {
        PatternEngine pe; fillRandomGolden(pe);   // no bank wired
        for (int v = 0; v < 16; v += 5)
            for (int l = 0; l < 7; ++l)
                EXPECT(&pe.warpRow(v, l)[0] == &pe.random_[v][l][0]);
    });

    // ── S1b: the rebuild fires inside the REAL recompute path (after the write-back) ──
    TEST("S1b recomputeEffectiveMelody rebuilds the mirror from post-slew random_", {
        PatternEngine pe;
        pe.seedMelodyPhilox(0.17f);
        for (int k = 0; k < 5; ++k) pe.advanceMelodyDraw(+1);
        pe.latchMix(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, /*R=*/false, /*M=*/true, /*Q=*/false);
        pe.recomputeEffectiveMelody();   // settles random_[0][MEL/OCT] from the draw
        WarpBank bank;
        bank.warpW[WarpBank::index(0, MEL)] = 1.f;
        bank.warpF[WarpBank::index(0, MEL)] = dotModular::TriangularWarp{0.25};
        pe.setWarpBank(&bank);           // wires + full rebuild
        pe.advanceMelodyDraw(+1);        // a NEW draw...
        pe.recomputeEffectiveMelody();   // ...write-back + hook must refresh the mirror
        const dotModular::TriangularWarp tri{0.25};
        for (int i = 0; i < 16; ++i) {
            const float u = pe.random_[0][MEL][i];
            // random_ itself stays the UNWARPED w=0 basis:
            EXPECT(pe.melodyRandom[i] == u);
            // ...and the mirror holds the w=1 mixture of exactly that basis:
            const float want = (float)tri.invCDF((double)u);
            EXPECTF(pe.warped_[0][MEL][i] == want, "step %d: got %.9g want %.9g (u=%.9g)",
                    i, (double)pe.warped_[0][MEL][i], (double)want, (double)u);
        }
    });

    // ── S2: w=1 + IdentityWarp is still identity ────────────────────────────────
    TEST("S2 w=1 + IdentityWarp: warped_ == random_ bit-exact", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank;                    // all-Identity by construction
        for (int i = 0; i < WarpBank::N; ++i) bank.warpW[i] = 1.f;
        pe.setWarpBank(&bank);
        pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    EXPECTF(pe.warped_[v][l][s] == pe.random_[v][l][s],
                            "cell (%d,%d,%d)", v, l, s);
    });

    // ── S3: parametric exactness — double compute, float store ──────────────────
    TEST("S3 w=1 parametric: warped == (float)invCDF(F,u) exactly (several kinds)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank;
        pe.setWarpBank(&bank);
        const int v = 2, lane = MEL;
        const auto kinds = allParametricKinds();
        for (size_t k = 0; k < kinds.size(); ++k) {
            for (int i = 0; i < WarpBank::N; ++i) { bank.warpF[i] = dotModular::IdentityWarp{}; bank.warpW[i] = 0.f; }
            bank.warpF[WarpBank::index(v, lane)] = kinds[k];
            bank.warpW[WarpBank::index(v, lane)] = 1.f;
            pe.rebuildWarpMirrorAll();
            for (int s = 0; s < 16; ++s) {
                const double u = (double)pe.random_[v][lane][s];
                const float want = (float)dotModular::invCDF(kinds[k], u, bank.pool);
                EXPECTF(pe.warped_[v][lane][s] == want, "kind %zu step %d: got %.9g want %.9g",
                        k, s, (double)pe.warped_[v][lane][s], (double)want);
            }
        }
    });

    TEST("S3 mixed w: warped == (float)((1-w)u + w·invCDF) exactly, incl. drawn pool curve", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank;
        pe.setWarpBank(&bank);
        const float w = 0.37f;
        const int poolIdx = bank.putCurve(
            dotModular::MonotoneCurve::fromDistribution(dotModular::TruncatedNormalDistribution(0.5, 0.2), 11));
        bank.warpF[WarpBank::index(3, OCT)] = dotModular::DrawnWarp{(uint16_t)poolIdx};
        bank.warpW[WarpBank::index(3, OCT)] = w;
        bank.warpF[WarpBank::index(7, MEL)] = dotModular::KumaraswamyWarp{5, 25.8};
        bank.warpW[WarpBank::index(7, MEL)] = w;
        pe.rebuildWarpMirrorAll();
        for (int s = 0; s < 16; ++s) {
            const double u = (double)pe.random_[3][OCT][s];
            const double F = bank.pool[(size_t)poolIdx].evalInverse(u);
            const float want = (float)((1.0 - w) * u + (double)w * F);
            EXPECTF(pe.warped_[3][OCT][s] == want, "drawn step %d: got %.9g want %.9g",
                    s, (double)pe.warped_[3][OCT][s], (double)want);
        }
        for (int s = 0; s < 16; ++s) {
            const double u = (double)pe.random_[7][MEL][s];
            const double F = dotModular::KumaraswamyWarp{5, 25.8}.invCDF(u);
            const float want = (float)((1.0 - w) * u + (double)w * F);
            EXPECTF(pe.warped_[7][MEL][s] == want, "kuma step %d: got %.9g want %.9g",
                    s, (double)pe.warped_[7][MEL][s], (double)want);
        }
    });

    // ── S4: MONOTONE — the Sklar-safety property on the stored float ────────────
    TEST("S4 monotone: dense ascending sweep never decreases; strict for u-gap ≥ 1/256", {
        PatternEngine pe;
        WarpBank bank;
        const int v = 3, lane = OCT;
        const float w = 0.5f;
        bank.warpF[WarpBank::index(v, lane)] = dotModular::TruncNormalWarp{0.5, 0.2};
        bank.warpW[WarpBank::index(v, lane)] = w;
        pe.setWarpBank(&bank);
        const int N = 1024;
        float prev = -1.f; int ties = 0;
        for (int k = 0; k < N; ++k) {
            const float u = (float)((double)k / (double)(N - 1));
            pe.random_[v][lane][0] = u;
            pe.rebuildWarpMirrorAll();
            const float cur = pe.warped_[v][lane][0];
            if (k > 0) {
                EXPECTF(cur >= prev, "k=%d DECREASED: %.9g < %.9g (u=%.9g)", k, (double)cur, (double)prev, (double)u);
                if (cur == prev) { ++ties; EXPECTF(u - (double)(float)((double)(k - 1) / (N - 1)) < 1.0 / 256.0,
                                               "tie at u-gap >= 1/256 (k=%d)", k); }
            }
            prev = cur;
        }
        EXPECTF(ties == 0, "expected zero ties on this sweep, got %d", ties);
    });

    TEST("S4 monotone: per-row 16-step ascending sweep strictly increasing (steep kuma 5,25.8 @ w=0.5)", {
        PatternEngine pe;
        WarpBank bank;
        const float w = 0.5f;
        // Steepest authored case (test_warp_f_bank S7's list) across several (v,lane) rows.
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                bank.warpF[WarpBank::index(v, l)] = dotModular::KumaraswamyWarp{5, 25.8};
                bank.warpW[WarpBank::index(v, l)] = w;
                for (int s = 0; s < 16; ++s)
                    pe.random_[v][l][s] = (float)((0.5 + s) / 16.5);   // ascending, ~0.0606 spacing
            }
        pe.setWarpBank(&bank);
        pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 15; ++s)
                    EXPECTF(pe.warped_[v][l][s + 1] > pe.warped_[v][l][s],
                            "row (%d,%d) step %d: %.9g !> %.9g", v, l, s,
                            (double)pe.warped_[v][l][s + 1], (double)pe.warped_[v][l][s]);
    });

    // ── S5: the dirty mask — scoped rebuilds touch EXACTLY their scope ───────────
    TEST("S5 Macro lane edit: rebuilds ONLY that lane's DELEGATING voices (others stale)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        // State A: every own-slot is steep kuma @ w=1; Macro's global is inert.
        for (int i = 0; i < WarpBank::N; ++i) {
            bank.warpF[i] = dotModular::KumaraswamyWarp{5, 25.8};
            bank.warpW[i] = 1.f;
        }
        const int lane = MEL;
        // Voices 0/3/7 delegate this lane to Macro; the rest own their slots.
        pe.setWarpDelegated(0, lane, true);
        pe.setWarpDelegated(3, lane, true);
        pe.setWarpDelegated(7, lane, true);
        pe.rebuildWarpMirrorAll();
        static float snap[16][7][16];
        std::memcpy(snap, pe.warped_, sizeof snap);
        // The edit event: Macro's curve for the lane changes...
        bank.globalWarpF[lane] = dotModular::TriangularWarp{0.8};
        bank.globalWarpW[lane] = 1.f;
        // ...AND a fresh draw lands on OTHER cells (random_ changes there too — a
        // full rebuild would pick these up; the scope must NOT).
        for (int v = 0; v < 16; ++v) for (int s = 0; s < 16; ++s)
            pe.random_[v][OCT][s] = (float)std::fmod(0.777 + (v * 16 + s) * 0.3819660113, 1.0);
        pe.rebuildWarpMirrorLaneDelegating(lane);
        const dotModular::TriangularWarp tri{0.8};
        // (1) delegating voices: lane cells now the global curve over the CURRENT basis.
        for (int v : {0, 3, 7})
            for (int s = 0; s < 16; ++s) {
                const float want = (float)tri.invCDF((double)pe.random_[v][lane][s]);
                EXPECTF(pe.warped_[v][lane][s] == want, "delegating (%d,%d): got %.9g want %.9g",
                        v, s, (double)pe.warped_[v][lane][s], (double)want);
            }
        // (2) non-delegating voices: lane cells UNTOUCHED (own slots didn't change).
        for (int v = 0; v < 16; ++v) {
            if (v == 0 || v == 3 || v == 7) continue;
            for (int s = 0; s < 16; ++s)
                EXPECTF(pe.warped_[v][lane][s] == snap[v][lane][s], "own-slot voice %d step %d", v, s);
        }
        // (3) other lanes: UNTOUCHED even though their random_ basis changed above.
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                if (l == lane) continue;
                for (int s = 0; s < 16; ++s)
                    EXPECTF(pe.warped_[v][l][s] == snap[v][l][s], "other lane (%d,%d,%d)", v, l, s);
            }
    });

    TEST("S5 Sands (East) edit: rebuilds exactly ONE voice's lane (16 cells), fresh basis", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        pe.rebuildWarpMirrorAll();
        static float snap[16][7][16];
        std::memcpy(snap, pe.warped_, sizeof snap);
        // The edit event: voice 5's melody slot gets a curve + a fresh draw on that lane.
        const int v = 5, lane = MEL;
        bank.warpF[WarpBank::index(v, lane)] = dotModular::TruncNormalWarp{0.3, 0.12};
        bank.warpW[WarpBank::index(v, lane)] = 1.f;
        for (int s = 0; s < 16; ++s)
            pe.random_[v][lane][s] = (float)std::fmod(0.313 + s * 0.2718281828, 1.0);
        pe.rebuildWarpMirrorVoiceLane(v, lane);
        const dotModular::TruncNormalWarp tn{0.3, 0.12};
        for (int s = 0; s < 16; ++s) {
            const float want = (float)tn.invCDF((double)pe.random_[v][lane][s]);
            EXPECTF(pe.warped_[v][lane][s] == want, "step %d: got %.9g want %.9g",
                    s, (double)pe.warped_[v][lane][s], (double)want);
        }
        // Everything else is UNTOUCHED (still the state-A snapshot).
        for (int vv = 0; vv < 16; ++vv)
            for (int l = 0; l < 7; ++l) {
                if (vv == v && l == lane) continue;
                for (int s = 0; s < 16; ++s)
                    EXPECTF(pe.warped_[vv][l][s] == snap[vv][l][s], "cell (%d,%d,%d)", vv, l, s);
            }
    });

    TEST("S5 cell scope: rebuilds exactly ONE cell (the step-rate companion)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        pe.rebuildWarpMirrorAll();
        static float snap[16][7][16];
        std::memcpy(snap, pe.warped_, sizeof snap);
        const int v = 9, lane = OCT, step = 4;
        bank.warpW[WarpBank::index(v, lane)] = 1.f;
        pe.random_[v][lane][step] = 0.123456f;
        pe.rebuildWarpMirrorCell(v, lane, step);
        EXPECT(pe.warped_[v][lane][step] == pe.random_[v][lane][step]);   // identity F @ w=1
        for (int vv = 0; vv < 16; ++vv)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s) {
                    if (vv == v && l == lane && s == step) continue;
                    EXPECTF(pe.warped_[vv][l][s] == snap[vv][l][s], "cell (%d,%d,%d)", vv, l, s);
                }
    });

    // ── S6: CONSUMER ROUTING — no consumer reads raw random_ for ANY lane ────────
    TEST("S6 display routing: finalRandomByStrand returns the MIRROR for EVERY strand (w>0; CC_PROMPT_05 uniform)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        bank.warpW[WarpBank::index(0, MEL)] = 1.f;
        bank.warpF[WarpBank::index(0, MEL)] = dotModular::TriangularWarp{0.25};
        bank.warpW[WarpBank::index(0, OCT)] = 1.f;
        bank.warpF[WarpBank::index(0, OCT)] = dotModular::PowerWarp{2};
        // Warp authored on a BOOLEAN lane too — CC_PROMPT_05: the per-lane gate is
        // GONE; every lane's display reads the mirror because every lane's engine
        // consumer does (the flip-together rule, now uniform by construction).
        bank.warpW[WarpBank::index(0, PatternEngine::PL_REST)] = 1.f;
        bank.warpF[WarpBank::index(0, PatternEngine::PL_REST)] = dotModular::PowerWarp{3};
        pe.rebuildWarpMirrorAll();
        int melDiffs = 0, octDiffs = 0, restDiffs = 0;
        for (int s = 0; s < 16; ++s) {
            const float m = pe.finalRandomByStrand(dotModular::STRAND_MELODY, s);
            EXPECT(m == pe.warped_[0][MEL][s]);
            if (m != pe.random_[0][MEL][s]) ++melDiffs;
            const float o = pe.finalRandomByStrand(dotModular::STRAND_OCTAVE, s);
            EXPECT(o == pe.warped_[0][OCT][s]);
            if (o != pe.random_[0][OCT][s]) ++octDiffs;
            // boolean lane: the mirror too — warp REACHES it now (test_warp_all_lanes
            // A3 pins the full 7-strand version of this contract).
            const float r = pe.finalRandomByStrand(dotModular::STRAND_RHYTHM, s);
            EXPECT(r == pe.warped_[0][PatternEngine::PL_REST][s]);
            if (r != pe.random_[0][PatternEngine::PL_REST][s]) ++restDiffs;
        }
        EXPECT(melDiffs > 0 && octDiffs > 0 && restDiffs > 0);   // visibly warped, not a vacuous pass
        // Null-bank host: raw everywhere (byte-identical legacy).
        PatternEngine pe2; fillRandomGolden(pe2);
        for (int s = 0; s < 16; ++s)
            EXPECT(pe2.finalRandomByStrand(dotModular::STRAND_MELODY, s) == pe2.random_[0][MEL][s]);
    });

    TEST("S6 sounding routing: the CV-out consumer carries the mirror for EVERY lane (CC_PROMPT_05 uniform)", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        bank.warpW[WarpBank::index(3, MEL)] = 1.f;
        bank.warpF[WarpBank::index(3, MEL)] = dotModular::TriangularWarp{0.25};
        bank.warpW[WarpBank::index(3, OCT)] = 1.f;
        bank.warpF[WarpBank::index(3, OCT)] = dotModular::PowerWarp{2};
        se.pe.rebuildWarpMirrorAll();
        int melDiffs = 0, octDiffs = 0;
        for (int s = 0; s < 16; ++s) {
            const float m = se.polyLaneProbabilityAtStep(SequencerEngine::PL_MELODY, 3, s);
            EXPECT(m == se.pe.warped_[3][MEL][s]);
            if (m != se.pe.random_[3][MEL][s]) ++melDiffs;
            const float o = se.polyLaneProbabilityAtStep(SequencerEngine::PL_OCTAVE, 3, s);
            EXPECT(o == se.pe.warped_[3][OCT][s]);
            if (o != se.pe.random_[3][OCT][s]) ++octDiffs;
            // non-authored lanes: mirror == raw at their identity defaults (warp
            // reaches ONLY authored cells — test_warp_all_lanes A2/A3 pin the rest):
            EXPECT(se.polyLaneProbabilityAtStep(SequencerEngine::PL_REST, 3, s) == se.pe.random_[3][PatternEngine::PL_REST][s]);
            EXPECT(se.polyLaneProbabilityAtStep(SequencerEngine::PL_ACCENT, 3, s) == se.pe.random_[3][PatternEngine::PL_ACCENT][s]);
        }
        EXPECT(melDiffs > 0 && octDiffs > 0);
    });

    // ── S7: the NO-OP PROOF — a wired bank at defaults is byte-identical to raw ──
    TEST("S7 no-op proof: bank wired at defaults (all-identity, w=0) — every consumer byte-identical", {
        PatternEngine pe; fillRandomGolden(pe);
        // A freshly-defaulted bank is exactly what Monsoon wires today (02 shipped
        // all-Identity + w=0). Wire it, rebuild, and prove BIT-IDENTITY end to end.
        WarpBank bank;
        pe.setWarpBank(&bank);   // wires + full rebuild from random_
        PatternInput in;
        in.octaveLo = 0.f; in.octaveHi = 3.f;
        for (int i = 0; i < 24; ++i) in.semiWeights[i] = (i % 3 == 0) ? 0.f : (0.25f + 0.5f * (float)((i * 7) % 5) / 4.f);
        for (int s = 0; s < 16; ++s) {
            int semA = 0, semB = 0;
            const float a = pe.genPitchLive(semA, in, pe.random_[0][MEL][s], pe.random_[0][OCT][s]);
            const float b = pe.genPitchLive(semB, in, pe.warpRow(0, MEL)[s], pe.warpRow(0, OCT)[s]);
            EXPECT(semA == semB);
            EXPECT(std::memcmp(&a, &b, sizeof(float)) == 0);
            // every strand's display funnel:
            for (int st = 0; st < dotModular::NUM_STRANDS; ++st)
                EXPECT(pe.finalRandomByStrand(st, s) == pe.random_[0][st][s]);
        }
        // and the sounding accessor, through a real SequencerEngine (every lane):
        SequencerEngine se;
        fillRandomGolden(se.pe);
        se.pe.setWarpBank(&bank);
        se.pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; v += 5)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    EXPECT(se.polyLaneProbabilityAtStep(l, v, s) == se.pe.random_[v][l][s]);
    });

    // ── S8: DISPLAY COMPOSITION (CC_PROMPT_04 feel-test bug 1) ─────────────────────
    // The bar fills must compose the warp mix ON TOP of the display's spread-
    // interpolated value via warpDisplayMix — the display twin of warpMixCell. It has
    // to resolve (F,w) EXACTLY as the mirror does (delegation included), so a bar can
    // never show a value the audio doesn't consume.
    TEST("S8 warpDisplayMix: same (F,w) as the mirror over an arbitrary display value (delegation-resolved)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        bank.warpF[WarpBank::index(4, MEL)] = dotModular::PowerWarp{2};
        bank.warpW[WarpBank::index(4, MEL)] = 0.25f;
        // A DELEGATED OCT cell: the own slot says power @ w=1, but delegation must win —
        // the mix uses Macro's GLOBAL curve/w, exactly like rebuildWarpMirror*.
        bank.warpF[WarpBank::index(4, OCT)] = dotModular::PowerWarp{2};
        bank.warpW[WarpBank::index(4, OCT)] = 1.f;
        bank.globalWarpF[OCT] = dotModular::ReversedPowerWarp{2};
        bank.globalWarpW[OCT] = 0.5f;
        pe.setWarpDelegated(4, OCT, true);
        const double disp = 0.6;   // a spread-interpolated DISPLAY value, not random_
        EXPECT(pe.warpDisplayMix(4, MEL, disp) ==
               (float)((1.0 - 0.25) * disp + 0.25 * dotModular::PowerWarp{2}.invCDF(disp)));
        EXPECT(pe.warpDisplayMix(4, OCT, disp) ==
               (float)((1.0 - 0.5) * disp + 0.5 * dotModular::ReversedPowerWarp{2}.invCDF(disp)));
        // out-of-range args clamp like every accessor (v→0, lane→0)
        EXPECT(pe.warpDisplayMix(-3, MEL, disp) == pe.warpDisplayMix(0, MEL, disp));
        EXPECT(pe.warpDisplayMix(4, 99, disp) == pe.warpDisplayMix(4, 0, disp));
    });

    TEST("S8 warpDisplayMix: no bank ⇒ bit-identical passthrough (bank-less hosts unchanged)", {
        PatternEngine pe; fillRandomGolden(pe);   // no bank wired
        const float u = pe.random_[6][OCT][3];
        const float got = pe.warpDisplayMix(6, OCT, (double)u);
        EXPECT(std::memcmp(&got, &u, sizeof(float)) == 0);
    });

    TEST("S8 warpDisplayMix: consistency with the mirror — same u ⇒ same cell (display never disagrees with audio)", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        bank.warpF[WarpBank::index(9, MEL)] = dotModular::KumaraswamyWarp{2, 3};
        bank.warpW[WarpBank::index(9, MEL)] = 0.7f;
        bank.globalWarpF[MEL] = dotModular::TruncNormalWarp{0.4, 0.15};
        bank.globalWarpW[MEL] = 0.9f;
        pe.setWarpDelegated(9, MEL, true);   // the delegated branch, against the mirror
        pe.rebuildWarpMirrorAll();
        for (int s = 0; s < 16; ++s) {
            const double u = (double)pe.random_[9][MEL][s];
            EXPECT(pe.warpDisplayMix(9, MEL, u) == pe.warped_[9][MEL][s]);
        }
    });

    // ── S9: DRAW-PATH WARP CONTRACT (CC_PROMPT_04-FIX-2 — the :787 raw bypass) ───
    // redrawMelody is the PRIMARY draw and one of the TWO melodyPitchV writers (the
    // other is refreshVisualCache's melodyChanged site). The contract: the uniform
    // fed to genPitchLive at the DRAW is the MIRROR row warpRow(displayedVoice_,
    // MEL/OCT) — the same accessor + voice key the display writer uses — so (a) a
    // draw with warp set produces a DIFFERENT pattern than the same draw at w=0
    // (the thing the scope should show), and (b) the two writers can never disagree.
    // (Pre-fix, the draw fed RAW slewedMelody[0]/slewedOctave[0] — the residue the
    // prompt pinned at PatternEngine.cpp:787.)

    // Deterministic twin setup: identical seed + latch + draw sequence per engine;
    // ONLY the warp bank differs between engines.
    auto makeInput = [] {
        PatternInput in;
        in.octaveLo = 0.f; in.octaveHi = 3.f;
        for (int i = 0; i < 24; ++i) in.semiWeights[i] = (i % 3 == 0) ? 0.f : (0.25f + 0.5f * (float)((i * 7) % 5) / 4.f);
        return in;
    };
    auto drawTwice = [](PatternEngine& pe, const PatternInput& in) {
        pe.seedMelodyPhilox(0.42f);
        pe.latchMix(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, /*R=*/false, /*M=*/true, /*Q=*/false);
        pe.redrawMelody(in);   // first draw (no counter advance)
        pe.redrawMelody(in);   // second draw (advanceMelodyDraw path)
    };

    TEST("S9 draw-path warp: redrawMelody feeds genPitchLive the MIRROR, not raw slewed (w>0 ⇒ different pattern)", {
        const PatternInput in = makeInput();
        PatternEngine peW, pe0, peNoBank;
        // peW: non-identity warp on (displayedVoice_=0, MELODY) at w=1.
        // pe0: same bank wired, w=0 (the no-op twin). peNoBank: bank-less legacy twin.
        WarpBank bankW, bank0;
        bankW.warpF[WarpBank::index(0, MEL)] = dotModular::PowerWarp{3};
        bankW.warpW[WarpBank::index(0, MEL)] = 1.f;
        peW.setWarpBank(&bankW);
        pe0.setWarpBank(&bank0);
        drawTwice(peW, in); drawTwice(pe0, in); drawTwice(peNoBank, in);
        // Non-vacuous: the warp actually moves the MEL mirror row (w=1, u→u³).
        int mirrorDiffs = 0;
        for (int i = 0; i < 16; ++i)
            if (peW.warpRow(0, MEL)[i] != peW.random_[0][MEL][i]) ++mirrorDiffs;
        EXPECT(mirrorDiffs > 0);
        // (1) EXACT: the drawn melodyPitchV/semitone ARE the mirror-fed values.
        for (int i = 0; i < 16; ++i) {
            int sem = 0;
            const float want = peW.genPitchLive(sem, in, peW.warpRow(0, MEL)[i], peW.warpRow(0, OCT)[i]);
            EXPECTF(peW.melodyPitchV[i] == want && peW.melodySemitone[i] == sem,
                    "step %d: draw got (%.9g, %d) want the mirror-fed (%.9g, %d)",
                    i, (double)peW.melodyPitchV[i], peW.melodySemitone[i], (double)want, sem);
        }
        // (2) DIFFERENTIAL: a warp-on draw differs from the SAME draw at w=0, and
        // w=0 with a bank is bit-identical to bank-less legacy (the no-op guarantee
        // must hold at the DRAW site too, not just in the mirror).
        int pitchDiffs = 0, semDiffs = 0;
        for (int i = 0; i < 16; ++i) {
            if (peW.melodyPitchV[i] != pe0.melodyPitchV[i]) ++pitchDiffs;
            if (peW.melodySemitone[i] != pe0.melodySemitone[i]) ++semDiffs;
            EXPECT(pe0.melodyPitchV[i] == peNoBank.melodyPitchV[i]);
            EXPECT(pe0.melodySemitone[i] == peNoBank.melodySemitone[i]);
        }
        EXPECTF(pitchDiffs > 0 && semDiffs > 0, "warp changed %d pitchV / %d semitone steps", pitchDiffs, semDiffs);
    });

    TEST("S9 draw/display agreement: the melodyChanged refresh re-derive leaves the drawn melodyPitchV bit-identical", {
        const PatternInput in = makeInput();
        PatternEngine pe;
        WarpBank bank;
        bank.warpF[WarpBank::index(0, MEL)] = dotModular::PowerWarp{3};
        bank.warpW[WarpBank::index(0, MEL)] = 1.f;
        pe.setWarpBank(&bank);
        drawTwice(pe, in);
        static float snapV[16]; static int snapS[16];
        std::memcpy(snapV, pe.melodyPitchV, sizeof snapV);
        std::memcpy(snapS, pe.melodySemitone, sizeof snapS);
        // Trigger the OTHER melodyPitchV writer (the melodyChanged display refresh)
        // WITHOUT a new draw: a pending mix change re-derives from the same
        // warpRow(displayedVoice_,·) — the two writers must agree bit-exactly.
        pe.melodyMixLatched = 0.5f;   // != melodyMixApplied (0) ⇒ the melodyChanged path
        pe.refreshVisualCache(in);
        for (int i = 0; i < 16; ++i)
            EXPECTF(pe.melodyPitchV[i] == snapV[i] && pe.melodySemitone[i] == snapS[i],
                    "step %d: display refresh rewrote the draw ((%.9g, %d) vs (%.9g, %d))",
                    i, (double)pe.melodyPitchV[i], pe.melodySemitone[i], (double)snapV[i], snapS[i]);
    });

    TEST("S9 draw-path warp keys the DISPLAYED voice: displayedVoice_=4 draws from warpRow(4,·)", {
        const PatternInput in = makeInput();
        PatternEngine pe;
        pe.numPolyVoicesHint = 5;    // the recompute covers rows 0..5, mirror rebuilt for all
        pe.displayedVoice_ = dotModular::VoiceResolver::slot(5);   // V5 = slot 4 (typed member)
        WarpBank bank;
        bank.warpF[WarpBank::index(4, MEL)] = dotModular::PowerWarp{3};
        bank.warpW[WarpBank::index(4, MEL)] = 1.f;
        pe.setWarpBank(&bank);
        drawTwice(pe, in);
        for (int i = 0; i < 16; ++i) {
            int sem = 0;
            const float want = pe.genPitchLive(sem, in, pe.warpRow(4, MEL)[i], pe.warpRow(4, OCT)[i]);
            EXPECTF(pe.melodyPitchV[i] == want && pe.melodySemitone[i] == sem,
                    "step %d: got (%.9g, %d) want the mirror(4)-fed (%.9g, %d)",
                    i, (double)pe.melodyPitchV[i], pe.melodySemitone[i], (double)want, sem);
        }
        // And NOT the old voice-0 raw read in disguise: the draw must differ from
        // what raw slewed[0] would have produced (the pre-fix :787 expression).
        int diffs = 0;
        for (int i = 0; i < 16; ++i) {
            int semRaw = 0;
            const float raw = pe.genPitchLive(semRaw, in, pe.slewedMelody[0][i], pe.slewedOctave[0][i]);
            if (raw != pe.melodyPitchV[i] || semRaw != pe.melodySemitone[i]) ++diffs;
        }
        EXPECT(diffs > 0);
    });

    TEST("S9 poly landing: refreshPolyRandomCell → warpRow reads the CURRENT cell's mirror (fresh, not raw)", {
        // The executePolyVoice call sequence (refresh the cell, then read the pitch
        // uniform through warpRow) must land on the mirror of the JUST-written
        // random_ value — the freshness half of the sounding-path contract.
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        const int v = 6, step = 9;
        bank.warpF[WarpBank::index(v, MEL)] = dotModular::TriangularWarp{0.25};
        bank.warpW[WarpBank::index(v, MEL)] = 1.f;
        // Make the step-rate refresh live and deterministic (Anchor-V1 spread).
        se.pe.cachedSpreadInitialized = true;          // cachedSpreadLiveM defaults true
        se.pe.cachedPolySpread[v][MEL] = 0.5f;
        se.pe.slewedMelody[v][step] = 0.2f;            // the voice's own draw
        se.pe.slewedMelody[0][step] = 0.9f;            // the V1 anchor target
        se.pe.random_[v][MEL][step] = 0.123456f;       // clearly-stale basis
        se.refreshPolyRandomCell(v, SequencerEngine::PL_MELODY, step);
        const float u = se.pe.random_[v][MEL][step];   // the JUST-refreshed basis
        EXPECTF(u != 0.123456f, "refresh did not move the cell (u=%.9g)", (double)u);
        const float want = (float)dotModular::TriangularWarp{0.25}.invCDF((double)u);   // w=1
        EXPECTF(se.pe.warpRow(v, SequencerEngine::PL_MELODY)[step] == want,
                "poly landing read %.9g, want the FRESH mirror %.9g (u=%.9g)",
                (double)se.pe.warpRow(v, SequencerEngine::PL_MELODY)[step], (double)want, (double)u);
        EXPECT(se.pe.warped_[v][MEL][step] == want);   // the cell-scope rebuild landed
    });

    std::cout << "-----\n";
    std::printf("warp mirror (STEP 2-5): %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
