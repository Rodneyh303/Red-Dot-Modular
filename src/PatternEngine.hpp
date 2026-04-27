#pragma once
/**
 * PatternEngine.hpp
 * Stochastic pattern generation for MeloDicer.
 *
 * Owns:
 *   - Both RNG states (rhythm + melody)
 *   - Generated pattern arrays (rhythmPattern, melodySemitone, melodyPitchV)
 *   - Seed management (float seeds, pending seeds, mode cache)
 *
 * Does NOT touch:
 *   - Rack ports/params (receives pre-read values via Input struct)
 *   - Gate/playback state
 *   - Step position
 *   - UI / lights
 *
 * Interface contract:
 *   Caller reads knobs/CVs once per block and populates a PatternInput struct,
 *   then calls generate() at phrase boundaries.
 */

#include <rack.hpp>
#include <cmath>
#include <cstdint>
#include <algorithm>


template<typename T>
static inline T pe_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

// ── Input snapshot — filled by MeloDicer::process() each block ────────────────
// All knob/CV values are pre-read so PatternEngine is Rack-port-free.
struct PatternInput {
    float semiWeights[12]  = {};  // semitone fader weights 0..1 (with CV applied)
    float restProb         = 0.1f;
    float variationAmount  = 0.5f;
    float octaveLo         = 2.f;
    float octaveHi         = 5.f;
    float transpose        = 0.f;
    int   noteVariationMask= 0b111;
    bool  locked           = false;
};

// ── PatternEngine ─────────────────────────────────────────────────────────────
struct PatternEngine {

    // ── Output arrays (read by MeloDicer, never written externally) ───────────
    bool  rhythmPattern[16]   = {};
    int   melodySemitone[16]  = {};
    float melodyPitchV[16]    = {};

    // ── RNG state ─────────────────────────────────────────────────────────────
    rack::random::Xoroshiro128Plus rhythmRng, melodyRng;

    // ── Seed management ───────────────────────────────────────────────────────
    float rhythmSeedFloat  = 0.f;
    float melodySeedFloat  = 0.f;
    bool  rhythmSeedPending = false;
    bool  melodySeedPending = false;
    float rhythmSeedPendingFloat = 0.f;
    float melodySeedPendingFloat = 0.f;
    int   rhythmMode = 0;  // 0=dice, 1=realtime
    int   melodyMode = 0;

    // ── Mode switch cache ─────────────────────────────────────────────────────
    float cachedMelodySeedFloat  = 0.f;
    float cachedRhythmSeedFloat  = 0.f;
    bool  melodySeedCached       = false;
    bool  rhythmSeedCached       = false;
    float cachedMelodyPitchV[16] = {};
    bool  cachedRhythmPattern[16]= {};
    int   cachedMelodyStepIndex  = -1;
    int   cachedMelodyLastStepIndex = -1;
    int   cachedRhythmStepIndex  = -1;
    int   cachedRhythmLastStepIndex = -1;

    // ── RNG helpers ───────────────────────────────────────────────────────────
    // Convert Xoroshiro128Plus output to float [0,1)
    // Works with both the real Rack RNG and our test stub.
    static inline float rngToFloat(rack::random::Xoroshiro128Plus& rng) {
        return (rng() >> 11) * (1.f / float(1ull << 53));
    }
    inline float unitRhythm() { return rngToFloat(rhythmRng); }
    inline float unitMelody() { return rngToFloat(melodyRng); }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

