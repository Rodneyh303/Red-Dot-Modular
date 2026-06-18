#include "PatternEngine.hpp"

// ── RNG helpers ───────────────────────────────────────────────────────────
void PatternEngine::seedRngFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat) {
    float s  = pe_clamp(seedFloat, 0.f, 10.f);
    uint64_t s1 = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
    uint64_t s2 = s1 + 0x9e3779b97f4a7c15ULL;
    s2 = (s2 ^ (s2 >> 30)) * 0xbf58476d1ce4e5b9ULL;
    s2 = (s2 ^ (s2 >> 27)) * 0x94d049bb133111ebULL;
    s2 ^= (s2 >> 31);
    rng.seed(s1, s2);
}

void PatternEngine::seedRngFull(rack::random::Xoroshiro128Plus& rng) {
    // Full state-space seed from two independent 64-bit entropy words — used for
    // internal (no-CV) reseeds so they are not bottlenecked through the 0..10
    // float that exists only for CV compatibility.
    uint64_t s1 = rack::random::u64();
    uint64_t s2 = rack::random::u64();
    if (s1 == 0 && s2 == 0) s1 = 0x9e3779b97f4a7c15ULL;  // avoid all-zero state
    rng.seed(s1, s2);
}

void PatternEngine::reset() {
    rhythmSeedPending = melodySeedPending = false;
    rhythmRollPending = melodyRollPending = false;
    rhythmTrialPending = melodyTrialPending = false;
    rhythmReseedRollPending = melodyReseedRollPending = false;
    rhythmReseedRollFull = melodyReseedRollFull = false;
    rhythmABCached = melodyABCached = false;
    rhythmMode = melodyMode = 0;
    rhythmSeedCached = melodySeedCached = false;

    // strands must not be all-zero or module is silent until dice/phrase
    for (int i = 0; i < 16; ++i) {
        rhythmRandom[i] = 1.0f;     // Mono triggers by default
        variationRandom[i] = 0.5f;  // No variation bias
        legatoRandom[i] = 0.0f;     // No legato/ties
        accentRandom[i] = 1.0f;     // No accents by default (roll < prob)
        melodyRandom[i] = 0.5f;     // Default to middle of weighted sum
        octaveRandom[i] = 0.5f;     // Default to middle of octave range
        
        for (int v = 0; v < 15; v++) {
            polyRhythmRandom[v][i] = 1.0f; // Poly voices trigger by default
            polyMelodyRandom[v][i] = 0.5f;
            polyOctaveRandom[v][i] = 0.5f;
            
            polyRhythmSource[v][i] = 1.0f;
            polyMelodySource[v][i] = polyMelodyRandom[v][i];
            polyOctaveSource[v][i] = polyOctaveRandom[v][i];
        }
        
        rhythmSource[i] = rhythmRandom[i];
        variationSource[i] = variationRandom[i];
        legatoSource[i] = legatoRandom[i];
        accentSource[i] = accentRandom[i];
        melodySource[i] = melodyRandom[i];
        octaveSource[i] = octaveRandom[i];

        rhythmPattern[i] = true;
    }

    // Playable slew: mirror defaults into A and B so the morph is a no-op until
    // the first dice, and arm the first-draw guards.
    for (int i = 0; i < 16; ++i) {
        rhythmLockedA[i] = variationLockedA[i] = legatoLockedA[i] = 0.f;
        accentLockedA[i] = melodyLockedA[i] = octaveLockedA[i] = 0.f;

        rhythmCandB[i] = rhythmRandom[i];
        variationCandB[i] = variationRandom[i];
        legatoCandB[i] = legatoRandom[i];
        accentCandB[i] = accentRandom[i];
        melodyCandB[i] = melodyRandom[i];
        octaveCandB[i] = octaveRandom[i];

        for (int v=0;v<15;v++){
            polyRhythmLockedA[v][i] = polyMelodyLockedA[v][i] = polyOctaveLockedA[v][i] = 0.f;
            polyRhythmCandB[v][i] = polyRhythmRandom[v][i];
            polyMelodyCandB[v][i] = polyMelodyRandom[v][i];
            polyOctaveCandB[v][i] = polyOctaveRandom[v][i];
        }
    }

    rhythmSlewLatched=melodySlewLatched=1.f;
    rhythmSlewApplied=melodySlewApplied=1.f;
    rhythmFirstDraw=melodyFirstDraw=true;
    sandsActive=false;
    // Mirror defaults into slewedDraw too (final == slewed at reset)
    for (int i=0;i<16;i++){
        slewedRhythm[i]=rhythmRandom[i]; slewedVariation[i]=variationRandom[i];
        slewedLegato[i]=legatoRandom[i]; slewedAccent[i]=accentRandom[i];
        slewedMelody[i]=melodyRandom[i]; slewedOctave[i]=octaveRandom[i];
        for (int v=0;v<15;v++){
            slewedPolyRhythm[v][i]=polyRhythmRandom[v][i];
            slewedPolyMelody[v][i]=polyMelodyRandom[v][i];
            slewedPolyOctave[v][i]=polyOctaveRandom[v][i];
        }
    }
}

