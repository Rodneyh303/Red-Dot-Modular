#include "PatternEngine.hpp"

// ── OU-bridge blend coefficients (UNIFIED 2×2 conditioning for ALL r) ─────────
// File-local: lives here (not in the header) to avoid pulling PhiInvLUT::table()
// into header-only test builds. One bridge for all r: r>0 (monotone kernel),
// r=0 (fresh ξ), r<0 (oscillatory kernel). ξ ALWAYS injected.
namespace {
struct OUBridgeCoeffs {
    double a = 0, b = 0, sqrtV = 0, xi = 0;
    void compute(float r, float frac, const redDot::PhiloxRng& bridge, int64_t pairIndex) {
        const double dfrac = (double)frac;
        if (r == 0.f) {
            if (frac == 0.f) { a = 1; b = 0; sqrtV = 0; }
            else if (frac == 1.f) { a = 0; b = 1; sqrtV = 0; }
            else { a = 0; b = 0; sqrtV = 1; }
        } else {
            const double absR  = std::fabs((double)r);
            const double omega = (r < 0.f) ? 3.14159265358979323846 : 0.0;
            const double rhoA  = std::pow(absR, dfrac)        * std::cos(omega * dfrac);
            const double rhoB  = std::pow(absR, 1.0 - dfrac) * std::cos(omega * (1.0 - dfrac));
            const double dr    = (double)r;
            const double det   = 1.0 - dr * dr;
            a = (rhoA - dr * rhoB) / det;
            b = (rhoB - dr * rhoA) / det;
            double v = 1.0 - (rhoA * a + rhoB * b);
            sqrtV = std::sqrt(v < 0.0 ? 0.0 : v);
        }
        xi = redDot::copula::PhiInv((double)bridge.atUniform((uint64_t)pairIndex));
    }
    double blend(double z0, double z1) const {
        return a * z0 + b * z1 + sqrtV * xi;
    }
};
} // anon namespace

void PatternEngine::reset() {
    rhythmSeedPending = melodySeedPending = qmixSeedPending = false;
    rhythmRollPending = melodyRollPending = qmixRollPending = false;
    rhythmPendingLast = melodyPendingLast = qmixPendingLast = false;
    rhythmReseedRollPending = melodyReseedRollPending = qmixReseedRollPending = false;
    rhythmReseedRollFull = melodyReseedRollFull = qmixReseedRollFull = false;
    rhythmMode = melodyMode = qmixMode = 0;
    rhythmSeedCached = melodySeedCached = qmixSeedCached = false;

    // Initialise the addressable Philox draw engines from the current seed floats
    // (default 0 → a fixed reproducible starting sequence, counter at 0).
    seedRhythmPhilox(rhythmSeedFloat);
    seedMelodyPhilox(melodySeedFloat);
    seedQmixPhilox(qmixSeedFloat);   // q-mix twin — its OWN STREAM_SOURCE_SELECT stream
    seedBridgeRhythmPhilox(rhythmSeedFloat);  // OU-bridge noise (S+4/5/6, user-seedable)
    seedBridgeMelodyPhilox(melodySeedFloat);
    seedBridgeQmixPhilox(qmixSeedFloat);

    // strands must not be all-zero or module is silent until dice/phrase
    for (int i = 0; i < 16; ++i) {
        rhythmRandom[i] = 1.0f;     // Mono triggers by default
        variationRandom[i] = 0.5f;  // No variation bias
        legatoRandom[i] = 0.0f;     // No legato/ties
        accentRandom[i] = 1.0f;     // No accents by default (roll < prob)
        melodyRandom[i] = 0.5f;     // Default to middle of weighted sum
        octaveRandom[i] = 0.5f;     // Default to middle of octave range
        qmixRandom[i]   = 0.5f;     // q-mix twin — default to middle
        
        for (int v = 0; v < 15; v++) {
            polyRandom(v + 1, PL_REST)[i] = 1.0f; // Poly voices trigger by default
            polyRandom(v + 1, PL_ACCENT)[i] = 1.0f; // No accent by default (1.0 < accentProb is false).
                                           // BUG FIX: this seed was missing (rhythm had it, accent
                                           // didn't), so polyAccentRandom stayed 0 → 0<accentProb
                                           // always true → EVERY poly note accented on any nonzero
                                           // accent knob. Mirrors polyAccentSource=1.0 below.
            polyRandom(v + 1, PL_MELODY)[i] = 0.5f;
            polyRandom(v + 1, PL_OCTAVE)[i] = 0.5f;
            polyRandom(v + 1, PL_QMIX)[i]   = 0.5f;   // q-mix twin
            polyRandom(v + 1, PL_VARIATION)[i] = 0.5f;  // Default — V1 is voice 0, not a mono reference
            polyRandom(v + 1, PL_LEGATO)[i]   = 0.0f;   // Default — V1 is voice 0, not a mono reference
            
            polyRhythmSource[v][i] = 1.0f;
            polyAccentSource[v][i] = 1.0f;
            polyMelodySource[v][i] = polyRandom(v + 1, PL_MELODY)[i];
            polyOctaveSource[v][i] = polyRandom(v + 1, PL_OCTAVE)[i];
            polyQmixSource[v][i]   = polyRandom(v + 1, PL_QMIX)[i];
            polyVariationSource[v][i] = polyRandom(v + 1, PL_VARIATION)[i];
            polyLegatoSource[v][i]   = polyRandom(v + 1, PL_LEGATO)[i];
        }
        
        rhythmSource[i] = rhythmRandom[i];
        variationSource[i] = variationRandom[i];
        legatoSource[i] = legatoRandom[i];
        accentSource[i] = accentRandom[i];
        melodySource[i] = melodyRandom[i];
        octaveSource[i] = octaveRandom[i];
        qmixSource[i]   = qmixRandom[i];

        rhythmPattern[i] = true;
    }

    // (Step 4c: removed A/B init -- no stored A/B arrays under the scrub model.)

    rhythmSlewLatched=melodySlewLatched=qmixSlewLatched=0.f;   // bipolar: 0 = independent
    rhythmSlewApplied=melodySlewApplied=qmixSlewApplied=-1.f;   // force first recompute
    rhythmFirstDraw=melodyFirstDraw=qmixFirstDraw=true;
    sandsActive=false;
    // Mirror defaults into slewedDraw too (final == slewed at reset)
    for (int i=0;i<16;i++){
        slewedRhythm[i]=rhythmRandom[i]; slewedVariation[i]=variationRandom[i];
        slewedLegato[i]=legatoRandom[i]; slewedAccent[i]=accentRandom[i];
        slewedMelody[i]=melodyRandom[i]; slewedOctave[i]=octaveRandom[i];
        slewedQmix[i]=qmixRandom[i];
        for (int v=0;v<numPolyVoicesHint;v++){
            slewedPolyRhythm[v][i]=polyRandom(v + 1, PL_REST)[i];
            slewedPolyMelody[v][i]=polyRandom(v + 1, PL_MELODY)[i];
            slewedPolyOctave[v][i]=polyRandom(v + 1, PL_OCTAVE)[i];
            slewedPolyQmix[v][i]=polyRandom(v + 1, PL_QMIX)[i];
        }
    }
}

