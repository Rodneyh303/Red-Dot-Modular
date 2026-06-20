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
#include <vector>
#include <memory>
#include "../PhiloxRng.hpp"


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
    // Playable dice slew (0..1) per group. Latched at step 0; morphs the
    // effective pattern between the locked (A) and candidate (B) draws.
    float rhythmSlew       = 1.f;
    float melodySlew       = 1.f;
    // Live A<->B blend (MIX). Separate from slew: slew is consumed at roll
    // (shapes B); mix is the live, continuous A<->B morph used for output.
    float rhythmMix        = 0.f;
    float melodyMix        = 0.f;
    // Reseed policy passed through from the module (context-menu option). When
    // set, continuous Realtime-mode redraws also reseed each cycle from fresh
    // entropy (or the SEED CV if seedConnected), so realtime stays genuinely
    // random rather than walking one deterministic stream.
    bool  reseedOnRoll     = false;
    // Which dice the LIVE mode (rhythmMode/melodyMode==1) drives:
    // false = MAIN (promote, A walks); true = TRIAL (anchored A, variations on a
    // theme; never reseeds). Resolves the "two live modes" conflict — live is one
    // switch, the source is a separate switch, so only one dice is ever live.
    bool  rhythmLiveTrial  = false;
    bool  melodyLiveTrial  = false;
    bool  seedConnected    = false;
    float seedSampleValue  = 0.f;   // current SEED CV (0..10) when seedConnected
};

// ── PatternEngine ─────────────────────────────────────────────────────────────
struct PatternEngine {

    // ── Output arrays (read by MeloDicer, never written externally) ───────────
    float rhythmRandom[16]    = {};
    float variationRandom[16] = {};
    float legatoRandom[16]    = {};
    float accentRandom[16]    = {};  // New: accent strand probabilities
    float melodyRandom[16]    = {};
    float octaveRandom[16]    = {};
    
    // Poly strands: 15 voices, each with Rhythm, Melody, and Octave draws
    float polyRhythmRandom[15][16] = {};
    float polyMelodyRandom[15][16] = {};
    float polyOctaveRandom[15][16] = {};

    // ── Playable slew: locked (A) + candidate (B) endpoints ───────────────────
    // The public arrays above are the EFFECTIVE output = A + slew*(B-A).
    // Reroll promotes B→A and draws a fresh B; the slew knob (latched at step 0)
    // morphs between the two committed grooves live. SequencerEngine reads the
    // public arrays unchanged.
    // Rhythm group: rhythm / variation / legato / accent (+ poly rhythm)
    float rhythmLockedA[16]={},    rhythmCandB[16]={};
    float variationLockedA[16]={}, variationCandB[16]={};
    float legatoLockedA[16]={},    legatoCandB[16]={};
    float accentLockedA[16]={},    accentCandB[16]={};
    float polyRhythmLockedA[15][16]={}, polyRhythmCandB[15][16]={};
    // Melody group: melody / octave (+ poly melody / poly octave)
    float melodyLockedA[16]={},    melodyCandB[16]={};
    float octaveLockedA[16]={},    octaveCandB[16]={};
    float polyMelodyLockedA[15][16]={}, polyMelodyCandB[15][16]={};
    float polyOctaveLockedA[15][16]={}, polyOctaveCandB[15][16]={};
    // Latched slew (sampled at step 0), and the last value we recomputed at.
    float rhythmSlewLatched = 1.f, melodySlewLatched = 1.f;
    float rhythmSlewApplied =-1.f, melodySlewApplied =-1.f;  // force first recompute
    // Live MIX (A<->B blend) latched at control rate; the effective arrays are
    // recomputed when it changes. This is what drives the continuous morph.
    float rhythmMixLatched = 0.f, melodyMixLatched = 0.f;
    float rhythmMixApplied =-1.f, melodyMixApplied =-1.f;

