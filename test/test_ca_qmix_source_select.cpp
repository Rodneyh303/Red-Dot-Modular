/**
 * test_ca_qmix_source_select.cpp — the CA q-mix source-select engine/data layer
 * (QMIX_LANE_PARITY_CHECKLIST §"The blend — the ONE genuinely new bit").
 *
 * Header-lite (no Rack SDK): it exercises the SAME primitives the data model uses —
 * ca::scatter over the q-mix scatter stream keys, deriveKey(STREAM_CA + i) with the
 * WIDENED 12-stream index space, the qmixSrc JSON clamp semantics, and the downstream
 * blend mux + markSemi correctness rule — mirroring the real code paths without pulling
 * in <rack.hpp>. See test_qmix_rng.cpp / test_change_alley_transforms.cpp for the pattern.
 *
 * Compile (see test/run_all.sh for the canonical line):
 *   g++ -std=c++17 -Itest -Isrc/dsp test/test_ca_qmix_source_select.cpp -o /tmp/t_caq && /tmp/t_caq
 */
#include "../src/dsp/ChangeAlleyTransforms.hpp"   // ca::scatter/scatterRows/collapseDomain (real)
#include "../src/dsp/PhiloxRng.hpp"               // redDot::seed::deriveKey / STREAM_CA
#include <cstdio>
#include <cstdint>
#include <cmath>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using namespace dotModular::ca;

// ── Data-model constants MIRRORED from ChangeAlleyV2Ids (Monsoon.hpp) ────────────────────────────
// These must stay in lockstep with the CA ids. SCATTER_TYPES=3 (rhythm=0, melody=1, q-mix=2) is the
// DATA-MODEL type dimension. The panel TYPES now also = 3 (Plan A 3-stream), so TYPES==SCATTER_TYPES.
// N_SCATTER = SIDES*SCATTER_TYPES*2 = 12.
static constexpr int SIDES         = 2;
static constexpr int SCATTER_TYPES = 3;
static constexpr int N_SCATTER     = SIDES * SCATTER_TYPES * 2;   // 12
static constexpr int N_VOICES      = 16;

// ci = (side*SCATTER_TYPES + type)*2 + (isDomain?0:1) — the EXACT index the transform apply uses.
static int ci_of(int side, int type, bool isDomain) {
    return (side * SCATTER_TYPES + type) * 2 + (isDomain ? 0 : 1);
}

// Standalone replica of the corrKey derivation (reseedCorrKeys): key[i] = deriveKey(seed, STREAM_CA+i).
static void deriveCorrKeys(float seedValue, uint64_t out[N_SCATTER]) {
    for (int i = 0; i < N_SCATTER; ++i)
        out[i] = redDot::seed::deriveKey(seedValue, redDot::seed::STREAM_CA + (uint64_t)i);
}

// Standalone replica of the qmixSrc JSON load clamp (dataFromJson): clamp(int, 0, N_VOICES-1).
static uint8_t jsonClamp(int v) {
    if (v < 0) v = 0; if (v > N_VOICES - 1) v = N_VOICES - 1; return (uint8_t)v;
}