// ── Core generation ───────────────────────────────────────────────────────

// Pick a semitone (0..11) weighted by fader values using a provided random value.
int PatternEngine::pickSemitone(const float weights[12], float r_val) {
    float sum = 0.f;
    for (int i = 0; i < 12; ++i) sum += weights[i];
    if (sum <= 0.0001f) return -1;

    float r = pe_clamp(r_val, 0.f, 1.f - 1e-7f) * sum;
    float acc = 0.f;
    for (int i = 0; i < 12; ++i) {
        if (weights[i] <= 0.f) continue;
        acc += weights[i];
        if (r < acc) return i;
    }
    return 11;  // float safety
}

// Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
float PatternEngine::genPitch(int& outSemitone, const PatternInput& in) {
    return genPitchLive(outSemitone, in, unitMelody(), unitMelody());
}

// Generate a pitch voltage using provided random floats for semitone and octave selection.
float PatternEngine::genPitchLive(int& outSemitone, const PatternInput& in, float r_semi, float r_oct) {
    int sem = pickSemitone(in.semiWeights, r_semi);
    outSemitone = (sem < 0) ? 0 : sem;
    if (sem < 0) return 0.f;

    float lo = in.octaveLo, hi = in.octaveHi;
    if (hi < lo) std::swap(lo, hi);
    int oL = (int)std::floor(lo);
    int oH = (int)std::floor(hi);
    // Ensure r_oct roll isn't exactly 1.0 to prevent oOB indexing
    float roll = pe_clamp(r_oct, 0.f, 0.9999f);
    int oct = oL + (int)std::floor(roll * float(oH - oL + 1));

    float v = float(oct) - 4.f + (sem + in.transpose) / 12.f;
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
void PatternEngine::redrawRhythm(const PatternInput& in, bool promoteToA) {
    if (in.locked) return;

    // Two dice modes (see header). MAIN (promoteToA=true): the current candidate
    // B is committed into A FIRST, then a fresh B is drawn, and slew blends A↔B
    // at the roll. A walks forward each roll → groove mutates; low slew = tight
    // variations near the evolving A, slew=1 = full replace (MeloDicer mode).
    // TRIAL/AUDITION (promoteToA=false): A is left fixed; only a fresh B is drawn.
    // Slew still blends at the roll, so the user auditions candidates against the
    // same anchor A (raise slew to move toward B, lower to fall back to A).
    // First draw (or post-seed): A := B := draw, so effective == draw at any slew.
    const bool first = rhythmFirstDraw;
    rhythmFirstDraw = false;

    for (int i = 0; i < 16; ++i) {
        // MAIN mode: promote current B -> A (commit the last candidate) before
        // drawing the new one. TRIAL mode skips this so A stays anchored.
        if (promoteToA) {
            rhythmLockedA[i]    = rhythmCandB[i];
            variationLockedA[i] = variationCandB[i];
            legatoLockedA[i]    = legatoCandB[i];
            accentLockedA[i]    = accentCandB[i];
            for (int v = 0; v < 15; v++) polyRhythmLockedA[v][i] = polyRhythmCandB[v][i];
        }

        // fresh candidate B. MODEL 2: SLEW is consumed here — instead of storing
        // a full independent draw, store B = A + slew*(T-A), so slew LIMITS how
        // far this roll can move B from the current A (low slew = small step). On
        // MAIN dice A was just promoted to the previous B, so repeated low-slew
        // rolls give a bounded random walk. On TRIAL dice A is the fixed anchor,
        // so slew controls audition boldness. The live A<->B morph is MIX, not
        // slew (see recomputeEffectiveRhythm).
        const float sl = rack::math::clamp(in.rhythmSlew, 0.f, 1.f);
        auto step = [&](float a){ return a + sl * (unitRhythm() - a); };
        if (first) {
            // No prior A to step from: draw a full independent pattern.
            rhythmCandB[i]    = unitRhythm();
            variationCandB[i] = unitRhythm();
            legatoCandB[i]    = unitRhythm();
            accentCandB[i]    = unitRhythm();
            for (int v = 0; v < 15; v++) polyRhythmCandB[v][i] = unitRhythm();
        } else {
            rhythmCandB[i]    = step(rhythmLockedA[i]);
            variationCandB[i] = step(variationLockedA[i]);
            legatoCandB[i]    = step(legatoLockedA[i]);
            accentCandB[i]    = step(accentLockedA[i]);
            for (int v = 0; v < 15; v++) polyRhythmCandB[v][i] = step(polyRhythmLockedA[v][i]);
        }
    }
    rhythmSlewApplied = -1.f;       // force recompute of slewedDraw
    recomputeEffectiveRhythm();
    // Cache source from the SLEWED draw (canonical draw; final may be rewritten
    // by Sands later this cycle). UI pattern previews the no-Sands final.
    for (int i = 0; i < 16; ++i) {
        rhythmSource[i]=slewedRhythm[i]; variationSource[i]=slewedVariation[i];
        legatoSource[i]=slewedLegato[i]; accentSource[i]=slewedAccent[i];
        for (int v=0;v<15;v++) polyRhythmSource[v][i]=slewedPolyRhythm[v][i];
        rhythmPattern[i] = (slewedRhythm[i] >= in.restProb);
    }
}

// slew: slewedDraw[] = A + slew*(B-A). When no Sands owns the spread stage,
// copy slewedDraw → final (the public arrays the sequencer reads).
void PatternEngine::recomputeEffectiveRhythm() {
    // Live A<->B morph: effective = A + mix*(B-A). MIX is the continuous, playable
    // blend (like spread). SLEW is NOT used here — it was already consumed at roll
    // time to shape B (see redrawRhythm). mix=0 -> committed A; mix=1 -> candidate B.
    const float s = rack::math::clamp(rhythmMixLatched, 0.f, 1.f);
    auto bl = [s](float a, float b){ return a + s*(b-a); };
    for (int i = 0; i < 16; ++i) {
        slewedRhythm[i]    = bl(rhythmLockedA[i],    rhythmCandB[i]);
        slewedVariation[i] = bl(variationLockedA[i], variationCandB[i]);
        slewedLegato[i]    = bl(legatoLockedA[i],    legatoCandB[i]);
        slewedAccent[i]    = bl(accentLockedA[i],    accentCandB[i]);
        for (int v = 0; v < 15; v++)
            slewedPolyRhythm[v][i] = bl(polyRhythmLockedA[v][i], polyRhythmCandB[v][i]);
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            rhythmRandom[i]=slewedRhythm[i]; variationRandom[i]=slewedVariation[i];
            legatoRandom[i]=slewedLegato[i]; accentRandom[i]=slewedAccent[i];
            for (int v=0;v<15;v++) polyRhythmRandom[v][i]=slewedPolyRhythm[v][i];
        }
    }
    rhythmMixApplied = s;
}