    // ── Slew output buffers (Option W) ────────────────────────────────────────
    // slew writes the A/B blend here (step-0 latched). The PUBLIC arrays above
    // (rhythmRandom[] etc.) are the FINAL vectors the sequencer reads:
    //   no Sands  → final = copy of slewedDraw (done when slew re-latches).
    //   Sands     → Sands reads slewedDraw, applies spread at control rate, and
    //               writes the result into the public/final arrays itself.
    float slewedRhythm[16]={}, slewedVariation[16]={}, slewedLegato[16]={}, slewedAccent[16]={};
    float slewedMelody[16]={}, slewedOctave[16]={};
    float slewedPolyRhythm[15][16]={}, slewedPolyMelody[15][16]={}, slewedPolyOctave[15][16]={};
    // Set true when any Sands visual expander owns the spread→final stage this
    // cycle. When false, slew copies slewedDraw → final.
    bool  sandsActive = false;
    // Active poly voice count mirrored from SequencerEngine (for Sands display
    // ensemble sizing — the audio path uses SequencerEngine::numPolyVoices).
    int   numPolyVoicesHint = 0;

    // ── Source DNA Cache (Original draws before rotation/scramble) ───────────
    float rhythmSource[16]    = {};
    float variationSource[16] = {};
    float legatoSource[16]    = {};
    float accentSource[16]    = {};  // New: cache for accent before scramble
    float melodySource[16]    = {};
    float octaveSource[16]    = {};
    float polyRhythmSource[15][16] = {};
    float polyMelodySource[15][16] = {};
    float polyOctaveSource[15][16] = {};

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
    // Pending ROLL (dice press) — advance the RNG and redraw WITHOUT reseeding.
    // Distinct from a seed-pending, which reseeds for reproducibility. A dice
    // press should walk the RNG forward (A/B morph), not reset to a fixed seed
    // on every press. The TRIAL variants roll with A anchored (promoteToA=false)
    // so the user auditions candidates against a fixed A; the regular roll
    // promotes B→A (main mode), so A walks forward.
    bool  rhythmRollPending = false;
    bool  melodyRollPending = false;
    bool  rhythmTrialPending = false;
    bool  melodyTrialPending = false;
    // Pending RESEED-ROLL — like a (main) roll but ALSO reseeds the RNG from a
    // fresh value, while keeping the A/B morph: promote B→A, reseed, draw fresh
    // B, no firstDraw. Used by the "Reseed on roll" option. Trial rolls never
    // use this — auditioning stays in a controlled space (no entropy injection).
    bool  rhythmReseedRollPending = false;
    bool  melodyReseedRollPending = false;
    float rhythmReseedRollFloat = 0.f;
    float melodyReseedRollFloat = 0.f;
    // When true, the reseed-roll uses FULL 64-bit internal entropy (the float is
    // ignored). When false, it reseeds from the (lower-precision) CV-derived
    // float. CV seeds are intentionally low-precision (0..10V → seed); internal
    // reseeds get the full state space.
    bool  rhythmReseedRollFull = false;
    bool  melodyReseedRollFull = false;
    int   rhythmMode = 0;  // 0=dice, 1=realtime
    int   melodyMode = 0;

    // First reroll after construction / new seed draws A=B (full strength,
    // preserves seed determinism); slew morph applies on subsequent rerolls.
    bool  rhythmFirstDraw = true;
    bool  melodyFirstDraw = true;

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
    // Full A/B buffer snapshot for a LOSSLESS realtime round-trip: entering
    // realtime caches A and B, returning restores them exactly (preserving the
    // slew morph position), rather than reseeding to an A=B approximation.
    bool  rhythmABCached = false, melodyABCached = false;
    float cRhythmA[16]={}, cRhythmB[16]={}, cVariationA[16]={}, cVariationB[16]={};
    float cLegatoA[16]={}, cLegatoB[16]={}, cAccentA[16]={}, cAccentB[16]={};
    float cPolyRhythmA[15][16]={}, cPolyRhythmB[15][16]={};
    float cMelodyA[16]={}, cMelodyB[16]={}, cOctaveA[16]={}, cOctaveB[16]={};
    float cPolyMelodyA[15][16]={}, cPolyMelodyB[15][16]={};
    float cPolyOctaveA[15][16]={}, cPolyOctaveB[15][16]={};