// ── Core generation ───────────────────────────────────────────────────────

// Pick a DEGREE (0..n-1) weighted by fader values using a provided random value. `n` = active degree
// count (tuning.N): 12 for built-in/Sikit/Micro-12, up to 24 for Micro-24. At n=12 the loop bounds,
// sum, walk, and float-safety return are all identical to the legacy pickSemitone → byte-identical.
int PatternEngine::pickSemitone(const float weights[], int n, float r_val) {
    float sum = 0.f;
    for (int i = 0; i < n; ++i) sum += weights[i];
    if (sum <= 0.0001f) return -1;

    float r = pe_clamp(r_val, 0.f, 1.f - 1e-7f) * sum;
    float acc = 0.f;
    for (int i = 0; i < n; ++i) {
        if (weights[i] <= 0.f) continue;
        acc += weights[i];
        if (r < acc) return i;
    }
    return n - 1;  // float safety (was 11 at n=12)
}

// Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
float PatternEngine::genPitch(int& outSemitone, const PatternInput& in) {
    return genPitchLive(outSemitone, in, unitMelody(), unitMelody());
}

// Generate a pitch voltage using provided random floats for semitone and octave selection.
float PatternEngine::genPitchLive(int& outSemitone, const PatternInput& in, float r_semi, float r_oct) {
    // Degree count from the shared tuning table (12 default; up to 24 for a Micro-24). pickSemitone is
    // N-bounded so the weighted walk only considers active degrees.
    int sem = pickSemitone(in.semiWeights, tuning.N, r_semi);
    outSemitone = (sem < 0) ? 0 : sem;
    if (sem < 0) return 0.f;

    float lo = in.octaveLo, hi = in.octaveHi;
    if (hi < lo) std::swap(lo, hi);
    int oL = (int)std::floor(lo);
    int oH = (int)std::floor(hi);
    // Ensure r_oct roll isn't exactly 1.0 to prevent oOB indexing
    float roll = pe_clamp(r_oct, 0.f, 0.9999f);
    int oct = oL + (int)std::floor(roll * float(oH - oL + 1));

    // Degree -> voltage via the shared TuningTable (Sikit Phase 1). The octave integer and the
    // 12-TET transpose stay unchanged (Rodney's Phase-1 ruling: transpose remains 12-TET semitones);
    // only the WITHIN-OCTAVE degree term is table-driven. While the table is the equal-division
    // 12-TET default, take the EXACT legacy expression so this refactor is bit-identical; the cents
    // path activates only when a Sikit publishes non-default cents.
    float v;
    if (tuning.isDefault12TET) {
        v = float(oct) - 4.f + (sem + in.transpose) / 12.f;   // legacy path — byte-identical
    } else {
        // Custom tuning (Micro): TRANSPOSE IS OCTAVES-ONLY (Rodney). A 12-TET semitone shift (/12)
        // lands OFF the tuning grid; an octave is +1V in ANY tuning, so it's always tuning-native.
        // Monsoon's transpose knob is ±12 semitones → snap to the nearest whole octave and apply as
        // whole volts (±1 octave from this knob; Intertropical's own per-output knobs cover ±2).
        const float octShift = std::round(in.transpose / 12.f);   // -1 / 0 / +1
        v = float(oct) - 4.f + tuning.degreeVolts(sem) + octShift;
    }
    // Allow full bipolar range for 1V/oct standard; clamping to 0V clips octaves 0-3
    return pe_clamp(v, -5.f, 5.f);
}

// Apply variation bias to a note length index.
int PatternEngine::varyNoteIndex(int baseIdx, const PatternInput& in, float r) {
    // Weights are evenly distributed over a window that expands from the base 
    // index toward the shorter (var > 0.5) or longer (var < 0.5) extremes. 
    // At 50% variation, only the base note is played. At extremes, weight 
    // is shared uniformly across the reachable range.

    auto allowed = [&](int idx) -> bool {
        if (idx < 0 || idx >= 8) return false;
        if (idx == 3) return (in.noteVariationMask & 0b001) != 0; // 1/4T
        if (idx == 5) return (in.noteVariationMask & 0b010) != 0; // 1/8T
        if (idx == 7) return (in.noteVariationMask & 0b100) != 0; // 1/32 & 1/32T
        return true;
    };

    float var = in.variationAmount;
    if (std::fabs(var - 0.5f) < 1e-4f) return baseIdx;

    float spread = 2.f * std::fabs(var - 0.5f); // 0..1
    int direction = (var < 0.5f) ? -1 : 1;
    int targetExtreme = (direction == -1) ? 0 : 7;
    float maxDist = (float)std::abs(targetExtreme - baseIdx);
    
    if (maxDist < 0.5f) return baseIdx; 

    float reach = spread * maxDist;
    float weights[8] = {};
    float total = 0.f;

    for (int i = 0; i < 8; ++i) {
        int dist = direction * (i - baseIdx);
        if (dist < 0) continue; // Only consider chosen direction
        
        if (i == baseIdx) {
            weights[i] = 1.0f; // Base note always active
        } else {
            if (!allowed(i)) continue;

            float fDist = (float)dist;
            if (fDist <= reach) {
                weights[i] = 1.0f;
            } else if (fDist < reach + 1.0f) {
                weights[i] = reach - (fDist - 1.0f); // partial weight for the leading edge
            }
        }
        total += weights[i];
    }
    
    if (total <= 1e-6f) return baseIdx;
    
    float acc = 0.f;
    float roll = r * total;
    for (int i = 0; i < 8; ++i) {
        if (weights[i] > 0.f) {
            acc += weights[i];
            if (roll <= acc) return i;
        }
    }
    return baseIdx;
}

