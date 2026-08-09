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
#include "../../tuning/TuningTable.hpp"   // dotModular::TuningTable — shared per-degree tuning (Sikit)

template<typename T>
static inline T pe_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

// ── Input snapshot — filled by MeloDicer::process() each block ────────────────
// All knob/CV values are pre-read so PatternEngine is Rack-port-free.
struct PatternInput {
    // Per-DEGREE weights 0..1 (with CV applied). Sized MAXN (24) for Micro-24 (Phase 3); only the
    // first tuning.N entries are read (pickSemitone is N-bounded). At N=12 this is the legacy 12-fader
    // mask, byte-identical (the tail 12..23 stays 0 and is never summed).
    float semiWeights[dotModular::TuningTable::MAXN] = {};
    float restProb         = 0.1f;
    float variationAmount  = 0.5f;
    // LOCK Phase 2 (LOCK_SEMANTICS §9): mono BigFive LEGATO + NOTE_VALUE + ACCENT staged on the
    // snapshot so they LATCH like restProb/variationAmount. LEGATO/NOTE_VALUE were previously passed
    // live at the executeMode call sites; ACCENT lived on engine.accentProb written at THREE sites
    // (control-rate + a redundant re-fetch in executeModeE/A) — a code smell the STEP1 WriteLedger
    // A1/A2/A3 notes existed to police. Collapsing accent to this SINGLE writer removes the drift
    // hazard entirely (ledger tripwire retired) and latches it for free. Call sites read in.*.
    float legato           = 0.f;   // mono legato/tie probability 0..1
    float noteValue        = 2.f;   // mono note-value INDEX 0..7 (2 = 1/4 note)
    float accentProb        = 0.25f; // mono accent probability 0..1 (was engine.accentProb; single-writer now)
    float octaveLo         = 2.f;
    float octaveHi         = 5.f;
    float transpose        = 0.f;
    int   noteVariationMask= 0b111;
    int   dnaLength        = 16;
    int   dnaOffset        = 0;
    bool  locked           = false;
    // LOCK_SCOPE_MENU dice-scope: when locked, a stream whose dice bit is opted live may still redraw
    // its ROLL / live-mode per-cycle reroll (see applyPendingSeedsAndRedraw). Seeds/reseed-rolls stay
    // frozen even then (Reseed control, separately scoped) — the dice bit frees the DRAW only. Set from
    // engine.scopeLiveMask in updatePatternInput. false when unlocked-irrelevant / whole-module lock.
    bool  diceLiveR        = false;   // rhythm dice stream allowed to redraw under lock
    bool  diceLiveM        = false;   // melody dice stream allowed to redraw under lock
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
    // Which dice the LIVE mode (rhythmMode/melodyMode==1) drives:
    // false = MAIN (promote, A walks); true = TRIAL (anchored A, variations on a
    // theme; never reseeds). Resolves the "two live modes" conflict — live is one
    // switch, the source is a separate switch, so only one dice is ever live.
    bool  seedConnected    = false;
    // (seedSampleValue removed: was a per-block SEED sample that no consumer ever read —
    //  the continuous-reseed path it was meant to feed was never built. See
    //  PHILOX_KEY_DERIVATION_AND_CA_SEED.md Finding 1.)
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

    // ── Shared tuning table (Sikit Phase 1) ─────────────────────────────────────────────────────
    // The degree->voltage map read by genPitchLive (mono + all poly voices — single source of truth).
    // Default is inert equal-division 12-TET; a claimed Sikit publishes cents[] into it (Step F).
    // While isDefault12TET, genPitchLive takes the EXACT legacy expression (byte-identical). All poly
    // paths read THIS table (no per-voice tuning). Populated by the module layer each block.
    dotModular::TuningTable tuning;

    inline int caSrcRow(int row, int strand) const {
        const bool mel = (strand == dotModular::STRAND_MELODY || strand == dotModular::STRAND_OCTAVE);
        const int r = (row >= 0 && row < 16) ? row : 0;
        return mel ? (int)caMelodySrc[r] : (int)caRhythmSrc[r];
    }