    // ── RNG helpers ───────────────────────────────────────────────────────────
    // Convert Xoroshiro128Plus output to float [0,1)
    // Works with both the real Rack RNG and our test stub.
    static inline float rngToFloat(rack::random::Xoroshiro128Plus& rng) {
        return (rng() >> 11) * (1.f / float(1ull << 53));
    }

    // ── Counter-addressable Philox draw path (Mode E reverse/jump foundation) ──
    // Each stream (rhythm, melody) has a Philox4x32 engine keyed by the stream seed,
    // a DRAW-COUNTER (the phrase index — up on forward redraw, down on reverse), a
    // fixed CHUNK of stream positions per draw, and an intra-draw CURSOR that the
    // redraw resets and advances per unit() call. So draw N is the addressable block
    // [N*CHUNK, N*CHUNK+CHUNK) — pure fn of (counter, key) ⇒ reproducible forward AND
    // backward without stored history. 32-bit Philox: a 24-bit-mantissa float, exactly
    // float precision, which is all the probability lanes need.
    //
    // CHUNK must exceed the max unit() calls per redraw of a stream. Worst case:
    //   rhythm  16 * (rhythm+variation+legato+accent=4 mono + 15 poly) = 304
    //   melody  16 * (melody+octave=2 mono + 15*2 poly)               = 512
    // 1024 leaves generous headroom and is a clean power of two.
    static constexpr uint64_t DRAW_CHUNK = 1024;
    bool      usePhiloxDraws = true;          // false → legacy Xoroshiro (A/B compare)
    redDot::PhiloxRng rhythmPhilox, melodyPhilox;
    int64_t   rhythmDrawCtr = 0, melodyDrawCtr = 0;   // signed: can go negative on reverse
    uint64_t  rhythmCursor  = 0, melodyCursor  = 0;   // intra-draw position, reset per redraw

    // ── Reverse "tape" model (Mode E phase reverse) ──
    // Forward play writes a per-roll audition-count tape; reverse is a READ-ONLY walk
    // back through it (no auditioning in reverse). The committed draw index after roll
    // R = sum over rolls 0..R of (auditionCount[r] + 1), so the tape lets us
    // reconstruct the draw-counter going backward. A forward roll while behind the
    // head TRUNCATES the tape there (tape-machine record-over-rewind). The tape is a
    // PREALLOCATED power-of-two RING (sliding window): allocated once at construction,
    // never in process(); oldest entries silently overwritten past capacity. All ops
    // are O(1) index arithmetic — no audio-thread allocation.
    enum class AuditionPolicy : uint8_t { ForwardOnly, None };
    AuditionPolicy auditionPolicy = AuditionPolicy::ForwardOnly;
    bool reverseActive = false;                       // set from phase direction each block

    static constexpr uint64_t TAPE_CAP  = (1u << 20); // 1,048,576 rolls/stream (2MB each)
    static constexpr uint64_t TAPE_MASK = TAPE_CAP - 1;
    static constexpr int      MAX_AUDITIONS_PER_ROLL = 65535;  // uint16_t ceiling; saturate
    std::unique_ptr<uint16_t[]> rhythmTape, melodyTape;        // [TAPE_CAP], alloc in ctor
    // head = number of committed rolls written; cur = current roll position; floor =
    // oldest roll still in the ring (head-CAP). Reverse clamps at max(0,floor).
    uint64_t rhythmHead = 0, rhythmCur = 0;
    uint64_t melodyHead = 0, melodyCur = 0;
    int   rhythmPendingAuditions = 0, melodyPendingAuditions = 0;