// Regenerate rhythm pattern (16 steps of bool: true=active, false=rest)
void PatternEngine::redrawRhythm(const PatternInput& in) {
    if (in.locked && !in.diceLiveR) return;   // LOCK_SCOPE_MENU: rhythm dice may draw under lock if opted live

    // B is committed into A FIRST, then a fresh B is drawn, and slew blends A↔B
    // at the roll. A walks forward each roll → groove mutates; slew=0 = independent
    // (full replace, MeloDicer mode), slew>0 = correlated (smooth), slew<0 = anti-correlated.
    // Slew still blends at the roll, so the user auditions candidates against the
    // same anchor A (raise slew to move toward B, lower to fall back to A).
    // First draw (or post-seed): A := B := draw, so effective == draw at any slew.
    const bool first = rhythmFirstDraw;
    rhythmFirstDraw = false;

    // Philox addressable draw bookkeeping. A fresh seed (first) draws chunk 0. Forward
    // → index +1; a reversible stream under backward phase → index -1 (no floor —
    // negative indices are valid reproducible draws). Cursor resets to map unit() calls
    // to this draw's chunk base.
    if (!first) advanceRhythmDraw(rhythmDrawDir());
    beginRhythmDraw();

    // Dice roll = advance the counter (done above). The effective pattern is re-derived from the
    // counter by recomputeEffectiveRhythm (scrub/B2 window) -- no stored A/B walk. (Step 4a removed
    // the old promote-A<-B + step()=a+slew*(unit()-a) population; slew is now the FIR smoothing
    // width in patternRhythmAt, not a roll-time B-shaping step.)
    (void)first;   // first-draw no longer special-cased: infinite-line, patternAt(N) is self-contained
    rhythmSlewApplied = -1.f;       // force recompute of slewedDraw
    recomputeEffectiveRhythm();
    // Cache source from the SLEWED draw (canonical draw; final may be rewritten
    // by Sands later this cycle). UI pattern previews the no-Sands final.
    for (int i = 0; i < 16; ++i) {
        rhythmSource[i]=slewedRhythm[i]; variationSource[i]=slewedVariation[i];
        legatoSource[i]=slewedLegato[i]; accentSource[i]=slewedAccent[i];
        for (int v=0;v<numPolyVoicesHint;v++) polyRhythmSource[v][i]=slewedPolyRhythm[v][i];
        for (int v=0;v<numPolyVoicesHint;v++) polyAccentSource[v][i]=slewedPolyAccent[v][i];
        rhythmPattern[i] = (slewedRhythm[i] >= in.restProb);
    }
}

// slew: slewedDraw[] = A + slew*(B-A). When no Sands owns the spread stage,
// copy slewedDraw → final (the public arrays the sequencer reads).
void PatternEngine::recomputeEffectiveRhythm() {
    // SCRUB + copula slew. Two adjacent copula windows (N-f, N-f-1) are interpolated at the scrub
    // fraction. Phase 2: at r!=0 the blend is in NORMAL SPACE (blend the two z values, apply PhiFast
    // once) — blending the uniform outputs would be a linear blend of uniforms (the distortion
    // Phase 2 eliminates). At r==0 (slew knob=0) the old linear blend of raw draws is kept, so
    // bit-identity at scrub=0 is preserved.
    const float s = rack::math::clamp(rhythmMixLatched, 0.f, 1.f) * 6.f;
    const int   f    = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = rhythmDrawCtr;
    const float slew = rack::math::clamp(rhythmSlewLatched, -1.f, 1.f);
    const float r    = slewKnobToR(slew);
    constexpr std::size_t K = redDot::MovingAverageCopula::K;
    if (r != 0.f) {
        // Weight-cache: r is phrase-fixed, so weights(r) is the same for every sumZ call in
        // this recompute (16 steps x lanes x voices x 2 windows).  Compute once, reuse.
        if (!rhythmWeightCache_.matches(r)) rhythmWeightCache_.update(r);
        const double* wc = rhythmWeightCache_.w;
        // z0/z1 cache: within a phrase (r fixed) + same integer position f, the two window
        // sumZ results depend ONLY on position -> cache and reuse on frac-only changes (scrub
        // within a position-pair).  Invalidate on roll (ctr), position-cross (f), or r change.
        const bool cacheHit = rhythmZCache_.hit(N, f, r);
        // B2: OU-bridge coefficients — computed once per (r, frac, pair). Three branches:
        // r>0 (OU with ξ), r=0 (fresh ξ), r<0 (variance-normalized, no ξ).
        OUBridgeCoeffs ouBC;
        ouBC.compute(r, frac, bridgeRhythmPhilox, N - f - 1);
        const CachedRhythmDraw* w0[K];
        const CachedRhythmDraw* w1[K];
        if (!cacheHit) {
            for (std::size_t j = 0; j < K; ++j) {
                w0[j] = &cachedRhythmDraw(N - f     - (int64_t)j);
                w1[j] = &cachedRhythmDraw(N - f - 1 - (int64_t)j);
            }
        }
        double zw[K];
        auto blend = [&](int i, auto field, int fi) -> float {
            double z0, z1;
            if (cacheHit) {
                z0 = rhythmZCache_.z0[fi];
                z1 = rhythmZCache_.z1[fi];
            } else {
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w0[j], i);
                z0 = redDot::MovingAverageCopula::sumZ(zw, wc);
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w1[j], i);
                z1 = redDot::MovingAverageCopula::sumZ(zw, wc);
                rhythmZCache_.z0[fi] = z0;
                rhythmZCache_.z1[fi] = z1;
            }
            return (float)redDot::copula::PhiFast(ouBC.blend(z0, z1));
        };
        int fi = 0;
        for (int i = 0; i < 16; ++i) {
            slewedRhythm[i]    = blend(i, [](const CachedRhythmDraw* c, int i){ return c->zRhythm[i]; }, fi++);
            slewedVariation[i] = blend(i, [](const CachedRhythmDraw* c, int i){ return c->zVariation[i]; }, fi++);
            slewedLegato[i]    = blend(i, [](const CachedRhythmDraw* c, int i){ return c->zLegato[i]; }, fi++);
            slewedAccent[i]    = blend(i, [](const CachedRhythmDraw* c, int i){ return c->zAccent[i]; }, fi++);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyRhythm[v][i] = blend(i, [v](const CachedRhythmDraw* c, int i){ return c->zPolyRhythm[v][i]; }, fi++);
                slewedPolyAccent[v][i] = blend(i, [v](const CachedRhythmDraw* c, int i){ return c->zPolyAccent[v][i]; }, fi++);
                slewedPolyVariation[v][i] = blend(i, [v](const CachedRhythmDraw* c, int i){ return c->zPolyVariation[v][i]; }, fi++);
                slewedPolyLegato[v][i] = blend(i, [v](const CachedRhythmDraw* c, int i){ return c->zPolyLegato[v][i]; }, fi++);
            }
        }
        if (!cacheHit) {
            rhythmZCache_.cachedCtr = N; rhythmZCache_.cachedF = f;
            rhythmZCache_.cachedR = r;  rhythmZCache_.valid = true;
        }
    } else {
        // r==0: old linear blend of raw draws (bit-identity at frac=0).
        RhythmDraw d0, d1;
        patternRhythmAt(N - f,     slew, d0);
        patternRhythmAt(N - f - 1, slew, d1);
        auto bl = [frac](float a, float b){ return a + frac*(b-a); };
        for (int i = 0; i < 16; ++i) {
            slewedRhythm[i]=bl(d0.rhythm[i],d1.rhythm[i]);
            slewedVariation[i]=bl(d0.variation[i],d1.variation[i]);
            slewedLegato[i]=bl(d0.legato[i],d1.legato[i]);
            slewedAccent[i]=bl(d0.accent[i],d1.accent[i]);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyRhythm[v][i]=bl(d0.polyRhythm[v][i],d1.polyRhythm[v][i]);
                slewedPolyAccent[v][i]=bl(d0.polyAccent[v][i],d1.polyAccent[v][i]);
                slewedPolyVariation[v][i]=bl(d0.polyVariation[v][i],d1.polyVariation[v][i]);
                slewedPolyLegato[v][i]=bl(d0.polyLegato[v][i],d1.polyLegato[v][i]);
            }
        }
    }
    // Poly VAR/LEG are now drawn by the Philox stream and copula-blended above
    // (same as the other 5 lanes). No raw fallback needed.
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            rhythmRandom[i]=slewedRhythm[i]; variationRandom[i]=slewedVariation[i];
            legatoRandom[i]=slewedLegato[i]; accentRandom[i]=slewedAccent[i];
            for (int v=0;v<numPolyVoicesHint;v++) polyRandom(v + 1, PL_REST)[i]=slewedPolyRhythm[v][i];
            // BUG FIX: poly accent was NOT promoted here (only rhythm was), so in the non-sands
            // path polyAccentRandom never received its slewed random values — it stayed at its
            // init value (0 → all notes accent; or 1.0 after the init-seed fix → no notes
            // accent). This is the real root cause; the init seed only changed which stuck
            // value showed. Mirror the rhythm promotion. (sandsActive path already sets both via
            // SpreadInterp at MonsoonSandsManager 460/463.)
            for (int v=0;v<numPolyVoicesHint;v++) polyRandom(v + 1, PL_ACCENT)[i]=slewedPolyAccent[v][i];
            // SANDS CONSOLIDATION: promote poly VAR/LEG too (same family — rhythm Philox stream).
            for (int v=0;v<numPolyVoicesHint;v++) polyRandom(v + 1, PL_VARIATION)[i]=slewedPolyVariation[v][i];
            for (int v=0;v<numPolyVoicesHint;v++) polyRandom(v + 1, PL_LEGATO)[i]=slewedPolyLegato[v][i];
        }
    }
    publishSlewedRhythm();   // publish coherent snapshot for the UI thread
    rhythmMixApplied = rhythmMixLatched; rhythmSlewApplied = slew; rhythmCtrApplied = N;
}

