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
//
// Companion source: PatternEngine.cpp (the run_all.sh:59 test_slew_copula pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
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

    // ── S6: CONSUMER ROUTING — no consumer reads raw random_ for melody/octave ───
    TEST("S6 display routing: finalRandomByStrand returns the MIRROR for MEL/OCT (w>0); RHYTHM stays raw", {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        bank.warpW[WarpBank::index(0, MEL)] = 1.f;
        bank.warpF[WarpBank::index(0, MEL)] = dotModular::TriangularWarp{0.25};
        bank.warpW[WarpBank::index(0, OCT)] = 1.f;
        bank.warpF[WarpBank::index(0, OCT)] = dotModular::PowerWarp{2};
        // Warp authored on a BOOLEAN lane too — the gate must keep its display raw
        // (a lane's display reads the mirror IFF its engine consumer does).
        bank.warpW[WarpBank::index(0, PatternEngine::PL_REST)] = 1.f;
        bank.warpF[WarpBank::index(0, PatternEngine::PL_REST)] = dotModular::PowerWarp{3};
        pe.rebuildWarpMirrorAll();
        int melDiffs = 0, octDiffs = 0;
        for (int s = 0; s < 16; ++s) {
            const float m = pe.finalRandomByStrand(dotModular::STRAND_MELODY, s);
            EXPECT(m == pe.warped_[0][MEL][s]);
            if (m != pe.random_[0][MEL][s]) ++melDiffs;
            const float o = pe.finalRandomByStrand(dotModular::STRAND_OCTAVE, s);
            EXPECT(o == pe.warped_[0][OCT][s]);
            if (o != pe.random_[0][OCT][s]) ++octDiffs;
            // boolean lane: raw even with warp authored on it (flip-together gate)
            EXPECT(pe.finalRandomByStrand(dotModular::STRAND_RHYTHM, s) == pe.random_[0][PatternEngine::PL_REST][s]);
        }
        EXPECT(melDiffs > 0 && octDiffs > 0);   // visibly warped, not a vacuous pass
        // Null-bank host: raw everywhere (byte-identical legacy).
        PatternEngine pe2; fillRandomGolden(pe2);
        for (int s = 0; s < 16; ++s)
            EXPECT(pe2.finalRandomByStrand(dotModular::STRAND_MELODY, s) == pe2.random_[0][MEL][s]);
    });

    TEST("S6 sounding routing: polyRandomSrc(MEL/OCT) carries the mirror; other lanes raw", {
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
            const float m = se.polyRandomSrc(3, SequencerEngine::PL_MELODY)[s];
            EXPECT(m == se.pe.warped_[3][MEL][s]);
            if (m != se.pe.random_[3][MEL][s]) ++melDiffs;
            const float o = se.polyRandomSrc(3, SequencerEngine::PL_OCTAVE)[s];
            EXPECT(o == se.pe.warped_[3][OCT][s]);
            if (o != se.pe.random_[3][OCT][s]) ++octDiffs;
            // non-melody lanes stay on the raw bank (their consumers don't warp yet):
            EXPECT(se.polyRandomSrc(3, SequencerEngine::PL_REST)[s] == se.pe.random_[3][PatternEngine::PL_REST][s]);
            EXPECT(se.polyRandomSrc(3, SequencerEngine::PL_ACCENT)[s] == se.pe.random_[3][PatternEngine::PL_ACCENT][s]);
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
        // and the sounding accessor, through a real SequencerEngine:
        SequencerEngine se;
        fillRandomGolden(se.pe);
        se.pe.setWarpBank(&bank);
        se.pe.rebuildWarpMirrorAll();
        for (int v = 0; v < 16; v += 5)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    EXPECT(se.polyRandomSrc(v, l)[s] == se.pe.random_[v][l][s]);
    });

    std::cout << "-----\n";
    std::printf("warp mirror (STEP 2-5): %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