    void allocTape() {
        if (!rhythmTape) rhythmTape.reset(new uint16_t[TAPE_CAP]());
        if (!melodyTape) melodyTape.reset(new uint16_t[TAPE_CAP]());
    }
    void setReverseActive(bool rev) { reverseActive = rev; }
    void setAuditionPolicy(AuditionPolicy p) { auditionPolicy = p; }
    bool auditionsAllowed() const {
        return !reverseActive && auditionPolicy == AuditionPolicy::ForwardOnly;
    }
    // None mode: auditions disabled, A never promoted (stays initial zero), no tape —
    // pure increment/decrement of the draw-counter. The simplest fully-reversible model;
    // the only floor is the counter reaching 0 (the initial draw). Mix/slew untouched.
    bool noneMode() const { return auditionPolicy == AuditionPolicy::None; }

    // Reset the intra-draw cursor at the start of a redraw (called by redrawRhythm/
    // redrawMelody before any unit() calls so the draw maps to its chunk base).
    inline void beginRhythmDraw() { rhythmCursor = 0; }
    inline void beginMelodyDraw() { melodyCursor = 0; }
    // Step the draw-counter (dir>0 forward, dir<0 reverse). Forward-only for now;
    // the reverse/cross-boundary branch will drive dir<0.
    inline void advanceRhythmDraw(int dir) { rhythmDrawCtr += (dir < 0 ? -1 : +1); }
    inline void advanceMelodyDraw(int dir) { melodyDrawCtr += (dir < 0 ? -1 : +1); }

    // ── Tape: forward roll commit vs reverse roll, per stream (ring buffer) ──
    // A committed forward roll consumes (pendingAuditions + 1) draw chunks. Records the
    // count in the ring; truncates if committing while BEHIND the head (fork). Sliding
    // window: writing past TAPE_CAP overwrites the oldest slot via the mask.
    void commitRhythmForwardRoll() {
        if (rhythmCur < rhythmHead) rhythmHead = rhythmCur;          // behind head → fork
        rhythmTape[rhythmHead & TAPE_MASK] = (uint16_t)rhythmPendingAuditions;
        rhythmPendingAuditions = 0;
        rhythmHead++; rhythmCur = rhythmHead;
    }
    void commitMelodyForwardRoll() {
        if (melodyCur < melodyHead) melodyHead = melodyCur;
        melodyTape[melodyHead & TAPE_MASK] = (uint16_t)melodyPendingAuditions;
        melodyPendingAuditions = 0;
        melodyHead++; melodyCur = melodyHead;
    }
    // Oldest roll still reversible (sliding window floor).
    uint64_t rhythmFloor() const { return rhythmHead > TAPE_CAP ? rhythmHead - TAPE_CAP : 0; }
    uint64_t melodyFloor() const { return melodyHead > TAPE_CAP ? melodyHead - TAPE_CAP : 0; }
    // Reverse roll: step back one committed roll, undoing its (auditions+1) chunks.
    // Returns false at the floor (no more reversible history — caller restores initial).
    bool reverseRhythmRoll() {
        if (rhythmCur <= rhythmFloor()) return false;
        rhythmCur--;
        uint16_t n = rhythmTape[rhythmCur & TAPE_MASK];
        rhythmDrawCtr -= (int64_t)n + 1;
        return true;
    }
    bool reverseMelodyRoll() {
        if (melodyCur <= melodyFloor()) return false;
        melodyCur--;
        uint16_t n = melodyTape[melodyCur & TAPE_MASK];
        melodyDrawCtr -= (int64_t)n + 1;
        return true;
    }
    void resetRhythmTape() {
        rhythmHead = rhythmCur = 0; rhythmPendingAuditions = 0; rhythmDrawCtr = 0;
    }
    void resetMelodyTape() {
        melodyHead = melodyCur = 0; melodyPendingAuditions = 0; melodyDrawCtr = 0;
    }
    void resetTape() { resetRhythmTape(); resetMelodyTape(); }
    // Terminal reverse state: A = all zeros, B = the nonzero *Random defaults — the
    // exact pre-first-roll state (shared with reset()). Restored when reverse hits the
    // floor so bottoming-out lands on the deterministic initial pattern.
    void restoreInitialAB();