void PatternEngine::recomputeEffectiveMelody() {

    // Phase 2: normal-space scrub blend (mirror of recomputeEffectiveRhythm).
    const float s = rack::math::clamp(melodyMixLatched, 0.f, 1.f) * 6.f;
    const int   f    = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = melodyDrawCtr;
    const float slew = rack::math::clamp(melodySlewLatched, -1.f, 1.f);
    const float r    = slewKnobToR(slew);
    constexpr std::size_t K = redDot::MovingAverageCopula::K;
    if (r != 0.f) {
        if (!melodyWeightCache_.matches(r)) melodyWeightCache_.update(r);
        const double* wc = melodyWeightCache_.w;
        const bool cacheHit = melodyZCache_.hit(N, f, r);
        OUBridgeCoeffs ouBC; ouBC.compute(r, frac, bridgeMelodyPhilox, N - f - 1);
        const CachedMelodyDraw* w0[K];
        const CachedMelodyDraw* w1[K];
        if (!cacheHit) {
            for (std::size_t j = 0; j < K; ++j) {
                w0[j] = &cachedMelodyDraw(N - f     - (int64_t)j);
                w1[j] = &cachedMelodyDraw(N - f - 1 - (int64_t)j);
            }
        }
        double zw[K];
        auto blend = [&](int i, auto field, int fi) -> float {
            double z0, z1;
            if (cacheHit) {
                z0 = melodyZCache_.z0[fi];
                z1 = melodyZCache_.z1[fi];
            } else {
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w0[j], i);
                z0 = redDot::MovingAverageCopula::sumZ(zw, wc);
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w1[j], i);
                z1 = redDot::MovingAverageCopula::sumZ(zw, wc);
                melodyZCache_.z0[fi] = z0;
                melodyZCache_.z1[fi] = z1;
            }
            return (float)redDot::copula::PhiFast(ouBC.blend(z0, z1));
        };
        int fi = 0;
        for (int i = 0; i < 16; ++i) {
            slewedMelody[i] = blend(i, [](const CachedMelodyDraw* c, int i){ return c->zMelody[i]; }, fi++);
            slewedOctave[i] = blend(i, [](const CachedMelodyDraw* c, int i){ return c->zOctave[i]; }, fi++);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyMelody[v][i] = blend(i, [v](const CachedMelodyDraw* c, int i){ return c->zPolyMelody[v][i]; }, fi++);
                slewedPolyOctave[v][i] = blend(i, [v](const CachedMelodyDraw* c, int i){ return c->zPolyOctave[v][i]; }, fi++);
            }
        }
        if (!cacheHit) {
            melodyZCache_.cachedCtr = N; melodyZCache_.cachedF = f;
            melodyZCache_.cachedR = r;  melodyZCache_.valid = true;
        }
    } else {
        MelodyDraw d0, d1;
        patternMelodyAt(N - f,     slew, d0);
        patternMelodyAt(N - f - 1, slew, d1);
        auto bl = [frac](float a, float b){ return a + frac*(b-a); };
        for (int i = 0; i < 16; ++i) {
            slewedMelody[i]=bl(d0.melody[i],d1.melody[i]);
            slewedOctave[i]=bl(d0.octave[i],d1.octave[i]);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyMelody[v][i]=bl(d0.polyMelody[v][i],d1.polyMelody[v][i]);
                slewedPolyOctave[v][i]=bl(d0.polyOctave[v][i],d1.polyOctave[v][i]);
            }
        }
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
            for (int v=0;v<numPolyVoicesHint;v++){ polyRandom(v + 1, PL_MELODY)[i]=slewedPolyMelody[v][i];
                                    polyRandom(v + 1, PL_OCTAVE)[i]=slewedPolyOctave[v][i]; }
        }
    }
    publishSlewedMelody();
    melodyMixApplied = melodyMixLatched; melodySlewApplied = slew; melodyCtrApplied = N;
}