    // Remap the slewed buffers by pins, ONCE per cycle, BEFORE spread (called from
    // MonsoonSandsManager::processDNA head). Snapshot then rewrite: row's strand takes its
    // pinned source row's slewed value. Mono buffers are row 0; poly buffers row v are
    // "expander row v+1". A source row of 0 = borrow mono's slewed; 1..15 = poly v-1.
    // doR/doM (LOCK_SCOPE_MENU): remap+promote only the requested strand FAMILIES. Rhythm family =
    // RHYTHM/ACCENT/VARIATION/LEGATO (caRhythmSrc); melody family = MELODY/OCTAVE (caMelodySrc). A
    // frozen axis (do* false) leaves its slewed + random_ arrays untouched, holding the pre-lock
    // pinned values. Default true/true = remap both (unlocked). The families are fully independent
    // (separate src arrays + separate buffers), so the per-axis freeze is exact.
    void remapSlewedByPins(bool doR = true, bool doM = true) {
        // Fast identity skip — only over the families we would actually remap.
        bool identity = true;
        for (int v = 0; v < 16 && identity; ++v) {
            if (doR && caRhythmSrc[v] != v) identity = false;
            if (doM && caMelodySrc[v] != v) identity = false;
        }
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
        // Mono row 0 — rhythm family (RHYTHM/ACCENT/VAR/LEG) gated by doR, melody family (MEL/OCT) by doM.
        for (int i = 0; i < 16; ++i) {
            if (doR) {
                slewedRhythm[i]    = pickMono(caSrcRow(0, dotModular::STRAND_RHYTHM),    dotModular::STRAND_RHYTHM,    i);
                slewedAccent[i]    = pickMono(caSrcRow(0, dotModular::STRAND_ACCENT),    dotModular::STRAND_ACCENT,    i);
                slewedVariation[i] = pickMono(caSrcRow(0, dotModular::STRAND_VARIATION), dotModular::STRAND_VARIATION, i);
                slewedLegato[i]    = pickMono(caSrcRow(0, dotModular::STRAND_LEGATO),    dotModular::STRAND_LEGATO,    i);
            }
            if (doM) {
                slewedMelody[i]    = pickMono(caSrcRow(0, dotModular::STRAND_MELODY),    dotModular::STRAND_MELODY,    i);
                slewedOctave[i]    = pickMono(caSrcRow(0, dotModular::STRAND_OCTAVE),    dotModular::STRAND_OCTAVE,    i);
            }
        }
        // Poly rows 1..15 → poly buffers 0..14.
        for (int v = 0; v < 15; ++v) {
            const int row = v + 1;
            const int sR = caSrcRow(row, dotModular::STRAND_RHYTHM);
            const int sA = caSrcRow(row, dotModular::STRAND_ACCENT);
            const int sM = caSrcRow(row, dotModular::STRAND_MELODY);
            const int sO = caSrcRow(row, dotModular::STRAND_OCTAVE);
            for (int i = 0; i < 16; ++i) {
                if (doR) {
                    slewedPolyRhythm[v][i] = pickMono(sR, dotModular::STRAND_RHYTHM, i);
                    slewedPolyAccent[v][i] = pickMono(sA, dotModular::STRAND_ACCENT, i);
                }
                if (doM) {
                    slewedPolyMelody[v][i] = pickMono(sM, dotModular::STRAND_MELODY, i);
                    slewedPolyOctave[v][i] = pickMono(sO, dotModular::STRAND_OCTAVE, i);
                }
            }
        }
        // INVARIANT: the pin remap is a property of CHANGE ALLEY ALONE. It must reach the
        // output whether or not any Sands expander is attached -- Sands modules only DISPLAY
        // and MODULATE these probabilities; they are never a precondition for the correlation
        // itself. So the remap ALWAYS re-promotes every lane into random_ here, with no
        // dependence on sandsActive / hasMonoVisual / anything downstream.
        // (This runs BEFORE the spread stage, which re-reads `slewed`, so when spread is
        //  present and non-zero it simply overwrites random_ with the spread result on top of
        //  the already-correct remapped draw. When spread is absent or zero, this promote is
        //  the value the sequencer reads.)
        // The earlier `if (!sandsActive)` guard broke exactly this: with Macro attached but no
        // Mono visual, sandsActive was true yet the mono spread block (inside if(hasMonoVisual))
        // never ran, so remapped melody/octave never reached random_ while rhythm did.
        for (int i = 0; i < 16; ++i) {
            if (doR) {
                rhythmRandom[i]=slewedRhythm[i]; accentRandom[i]=slewedAccent[i];
                variationRandom[i]=slewedVariation[i]; legatoRandom[i]=slewedLegato[i];
            }
            if (doM) {
                melodyRandom[i]=slewedMelody[i]; octaveRandom[i]=slewedOctave[i];
            }
            for (int v=0;v<15;v++){
                if (doR) {
                    polyRandom(v, PL_REST)[i]=slewedPolyRhythm[v][i];
                    polyRandom(v, PL_ACCENT)[i]=slewedPolyAccent[v][i];
                }
                if (doM) {
                    polyRandom(v, PL_MELODY)[i]=slewedPolyMelody[v][i];
                    polyRandom(v, PL_OCTAVE)[i]=slewedPolyOctave[v][i];
                }
            }
        }
    }