    // Seed a stream's Philox from the same 0..10 float (reseed → new key, counter
    // reset to 0 = sequence restarts) or from full entropy. Mirrors the Xoroshiro
    // seed sites so seed/reseed events affect both engines identically.
    inline void seedRhythmPhilox(float seedFloat) {
        float s = pe_clamp(seedFloat, 0.f, 10.f);
        uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
        rhythmPhilox.seed64(sd); rhythmDrawCtr = 0;
    }
    inline void seedMelodyPhilox(float seedFloat) {
        float s = pe_clamp(seedFloat, 0.f, 10.f);
        uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
        melodyPhilox.seed64(sd); melodyDrawCtr = 0;
    }
    inline void seedRhythmPhiloxFull() { rhythmPhilox.seed64(rack::random::u64()); rhythmDrawCtr = 0; }
    inline void seedMelodyPhiloxFull() { melodyPhilox.seed64(rack::random::u64()); melodyDrawCtr = 0; }

    inline float philoxRhythm() {
        uint64_t base = (uint64_t)(rhythmDrawCtr) * DRAW_CHUNK + rhythmCursor++;
        return rhythmPhilox.atUniform(base);
    }
    inline float philoxMelody() {
        uint64_t base = (uint64_t)(melodyDrawCtr) * DRAW_CHUNK + melodyCursor++;
        return melodyPhilox.atUniform(base);
    }

    //inline float unitStochastic() { return rngToFloat(stochasticRng); }
    inline float unitRhythm() { return usePhiloxDraws ? philoxRhythm() : rngToFloat(rhythmRng); }
    inline float unitMelody() { return usePhiloxDraws ? philoxMelody() : rngToFloat(melodyRng); }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

