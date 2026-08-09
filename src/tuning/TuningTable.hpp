#pragma once
// ── dotModular::TuningTable — the shared per-degree tuning/scale table ────────────────────────
// One structure, read by ALL pitch generation (genPitchLive, mono + poly) so every voice tunes
// from the same source. Populated by the built-in 12-TET default, or (Phase 1) by a Sikit expander
// writing cents[]. See MICRO_TUNING_INTEGRATION_PLAN.md + SIKIT_CLAUDE_CODE_GUIDE.md.
//
// PURE C++ (no Rack) so it is unit-testable and shareable. Arrays are sized MAXN=24 NOW (Rodney's
// Phase-1 ruling: free to size the arrays up-front, avoids a re-widen in Phase 3) even though N
// stays 12 for Sikit — only the first N entries are meaningful.
//
// FIELD OWNERSHIP (single-writer discipline, WriteLedger territory):
//   cents[]  — written by the claimed tuning-authoring expander (Sikit), else the equal-division
//              default below. Sikit is a PARTIAL writer: cents only.
//   weight[] — the scale mask; stays with Monsoon's own scale system in Phase 1 (Sikit never writes
//              it). Present here for the Micros (Phase 2/3) which will take it over. Phase-1 pitch
//              generation still reads the mask via the existing semiWeights path, NOT this field yet.

#include <cstdint>
#include <cmath>   // std::fabs (nearestDegree)

namespace dotModular {

struct TuningTable {
    static constexpr int MAXN = 24;   // room for Micro-24 (Phase 3); N stays 12 for Sikit/Phase 1

    int   N = 12;                     // active degree count (12 built-in; up to MAXN from a Micro)
    float cents[MAXN];                // per-degree cents from root (root = degree 0 = 0 cents)
    float weight[MAXN];               // per-degree LOUDNESS within scale (the live fader mix). Pure
                                      // loudness — NO LONGER carries mask meaning (see enabled[] below).
    // SCALE MEMBERSHIP mask (ENABLED_MASK_BUILD_BRIEF): enabled[i]=false => degree is OUT OF SCALE
    // (zeroed at read regardless of weight, fader dimmed). enabled[i]=true => in scale (weight is then
    // its loudness; weight 0 = in-scale-but-silent, raisable). Split from weight so a fader turned to 0
    // no longer freezes out-of-scale. Authoritative only when maskAuthored (a Colonnades/Duo rig);
    // otherwise Monsoon's own ScaleManager owns the mask and this is all-true (ignored) → 12-TET path
    // byte-identical. Defaults all-true so a fresh table behaves exactly as before.
    bool  enabled[MAXN];

    // TRUE => genPitchLive uses the EXACT legacy `(sem + transpose)/12` path (bit-identical to the
    // pre-refactor engine). Flips to FALSE only when a tuning-authoring expander publishes non-default
    // cents (Step F). Guarantees the Step-B refactor is a provable no-op at 12-TET.
    bool  isDefault12TET = true;

    // TRUE => a mask-authoring expander (Micro-12/24, Phase 2/3) owns weight[] this block, so the
    // engine should read the SCALE MASK from weight[] instead of Monsoon's own semiWeights/ScaleManager.
    // FALSE (default) => weight[] is NOT authoritative: Monsoon's own scale system owns the mask (the
    // Phase-1 world — Sikit writes cents only and never sets this, so the scale path is unchanged).
    // Reset to false each block by the module layer before expander sync; a claiming Micro sets it.
    bool  maskAuthored = false;

    TuningTable() { resetToEqual12TET(); }

    // Equal-division 12-TET default: cents[i] = i*100 (so cents[i]/1200 == i/12 as reals; because
    // i*100 and 1200 are exact floats and IEEE division is correctly rounded, the cents path is
    // bit-identical to the legacy sem/12 for the degree term — proven, not assumed).
    void resetToEqual12TET() {
        N = 12;
        for (int i = 0; i < MAXN; ++i) {
            cents[i]   = (i < 12) ? (float)i * 100.f : 0.f;
            weight[i]  = 0.f;
            enabled[i] = true;    // all degrees in-scale by default; mask carries no authority here
        }
        isDefault12TET = true;
        maskAuthored   = false;
    }

    // Within-octave voltage contribution of a degree (cents/1200). Callers add the octave integer
    // and (12-TET) transpose. Clamped index for safety.
    inline float degreeVolts(int degree) const {
        const int d = (degree < 0) ? 0 : (degree >= MAXN ? MAXN - 1 : degree);
        return cents[d] * (1.f / 1200.f);
    }

    // Degree (0..N-1) whose within-octave voltage is closest to `frac` (a 0..1 within-octave value =
    // pitch minus its octave). Used by Mode C/D to name the sounding degree for the flash LEDs under a
    // custom tuning (where the 12-TET `round(frac*12)` is wrong). At the 12-TET default the caller uses
    // the legacy `round(frac*12)%12` instead — this is only invoked when isDefault12TET is false.
    inline int nearestDegree(float frac) const {
        int best = 0; float bestD = 1e9f;
        for (int i = 0; i < N; ++i) {
            float d = std::fabs(frac - cents[i] * (1.f / 1200.f));
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    // Recompute isDefault12TET by EXACT comparison against the equal-division default (N==12 and
    // every cents[i]==i*100 exactly). A Sikit at its default knobs writes i*100.f verbatim, so this
    // stays true and genPitchLive keeps the byte-identical legacy path — the Step-I regression
    // guarantee. Any user/.scl detune flips it false, activating the cents path.
    void recomputeDefaultFlag() {
        bool def = (N == 12);
        for (int i = 0; def && i < 12; ++i)
            if (cents[i] != (float)i * 100.f) def = false;
        isDefault12TET = def;
    }
};

} // namespace dotModular
