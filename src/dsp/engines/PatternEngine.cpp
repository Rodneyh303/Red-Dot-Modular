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

void PatternEngine::reset() {
    rhythmSeedPending = melodySeedPending = false;
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
        rhythmLockedA[i]=rhythmCandB[i]=rhythmRandom[i];
        variationLockedA[i]=variationCandB[i]=variationRandom[i];
        legatoLockedA[i]=legatoCandB[i]=legatoRandom[i];
        accentLockedA[i]=accentCandB[i]=accentRandom[i];
        melodyLockedA[i]=melodyCandB[i]=melodyRandom[i];
        octaveLockedA[i]=octaveCandB[i]=octaveRandom[i];
        for (int v=0;v<15;v++){
            polyRhythmLockedA[v][i]=polyRhythmCandB[v][i]=polyRhythmRandom[v][i];
            polyMelodyLockedA[v][i]=polyMelodyCandB[v][i]=polyMelodyRandom[v][i];
            polyOctaveLockedA[v][i]=polyOctaveCandB[v][i]=polyOctaveRandom[v][i];
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
    // variationAmount=0.5 → zero variation (all weight on baseIdx, no spread).
    // variationAmount → 0 → bias toward longer notes (lower index).
    // variationAmount → 1 → bias toward shorter notes (higher index).
    // spread = 0 at centre, 1 at extremes — controls how much adjacent
    // indices are weighted relative to baseIdx.
    auto allowed = [&](int idx) -> bool {
        if (idx < 0 || idx >= 8) return false;
        if (idx == 4) return (in.noteVariationMask & 0b001) != 0;
        if (idx == 6) return (in.noteVariationMask & 0b010) != 0;
        if (idx == 7) return (in.noteVariationMask & 0b100) != 0;
        return true;
    };

    float var    = in.variationAmount;
    float spread = 2.f * std::fabs(var - 0.5f);  // 0 at 50%, 1 at extremes
    if (spread < 1e-4f) return baseIdx;           // exactly 50%: no variation

    float weights[8] = {};
    float total = 0.f;

    for (int i = 0; i < 8; ++i) {
        if (!allowed(i)) continue;

        if (i == baseIdx) {
            // The original note's weight drops as variation increases.
            // At 100% (spread = 1.0), the base note weight is 0.
            weights[i] = 1.0f - spread;
        } else {
            bool isShorter = (i > baseIdx);
            bool isLonger  = (i < baseIdx);

            // Strict Directional Filtering:
            // If variation is > 50%, we exclude longer notes entirely.
            // If variation is < 50%, we exclude shorter notes entirely.
            if (var > 0.5f && isLonger) weights[i] = 0.f;
            else if (var < 0.5f && isShorter) weights[i] = 0.f;
            else {
                // Weight varies by distance, but is enabled by 'spread'
                float dist = (float)std::abs(i - baseIdx);
                weights[i] = spread / dist;
            }
        }
        total += weights[i];
    }

    if (total <= 0.f) return baseIdx;
    
    float acc = 0.f;
    r *= total;
    for (int i = 0; i < 8; ++i) {
        if (weights[i] > 0.f) {
            acc += weights[i];
            if (r <= acc) return i;
        }
    }
    return baseIdx;
}

// Regenerate rhythm pattern (16 steps of bool: true=active, false=rest)
void PatternEngine::redrawRhythm(const PatternInput& in) {
    if (in.locked) return;

    // Promote previous candidate (B) to locked (A), then draw a fresh B.
    // First draw (or post-seed): A := B := draw, so effective == draw at any slew.
    const bool first = rhythmFirstDraw;
    rhythmFirstDraw = false;

    for (int i = 0; i < 16; ++i) {
        // promote B -> A
        rhythmLockedA[i]    = rhythmCandB[i];
        variationLockedA[i] = variationCandB[i];
        legatoLockedA[i]    = legatoCandB[i];
        accentLockedA[i]    = accentCandB[i];
        for (int v = 0; v < 15; v++) polyRhythmLockedA[v][i] = polyRhythmCandB[v][i];

        // fresh B
        rhythmCandB[i]    = unitRhythm();
        variationCandB[i] = unitRhythm();
        legatoCandB[i]    = unitRhythm();
        accentCandB[i]    = unitRhythm();
        for (int v = 0; v < 15; v++) polyRhythmCandB[v][i] = unitRhythm();

        if (first) {  // lock A to the same draw so effective == draw
            rhythmLockedA[i]    = rhythmCandB[i];
            variationLockedA[i] = variationCandB[i];
            legatoLockedA[i]    = legatoCandB[i];
            accentLockedA[i]    = accentCandB[i];
            for (int v = 0; v < 15; v++) polyRhythmLockedA[v][i] = polyRhythmCandB[v][i];
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
    const float s = rack::math::clamp(rhythmSlewLatched, 0.f, 1.f);
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
    rhythmSlewApplied = s;
}

void PatternEngine::recomputeEffectiveMelody() {
    const float s = rack::math::clamp(melodySlewLatched, 0.f, 1.f);
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
    melodySlewApplied = s;
}

// Regenerate melody pattern (16 steps of semitone + pitch voltage)
void PatternEngine::redrawMelody(const PatternInput& in) {
    if (in.locked) return;
    const bool first = melodyFirstDraw;
    melodyFirstDraw = false;

    for (int i = 0; i < 16; ++i) {
        melodyLockedA[i] = melodyCandB[i];
        octaveLockedA[i] = octaveCandB[i];
        for (int v=0;v<15;v++){ polyMelodyLockedA[v][i]=polyMelodyCandB[v][i];
                                polyOctaveLockedA[v][i]=polyOctaveCandB[v][i]; }

        melodyCandB[i] = unitMelody();
        octaveCandB[i] = unitMelody();
        for (int v=0;v<15;v++){ polyMelodyCandB[v][i]=unitMelody();
                                polyOctaveCandB[v][i]=unitMelody(); }

        if (first) {
            melodyLockedA[i]=melodyCandB[i]; octaveLockedA[i]=octaveCandB[i];
            for (int v=0;v<15;v++){ polyMelodyLockedA[v][i]=polyMelodyCandB[v][i];
                                    polyOctaveLockedA[v][i]=polyOctaveCandB[v][i]; }
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

void PatternEngine::latchSlew(float rhythmSlew, float melodySlew) {
    // Sample the live slew (called at step-0 wrap). Recompute effective arrays
    // only when the latched value actually changes.
    rhythmSlewLatched = rhythmSlew;
    melodySlewLatched = melodySlew;
    if (rhythmSlewLatched != rhythmSlewApplied) recomputeEffectiveRhythm();
    if (melodySlewLatched != melodySlewApplied) recomputeEffectiveMelody();
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

    // Only advance the RNG if a new dice roll is pending or we are in Realtime mode.
    bool shouldRedrawR = rhythmSeedPending || (rhythmMode == 1);
    bool shouldRedrawM = melodySeedPending || (melodyMode == 1);

    if (rhythmSeedPending) {
        rhythmSeedFloat = rhythmSeedPendingFloat;
        seedRngFromFloat(rhythmRng, rhythmSeedFloat);
        rhythmSeedPending = false;
        rhythmFirstDraw = true;   // new seed → A=B=draw, reproducible at any slew
    }
    if (shouldRedrawR) redrawRhythm(in);

    if (melodySeedPending) {
        melodySeedFloat = melodySeedPendingFloat;
        seedRngFromFloat(melodyRng, melodySeedFloat);
        melodySeedPending = false;
        melodyFirstDraw = true;
    }
    if (shouldRedrawM) redrawMelody(in);

    // Always refresh the cache so the LEDs react to live knob changes in Dice mode
    if (!shouldRedrawR || !shouldRedrawM) refreshVisualCache(in);
}

// ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
// stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

void PatternEngine::switchMelodyMode(int& stepIndex, int& lastStepIndex) {
    int prev = melodyMode;
    int next = 1 - prev;
    if (prev == 0 && next == 1) {
        // Entering realtime: cache current state
        cachedMelodySeedFloat = melodySeedPending
            ? melodySeedPendingFloat : melodySeedFloat;
        melodySeedCached = true;
        for (int i = 0; i < 16; ++i) cachedMelodyPitchV[i] = melodyPitchV[i];
        cachedMelodyStepIndex     = stepIndex;
        cachedMelodyLastStepIndex = lastStepIndex;
    } else if (prev == 1 && next == 0 && melodySeedCached) {
        // Returning to dice: restore cached state
        melodySeedFloat = cachedMelodySeedFloat;
        melodySeedPendingFloat = cachedMelodySeedFloat;
        melodySeedPending = true;
        for (int i = 0; i < 16; ++i) melodyPitchV[i] = cachedMelodyPitchV[i];
        stepIndex     = cachedMelodyStepIndex;
        lastStepIndex = cachedMelodyLastStepIndex;
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
    } else if (prev == 1 && next == 0 && rhythmSeedCached) {
        // Returning to dice: restore cached state
        rhythmSeedFloat = cachedRhythmSeedFloat;
        rhythmSeedPendingFloat = cachedRhythmSeedFloat;
        rhythmSeedPending = true;
        for (int i = 0; i < 16; ++i) rhythmPattern[i] = cachedRhythmPattern[i];
        stepIndex     = cachedRhythmStepIndex;
        lastStepIndex = cachedRhythmLastStepIndex;
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
