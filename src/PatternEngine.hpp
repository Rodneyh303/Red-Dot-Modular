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
    int   dnaLength        = 16;
    int   dnaOffset        = 0;
    bool  locked           = false;
};

// ── PatternEngine ─────────────────────────────────────────────────────────────
struct PatternEngine {

    // ── Output arrays (read by MeloDicer, never written externally) ───────────
    float rhythmRandom[16]    = {};
    float variationRandom[16] = {};
    float legatoRandom[16]    = {};
    float melodyRandom[16]    = {};
    float octaveRandom[16]    = {};
    
    // Poly strands: 7 voices, each with Rhythm, Melody, and Octave draws
    float polyRhythmRandom[7][16] = {};
    float polyMelodyRandom[7][16] = {};
    float polyOctaveRandom[7][16] = {};

    // ── Source DNA Cache (Original draws before rotation/scramble) ───────────
    float rhythmSource[16]    = {};
    float variationSource[16] = {};
    float legatoSource[16]    = {};
    float melodySource[16]    = {};
    float octaveSource[16]    = {};
    float polyRhythmSource[7][16] = {};
    float polyMelodySource[7][16] = {};
    float polyOctaveSource[7][16] = {};

    // Caches for UI/Lights to reflect the current state
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
    //inline float unitStochastic() { return rngToFloat(stochasticRng); }
    inline float unitRhythm() { return rngToFloat(rhythmRng); }
    inline float unitMelody() { return rngToFloat(melodyRng); }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

    void seedRngFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat);
    void reset();

    // ── Core generation ───────────────────────────────────────────────────────

    // Pick a semitone (0..11) weighted by fader values using a provided random value.
    int pickSemitone(const float weights[12], float r_val);

    // Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
    float genPitch(int& outSemitone, const PatternInput& in);

    // Generate a pitch voltage using provided random floats for semitone and octave selection.
    float genPitchLive(int& outSemitone, const PatternInput& in, float r_semi, float r_oct);

    // Apply variation bias to a note length index.
    int varyNoteIndex(int baseIdx, const PatternInput& in, float r);

    // Regenerate rhythm pattern (16 steps of bool: true=active, false=rest)
    void redrawRhythm(const PatternInput& in);

    // Regenerate melody pattern (16 steps of semitone + pitch voltage)
    void redrawMelody(const PatternInput& in);

    // Updates the rhythm/melody arrays used for UI and LEDs based on the 
    // current knob positions and the *existing* random buffers.
    void refreshVisualCache(const PatternInput& in);

    // Apply any pending seeds, then redraw both patterns.
    // Called at phrase boundaries and on reset.
    void applyPendingSeedsAndRedraw(const PatternInput& in);

    // ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
    // stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

    void switchMelodyMode(int& stepIndex, int& lastStepIndex);
    void switchRhythmMode(int& stepIndex, int& lastStepIndex);

    // Circularly shifts the internal random buffers
    void rotateRhythm(int steps);     // Gates and Rests
    void rotateVariation(int steps);  // Note length biasing
    void rotateLegato(int steps);     // Legato and Tie probability
    void rotateMelody(int steps);     // Pitch selection
    void rotateOctave(int steps);     // Octave selection

    void resetDnaRotation();          // Restore from Source Cache
};