void PatternEngine::recomputeEffectiveQmix() {

    // Phase 2: normal-space scrub blend (mirror, q-mix stream).
    const float s = rack::math::clamp(qmixMixLatched, 0.f, 1.f) * 6.f;
    const int   f    = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = qmixDrawCtr;
    const float slew = rack::math::clamp(qmixSlewLatched, -1.f, 1.f);
    const float r    = slewKnobToR(slew);
    constexpr std::size_t K = redDot::MovingAverageCopula::K;
    if (r != 0.f) {
        if (!qmixWeightCache_.matches(r)) qmixWeightCache_.update(r);
        const double* wc = qmixWeightCache_.w;
        const bool cacheHit = qmixZCache_.hit(N, f, r);
        OUBridgeCoeffs ouBC; ouBC.compute(r, frac, bridgeQmixPhilox, N - f - 1);
        const CachedQmixDraw* w0[K];
        const CachedQmixDraw* w1[K];
        if (!cacheHit) {
            for (std::size_t j = 0; j < K; ++j) {
                w0[j] = &cachedQmixDraw(N - f     - (int64_t)j);
                w1[j] = &cachedQmixDraw(N - f - 1 - (int64_t)j);
            }
        }
        double zw[K];
        auto blend = [&](int i, auto field, int fi) -> float {
            double z0, z1;
            if (cacheHit) {
                z0 = qmixZCache_.z0[fi];
                z1 = qmixZCache_.z1[fi];
            } else {
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w0[j], i);
                z0 = redDot::MovingAverageCopula::sumZ(zw, wc);
                for (std::size_t j = 0; j < K; ++j) zw[j] = field(w1[j], i);
                z1 = redDot::MovingAverageCopula::sumZ(zw, wc);
                qmixZCache_.z0[fi] = z0;
                qmixZCache_.z1[fi] = z1;
            }
            return (float)redDot::copula::PhiFast(ouBC.blend(z0, z1));
        };
        int fi = 0;
        for (int i = 0; i < 16; ++i) {
            slewedQmix[i] = blend(i, [](const CachedQmixDraw* c, int i){ return c->zQmix[i]; }, fi++);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyQmix[v][i] = blend(i, [v](const CachedQmixDraw* c, int i){ return c->zPolyQmix[v][i]; }, fi++);
            }
        }
        if (!cacheHit) {
            qmixZCache_.cachedCtr = N; qmixZCache_.cachedF = f;
            qmixZCache_.cachedR = r;  qmixZCache_.valid = true;
        }
    } else {
        QmixDraw d0, d1;
        patternQmixAt(N - f,     slew, d0);
        patternQmixAt(N - f - 1, slew, d1);
        auto bl = [frac](float a, float b){ return a + frac*(b-a); };
        for (int i = 0; i < 16; ++i) {
            slewedQmix[i]=bl(d0.qmix[i],d1.qmix[i]);
            for (int v = 0; v < numPolyVoicesHint; v++) {
                slewedPolyQmix[v][i]=bl(d0.polyQmix[v][i],d1.polyQmix[v][i]);
            }
        }
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            qmixRandom[i]=slewedQmix[i];
            for (int v=0;v<numPolyVoicesHint;v++) polyRandom(v + 1, PL_QMIX)[i]=slewedPolyQmix[v][i];
        }
    }
    publishSlewedQmix();
    qmixMixApplied = qmixMixLatched; qmixSlewApplied = slew; qmixCtrApplied = N;
}

// ── Voice-scoped recompute: blend only the given voice's 16 steps from cached z0/z1 ──
// Called by refreshVisualCache for the DISPLAYED voice only (not all 15 poly voices).
// ~16x less work than the full recomputeEffective*. Cache hit required (populated at roll).
void PatternEngine::recomputeEffectiveRhythmForVoice(int voice) {
    const float s = rack::math::clamp(rhythmMixLatched, 0.f, 1.f) * 6.f;
    const int   f = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = rhythmDrawCtr;
    const float slew = rack::math::clamp(rhythmSlewLatched, -1.f, 1.f);
    const float r = slewKnobToR(slew);
    if (r == 0.f) return;
    if (!rhythmZCache_.hit(N, f, r)) { recomputeEffectiveRhythm(); return; }

    const int stride = 4 + 4 * numPolyVoicesHint;
    OUBridgeCoeffs ouBC; ouBC.compute(r, frac, bridgeRhythmPhilox, N - f - 1);
    auto bl = [&](int fi) -> float {
        return (float)redDot::copula::PhiFast(ouBC.blend(rhythmZCache_.z0[fi], rhythmZCache_.z1[fi]));
    };
    if (voice == 0) {
        for (int i = 0; i < 16; ++i) {
            int b = i * stride;
            slewedRhythm[i]    = bl(b); slewedVariation[i] = bl(b+1);
            slewedLegato[i]    = bl(b+2); slewedAccent[i]   = bl(b+3);
        }
        if (!sandsActive) for (int i = 0; i < 16; ++i) {
            rhythmRandom[i]=slewedRhythm[i]; variationRandom[i]=slewedVariation[i];
            legatoRandom[i]=slewedLegato[i]; accentRandom[i]=slewedAccent[i];
        }
    } else {
        int v = voice - 1; if (v >= numPolyVoicesHint) return;
        int off = 4 + v * 4;
        for (int i = 0; i < 16; ++i) {
            int b = i * stride + off;
            slewedPolyRhythm[v][i]    = bl(b); slewedPolyAccent[v][i]    = bl(b+1);
            slewedPolyVariation[v][i] = bl(b+2); slewedPolyLegato[v][i]    = bl(b+3);
        }
    }
    publishSlewedRhythm();
    rhythmMixApplied = rhythmMixLatched;
}
void PatternEngine::recomputeEffectiveMelodyForVoice(int voice) {
    const float s = rack::math::clamp(melodyMixLatched, 0.f, 1.f) * 6.f;
    const int   f = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = melodyDrawCtr;
    const float slew = rack::math::clamp(melodySlewLatched, -1.f, 1.f);
    const float r = slewKnobToR(slew);
    if (r == 0.f) return;
    if (!melodyZCache_.hit(N, f, r)) { recomputeEffectiveMelody(); return; }
    const int stride = 2 + 2 * numPolyVoicesHint;
    OUBridgeCoeffs ouBC; ouBC.compute(r, frac, bridgeMelodyPhilox, N - f - 1);
    auto bl = [&](int fi) -> float {
        return (float)redDot::copula::PhiFast(ouBC.blend(melodyZCache_.z0[fi], melodyZCache_.z1[fi]));
    };
    if (voice == 0) {
        for (int i = 0; i < 16; ++i) {
            int b = i * stride;
            slewedMelody[i] = bl(b); slewedOctave[i] = bl(b+1);
        }
        if (!sandsActive) for (int i = 0; i < 16; ++i) {
            melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
        }
    } else {
        int v = voice - 1; if (v >= numPolyVoicesHint) return;
        int off = 2 + v * 2;
        for (int i = 0; i < 16; ++i) {
            int b = i * stride + off;
            slewedPolyMelody[v][i] = bl(b); slewedPolyOctave[v][i] = bl(b+1);
        }
    }
    publishSlewedMelody();
    melodyMixApplied = melodyMixLatched;
}
void PatternEngine::recomputeEffectiveQmixForVoice(int voice) {
    const float s = rack::math::clamp(qmixMixLatched, 0.f, 1.f) * 6.f;
    const int   f = (int)s;
    const float frac = s - (float)f;
    const int64_t N  = qmixDrawCtr;
    const float slew = rack::math::clamp(qmixSlewLatched, -1.f, 1.f);
    const float r = slewKnobToR(slew);
    if (r == 0.f) return;
    if (!qmixZCache_.hit(N, f, r)) { recomputeEffectiveQmix(); return; }
    const int stride = 1 + numPolyVoicesHint;
    OUBridgeCoeffs ouBC; ouBC.compute(r, frac, bridgeQmixPhilox, N - f - 1);
    auto bl = [&](int fi) -> float {
        return (float)redDot::copula::PhiFast(ouBC.blend(qmixZCache_.z0[fi], qmixZCache_.z1[fi]));
    };
    if (voice == 0) {
        for (int i = 0; i < 16; ++i) slewedQmix[i] = bl(i * stride);
        if (!sandsActive) for (int i = 0; i < 16; ++i) qmixRandom[i]=slewedQmix[i];
    } else {
        int v = voice - 1; if (v >= numPolyVoicesHint) return;
        for (int i = 0; i < 16; ++i) slewedPolyQmix[v][i] = bl(i * stride + 1 + v);
    }
    publishSlewedQmix();
    qmixMixApplied = qmixMixLatched;
}