    void seedRngFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat) {
        float s  = pe_clamp(seedFloat, 0.f, 10.f);
        uint64_t s1 = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
        uint64_t s2 = s1 + 0x9e3779b97f4a7c15ULL;
        s2 = (s2 ^ (s2 >> 30)) * 0xbf58476d1ce4e5b9ULL;
        s2 = (s2 ^ (s2 >> 27)) * 0x94d049bb133111ebULL;
        s2 ^= (s2 >> 31);
        rng.seed(s1, s2);
    }

    void reset() {
        rhythmSeedFloat = melodySeedFloat = 0.f;
        rhythmSeedPending = melodySeedPending = false;
        rhythmMode = melodyMode = 0;
        seedRngFromFloat(rhythmRng, 0.f);
        seedRngFromFloat(melodyRng, 0.f);
    }

    // ── Core generation ───────────────────────────────────────────────────────

    // Pick a semitone (0..11) weighted by fader values. Returns -1 if all zero.
    int pickSemitone(const float weights[12]) {
        float sum = 0.f;
        for (int i = 0; i < 12; ++i) sum += weights[i];
        if (sum <= 0.f) return -1;

        float r = pe_clamp(unitMelody(), 0.f, 1.f - 1e-7f) * sum;
        float acc = 0.f;
        for (int i = 0; i < 12; ++i) {
            if (weights[i] <= 0.f) continue;
            acc += weights[i];
            if (r < acc) return i;
        }
        return 11;  // float safety
    }

    // Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
    float genPitch(int& outSemitone, const PatternInput& in) {
        int sem = pickSemitone(in.semiWeights);
        outSemitone = (sem < 0) ? 0 : sem;
        if (sem < 0) return 0.f;

        float lo = in.octaveLo, hi = in.octaveHi;
        if (hi < lo) std::swap(lo, hi);
        int oL = (int)std::floor(lo);
        int oH = (int)std::floor(hi);
        if (oH < oL) oH = oL;
        int oct = oL + (int)std::floor(unitMelody() * float(oH - oL + 1));

        float v = float(oct) + (sem + in.transpose) / 12.f;
        return pe_clamp(v, 0.f, 5.f);
    }

    // Apply variation bias to a note length index.
    int varyNoteIndex(int baseIdx, const PatternInput& in) {
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

        int lo = std::max(0, baseIdx - 2);
        int hi = std::min(7, baseIdx + 2);
        float total = 0.f;
        float weights[8] = {};
        for (int i = lo; i <= hi; ++i) if (allowed(i)) {
            float dist = float(std::abs(i - baseIdx));
            if (dist == 0.f) {
                weights[i] = 1.f;  // base index always has weight 1
            } else {
                // Adjacent positions get weight proportional to spread
                // Directional bias: shorter (i>base) when var>0.5, longer when var<0.5
                float w = spread / (1.f + dist);
                if (i > baseIdx && var > 0.5f) w *= 1.f + spread;   // boost shorter
                if (i < baseIdx && var < 0.5f) w *= 1.f + spread;   // boost longer
                weights[i] = w;
            }
            total += weights[i];
        }
        if (total <= 0.f) return baseIdx;
        float r = unitRhythm() * total, acc = 0.f;
        for (int i = lo; i <= hi; ++i) if (weights[i] > 0.f) {
            acc += weights[i];
            if (r <= acc) return i;
        }
        return baseIdx;
    }

    // Regenerate rhythm pattern (16 steps of bool: true=active, false=rest)
    void redrawRhythm(const PatternInput& in) {
        if (in.locked) return;
        for (int i = 0; i < 16; ++i)
            rhythmPattern[i] = (unitRhythm() >= in.restProb);
    }

    // Regenerate melody pattern (16 steps of semitone + pitch voltage)
    void redrawMelody(const PatternInput& in) {
        if (in.locked) return;
        for (int i = 0; i < 16; ++i) {
            int sem = 0;
            melodyPitchV[i]   = genPitch(sem, in);
            melodySemitone[i] = sem;
        }
    }

    // Apply any pending seeds, then redraw both patterns.
    // Called at phrase boundaries and on reset.
    //
    // When locked: seeds stay pending, RNG state is NOT advanced, patterns
    // are NOT redrawn. The pending seed fires on the next phrase boundary
    // after unlock — you can pre-arm a new pattern while the current plays.
    void applyPendingSeedsAndRedraw(const PatternInput& in) {
        if (in.locked) return;  // freeze everything: seeds, RNG, patterns

        if (rhythmSeedPending) {
            rhythmSeedFloat = rhythmSeedPendingFloat;
            seedRngFromFloat(rhythmRng, rhythmSeedFloat);
            rhythmSeedPending = false;
        }
        if (melodySeedPending) {
            melodySeedFloat = melodySeedPendingFloat;
            seedRngFromFloat(melodyRng, melodySeedFloat);
            melodySeedPending = false;
        }
        redrawRhythm(in);
        redrawMelody(in);
    }

    // ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
    // stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

    void switchMelodyMode(int& stepIndex, int& lastStepIndex) {
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

    void switchRhythmMode(int& stepIndex, int& lastStepIndex) {
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
            rhythmSeedFloat = cachedRhythmSeedFloat;
            rhythmSeedPendingFloat = cachedRhythmSeedFloat;
            rhythmSeedPending = true;
            for (int i = 0; i < 16; ++i) rhythmPattern[i] = cachedRhythmPattern[i];
            stepIndex     = cachedRhythmStepIndex;
            lastStepIndex = cachedRhythmLastStepIndex;
        }
        rhythmMode = next;
    }
};