void PatternEngine::recomputeEffectiveMelody() {
    const float s = rack::math::clamp(melodyMixLatched, 0.f, 1.f);
    auto bl = [s](float a, float b){ return a + s*(b-a); };
    for (int i = 0; i < 16; ++i) {
        slewedMelody[i] = bl(melodyLockedA[i], melodyCandB[i]);
        slewedOctave[i] = bl(octaveLockedA[i], octaveCandB[i]);
        for (int v = 0; v < 15; v++) {
            slewedPolyMelody[v][i] = bl(polyMelodyLockedA[v][i], polyMelodyCandB[v][i]);
            slewedPolyOctave[v][i] = bl(polyOctaveLockedA[v][i], polyOctaveCandB[v][i]);
        }
    }
    if (!sandsActive) {
        for (int i = 0; i < 16; ++i) {
            melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
            for (int v=0;v<15;v++){ polyMelodyRandom[v][i]=slewedPolyMelody[v][i];
                                    polyOctaveRandom[v][i]=slewedPolyOctave[v][i]; }
        }
    }
    melodyMixApplied = s;
}

// Regenerate melody pattern (16 steps of semitone + pitch voltage)
void PatternEngine::redrawMelody(const PatternInput& in, bool promoteToA) {
    if (in.locked) return;
    const bool first = melodyFirstDraw;
    melodyFirstDraw = false;

    for (int i = 0; i < 16; ++i) {
        // MAIN: commit current B → A before drawing fresh B. TRIAL: A anchored.
        if (promoteToA) {
            melodyLockedA[i] = melodyCandB[i];
            octaveLockedA[i] = octaveCandB[i];
            for (int v=0;v<15;v++){ polyMelodyLockedA[v][i]=polyMelodyCandB[v][i];
                                    polyOctaveLockedA[v][i]=polyOctaveCandB[v][i]; }
        }

        // MODEL 2: SLEW consumed here — B = A + slew*(T-A). See redrawRhythm.
        const float sl = rack::math::clamp(in.melodySlew, 0.f, 1.f);
        auto step = [&](float a){ return a + sl * (unitMelody() - a); };
        if (first) {
            melodyCandB[i] = unitMelody();
            octaveCandB[i] = unitMelody();
            for (int v=0;v<15;v++){ polyMelodyCandB[v][i]=unitMelody();
                                    polyOctaveCandB[v][i]=unitMelody(); }
        } else {
            melodyCandB[i] = step(melodyLockedA[i]);
            octaveCandB[i] = step(octaveLockedA[i]);
            for (int v=0;v<15;v++){ polyMelodyCandB[v][i]=step(polyMelodyLockedA[v][i]);
                                    polyOctaveCandB[v][i]=step(polyOctaveLockedA[v][i]); }
        }
    }
    melodySlewApplied = -1.f;
    recomputeEffectiveMelody();
    for (int i = 0; i < 16; ++i) {
        melodySource[i]=slewedMelody[i]; octaveSource[i]=slewedOctave[i];
        for (int v=0;v<15;v++){ polyMelodySource[v][i]=slewedPolyMelody[v][i];
                                polyOctaveSource[v][i]=slewedPolyOctave[v][i]; }
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, slewedMelody[i], slewedOctave[i]);
        melodySemitone[i] = sem;
    }
}