    // regardless of mix-latch state — used before the Change Alley pin remap so the
    // remap always operates on pristine (un-remapped) slewed. Cheap; idempotent.
    // NAMING TRAP (do not be misled): the "slewed*" buffers are NOT slew output. Actual
    // SLEW is consumed at ROLL/phrase time inside redrawRhythm/Melody (blends A↔B at the
    // roll — phrase-bounded, per design). recomputeEffective* only computes the stateless
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
    // Melody group: melody / octave (+ poly melody / poly octave)
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
    // Scrub recompute guard: also track the counter and slew that the last recompute used, so the
    // no-redraw refresh path recomputes ONLY when (mix, slew, counter) actually changed -- otherwise
    // it re-derived the full K-window every ~90Hz refresh for no reason (idle cost scaling with K).
    int64_t rhythmCtrApplied = INT64_MIN, melodyCtrApplied = INT64_MIN;

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

    // ── Dice-undo capture (item 4) ─────────────────────────────────────────────
    // A user ROLL (dice press) advances the draw counter at the phrase-boundary commit
    // (applyPendingSeedsAndRedraw). To make that roll Ctrl+Z-undoable we capture the
    // (seedFloat, counter) BEFORE/AFTER the redraw for whichever stream(s) the roll moved,
    // into this plain POD. PatternEngine stays Rack-free: the OWNER module (Monsoon) reads
    // diceUndoPending in onPhraseBoundary_ and publishes it to its lock-free audio→UI ring.
    // Gated on the ROLL-pending flags only (NOT realtime-mode auto-redraw, NOT reset/reseed —
    // see UNDO_ITEM4_DICE_BUILD_SPEC.md scope ruling). Seed float is the stream identity
    // (Philox exposes no key getter); restoring it re-derives the exact key.
    struct DiceUndoCapture {
        bool    valid  = false;
        bool    movedR = false, movedM = false;
        float   rSeedBefore = 0.f, mSeedBefore = 0.f;
        int64_t rCtrBefore  = 0,   mCtrBefore  = 0;
        float   rSeedAfter  = 0.f, mSeedAfter  = 0.f;
        int64_t rCtrAfter   = 0,   mCtrAfter   = 0;
    };
    DiceUndoCapture diceUndoPending;

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
    // state is the current index (rhythmDrawCtr/melodyDrawCtr).
    bool reverseActive = false;                       // phase direction, set each block
    void setReverseActive(bool rev) { reverseActive = rev; }
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
        int base = reverseActive ? -1 : +1;
        return rhythmPendingLast ? -base : base;
    }
    inline int melodyDrawDir() const {
        int base = reverseActive ? -1 : +1;
        return melodyPendingLast ? -base : base;
    }

    inline void advanceRhythmDraw(int dir) { rhythmDrawCtr += (dir < 0 ? -1 : +1); }
    inline void advanceMelodyDraw(int dir) { melodyDrawCtr += (dir < 0 ? -1 : +1); }

    // Seed a stream's Philox from the same 0..10 float (reseed → new key, counter
    // reset to 0 = sequence restarts) or from full entropy.
    // seed sites so seed/reseed events affect both engines identically.
    // Stream separation via redDot::seed::deriveKey (PHILOX_KEY_DERIVATION_AND_CA_SEED.md
    // Finding 1 fix): the SAME seed float now yields INDEPENDENT rhythm and melody keys
    // (STREAM_RHYTHM=+0, STREAM_MELODY=+1). Previously both used the identical derivation,
    // so patching the single SEED input collapsed the two streams into one.
    inline void seedRhythmPhilox(float seedFloat) {
        rhythmPhilox.seed64(redDot::seed::deriveKey(seedFloat, redDot::seed::STREAM_RHYTHM));
        rhythmDrawCtr = 0;
    }
    inline void seedMelodyPhilox(float seedFloat) {
        melodyPhilox.seed64(redDot::seed::deriveKey(seedFloat, redDot::seed::STREAM_MELODY));
        melodyDrawCtr = 0;
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

    inline float philoxRhythmAt(int64_t pos, uint64_t cursor) const {
        return rhythmPhilox.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
    }
    inline float philoxMelodyAt(int64_t pos, uint64_t cursor) const {
        return melodyPhilox.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
    }
    struct RhythmDraw { float rhythm[16], variation[16], legato[16], accent[16];
                        float polyRhythm[15][16], polyAccent[15][16]; };
    struct MelodyDraw { float melody[16], octave[16];
                        float polyMelody[15][16], polyOctave[15][16]; };
    inline void rawDrawRhythmPatternAt(int64_t pos, RhythmDraw& d) const {
        uint64_t c = 0;
        for (int i = 0; i < 16; ++i) {
            d.rhythm[i]=philoxRhythmAt(pos,c++); d.variation[i]=philoxRhythmAt(pos,c++);
            d.legato[i]=philoxRhythmAt(pos,c++); d.accent[i]=philoxRhythmAt(pos,c++);
            for (int v=0;v<15;++v) d.polyRhythm[v][i]=philoxRhythmAt(pos,c++);
            for (int v=0;v<15;++v) d.polyAccent[v][i]=philoxRhythmAt(pos,c++);
        }
    }
    inline void rawDrawMelodyPatternAt(int64_t pos, MelodyDraw& d) const {
        uint64_t c = 0;
        for (int i = 0; i < 16; ++i) {
            d.melody[i]=philoxMelodyAt(pos,c++); d.octave[i]=philoxMelodyAt(pos,c++);
            for (int v=0;v<15;++v) d.polyMelody[v][i]=philoxMelodyAt(pos,c++);
            for (int v=0;v<15;++v) d.polyOctave[v][i]=philoxMelodyAt(pos,c++);
        }
    }
    // B2 truncated-FIR slew smoothing (DICE_SCRUB_SLEW_B2.md): geometric moving average of raw
    // draws pos..pos-SCRUB_K, weights (1-slew)^j normalized. Pure fn of pos -> reversible.
    static constexpr int SCRUB_K = 6;
    inline void patternRhythmAt(int64_t pos, float slew, RhythmDraw& out) const {
        const float sl = slew<0.f?0.f:(slew>1.f?1.f:slew);
        float w[SCRUB_K+1], wsum=0.f, g=1.f;
        for (int j=0;j<=SCRUB_K;++j){ w[j]=g; wsum+=g; g*=(1.f-sl); }
        const float inv=(wsum>0.f)?1.f/wsum:1.f;
        for (int i=0;i<16;++i){ out.rhythm[i]=out.variation[i]=out.legato[i]=out.accent[i]=0.f;
            for(int v=0;v<15;++v){out.polyRhythm[v][i]=0.f;out.polyAccent[v][i]=0.f;} }
        RhythmDraw r;
        for (int j=0;j<=SCRUB_K;++j){ rawDrawRhythmPatternAt(pos-j,r); const float wj=w[j]*inv;
            for(int i=0;i<16;++i){ out.rhythm[i]+=wj*r.rhythm[i]; out.variation[i]+=wj*r.variation[i];
                out.legato[i]+=wj*r.legato[i]; out.accent[i]+=wj*r.accent[i];
                for(int v=0;v<15;++v){ out.polyRhythm[v][i]+=wj*r.polyRhythm[v][i];
                    out.polyAccent[v][i]+=wj*r.polyAccent[v][i]; } } }
    }
    inline void patternMelodyAt(int64_t pos, float slew, MelodyDraw& out) const {
        const float sl = slew<0.f?0.f:(slew>1.f?1.f:slew);
        float w[SCRUB_K+1], wsum=0.f, g=1.f;
        for (int j=0;j<=SCRUB_K;++j){ w[j]=g; wsum+=g; g*=(1.f-sl); }
        const float inv=(wsum>0.f)?1.f/wsum:1.f;
        for (int i=0;i<16;++i){ out.melody[i]=out.octave[i]=0.f;
            for(int v=0;v<15;++v){out.polyMelody[v][i]=0.f;out.polyOctave[v][i]=0.f;} }
        MelodyDraw r;
        for (int j=0;j<=SCRUB_K;++j){ rawDrawMelodyPatternAt(pos-j,r); const float wj=w[j]*inv;
            for(int i=0;i<16;++i){ out.melody[i]+=wj*r.melody[i]; out.octave[i]+=wj*r.octave[i];
                for(int v=0;v<15;++v){ out.polyMelody[v][i]+=wj*r.polyMelody[v][i];
                    out.polyOctave[v][i]+=wj*r.polyOctave[v][i]; } } }
    }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

    void reset();

    // ── Core generation ───────────────────────────────────────────────────────

    // Pick a DEGREE (0..n-1) weighted by fader values using a provided random value. `n` is the active
    // degree count (tuning.N): 12 for the built-in/Sikit/Micro-12, up to 24 for Micro-24. At n=12 this
    // is bit-identical to the legacy pickSemitone (same sum + walk + float-safety return).
    int pickSemitone(const float weights[], int n, float r_val);

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

    // ── Playable slew ──────────────────────────────────────────────────────────
    // Latch the live slew (call at step-0 wrap), then recompute effective arrays
    // if the latched value changed. Cheap; safe to call every step.
    // applyRhythm/applyMelody (LOCK_SCOPE_MENU): gate each stream's mix latch+recompute independently
    // (a frozen A/B axis holds its latched value). Default both true = latch both (unlocked).
    void latchMix(float rhythmMix, float melodyMix, float rhythmSlew, float melodySlew,
                  bool applyRhythm = true, bool applyMelody = true);
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
    // Scrub model: reload just recomputes the effective pattern from the restored counter+seed+
    // slew+mix -- no B reconstruction, no A restore. (Step 4c gutted the old Option-3 replay.)
    inline void regenerateRhythmB() { recomputeEffectiveRhythm(); }
    inline void regenerateMelodyB() { recomputeEffectiveMelody(); }

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
    // -roll (audition/reversible-mode gating removed under the scrub model).
    void setPendingRhythmLastRoll()  { rhythmRollPending = true; rhythmPendingLast = true; }
    void setPendingMelodyLastRoll()  { melodyRollPending = true; melodyPendingLast = true; }

    /// Arm a rhythm TRIAL/audition roll — like a roll but A stays anchored
    /// (promoteToA=false): auditions a fresh candidate B against the fixed A.
    /// Arm a melody TRIAL/audition roll.

    // LAST-TRIAL: audition the PREVIOUS candidate B (index −1, A still anchored).

    /// Arm a rhythm RESEED-ROLL — reseed but keep the A/B morph (promote B→A, no
    /// firstDraw). full=true → full 64-bit internal entropy (float ignored);
    /// full=false → reseed from the CV-derived float (lower precision).
    void setPendingRhythmReseedRoll(float seedValue, bool full) { rhythmReseedRollFloat = seedValue; rhythmReseedRollFull = full; rhythmReseedRollPending = true; }
    /// Arm a melody RESEED-ROLL.
    void setPendingMelodyReseedRoll(float seedValue, bool full) { melodyReseedRollFloat = seedValue; melodyReseedRollFull = full; melodyReseedRollPending = true; }

    /// Check if a rhythm dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isRhythmSeedPending() const { return rhythmSeedPending || rhythmRollPending || rhythmReseedRollPending; }

    /// Check if a melody dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isMelodySeedPending() const { return melodySeedPending || melodyRollPending || melodyReseedRollPending; }
    
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