    void seedRngFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat);
    /// Seed from full 64-bit internal entropy (two u64 words). Used for internal
    /// reseeds (no CV); gives the full state space rather than the 0..10 float.
    void seedRngFull(rack::random::Xoroshiro128Plus& rng);
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
    void redrawRhythm(const PatternInput& in, bool promoteToA = true);

    // Regenerate melody pattern (16 steps of semitone + pitch voltage)
    void redrawMelody(const PatternInput& in, bool promoteToA = true);

    // Updates the rhythm/melody arrays used for UI and LEDs based on the 
    // current knob positions and the *existing* random buffers.
    void refreshVisualCache(const PatternInput& in);

    // Apply any pending seeds, then redraw both patterns.
    // Called at phrase boundaries and on reset.
    void applyPendingSeedsAndRedraw(const PatternInput& in);

    // ── Playable slew ──────────────────────────────────────────────────────────
    // Latch the live slew (call at step-0 wrap), then recompute effective arrays
    // if the latched value changed. Cheap; safe to call every step.
    void latchMix(float rhythmMix, float melodyMix);
    void recomputeEffectiveRhythm();   // public[] = A + rhythmMixLatched*(B-A)
    void recomputeEffectiveMelody();   // public[] = A + melodyMixLatched*(B-A)

    // ── Sands spread-stage contract (Option W) ─────────────────────────────────
    // A Sands visual expander owns the spread→final stage when present:
    //   1. call setSandsActive(true) each control cycle it is connected,
    //   2. read the slewedDraw buffers below as its INPUT (post-slew draw),
    //   3. apply spread (+ LOR is index-mapping, unchanged) and write the result
    //      into the public/final arrays (rhythmRandom[] etc.).
    // When no Sands is connected, leave sandsActive=false and slew copies
    // slewedDraw → final automatically.
    void setSandsActive(bool a) { sandsActive = a; }
    // The slewedDraw buffers (slewedRhythm[], slewedPolyMelody[][], etc.) are
    // public members above — the Sands stage reads them directly as its input.
    
    // ── Seed Management API ────────────────────────────────────────────────────
    /// Arm a rhythm seed to be applied at next phrase boundary
    void setPendingRhythmSeed(float seedValue) {
        rhythmSeedPendingFloat = seedValue;
        rhythmSeedPending = true;
    }
    
    /// Arm a melody seed to be applied at next phrase boundary
    void setPendingMelodySeed(float seedValue) {
        melodySeedPendingFloat = seedValue;
        melodySeedPending = true;
    }

    /// Arm a rhythm ROLL (dice press) — redraw from the advancing RNG at the next
    /// phrase boundary WITHOUT reseeding. This is the normal dice action.
    void setPendingRhythmRoll() { rhythmRollPending = true; }
    /// Arm a melody ROLL (dice press) — redraw without reseeding.
    void setPendingMelodyRoll() { melodyRollPending = true; }

    /// Arm a rhythm TRIAL/audition roll — like a roll but A stays anchored
    /// (promoteToA=false): auditions a fresh candidate B against the fixed A.
    void setPendingRhythmTrial() { if (auditionsAllowed() && rhythmPendingAuditions < MAX_AUDITIONS_PER_ROLL) { rhythmTrialPending = true; rhythmPendingAuditions++; } }
    /// Arm a melody TRIAL/audition roll.
    void setPendingMelodyTrial() { if (auditionsAllowed() && melodyPendingAuditions < MAX_AUDITIONS_PER_ROLL) { melodyTrialPending = true; melodyPendingAuditions++; } }

    /// Arm a rhythm RESEED-ROLL — reseed but keep the A/B morph (promote B→A, no
    /// firstDraw). full=true → full 64-bit internal entropy (float ignored);
    /// full=false → reseed from the CV-derived float (lower precision).
    void setPendingRhythmReseedRoll(float seedValue, bool full) { rhythmReseedRollFloat = seedValue; rhythmReseedRollFull = full; rhythmReseedRollPending = true; }
    /// Arm a melody RESEED-ROLL.
    void setPendingMelodyReseedRoll(float seedValue, bool full) { melodyReseedRollFloat = seedValue; melodyReseedRollFull = full; melodyReseedRollPending = true; }

    /// Check if a rhythm dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isRhythmSeedPending() const { return rhythmSeedPending || rhythmRollPending || rhythmTrialPending || rhythmReseedRollPending; }

    /// Check if a melody dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isMelodySeedPending() const { return melodySeedPending || melodyRollPending || melodyTrialPending || melodyReseedRollPending; }
    
    /// Handle phrase boundary: apply pending seeds and redraw patterns
    void onPhraseBoundary(const PatternInput& in) {
        applyPendingSeedsAndRedraw(in);
    }
    // Reverse phrase-boundary crossing (Mode E): force a reverse redraw of both streams
    // so the tape steps back one committed roll and the PREVIOUS draw is regenerated
    // from the counter (or the initial A/B is restored at the floor). reverseActive
    // must be true (set from phase direction) so redraw* takes the reverse branch.
    // promote=true here is irrelevant in the reverse branch (no commit on reverse).
    void reverseBoundaryRedraw(const PatternInput& in) {
        if (in.locked) return;
        redrawRhythm(in, /*promoteToA=*/true);
        redrawMelody(in, /*promoteToA=*/true);
    }

    // ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
    // stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

    void switchMelodyMode(int& stepIndex, int& lastStepIndex);
    void switchRhythmMode(int& stepIndex, int& lastStepIndex);

    // Circularly shifts the internal random buffers
    void rotateRhythm(int steps);     // Gates and Rests
    void rotateVariation(int steps);  // Note length biasing
    void rotateLegato(int steps);     // Legato and Tie probability
    void rotateAccent(int steps);     // Accent gate probability (NEW)
    void rotateMelody(int steps);     // Pitch selection
    void rotateOctave(int steps);     // Octave selection
    
    // Composite operations (call multiple rotates + refresh in one call)
    void rotateRhythmPattern(int steps);  // Rotate rhythm + variation + legato
    void rotateMelodyPattern(int steps);  // Rotate melody + octave
    
    // Refresh visual cache after pattern changes
    void refreshPatternCache(const PatternInput& in);

    void resetDnaRotation();          // Restore from Source Cache
};
