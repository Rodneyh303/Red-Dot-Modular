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
#include "../PhiloxRng.hpp"
#include "../LaneMapping.hpp"   // dotModular::STRAND_* for finalRandomByStrand

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

    // ── Unified probability storage (item 4): ONE array for all 16 voices × 6 editor lanes × 16 steps,
    // replacing the separate mono named arrays (rhythmRandom…octaveRandom) and poly polyXRandom[15][16].
    // Indexed [voiceSlot][editorLane][step] — the SAME convention as lorStore_/spread (slot 0 = V1/mono,
    // slots 1..15 = V2..V16; editor lanes MEL=0,OCT=1,REST=2,ACC=3,VAR=4,LEG=5; VAR/LEG poly-unused).
    // The 6 mono named arrays below are REFERENCE VIEWS onto random_[0][lane] so the ~160 existing
    // rhythmRandom[step] / std::rotate / whole-array sites keep working unchanged; poly access goes via
    // polyRandom(voice,lane). (mono row lane == strand index: MONO_LANE_TO_STRAND is the identity.)
    float random_[16][dotModular::NUM_STRANDS][16] = {};

    // ── Mono output views (read by MeloDicer, never written externally) — bound to random_[0][lane].
    float (&melodyRandom)[16]    = random_[0][dotModular::STRAND_MELODY];
    float (&octaveRandom)[16]    = random_[0][dotModular::STRAND_OCTAVE];
    float (&rhythmRandom)[16]    = random_[0][dotModular::STRAND_RHYTHM];
    float (&accentRandom)[16]    = random_[0][dotModular::STRAND_ACCENT];  // accent strand probabilities
    float (&variationRandom)[16] = random_[0][dotModular::STRAND_VARIATION];
    float (&legatoRandom)[16]    = random_[0][dotModular::STRAND_LEGATO];

    // Poly engine lane constants (mirror SequencerEngine::PolyLane) for polyRandom callers in this
    // layer. 0=REST,1=MEL,2=OCT,3=ACC — the engine PL_ order (converted to editor order inside).
    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_ACCENT = 3, PL_LANES = 4 };

    // Poly probability view: voice bank b (0..14 = V2..V16) → slot b+1; lane is the engine PL_ lane,
    // converted to editor order. Returns the 16-step row (float(&)[16]) so callers index [step].
    float (&polyRandom(int bank, int engLane))[16] { return random_[bank + 1][dotModular::ENGINE_LANE_TO_EDITOR[engLane & 3]]; }
    const float (&polyRandom(int bank, int engLane) const)[16] { return random_[bank + 1][dotModular::ENGINE_LANE_TO_EDITOR[engLane & 3]]; }

    // Final post-everything (A/B-mix + spread + LOR feed in upstream) probability value for a given
    // ENGINE STRAND at a given step, 0..1. Now a direct index into random_[0] (mono row): strand index
    // IS the editor-lane column (MONO_LANE_TO_STRAND identity), so no table/permutation. Out-of-range
    // falls back to rhythm (matches the old default:). Used by the Sands visual probability CV outs.
    // ── Change Alley pin-matrix (CHANGE_ALLEY_DESIGN.md §3-REVISED, PRE-SPREAD) ──
    // The pins remap the SLEWED buffers (post A/B-mix, post-slew, PRE-spread) so that a
    // pinned voice's borrowed draw is then spread with the CONSUMER's own reference —
    // equivalent to pinning the A/B samples (the agreed design). random_ reads stay plain
    // own-bank. Row 0 = mono, rows 1..15 = poly V2..V16. Strand→pool: MELODY/OCTAVE =
    // melody pin; RHYTHM/ACCENT/VARIATION/LEGATO = rhythm pin. Identity = no-op.
    uint8_t caRhythmSrc[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint8_t caMelodySrc[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    inline int caSrcRow(int row, int strand) const {
        const bool mel = (strand == dotModular::STRAND_MELODY || strand == dotModular::STRAND_OCTAVE);
        const int r = (row >= 0 && row < 16) ? row : 0;
        return mel ? (int)caMelodySrc[r] : (int)caRhythmSrc[r];
    }

    // Remap the slewed buffers by pins, ONCE per cycle, BEFORE spread (called from
    // MonsoonSandsManager::processDNA head). Snapshot then rewrite: row's strand takes its
    // pinned source row's slewed value. Mono buffers are row 0; poly buffers row v are
    // "expander row v+1". A source row of 0 = borrow mono's slewed; 1..15 = poly v-1.
    void remapSlewedByPins() {
        // Fast identity skip.
        bool identity = true;
        for (int v = 0; v < 16 && identity; ++v)
            if (caRhythmSrc[v] != v || caMelodySrc[v] != v) identity = false;
        if (identity) return;

        // Snapshot mono[16] + poly[15][16] for the six strands.
        float mR[16], mM[16], mO[16], mA[16], mV[16], mL[16];
        for (int i = 0; i < 16; ++i) {
            mR[i]=slewedRhythm[i]; mM[i]=slewedMelody[i]; mO[i]=slewedOctave[i];
            mA[i]=slewedAccent[i]; mV[i]=slewedVariation[i]; mL[i]=slewedLegato[i];
        }
        static thread_local float pR[15][16], pM[15][16], pO[15][16], pA[15][16];
        for (int v = 0; v < 15; ++v) for (int i = 0; i < 16; ++i) {
            pR[v][i]=slewedPolyRhythm[v][i]; pM[v][i]=slewedPolyMelody[v][i];
            pO[v][i]=slewedPolyOctave[v][i]; pA[v][i]=slewedPolyAccent[v][i];
        }
        // src row → (mono buffer if 0, else poly buffer src-1) for a given strand family.
        auto pickMono = [&](int srcRow, int strand, int i) -> float {
            if (srcRow == 0) {
                switch (strand) {
                    case dotModular::STRAND_RHYTHM:    return mR[i];
                    case dotModular::STRAND_MELODY:    return mM[i];
                    case dotModular::STRAND_OCTAVE:    return mO[i];
                    case dotModular::STRAND_ACCENT:    return mA[i];
                    case dotModular::STRAND_VARIATION: return mV[i];
                    default:                           return mL[i];
                }
            }
            const int v = srcRow - 1;
            switch (strand) {
                case dotModular::STRAND_RHYTHM: return pR[v][i];
                case dotModular::STRAND_MELODY: return pM[v][i];
                case dotModular::STRAND_OCTAVE: return pO[v][i];
                case dotModular::STRAND_ACCENT: return pA[v][i];
                // VAR/LEG have no per-poly slewed buffer (shared mono §4d) — borrow mono.
                case dotModular::STRAND_VARIATION: return mV[i];
                default:                           return mL[i];
            }
        };
        // Mono row 0.
        for (int i = 0; i < 16; ++i) {
            slewedRhythm[i]    = pickMono(caSrcRow(0, dotModular::STRAND_RHYTHM),    dotModular::STRAND_RHYTHM,    i);
            slewedAccent[i]    = pickMono(caSrcRow(0, dotModular::STRAND_ACCENT),    dotModular::STRAND_ACCENT,    i);
            slewedVariation[i] = pickMono(caSrcRow(0, dotModular::STRAND_VARIATION), dotModular::STRAND_VARIATION, i);
            slewedLegato[i]    = pickMono(caSrcRow(0, dotModular::STRAND_LEGATO),    dotModular::STRAND_LEGATO,    i);
            slewedMelody[i]    = pickMono(caSrcRow(0, dotModular::STRAND_MELODY),    dotModular::STRAND_MELODY,    i);
            slewedOctave[i]    = pickMono(caSrcRow(0, dotModular::STRAND_OCTAVE),    dotModular::STRAND_OCTAVE,    i);
        }
        // Poly rows 1..15 → poly buffers 0..14.
        for (int v = 0; v < 15; ++v) {
            const int row = v + 1;
            const int sR = caSrcRow(row, dotModular::STRAND_RHYTHM);
            const int sA = caSrcRow(row, dotModular::STRAND_ACCENT);
            const int sM = caSrcRow(row, dotModular::STRAND_MELODY);
            const int sO = caSrcRow(row, dotModular::STRAND_OCTAVE);
            for (int i = 0; i < 16; ++i) {
                slewedPolyRhythm[v][i] = pickMono(sR, dotModular::STRAND_RHYTHM, i);
                slewedPolyAccent[v][i] = pickMono(sA, dotModular::STRAND_ACCENT, i);
                slewedPolyMelody[v][i] = pickMono(sM, dotModular::STRAND_MELODY, i);
                slewedPolyOctave[v][i] = pickMono(sO, dotModular::STRAND_OCTAVE, i);
            }
        }
        // NON-SANDS path: recomputeEffective already promoted the (un-remapped) slewed into
        // random_ before this runs. With no Sands there is no spread stage to re-read slewed,
        // so re-promote the remapped slewed into random_ here. (When sandsActive, spread reads
        // slewed and writes random_ itself, so this promote is skipped.)
        if (!sandsActive) {
            for (int i = 0; i < 16; ++i) {
                rhythmRandom[i]=slewedRhythm[i]; accentRandom[i]=slewedAccent[i];
                variationRandom[i]=slewedVariation[i]; legatoRandom[i]=slewedLegato[i];
                melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
                for (int v=0;v<15;v++){
                    polyRandom(v, PL_REST)[i]=slewedPolyRhythm[v][i];
                    polyRandom(v, PL_ACCENT)[i]=slewedPolyAccent[v][i];
                    polyRandom(v, PL_MELODY)[i]=slewedPolyMelody[v][i];
                    polyRandom(v, PL_OCTAVE)[i]=slewedPolyOctave[v][i];
                }
            }
        }
    }

    // Force a fresh slewed = bl(LockedA, CandB) for BOTH rhythm and melody families,
    // regardless of mix-latch state — used before the Change Alley pin remap so the
    // remap always operates on pristine (un-remapped) slewed. Cheap; idempotent.
    // NAMING TRAP (do not be misled): the "slewed*" buffers are NOT slew output. Actual
    // SLEW is consumed at ROLL/phrase time inside redrawRhythm/Melody (blends A↔B at the
    // roll — phrase-bounded, per design). recomputeEffective* only computes the stateless
    // MIX blend A + mix*(B-A) from the already-rolled LockedA/CandB, so calling it every
    // audio cycle re-derives the SAME values from unchanged inputs — no re-slew, safe.
    void forceRecomputeSlewed() {
        rhythmMixApplied = -999.f;   // invalidate so recompute* actually runs
        melodyMixApplied = -999.f;
        recomputeEffectiveRhythm();
        recomputeEffectiveMelody();
    }

    inline float finalRandomByStrand(int strand, int step) const {
        const int s = (strand >= 0 && strand < dotModular::NUM_STRANDS) ? strand : dotModular::STRAND_RHYTHM;
        return random_[0][s][step & 0x0F];   // plain own-bank; pin remap lives upstream in slewed
    }

    // Poly strands: 15 voices, each with Rhythm, Melody, and Octave draws
    // (poly probability arrays removed — poly voices live in random_[1..15], accessed via
    // polyRandom(bank, engLane). Previously polyRhythm/Melody/Octave/AccentRandom[15][16].)

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
    float polyAccentLockedA[15][16]={}, polyAccentCandB[15][16]={};
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
    // A/B morph coefficient — GLOBAL per strand family (one scalar for ALL voices).
    // LOAD-BEARING INVARIANT: because this is global and the blend is linear, remapping
    // Change Alley pins at the slewed buffers (post-mix) is provably IDENTICAL to remapping
    // at the A/B candidates (pre-mix): both give A[src] + s*(B[src]-A[src]). If this ever
    // becomes PER-VOICE, that equivalence breaks — post-mix would apply the SOURCE's mix
    // while "own manipulation" demands the CONSUMER's — and the Change Alley remap must
    // move to the A/B candidate buffers. See CHANGE_ALLEY_DESIGN.md §3.
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
    float slewedPolyAccent[15][16]={};
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
    float polyAccentSource[15][16] = {};
    float polyMelodySource[15][16] = {};
    float polyOctaveSource[15][16] = {};

    // Caches for UI/Lights to reflect the current state
    bool  rhythmPattern[16]   = {};
    int   melodySemitone[16]  = {};
    float melodyPitchV[16]    = {};

    // ── RNG state ─────────────────────────────────────────────────────────────
    // (Draws are Philox-only — counter-based, stateless. Seed lives in *SeedFloat
    //  and the per-strand Philox key; no stream-state members needed.)

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
    bool  rhythmPendingLast = false, melodyPendingLast = false; // Last* = invert dice dir this boundary
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
    float cPolyAccentA[15][16]={}, cPolyAccentB[15][16]={};
    float cMelodyA[16]={}, cMelodyB[16]={}, cOctaveA[16]={}, cOctaveB[16]={};
    float cPolyMelodyA[15][16]={}, cPolyMelodyB[15][16]={};
    float cPolyOctaveA[15][16]={}, cPolyOctaveB[15][16]={};

    // ── Seed management ───────────────────────────────────────────────────────

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
    // Draws are always Philox (counter-based). The legacy Xoroshiro A/B path is gone.
    redDot::PhiloxRng rhythmPhilox, melodyPhilox;
    int64_t   rhythmDrawCtr = 0, melodyDrawCtr = 0;   // signed: can go negative on reverse
    uint64_t  rhythmCursor  = 0, melodyCursor  = 0;   // intra-draw position, reset per redraw

    // Reset the intra-draw cursor at the start of a redraw (called by redrawRhythm/
    // redrawMelody before any unit() calls so the draw maps to its chunk base).
    inline void beginRhythmDraw() { rhythmCursor = 0; }
    inline void beginMelodyDraw() { melodyCursor = 0; }
    // Step the draw-counter (dir>0 forward, dir<0 reverse). Forward-only for now;
    // the reverse/cross-boundary branch will drive dir<0.
    // ── Reversible mode (Mode E phase reverse), per stream ──
    // NORMAL (default): all features (auditions, reseed-on-roll, live trial source);
    // reverse just keeps rolling forward-style (no backward draw-tracking). REVERSIBLE:
    // pure stochastic dice — the draw index is a SIGNED counter, +1 on a forward armed
    // roll, -1 on a reverse armed roll, NO floor/ceiling (Philox is a keyed bijection
    // over the full signed counter space, so any index is a valid reproducible draw).
    // Reversible blocks trial arming + trial-as-live-source + reseed-on-roll. The only
    // state is the current index (rhythmDrawCtr/melodyDrawCtr).
    bool rhythmReversible = false, melodyReversible = false;
    bool reverseActive = false;                       // phase direction, set each block
    void setReverseActive(bool rev) { reverseActive = rev; }
    void setRhythmReversible(bool r) { rhythmReversible = r; }
    void setMelodyReversible(bool r) { melodyReversible = r; }
    bool rhythmAuditionsAllowed() const { return !rhythmReversible; }
    bool melodyAuditionsAllowed() const { return !melodyReversible; }
    inline void zeroRhythmIndex() { rhythmDrawCtr = 0; }
    inline void zeroMelodyIndex() { melodyDrawCtr = 0; }
    // Draw-step direction for a stream this redraw: reverse only when the stream is
    // reversible AND the phase is moving backward; otherwise forward.
    // Draw-step direction this redraw. BASE = what a plain Dice does now: forward,
    // or backward in Mode E reverse on a reversible stream. LAST-dice/trial INVERTS
    // that base (forward mode: Last = −1; Mode E reverse: plain dice is already −1, so
    // Last = +1). So Last* is always "the opposite of dice in the current mode", not an
    // absolute reverse.
    inline int rhythmDrawDir() const {
        int base = (reverseActive && rhythmReversible) ? -1 : +1;
        return rhythmPendingLast ? -base : base;
    }
    inline int melodyDrawDir() const {
        int base = (reverseActive && melodyReversible) ? -1 : +1;
        return melodyPendingLast ? -base : base;
    }

    inline void advanceRhythmDraw(int dir) { rhythmDrawCtr += (dir < 0 ? -1 : +1); }
    inline void advanceMelodyDraw(int dir) { melodyDrawCtr += (dir < 0 ? -1 : +1); }

    // Seed a stream's Philox from the same 0..10 float (reseed → new key, counter
    // reset to 0 = sequence restarts) or from full entropy.
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

    inline float unitRhythm() { return philoxRhythm(); }
    inline float unitMelody() { return philoxMelody(); }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

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

    // ── State regeneration (Option 3 reload) ──────────────────────────────────
    // Reconstruct candidate B from the restored generative state: key (seeded),
    // drawCtr (restored), committed A (restored), and the latched slew. Replays
    // EXACTLY the draw that produced the live B — same addressable Philox indices
    // (drawCtr·DRAW_CHUNK + cursor) and the same per-field call order as
    // redrawRhythm/redrawMelody's else-branch — WITHOUT advancing the counter
    // (we reproduce the draw AT drawCtr, which already shaped the current B). A is
    // irreducible (carries the accumulated slew walk) so it is restored directly,
    // not regenerated. After this, recomputeEffective* yields the live pattern.
    inline void regenerateRhythmB() {
        if (rhythmFirstDraw) { recomputeEffectiveRhythm(); return; }  // never rolled: B==A path
        beginRhythmDraw();                                            // cursor = 0
        const float sl = rack::math::clamp(rhythmSlewLatched, 0.f, 1.f);
        auto step = [&](float a){ return a + sl * (unitRhythm() - a); };
        for (int i = 0; i < 16; ++i) {
            rhythmCandB[i]    = step(rhythmLockedA[i]);
            variationCandB[i] = step(variationLockedA[i]);
            legatoCandB[i]    = step(legatoLockedA[i]);
            accentCandB[i]    = step(accentLockedA[i]);
            for (int v = 0; v < 15; ++v) polyRhythmCandB[v][i] = step(polyRhythmLockedA[v][i]);
            for (int v = 0; v < 15; ++v) polyAccentCandB[v][i] = step(polyAccentLockedA[v][i]);
        }
        rhythmSlewApplied = -1.f;
        recomputeEffectiveRhythm();
        // Rebuild the cached SOURCE arrays (read for note output, not just preview) —
        // mirrors redrawRhythm's tail so playback matches exactly post-reload.
        for (int i = 0; i < 16; ++i) {
            rhythmSource[i]=slewedRhythm[i]; variationSource[i]=slewedVariation[i];
            legatoSource[i]=slewedLegato[i]; accentSource[i]=slewedAccent[i];
            for (int v=0; v<15; ++v) polyRhythmSource[v][i]=slewedPolyRhythm[v][i];
            for (int v=0; v<15; ++v) polyAccentSource[v][i]=slewedPolyAccent[v][i];
        }
    }
    inline void regenerateMelodyB() {
        if (melodyFirstDraw) { recomputeEffectiveMelody(); return; }
        beginMelodyDraw();
        const float sl = rack::math::clamp(melodySlewLatched, 0.f, 1.f);
        auto step = [&](float a){ return a + sl * (unitMelody() - a); };
        for (int i = 0; i < 16; ++i) {
            melodyCandB[i] = step(melodyLockedA[i]);
            octaveCandB[i] = step(octaveLockedA[i]);
            for (int v = 0; v < 15; ++v) {
                polyMelodyCandB[v][i] = step(polyMelodyLockedA[v][i]);
                polyOctaveCandB[v][i] = step(polyOctaveLockedA[v][i]);
            }
        }
        melodySlewApplied = -1.f;
        recomputeEffectiveMelody();
        for (int i = 0; i < 16; ++i) {
            melodySource[i]=slewedMelody[i]; octaveSource[i]=slewedOctave[i];
            for (int v=0; v<15; ++v) {
                polyMelodySource[v][i]=slewedPolyMelody[v][i];
                polyOctaveSource[v][i]=slewedPolyOctave[v][i];
            }
        }
    }

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
    void setPendingRhythmRoll() { rhythmRollPending = true; rhythmPendingLast = false; }
    /// Arm a melody ROLL (dice press) — redraw without reseeding.
    void setPendingMelodyRoll() { melodyRollPending = true; melodyPendingLast = false; }

    // LAST-DICE: a roll that steps the draw index the OTHER way at the next boundary —
    // "give me the previous draw." Enabled by Philox addressability. BLOCKED on a
    // reversible stream: there the index↔phase coupling IS the reproducibility contract,
    // and a manual index step (independent of phase) would silently void it. So Last* is
    // a Normal-mode navigation gesture only — same principle that blocks trial/reseed-on
    // -roll in reversible mode. (rhythmAuditionsAllowed() == !rhythmReversible.)
    void setPendingRhythmLastRoll()  { if (rhythmAuditionsAllowed()) { rhythmRollPending = true; rhythmPendingLast = true; } }
    void setPendingMelodyLastRoll()  { if (melodyAuditionsAllowed()) { melodyRollPending = true; melodyPendingLast = true; } }

    /// Arm a rhythm TRIAL/audition roll — like a roll but A stays anchored
    /// (promoteToA=false): auditions a fresh candidate B against the fixed A.
    void setPendingRhythmTrial() { if (rhythmAuditionsAllowed()) { rhythmTrialPending = true; rhythmPendingLast = false; } }
    /// Arm a melody TRIAL/audition roll.
    void setPendingMelodyTrial() { if (melodyAuditionsAllowed()) { melodyTrialPending = true; melodyPendingLast = false; } }

    // LAST-TRIAL: audition the PREVIOUS candidate B (index −1, A still anchored).
    void setPendingRhythmLastTrial() { if (rhythmAuditionsAllowed()) { rhythmTrialPending = true; rhythmPendingLast = true; } }
    void setPendingMelodyLastTrial() { if (melodyAuditionsAllowed()) { melodyTrialPending = true; melodyPendingLast = true; } }

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

    // ── Mode switching (dice ↔ realtime) ──────────────────────────────────────
    // stepIndex / lastStepIndex passed in+out so the engine can cache/restore them.

    void switchMelodyMode(int& stepIndex, int& lastStepIndex);
    void switchRhythmMode(int& stepIndex, int& lastStepIndex);

    // Circularly shifts the internal random buffers
    
    // Composite operations (call multiple rotates + refresh in one call)
    
    // Refresh visual cache after pattern changes

};