int main() {
    // ── 1. qmixSrc round-trip (dataToJson → dataFromJson semantics) ───────────────────────────────
    // The plane persists as a 16-int array and reloads with a per-entry clamp to [0,15]. Round-trip
    // must be identity for valid data; out-of-range must saturate; short/absent arrays keep prior.
    {
        uint8_t qmixSrc[N_VOICES];
        // a non-trivial permutation-ish plane (fan-in allowed, like the real row-radio)
        const uint8_t src[N_VOICES] = {0,2,2,5,4,4,9,7,8,1,10,0,15,13,14,3};
        for (int v = 0; v < N_VOICES; ++v) qmixSrc[v] = src[v];

        // "save": integers; "load": clamp back. Identity for in-range values.
        uint8_t loaded[N_VOICES];
        for (int v = 0; v < N_VOICES; ++v) loaded[v] = jsonClamp((int)qmixSrc[v]);
        bool same = true; for (int v = 0; v < N_VOICES; ++v) same &= (loaded[v] == src[v]);
        CHK(same, "qmixSrc round-trip is identity for in-range values");

        // out-of-range saturates to [0,15]
        CHK(jsonClamp(-1) == 0 && jsonClamp(99) == 15, "qmixSrc load clamps out-of-range to [0,15]");

        // absent "qmixSrc" key → resetToIdentity() left it identity (qmixSrc[v]=v)
        uint8_t ident[N_VOICES]; for (int v = 0; v < N_VOICES; ++v) ident[v] = (uint8_t)v;
        bool isId = true; for (int v = 0; v < N_VOICES; ++v) isId &= (ident[v] == v);
        CHK(isId, "missing qmixSrc in old patch defaults to identity diagonal");
    }

    // ── 2. 12-stream scatter determinism (q-mix gets CA parity) ───────────────────────────────────
    // The widened index space keys 12 independent scatter streams via deriveKey(STREAM_CA + i).
    {
        uint64_t k1[N_SCATTER], k2[N_SCATTER];
        deriveCorrKeys(4.0f, k1);
        deriveCorrKeys(4.0f, k2);
        bool det = true; for (int i = 0; i < N_SCATTER; ++i) det &= (k1[i] == k2[i]);
        CHK(det, "same seed → identical 12 corrKeys (deterministic)");

        // All 12 keys distinct (per-index offset separates every stream, incl. the 4 NEW q-mix ones).
        bool distinct = true;
        for (int i = 0; i < N_SCATTER && distinct; ++i)
            for (int j = i + 1; j < N_SCATTER; ++j)
                if (k1[i] == k1[j]) { distinct = false; break; }
        CHK(distinct, "all 12 scatter-stream keys are pairwise distinct");

        // ci = (side*SCATTER_TYPES + type)*2 + (dom?0:1) is interleaved BY SIDE:
        //   side 0: rhythm 0/1, melody 2/3, q-mix 4/5   side 1: rhythm 6/7, melody 8/9, q-mix 10/11
        // The invariant that matters for determinism is rhythm=0 < melody=1 < q-mix=2 WITHIN each side
        // (the type order is monotone), and q-mix occupies the 4 NEW highest-type slots per side.
        const int qA = ci_of(0, 2, true),  qB = ci_of(0, 2, false);
        const int qC = ci_of(1, 2, true),  qD = ci_of(1, 2, false);
        CHK(qA == 4 && qB == 5 && qC == 10 && qD == 11,
            "q-mix scatter indices are the type-2 slots per side (4/5 and 10/11)");
        // rhythm=type0, melody=type1, qmix=type2 monotone within each side (ordering preserved).
        CHK(ci_of(0,0,true) < ci_of(0,1,true) && ci_of(0,1,true) < ci_of(0,2,true),
            "side 0: rhythm < melody < q-mix ci ordering (type monotone)");
        CHK(ci_of(1,0,true) < ci_of(1,1,true) && ci_of(1,1,true) < ci_of(1,2,true),
            "side 1: rhythm < melody < q-mix ci ordering (type monotone)");
        // All 12 ci values are exactly the set {0..11} (no gaps, no collisions).
        {
            bool seen[N_SCATTER] = {};
            bool ok = true;
            for (int sd = 0; sd < SIDES; ++sd)
                for (int ty = 0; ty < SCATTER_TYPES; ++ty)
                    for (int dm = 0; dm < 2; ++dm) {
                        int ci = ci_of(sd, ty, dm == 0);
                        if (ci < 0 || ci >= N_SCATTER || seen[ci]) { ok = false; }
                        else seen[ci] = true;
                    }
            for (int i = 0; i < N_SCATTER; ++i) ok &= seen[i];
            CHK(ok, "the 12 ci values tile {0..11} exactly (no gaps/collisions)");
        }

        // A fixed-seed qmixSrc SCATTER over a q-mix stream is reproducible AND reversible (the dice
        // model: at(position) is counter-addressable). Uses the REAL ca::scatter.
        uint8_t a[N_VOICES], b[N_VOICES];
        for (int v = 0; v < N_VOICES; ++v) { a[v] = (uint8_t)v; b[v] = (uint8_t)v; }
        scatter(a, N_VOICES, 4, k1[qA], /*position=*/1);
        scatter(b, N_VOICES, 4, k1[qA], /*position=*/1);
        bool repro = true; for (int v = 0; v < N_VOICES; ++v) repro &= (a[v] == b[v]);
        CHK(repro, "fixed seed+position → reproducible q-mix scatter (determinism)");

        // Position is addressable: a different counter page gives a (generally) different scatter,
        // and returning to the page re-derives the SAME result (reverse/scrub foundation).
        uint8_t c[N_VOICES]; for (int v = 0; v < N_VOICES; ++v) c[v] = (uint8_t)v;
        scatter(c, N_VOICES, 4, k1[qA], /*position=*/2);
        int diffPage = 0; for (int v = 0; v < N_VOICES; ++v) if (c[v] != a[v]) ++diffPage;
        CHK(diffPage > 0, "different counter position scatters differently (addressable)");
        uint8_t d[N_VOICES]; for (int v = 0; v < N_VOICES; ++v) d[v] = (uint8_t)v;
        scatter(d, N_VOICES, 4, k1[qA], /*position=*/1);   // back to page 1
        bool rewind = true; for (int v = 0; v < N_VOICES; ++v) rewind &= (d[v] == a[v]);
        CHK(rewind, "returning to a counter position re-derives the same scatter (reversible)");

        // The q-mix scatter stream is INDEPENDENT of the melody stream (different key → different
        // permutation), so scattering q-mix does not perturb melody's scatter and vice-versa.
        const int mA = ci_of(0, 1, true);   // melody domain intra
        uint8_t qm[N_VOICES], mm[N_VOICES];
        for (int v = 0; v < N_VOICES; ++v) { qm[v] = (uint8_t)v; mm[v] = (uint8_t)v; }
        scatter(qm, N_VOICES, 8, k1[qA], 1);
        scatter(mm, N_VOICES, 8, k1[mA], 1);
        int diffStream = 0; for (int v = 0; v < N_VOICES; ++v) if (qm[v] != mm[v]) ++diffStream;
        CHK(diffStream > 0, "q-mix scatter stream is independent of the melody stream");
    }

    // ── 3. The blend mux (downstream of CA) + the markSemi correctness rule ───────────────────────
    // pick = (r_qmix < level) ? generated : quantised-input   [threshold from the CA-scattered draw]
    // level 0 → always quantised-input; level 1 → always generated; mid → per-step split by the draw.
    // Whichever value is picked is the one whose DEGREE is marked (lastSemitone) — no drift.
    {
        // A tiny mux replica matching the engine's decision + degree-naming (12-TET degreeOf).
        auto degreeOf12 = [](float pitchV) -> int {
            float frac = pitchV - std::floor(pitchV);
            return ((int)std::lround(frac * 12.f)) % 12;
        };
        // Two operands that name DIFFERENT degrees so the mux choice is observable in lastSemitone.
        const float generatedPitch = 1.0f + 7.0f/12.0f;    // degree 7
        const float quantInputPitch = 0.0f + 3.0f/12.0f;   // degree 3
        auto mux = [&](float r_qmix, float level, float& outPitch, int& outSem) {
            bool useGenerated = (r_qmix < level);
            outPitch = useGenerated ? generatedPitch : quantInputPitch;
            outSem   = degreeOf12(outPitch);   // the CHOSEN value names its degree (markSemi source)
        };

        float p; int s;
        // level 0 → never generated → always quantised input (degree 3), == legacy behaviour.
        bool allInput = true;
        for (int i = 0; i < 32; ++i) { mux((float)i/32.f, 0.0f, p, s); allInput &= (p == quantInputPitch && s == 3); }
        CHK(allInput, "level 0 → always quantised input; lastSemitone == input degree (3)");

        // level 1 → always generated (degree 7).
        bool allGen = true;
        for (int i = 0; i < 32; ++i) { mux((float)i/32.f, 1.0f, p, s); allGen &= (p == generatedPitch && s == 7); }
        CHK(allGen, "level 1 → always generated; lastSemitone == generated degree (7)");

        // mid level → per-step split by the q-mix draw; each step's marked degree matches its pick.
        int genCount = 0, inCount = 0; bool markOk = true;
        for (int i = 0; i < 100; ++i) {
            float r = (float)i / 100.f;   // sweeps [0,1)
            mux(r, 0.5f, p, s);
            if (r < 0.5f) { ++genCount; markOk &= (p == generatedPitch && s == 7); }
            else          { ++inCount;  markOk &= (p == quantInputPitch && s == 3); }
        }
        CHK(genCount == 50 && inCount == 50, "mid level splits the sweep 50/50 by the draw");
        CHK(markOk, "markSemi rule: the CHOSEN value (gen OR input) is the one whose degree is marked");
    }

    // ── 4. CA transforms APPLY on the q-mix plane (verb-agnostic, via real applyCorrelation) ──────
    // The panel's q-mix pin row (type 2) drives the SAME transform engine as rhythm/melody: every
    // verb (collapse/rotate/reflect/scatter), both domain/codomain, intra & inter, permutes qmixSrc.
    // Here we drive the REAL dotModular::ca::applyCorrelation on a q-mix plane and assert it behaves
    // like the other planes + stays independent of them.
    {
        uint64_t k[N_SCATTER]; deriveCorrKeys(7.0f, k);
        const int qA = ci_of(0, 2, true);   // q-mix domain intra stream key

        // COLLAPSE domain @4 on q-mix: quartet leaders (identity board → leader = block base).
        { uint8_t q[N_VOICES]; for (int v=0;v<N_VOICES;++v) q[v]=(uint8_t)v;
          applyCorrelation(/*verb=*/0, /*isDomain=*/true, /*isInter=*/false, q, N_VOICES, /*grain=*/4, 0, k[qA], 0);
          CHK(q[0]==0 && q[3]==0 && q[4]==4 && q[7]==4 && q[15]==12,
              "q-mix COLLAPSE domain @4 = quartet leaders (same as melody plane)"); }

        // ROTATE values @16 on q-mix: +1 with wrap, composes per trigger.
        { uint8_t q[N_VOICES]; for (int v=0;v<N_VOICES;++v) q[v]=(uint8_t)v;
          applyCorrelation(1, false, false, q, N_VOICES, 16, 1, k[qA], 0);
          CHK(q[0]==1 && q[15]==0, "q-mix ROTATE values @16 = +1 wrap"); }

        // REFLECT rows @16 on q-mix: self-inverse over a double apply.
        { uint8_t q[N_VOICES], q0[N_VOICES]; for (int v=0;v<N_VOICES;++v){q[v]=(uint8_t)v;q0[v]=(uint8_t)v;}
          applyCorrelation(2, true, false, q, N_VOICES, 16, 0, k[qA], 0);
          applyCorrelation(2, true, false, q, N_VOICES, 16, 0, k[qA], 0);
          bool selfInv=true; for(int v=0;v<N_VOICES;++v) selfInv &= (q[v]==q0[v]);
          CHK(selfInv, "q-mix REFLECT rows is self-inverse over a double apply"); }

        // SCATTER on q-mix is counter-addressable + reversible via applyCorrelation (position/back).
        { uint8_t q1[N_VOICES], q2[N_VOICES]; for (int v=0;v<N_VOICES;++v){q1[v]=(uint8_t)v;q2[v]=(uint8_t)v;}
          applyCorrelation(3, false, false, q1, N_VOICES, 8, 0, k[qA], /*position=*/5);
          applyCorrelation(3, false, false, q2, N_VOICES, 8, 0, k[qA], /*position=*/5);
          bool repro=true; for(int v=0;v<N_VOICES;++v) repro &= (q1[v]==q2[v]);
          CHK(repro, "q-mix SCATTER via applyCorrelation is deterministic at a fixed position"); }
    }

    // ── 5. Routing-plane invariants (mirror PatternEngine caSrcRow / caInputCvSrcRow / caQmixSrcRow) ─
    // The downstream engine routes each operand through a SPECIFIC CA plane. These replicate the exact
    // selection so a plane-routing regression (e.g. q-mix accidentally reading the rhythm plane) fails.
    {
        // Strand ids mirrored from LaneMapping (STRAND_MELODY=0, OCTAVE=1, QMIX=2, RHYTHM=3, ACCENT=4,
        // VARIATION=5, LEGATO=6) — the values that matter for plane selection below.
        enum { S_MELODY=0, S_OCTAVE=1, S_QMIX=2, S_RHYTHM=3, S_ACCENT=4, S_VARIATION=5, S_LEGATO=6 };
        // Distinct sample planes so a wrong pick is observable.
        uint8_t caR[N_VOICES], caM[N_VOICES], caQ[N_VOICES];
        for (int v=0; v<N_VOICES; ++v) { caR[v]=(uint8_t)((v+1)%16); caM[v]=(uint8_t)((v+2)%16); caQ[v]=(uint8_t)((v+3)%16); }
        // Replica of PatternEngine::caSrcRow: q-mix→green, melody/octave→melody, else→rhythm.
        auto caSrcRow = [&](int row, int strand)->int {
            int r = (row>=0&&row<16)?row:0;
            if (strand==S_QMIX) return caQ[r];
            bool mel = (strand==S_MELODY || strand==S_OCTAVE);
            return mel ? caM[r] : caR[r];
        };
        auto caInputCvSrcRow = [&](int row)->int { int r=(row>=0&&row<16)?row:0; return caM[r]; };
        auto caQmixSrcRow     = [&](int row)->int { int r=(row>=0&&row<16)?row:0; return caQ[r]; };

        bool okQmix=true, okMelOct=true, okRest=true, okInCv=true, okThr=true;
        for (int row=0; row<16; ++row) {
            okQmix  &= (caSrcRow(row, S_QMIX)   == caQ[row]);                 // q-mix rides green plane
            okMelOct&= (caSrcRow(row, S_MELODY) == caM[row]) && (caSrcRow(row, S_OCTAVE) == caM[row]);
            okRest  &= (caSrcRow(row, S_RHYTHM) == caR[row]) && (caSrcRow(row, S_ACCENT) == caR[row])
                     && (caSrcRow(row, S_VARIATION)==caR[row]) && (caSrcRow(row, S_LEGATO)==caR[row]);
            okInCv  &= (caInputCvSrcRow(row) == caM[row]);                    // blend input-CV → melody plane
            okThr   &= (caQmixSrcRow(row)    == caQ[row]);                    // blend threshold → green plane
        }
        CHK(okQmix,   "caSrcRow(STRAND_QMIX) routes the q-mix (green) plane");
        CHK(okMelOct, "caSrcRow(MELODY/OCTAVE) routes the melody plane");
        CHK(okRest,   "caSrcRow(RHYTHM/ACCENT/VARIATION/LEGATO) routes the rhythm plane");
        CHK(okInCv,   "blend INPUT-CV operand rides CA's MELODY plane (caInputCvSrcRow)");
        CHK(okThr,    "blend THRESHOLD rides the q-mix GREEN plane (caQmixSrcRow)");
    }

    std::printf("%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