// Regenerate melody pattern (16 steps of semitone + pitch voltage)
void PatternEngine::redrawMelody(const PatternInput& in) {
    if (in.locked && !in.diceLiveM) return;   // LOCK_SCOPE_MENU: melody dice may draw under lock if opted live
    const bool first = melodyFirstDraw;
    melodyFirstDraw = false;

    // Philox addressable draw bookkeeping (mirror of redrawRhythm).
    if (!first) advanceMelodyDraw(melodyDrawDir());
    beginMelodyDraw();

    // Dice roll = advance the counter (done above); effective pattern re-derived by
    // recomputeEffectiveMelody (scrub/B2). No stored A/B walk. (Step 4a.)
    (void)first;
    melodySlewApplied = -1.f;
    recomputeEffectiveMelody();
    for (int i = 0; i < 16; ++i) {
        melodySource[i]=slewedMelody[i]; octaveSource[i]=slewedOctave[i];
        for (int v=0;v<numPolyVoicesHint;v++){ polyMelodySource[v][i]=slewedPolyMelody[v][i];
                                polyOctaveSource[v][i]=slewedPolyOctave[v][i]; }
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, slewedMelody[i], slewedOctave[i]);
        melodySemitone[i] = sem;
    }
}

// q-mix twin of redrawMelody — advance the q-mix draw, recompute effective, cache source.
void PatternEngine::redrawQmix(const PatternInput& in) {
    // NOTE: q-mix draws under its OWN dice-live gate (SB_DICE_Q), independent of melody. Under lock
    // it draws only if q-mix was opted live (frozen otherwise), mirroring redrawMelody's own guard.
    if (in.locked && !in.diceLiveQ) return;
    const bool first = qmixFirstDraw;
    qmixFirstDraw = false;

    // Philox addressable draw bookkeeping (mirror of redrawMelody).
    if (!first) advanceQmixDraw(qmixDrawDir());
    beginQmixDraw();

    (void)first;
    qmixSlewApplied = -1.f;
    recomputeEffectiveQmix();
    for (int i = 0; i < 16; ++i) {
        qmixSource[i]=slewedQmix[i];
        for (int v=0;v<numPolyVoicesHint;v++){ polyQmixSource[v][i]=slewedPolyQmix[v][i]; }
    }
}

void PatternEngine::latchMix(float rhythmMix, float melodyMix, float qmixMix,
                             float rhythmSlew, float melodySlew, float qmixSlew,
                             bool applyRhythm, bool applyMelody, bool applyQmix) {
    // Sample the live SCRUB inputs (control rate). Under the scrub model BOTH the scrub position
    // (MIX repurposed) AND slew (FIR smoothing width) feed the effective pattern, so latch both.
    // SLEW is consumed at the PHRASE BOUNDARY (the roll/redraw already calls recomputeEffective*
    // with the latched slew). Mid-phrase slew changes are LATCHED but do NOT trigger recompute —
    // the effect applies at the next roll (distributional correctness: slew must be constant across
    // a phrase). MIX/scrub changes DO trigger recompute (they're instantaneous position controls,
    // not distributional parameters).
    // LOCK_SCOPE_MENU: applyRhythm/applyMelody gate EACH stream independently. A frozen stream (apply
    // false) does NOT re-latch its mix/slew and does NOT recompute — its latched value + effective
    // pattern hold from before the lock. Default true/true = latch both (unlocked).
    // B3: mix is NOT consumed here — refreshVisualCache handles the for-voice recompute at
    // the display rate (~90Hz lightDivider). latchMix just latches; the mix change stays
    // pending (mixApplied unchanged) so refreshVisualCache can detect and consume it.
    if (applyRhythm) {
        rhythmMixLatched  = rhythmMix;
        rhythmSlewLatched = rhythmSlew;
    }
    if (applyMelody) {
        melodyMixLatched  = melodyMix;
        melodySlewLatched = melodySlew;
    }
    if (applyQmix) {
        qmixMixLatched  = qmixMix;
        qmixSlewLatched = qmixSlew;
    }
}