void PatternEngine::latchMix(float rhythmMix, float melodyMix) {
    // Sample the live MIX (called at control rate). Recompute the effective A<->B
    // morph only when it actually changes. This is the playable, continuous blend
    // (the role spread has); SLEW is separate and consumed at roll time.
    rhythmMixLatched = rhythmMix;
    melodyMixLatched = melodyMix;
    if (rhythmMixLatched != rhythmMixApplied) recomputeEffectiveRhythm();
    if (melodyMixLatched != melodyMixApplied) recomputeEffectiveMelody();
}

// Updates the rhythm/melody arrays used for UI and LEDs based on the 
// current knob positions and the *existing* random buffers.
void PatternEngine::refreshVisualCache(const PatternInput& in) {
    for (int i = 0; i < 16; ++i) {
        rhythmPattern[i] = (rhythmRandom[i] >= in.restProb);
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, melodyRandom[i], octaveRandom[i]);
        melodySemitone[i] = sem;
    }
}

// Apply any pending seeds, then redraw both patterns.
// Called at phrase boundaries and on reset.
void PatternEngine::applyPendingSeedsAndRedraw(const PatternInput& in) {
    if (in.locked) return;  // freeze everything: seeds, RNG, patterns

    // Redraw if: a seed is pending (reproducible reseed, A=B), a ROLL is pending
    // (advance RNG, no reseed), a TRIAL is pending (audition, A anchored, never
    // reseeds), a RESEED-ROLL is pending (main roll + fresh entropy, keeps A/B
    // morph), or Realtime mode.
    bool shouldRedrawR = rhythmSeedPending || rhythmRollPending || rhythmTrialPending || rhythmReseedRollPending || (rhythmMode == 1);
    bool shouldRedrawM = melodySeedPending || melodyRollPending || melodyTrialPending || melodyReseedRollPending || (melodyMode == 1);

    if (rhythmSeedPending) {
        rhythmSeedFloat = rhythmSeedPendingFloat;
        seedRngFromFloat(rhythmRng, rhythmSeedFloat);
        rhythmSeedPending = false;
        rhythmFirstDraw = true;   // new seed → A=B=draw, reproducible at any slew
    } else if (rhythmReseedRollPending) {
        // Reseed WITHOUT firstDraw — redrawRhythm(promote=true) commits B→A then
        // draws fresh B from the reseeded stream, so A≠B and slew survives.
        if (rhythmReseedRollFull) seedRngFull(rhythmRng);
        else { rhythmSeedFloat = rhythmReseedRollFloat; seedRngFromFloat(rhythmRng, rhythmSeedFloat); }
    } else if (in.reseedOnRoll && rhythmMode == 1 && !in.rhythmLiveTrial) {
        // Realtime MAIN + reseed-on-roll: reseed each redraw. (Live TRIAL never
        // reseeds — it auditions against a fixed A.) CV if present, else full
        // 64-bit internal entropy.
        if (in.seedConnected) { rhythmSeedFloat = in.seedSampleValue; seedRngFromFloat(rhythmRng, rhythmSeedFloat); }
        else                  seedRngFull(rhythmRng);
    }
    // Promote (main, A walks) unless this is a momentary TRIAL roll OR live mode
    // is sourced from the TRIAL dice (anchored A → variations on a theme).
    const bool rLiveTrial = (rhythmMode == 1 && in.rhythmLiveTrial);
    const bool rPromote = !rhythmTrialPending && !rLiveTrial;
    rhythmRollPending = false;
    rhythmTrialPending = false;
    rhythmReseedRollPending = false;
    if (shouldRedrawR) redrawRhythm(in, rPromote);

    if (melodySeedPending) {
        melodySeedFloat = melodySeedPendingFloat;
        seedRngFromFloat(melodyRng, melodySeedFloat);
        melodySeedPending = false;
        melodyFirstDraw = true;
    } else if (melodyReseedRollPending) {
        if (melodyReseedRollFull) seedRngFull(melodyRng);
        else { melodySeedFloat = melodyReseedRollFloat; seedRngFromFloat(melodyRng, melodySeedFloat); }
    } else if (in.reseedOnRoll && melodyMode == 1 && !in.melodyLiveTrial) {
        // Realtime MAIN + reseed-on-roll only (live TRIAL never reseeds).
        if (in.seedConnected) { melodySeedFloat = in.seedSampleValue; seedRngFromFloat(melodyRng, melodySeedFloat); }
        else                  seedRngFull(melodyRng);
    }
    const bool mLiveTrial = (melodyMode == 1 && in.melodyLiveTrial);
    const bool mPromote = !melodyTrialPending && !mLiveTrial;
    melodyRollPending = false;
    melodyTrialPending = false;
    melodyReseedRollPending = false;
    if (shouldRedrawM) redrawMelody(in, mPromote);

    // Always refresh the cache so the LEDs react to live knob changes in Dice mode
    if (!shouldRedrawR || !shouldRedrawM) refreshVisualCache(in);
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
        // Lossless A/B snapshot (melody owns melody + octave, mono + poly).
        for (int i = 0; i < 16; ++i) {
            cMelodyA[i] = melodyLockedA[i]; cMelodyB[i] = melodyCandB[i];
            cOctaveA[i] = octaveLockedA[i]; cOctaveB[i] = octaveCandB[i];
            for (int v = 0; v < 15; ++v) {
                cPolyMelodyA[v][i] = polyMelodyLockedA[v][i]; cPolyMelodyB[v][i] = polyMelodyCandB[v][i];
                cPolyOctaveA[v][i] = polyOctaveLockedA[v][i]; cPolyOctaveB[v][i] = polyOctaveCandB[v][i];
            }
        }
        melodyABCached = true;
    } else if (prev == 1 && next == 0 && melodySeedCached) {
        // Returning to dice: restore the exact A/B buffers (lossless — preserves
        // the slew morph position), NOT a reseed-to-A=B approximation.
        for (int i = 0; i < 16; ++i) melodyPitchV[i] = cachedMelodyPitchV[i];
        stepIndex     = cachedMelodyStepIndex;
        lastStepIndex = cachedMelodyLastStepIndex;
        if (melodyABCached) {
            for (int i = 0; i < 16; ++i) {
                melodyLockedA[i] = cMelodyA[i]; melodyCandB[i] = cMelodyB[i];
                octaveLockedA[i] = cOctaveA[i]; octaveCandB[i] = cOctaveB[i];
                for (int v = 0; v < 15; ++v) {
                    polyMelodyLockedA[v][i] = cPolyMelodyA[v][i]; polyMelodyCandB[v][i] = cPolyMelodyB[v][i];
                    polyOctaveLockedA[v][i] = cPolyOctaveA[v][i]; polyOctaveCandB[v][i] = cPolyOctaveB[v][i];
                }
            }
            // Recompute effective from the restored A/B at the current slew;
            // do NOT set a pending seed (that would collapse to A=B).
            recomputeEffectiveMelody();
        } else {
            // Fallback (no A/B snapshot): reseed to reproduce the pattern.
            melodySeedFloat = cachedMelodySeedFloat;
            melodySeedPendingFloat = cachedMelodySeedFloat;
            melodySeedPending = true;
        }
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
        // Lossless A/B snapshot (rhythm owns rhythm + variation + legato +
        // accent, mono; plus poly rhythm).
        for (int i = 0; i < 16; ++i) {
            cRhythmA[i]    = rhythmLockedA[i];    cRhythmB[i]    = rhythmCandB[i];
            cVariationA[i] = variationLockedA[i]; cVariationB[i] = variationCandB[i];
            cLegatoA[i]    = legatoLockedA[i];    cLegatoB[i]    = legatoCandB[i];
            cAccentA[i]    = accentLockedA[i];    cAccentB[i]    = accentCandB[i];
            for (int v = 0; v < 15; ++v) { cPolyRhythmA[v][i] = polyRhythmLockedA[v][i]; cPolyRhythmB[v][i] = polyRhythmCandB[v][i]; }
        }
        rhythmABCached = true;
    } else if (prev == 1 && next == 0 && rhythmSeedCached) {
        for (int i = 0; i < 16; ++i) rhythmPattern[i] = cachedRhythmPattern[i];
        stepIndex     = cachedRhythmStepIndex;
        lastStepIndex = cachedRhythmLastStepIndex;
        if (rhythmABCached) {
            // Lossless A/B restore — preserves the slew morph position.
            for (int i = 0; i < 16; ++i) {
                rhythmLockedA[i]    = cRhythmA[i];    rhythmCandB[i]    = cRhythmB[i];
                variationLockedA[i] = cVariationA[i]; variationCandB[i] = cVariationB[i];
                legatoLockedA[i]    = cLegatoA[i];    legatoCandB[i]    = cLegatoB[i];
                accentLockedA[i]    = cAccentA[i];    accentCandB[i]    = cAccentB[i];
                for (int v = 0; v < 15; ++v) { polyRhythmLockedA[v][i] = cPolyRhythmA[v][i]; polyRhythmCandB[v][i] = cPolyRhythmB[v][i]; }
            }
            recomputeEffectiveRhythm();
        } else {
            rhythmSeedFloat = cachedRhythmSeedFloat;
            rhythmSeedPendingFloat = cachedRhythmSeedFloat;
            rhythmSeedPending = true;
        }
    }
    rhythmMode = next;
}

