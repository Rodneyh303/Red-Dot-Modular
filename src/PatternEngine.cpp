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
    rhythmSeedFloat = melodySeedFloat = stochasticSeedFloat = 0.f;
    rhythmSeedPending = melodySeedPending = false;
    rhythmMode = melodyMode = 0;
    seedRngFromFloat(rhythmRng, 0.f);
    seedRngFromFloat(melodyRng, 0.f);
    seedRngFromFloat(stochasticRng, 0.f);
}

// ── Core generation ───────────────────────────────────────────────────────

// Pick a semitone (0..11) weighted by fader values using a provided random value.
int PatternEngine::pickSemitone(const float weights[12], float r_val) {
    float sum = 0.f;
    for (int i = 0; i < 12; ++i) sum += weights[i];
    if (sum <= 0.f) return -1;

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

    int oL = (int)std::floor(in.octaveLo);
    float lo = in.octaveLo, hi = in.octaveHi;
    if (hi < lo) std::swap(lo, hi);
    int oH = (int)std::floor(hi);
    if (oH < oL) oH = oL;
    int oct = oL + (int)std::floor(r_oct * float(oH - oL + 1));

    float v = float(oct) + (sem + in.transpose) / 12.f;
    return pe_clamp(v, 0.f, 5.f);
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
    for (int i = 0; i < 16; ++i) {
        rhythmRandom[i]    = unitRhythm();
        variationRandom[i] = unitRhythm();
        legatoRandom[i]    = unitRhythm();
        // Update cache for UI
        rhythmPattern[i] = (rhythmRandom[i] >= in.restProb);
    }
}

// Regenerate melody pattern (16 steps of semitone + pitch voltage)
void PatternEngine::redrawMelody(const PatternInput& in) {
    if (in.locked) return;
    for (int i = 0; i < 16; ++i) {
        melodyRandom[i] = unitMelody();
        octaveRandom[i] = unitMelody();
        // Update cache for UI
        int sem = 0;
        melodyPitchV[i]   = genPitchLive(sem, in, melodyRandom[i], octaveRandom[i]);
        melodySemitone[i] = sem;
    }
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
    }
    if (shouldRedrawR) redrawRhythm(in);

    if (melodySeedPending) {
        melodySeedFloat = melodySeedPendingFloat;
        seedRngFromFloat(melodyRng, melodySeedFloat);
        melodySeedPending = false;
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