// Updates the rhythm/melody arrays used for UI and LEDs based on the 
// current knob positions and the *existing* random buffers.
void PatternEngine::refreshVisualCache(const PatternInput& in) {
    // No-redraw path (e.g. mix/scrub moved but no roll, or a stream that didn't redraw this cycle).
    // PART 3: mix changes use the FOR-VOICE recompute (displayed voice only, ~16x less
    // than all-voices). Counter change (roll/phrase boundary) uses the FULL recompute
    // (all voices — the audio needs them). Different scopes for the two consumers.
    bool rhythmChanged = false, melodyChanged = false;
    {
        if (rhythmDrawCtr != rhythmCtrApplied) {
            recomputeEffectiveRhythm();           // roll: full recompute (all voices)
            rhythmChanged = true;
        } else if (rhythmMixLatched != rhythmMixApplied) {
            recomputeEffectiveRhythmForVoice(displayedVoice_);  // mix: displayed voice only
            rhythmChanged = true;
        }
        if (melodyDrawCtr != melodyCtrApplied) {
            recomputeEffectiveMelody();
            melodyChanged = true;
        } else if (melodyMixLatched != melodyMixApplied) {
            recomputeEffectiveMelodyForVoice(displayedVoice_);
            melodyChanged = true;
        }
        if (qmixDrawCtr != qmixCtrApplied)
            recomputeEffectiveQmix();
        else if (qmixMixLatched != qmixMixApplied)
            recomputeEffectiveQmixForVoice(displayedVoice_);
    }
    // BUG FIX (rhythmmix_leaks_melody_bars): SPLIT the display derivation — rhythm-only
    // changes don't re-derive melodyPitchV (and vice versa). AND read the DISPLAYED VOICE's
    // array (poly for voice >= 1, mono for voice 0) — the old code always read the mono
    // arrays, which is wrong for a poly displayed voice and caused jumpy melody bars when
    // rhythm-mix forced the unconditional loop to re-run.
    if (rhythmChanged) {
        for (int i = 0; i < 16; ++i) {
            float rVal = (displayedVoice_ == 0) ? rhythmRandom[i] : polyRandom(displayedVoice_, PL_REST)[i];
            rhythmPattern[i] = (rVal >= in.restProb);
        }
    }
    if (melodyChanged) {
        for (int i = 0; i < 16; ++i) {
            float mVal = (displayedVoice_ == 0) ? melodyRandom[i] : polyRandom(displayedVoice_, PL_MELODY)[i];
            float oVal = (displayedVoice_ == 0) ? octaveRandom[i] : polyRandom(displayedVoice_, PL_OCTAVE)[i];
            int sem = 0;
            melodyPitchV[i]   = genPitchLive(sem, in, mVal, oVal);
            melodySemitone[i] = sem;
        }
    }
}

// Apply any pending seeds, then redraw both patterns.
// Called at phrase boundaries and on reset.
void PatternEngine::applyPendingSeedsAndRedraw(const PatternInput& in) {
    // Whole-module lock freezes everything (seeds, RNG, patterns) — UNLESS a dice stream is opted
    // live under lock (LOCK_SCOPE_MENU §6). Then that stream's ROLL / live-mode reroll may still draw,
    // while seeds/reseed-rolls stay frozen (those are Reseed-scoped). If locked and NEITHER dice
    // stream is live, nothing to do — early-out preserves the pre-menu behaviour exactly.
    if (in.locked && !in.diceLiveR && !in.diceLiveM && !in.diceLiveQ) return;
    // Per-stream "may this stream draw now?": unlocked, OR this stream's dice bit is opted live.
    const bool drawAllowedR = !in.locked || in.diceLiveR;
    const bool drawAllowedM = !in.locked || in.diceLiveM;
    const bool drawAllowedQ = !in.locked || in.diceLiveQ;   // q-mix own axis (SB_DICE_Q)

    // ── Dice-undo capture (item 4): a USER ROLL is exactly rhythmRollPending / melodyRollPending
    // at THIS commit. Realtime-mode auto-redraw sets neither (it uses rhythmMode==1); reset/reseed
    // use *SeedPending / *ReseedRollPending — all correctly EXCLUDED per the scope ruling. Capture
    // the roll intent NOW, before the pending flags are cleared below, and record before/after
    // (seedFloat, counter) around each moved stream's redraw. Published by Monsoon::onPhraseBoundary_.
    const bool undoR = rhythmRollPending;
    const bool undoM = melodyRollPending;
    const bool undoQ = qmixRollPending;
    if (undoR || undoM || undoQ) {
        diceUndoPending.valid  = true;
        diceUndoPending.movedR = undoR;
        diceUndoPending.movedM = undoM;
        diceUndoPending.movedQ = undoQ;
        diceUndoPending.rSeedBefore = rhythmSeedFloat; diceUndoPending.rCtrBefore = rhythmDrawCtr;
        diceUndoPending.mSeedBefore = melodySeedFloat; diceUndoPending.mCtrBefore = melodyDrawCtr;
        diceUndoPending.qSeedBefore = qmixSeedFloat;   diceUndoPending.qCtrBefore = qmixDrawCtr;
    }

    // Redraw if: a seed is pending (reproducible reseed, A=B), a ROLL is pending
    // (advance RNG, no reseed), a TRIAL is pending (audition, A anchored, never
    // reseeds), a RESEED-ROLL is pending (main roll + fresh entropy, keeps A/B
    // morph), or Realtime mode.
    // Under lock (dice-live), only the ROLL and the live-mode per-cycle reroll may draw; seeds and
    // reseed-rolls stay frozen (Reseed control, separately scoped — they won't even be armed under
    // lock, but guard for safety). Unlocked, all terms apply as before. drawAllowedR/M gates the whole
    // stream (false => this stream stays fully frozen, matching the old blanket early-return).
    bool shouldRedrawR = drawAllowedR && (((!in.locked) && (rhythmSeedPending || rhythmReseedRollPending))
                                          || rhythmRollPending || (rhythmMode == 1));
    bool shouldRedrawM = drawAllowedM && (((!in.locked) && (melodySeedPending || melodyReseedRollPending))
                                          || melodyRollPending || (melodyMode == 1));
    // q-mix draws under its OWN dice-live gate (SB_DICE_Q), independent of melody.
    bool shouldRedrawQ = drawAllowedQ && (((!in.locked) && (qmixSeedPending || qmixReseedRollPending))
                                          || qmixRollPending || (qmixMode == 1));

    if (!in.locked && rhythmSeedPending) {
        rhythmSeedFloat = rhythmSeedPendingFloat;
        seedRhythmPhilox(rhythmSeedFloat);     // mirror seed into Philox (counter→0)
        seedBridgeRhythmPhilox(rhythmSeedFloat);
        rhythmSeedPending = false;
        rhythmFirstDraw = true;   // new seed → A=B=draw, reproducible at any slew
    } else if (rhythmReseedRollPending) {
        // Reseed WITHOUT firstDraw — redrawRhythm(promote=true) commits B→A then
        // draws fresh B from the reseeded stream, so A≠B and slew survives.
        if (rhythmReseedRollFull) { seedRhythmPhiloxFull(); }
        else { rhythmSeedFloat = rhythmReseedRollFloat; seedRhythmPhilox(rhythmSeedFloat); }
    }
    // Only CONSUME (clear) the roll flags if this stream was allowed to draw. A frozen stream under a
    // mixed scope (e.g. rhythm live, melody frozen) must KEEP its pending flag so the queued roll fires
    // at unlock — clearing it unconditionally would silently DROP the roll (and blank its dice light).
    if (drawAllowedR) { rhythmRollPending = false; rhythmReseedRollPending = false; }
    if (shouldRedrawR) redrawRhythm(in);
    if (drawAllowedR) rhythmPendingLast = false;   // one-shot: consumed only if this stream drew (held otherwise)

    if (!in.locked && melodySeedPending) {   // seeds stay frozen under lock (Reseed-scoped)
        melodySeedFloat = melodySeedPendingFloat;
        seedMelodyPhilox(melodySeedFloat);     // mirror seed into Philox (counter→0)
        seedBridgeMelodyPhilox(melodySeedFloat);
        melodySeedPending = false;
        melodyFirstDraw = true;
    } else if (!in.locked && melodyReseedRollPending) {
        if (melodyReseedRollFull) { seedMelodyPhiloxFull(); }
        else { melodySeedFloat = melodyReseedRollFloat; seedMelodyPhilox(melodySeedFloat); }
    }
    // Only consume the melody roll flags if melody was allowed to draw (see rhythm note above).
    if (drawAllowedM) { melodyRollPending = false; melodyReseedRollPending = false; }
    if (shouldRedrawM) redrawMelody(in);
    if (drawAllowedM) melodyPendingLast = false;   // one-shot: consumed only if this stream drew (held otherwise)

    // q-mix twin of the melody block — its OWN independent stream (STREAM_SOURCE_SELECT).
    if (!in.locked && qmixSeedPending) {   // seeds stay frozen under lock (Reseed-scoped)
        qmixSeedFloat = qmixSeedPendingFloat;
        seedQmixPhilox(qmixSeedFloat);     // mirror seed into Philox (counter→0)
        seedBridgeQmixPhilox(qmixSeedFloat);
        qmixSeedPending = false;
        qmixFirstDraw = true;
    } else if (!in.locked && qmixReseedRollPending) {
        if (qmixReseedRollFull) { seedQmixPhiloxFull(); }
        else { qmixSeedFloat = qmixReseedRollFloat; seedQmixPhilox(qmixSeedFloat); }
    }
    if (drawAllowedQ) { qmixRollPending = false; qmixReseedRollPending = false; }
    if (shouldRedrawQ) redrawQmix(in);
    if (drawAllowedQ) qmixPendingLast = false;   // one-shot: consumed only if this stream drew (held otherwise)

    // ── Dice-undo capture (item 4): record the AFTER (seedFloat, counter) now that the roll's
    // redraw has advanced the counter. Only the moved streams matter; the other's before==after.
    if (diceUndoPending.valid) {
        diceUndoPending.rSeedAfter = rhythmSeedFloat; diceUndoPending.rCtrAfter = rhythmDrawCtr;
        diceUndoPending.mSeedAfter = melodySeedFloat; diceUndoPending.mCtrAfter = melodyDrawCtr;
        diceUndoPending.qSeedAfter = qmixSeedFloat;   diceUndoPending.qCtrAfter = qmixDrawCtr;
        // diceUndoPending stays valid=true until the owner (Monsoon::onPhraseBoundary_) drains it.
    }

    // Always refresh the cache so the LEDs react to live knob changes in Dice mode
    if (!shouldRedrawR || !shouldRedrawM || !shouldRedrawQ) refreshVisualCache(in);
}

// ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
// stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

void PatternEngine::switchMelodyMode(int& stepIndex, int& lastStepIndex) {
    int prev = melodyMode;
    int next = 1 - prev;
    if (prev == 0 && next == 1) {
        // Entering realtime: cache current state (seed + output + A/B buffers)
        cachedMelodySeedFloat = melodySeedPending
            ? melodySeedPendingFloat : melodySeedFloat;
        melodySeedCached = true;
        for (int i = 0; i < 16; ++i) cachedMelodyPitchV[i] = melodyPitchV[i];
        cachedMelodyStepIndex     = stepIndex;
        cachedMelodyLastStepIndex = lastStepIndex;
        // Scrub model: no A/B snapshot -- (counter, scrub, slew) is the morph position,
        // unchanged by a mode switch; returning to dice recomputes from the counter. (Step 4b.)
    } else if (prev == 1 && next == 0 && melodySeedCached) {
        // Returning to dice: restore the exact A/B buffers (lossless — preserves
        // the slew morph position), NOT a reseed-to-A=B approximation.
        for (int i = 0; i < 16; ++i) melodyPitchV[i] = cachedMelodyPitchV[i];
        stepIndex     = cachedMelodyStepIndex;
        lastStepIndex = cachedMelodyLastStepIndex;
        // Scrub model: recompute effective from the (unchanged) counter -- exact pre-switch pattern.
        recomputeEffectiveMelody();
    }
    melodyMode = next;
}

void PatternEngine::switchRhythmMode(int& stepIndex, int& lastStepIndex) {
    int prev = rhythmMode;
    int next = 1 - prev;
    if (prev == 0 && next == 1) {
        cachedRhythmSeedFloat = rhythmSeedPending
            ? rhythmSeedPendingFloat : rhythmSeedFloat;
        rhythmSeedCached = true;
        for (int i = 0; i < 16; ++i) cachedRhythmPattern[i] = rhythmPattern[i];
        cachedRhythmStepIndex     = stepIndex;
        cachedRhythmLastStepIndex = lastStepIndex;
        // Scrub model: no A/B snapshot -- (counter, scrub, slew) is the morph position,
        // unchanged by a mode switch; returning to dice recomputes from the counter. (Step 4b.)
    } else if (prev == 1 && next == 0 && rhythmSeedCached) {
        for (int i = 0; i < 16; ++i) rhythmPattern[i] = cachedRhythmPattern[i];
        stepIndex     = cachedRhythmStepIndex;
        lastStepIndex = cachedRhythmLastStepIndex;
        // Scrub model: recompute effective from the (unchanged) counter -- exact pre-switch pattern.
        recomputeEffectiveRhythm();
    }
    rhythmMode = next;
}

// ──── Composite Operations ──────────────────────────────────────────────