void PatternEngine::rotateRhythm(int steps) {
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(rhythmRandom), std::begin(rhythmRandom) + (16 - steps), std::end(rhythmRandom));
}

void PatternEngine::rotateVariation(int steps) {
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(variationRandom), std::begin(variationRandom) + (16 - steps), std::end(variationRandom));
}

void PatternEngine::rotateLegato(int steps) {
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(legatoRandom), std::begin(legatoRandom) + (16 - steps), std::end(legatoRandom));
}

void PatternEngine::rotateAccent(int steps) {  // New: rotate accent strand
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(accentRandom), std::begin(accentRandom) + (16 - steps), std::end(accentRandom));
}

void PatternEngine::rotateMelody(int steps) {
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(melodyRandom), std::begin(melodyRandom) + (16 - steps), std::end(melodyRandom));
}

void PatternEngine::rotateOctave(int steps) {
    steps = ((steps % 16) + 16) % 16;
    if (steps == 0) return;
    std::rotate(std::begin(octaveRandom), std::begin(octaveRandom) + (16 - steps), std::end(octaveRandom));
}

void PatternEngine::resetDnaRotation() {
    for (int i = 0; i < 16; ++i) {
        rhythmRandom[i]    = rhythmSource[i];
        variationRandom[i] = variationSource[i];
        legatoRandom[i]    = legatoSource[i];
        accentRandom[i]    = accentSource[i];
        melodyRandom[i]    = melodySource[i];
        octaveRandom[i]    = octaveSource[i];
    }
    for (int v = 0; v < 15; v++) {
        for (int i = 0; i < 16; i++) {
            polyRhythmRandom[v][i] = polyRhythmSource[v][i];
            polyMelodyRandom[v][i] = polyMelodySource[v][i];
            polyOctaveRandom[v][i] = polyOctaveSource[v][i];
        }
    }
}

// ──── Composite Operations ──────────────────────────────────────────────

void PatternEngine::rotateRhythmPattern(int steps) {
    rotateRhythm(steps);
    rotateVariation(steps);
    rotateLegato(steps);
}

void PatternEngine::rotateMelodyPattern(int steps) {
    rotateMelody(steps);
    rotateOctave(steps);
}

void PatternEngine::refreshPatternCache(const PatternInput& in) {
    refreshVisualCache(in);
}
