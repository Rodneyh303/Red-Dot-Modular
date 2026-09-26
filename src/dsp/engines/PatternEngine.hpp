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
#include <unordered_map>
#include <utility>
#include "../PhiloxRng.hpp"
#include "../LaneMapping.hpp"   // dotModular::STRAND_* for finalRandomByStrand
#include "../MovingAverageCopula.hpp"   // Phase 1: slew as normal-space moving-average copula (replaces L1 linear-uniform window)
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
    float qmixLevel         = 0.f;   // Task 4: mono q-mix threshold 0..1 (QMIX_LEVEL_PARAM). draw<level = hit.
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
    bool  diceLiveQ        = false;   // q-mix dice stream allowed to redraw under lock (own axis, SB_DICE_Q)
    // Playable dice slew (0..1) per group. Latched at step 0; morphs the
    // effective pattern between the locked (A) and candidate (B) draws.
    float rhythmSlew       = 0.f;   // bipolar: -1=anti, 0=independent, +1=correlated
    float melodySlew       = 0.f;
    float qmixSlew         = 0.f;   // q-mix twin of melodySlew (Task 4)
    // Live A<->B blend (MIX). Separate from slew: slew is consumed at roll
    // (shapes B); mix is the live, continuous A<->B morph used for output.
    float rhythmMix        = 0.f;
    float melodyMix        = 0.f;
    float qmixMix          = 0.f;   // q-mix twin of melodyMix (Task 4)
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

    // ── Unified probability storage (item 4): ONE array for all 16 voices × 7 editor lanes × 16 steps,
    // replacing the separate mono named arrays (rhythmRandom…octaveRandom) and poly polyXRandom[15][16].
    // Indexed [voiceSlot][editorLane][step] — the SAME convention as lorStore_/spread (slot 0 = V1/mono,
    // slots 1..15 = V2..V16; editor lanes MEL=0,OCT=1,QMIX=2,REST=3,ACC=4,VAR=5,LEG=6; VAR/LEG poly-unused).
    // The 7 mono named arrays below are REFERENCE VIEWS onto random_[0][lane] so the ~160 existing
    // rhythmRandom[step] / std::rotate / whole-array sites keep working unchanged; poly access goes via
    // polyRandom(voice,lane). (mono row lane == strand index: MONO_LANE_TO_STRAND is the identity.)
    float random_[16][dotModular::NUM_STRANDS][16] = {};

    // ── Mono output views (read by MeloDicer, never written externally) — bound to random_[0][lane].
    float (&melodyRandom)[16]    = random_[0][dotModular::STRAND_MELODY];
    float (&octaveRandom)[16]    = random_[0][dotModular::STRAND_OCTAVE];
    float (&qmixRandom)[16]      = random_[0][dotModular::STRAND_QMIX];
    float (&rhythmRandom)[16]    = random_[0][dotModular::STRAND_RHYTHM];
    float (&accentRandom)[16]    = random_[0][dotModular::STRAND_ACCENT];  // accent strand probabilities
    float (&variationRandom)[16] = random_[0][dotModular::STRAND_VARIATION];
    float (&legatoRandom)[16]    = random_[0][dotModular::STRAND_LEGATO];

    // Poly engine lane constants (mirror SequencerEngine::PolyLane) for polyRandom callers in this
    // layer. 0=REST,1=MEL,2=OCT,3=ACC,4=QMIX — the engine PL_ order (converted to editor order inside).
    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_ACCENT = 3, PL_QMIX = 4, PL_LANES = 5 };
    // q-mix ordering asserts (Task 4a): the q-mix Philox stream key MUST be STREAM_SOURCE_SELECT (=3),
    // and the PL_QMIX poly lane MUST round-trip to editor lane 2 (STRAND_QMIX). If either the RNG
    // stream order or the poly-lane order is renumbered, these fire at compile time.
    static_assert(dotModular::QMIX_STREAM_KEY == redDot::seed::STREAM_SOURCE_SELECT,
                  "q-mix stream key must equal redDot::seed::STREAM_SOURCE_SELECT (=3)");
    static_assert(dotModular::ENGINE_LANE_TO_EDITOR_QMIX[PL_QMIX] == dotModular::STRAND_QMIX,
                  "PL_QMIX poly lane must map to editor lane STRAND_QMIX (=2)");

    // Poly probability view: voice bank b (0..14 = V2..V16) → slot b+1; lane is the engine PL_ lane,
    // converted to editor order. Returns the 16-step row (float(&)[16]) so callers index [step].
    // Now handles 5 poly lanes (REST/MEL/OCT/ACC/QMIX) via ENGINE_LANE_TO_EDITOR_QMIX.
    float (&polyRandom(int bank, int engLane))[16] {
        int edLane = (engLane >= 0 && engLane < 5) ? dotModular::ENGINE_LANE_TO_EDITOR_QMIX[engLane] : 0;
        return random_[bank + 1][edLane];
    }
    const float (&polyRandom(int bank, int engLane) const)[16] {
        int edLane = (engLane >= 0 && engLane < 5) ? dotModular::ENGINE_LANE_TO_EDITOR_QMIX[engLane] : 0;
        return random_[bank + 1][edLane];
    }

    // Final post-everything (A/B-mix + spread + LOR feed in upstream) probability value for a given
    // ENGINE STRAND at a given step, 0..1. Now a direct index into random_[0] (mono row): strand index
    // IS the editor-lane column (MONO_LANE_TO_STRAND identity), so no table/permutation. Out-of-range
    // falls back to rhythm (matches the old default:). Used by the Sands visual probability CV outs.
    // Spread target mode per lane (0=Anchor V1, 1=Follow CA). Mirrored from the Monsoon's
    // EditorState each control cycle so the spread path (which only has PatternEngine&) can
    // read it without a Monsoon pointer. SPREAD_TARGET_MODES.md.
    uint8_t spreadTargetMode[5] = {0,0,0,0,0};   // 5 poly lanes: REST/MEL/OCT/ACC/QMIX

    // ── Change Alley pin-matrix (CHANGE_ALLEY_DESIGN.md §3-REVISED, PRE-SPREAD) ──
    // The pins remap the SLEWED buffers (post A/B-mix, post-slew, PRE-spread) so that a
    // pinned voice's borrowed draw is then spread with the CONSUMER's own reference —
    // equivalent to pinning the A/B samples (the agreed design). random_ reads stay plain
    // own-bank. Row 0 = mono, rows 1..15 = poly V2..V16. Strand→pool: MELODY/OCTAVE =
    // melody pin; RHYTHM/ACCENT/VARIATION/LEGATO = rhythm pin. Identity = no-op.
    uint8_t caRhythmSrc[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint8_t caMelodySrc[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    // Q-mix source-select plane (QMIX_LANE_PARITY §"The blend"): the NEW green pin plane staged from
    // MonsoonChangeAlleyV2::qmixSrc. STRAND_QMIX's slewed buffer + the downstream per-voice blend
    // THRESHOLD ride THIS array (not caRhythmSrc), so a voice can consume another voice's q-mix
    // probability — CA parity for q-mix. Identity default = no-op (Straits per-voice level as before).
    uint8_t caQmixSrc[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    // ── Shared tuning table (Sikit Phase 1) ─────────────────────────────────────────────────────
    // The degree->voltage map read by genPitchLive (mono + all poly voices — single source of truth).
    // Default is inert equal-division 12-TET; a claimed Sikit publishes cents[] into it (Step F).
    // While isDefault12TET, genPitchLive takes the EXACT legacy expression (byte-identical). All poly
    // paths read THIS table (no per-voice tuning). Populated by the module layer each block.
    dotModular::TuningTable tuning;

    inline int caSrcRow(int row, int strand) const {
        const int r = (row >= 0 && row < 16) ? row : 0;
        // Q-mix rides its OWN green plane (parity with white=rhythm / red=melody). MELODY/OCTAVE ride
        // the melody plane; everything else (RHYTHM/ACCENT/VARIATION/LEGATO) rides the rhythm plane.
        if (strand == dotModular::STRAND_QMIX) return (int)caQmixSrc[r];
        const bool mel = (strand == dotModular::STRAND_MELODY || strand == dotModular::STRAND_OCTAVE);
        return mel ? (int)caMelodySrc[r] : (int)caRhythmSrc[r];
    }
    // Per-voice q-mix source row for the downstream blend THRESHOLD (which voice's q-mix probability
    // consuming voice `row` reads). row 0 = mono/voice-0, rows 1..15 = poly V2..V16. Identity default.
    inline int caQmixSrcRow(int row) const {
        const int r = (row >= 0 && row < 16) ? row : 0;
        return (int)caQmixSrc[r];
    }
    // Per-voice input-CV source row for the blend's QUANTISED-INPUT operand: it rides CA's MELODY
    // plane (QMIX_LANE_PARITY §"The blend" step 2 — "input CV = a CA melody source"), so a voice can
    // quantise another voice's input line. Identity default = each voice reads its own input CV.
    inline int caInputCvSrcRow(int row) const {
        const int r = (row >= 0 && row < 16) ? row : 0;
        return (int)caMelodySrc[r];
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
    // doQ (SB_CA_Q): q-mix rides its OWN green pin plane (caQmixSrc), gated independently of the
    // melody plane. Defaults true (unlocked / callers pre-dating q-mix behave as before, but q-mix
    // now only remaps when doQ AND its own plane is non-identity).
    // Pre-remap snapshots — the voice's OWN draw before CA pins overwrite it. Follow-CA spread
    // interpolates between this (own) and the post-remap value (leader). Without it follow-CA is
    // a no-op: the remap already replaced own with leader, so spread would blend leader with itself.
    // Populated unconditionally at the start of remapSlewedByPins (even on identity skip, where
    // pre-remap == post-remap). Read by the spread path when the lane's mode is Follow CA.
    float preRemapSlewedRhythm[16]={}, preRemapSlewedVariation[16]={}, preRemapSlewedLegato[16]={}, preRemapSlewedAccent[16]={};
    float preRemapSlewedMelody[16]={}, preRemapSlewedOctave[16]={}, preRemapSlewedQmix[16]={};
    float preRemapSlewedPolyRhythm[15][16]={}, preRemapSlewedPolyMelody[15][16]={}, preRemapSlewedPolyOctave[15][16]={};
    float preRemapSlewedPolyAccent[15][16]={}, preRemapSlewedPolyQmix[15][16]={};
    void snapshotPreRemap() {
        for (int i=0;i<16;++i){
            preRemapSlewedRhythm[i]=slewedRhythm[i]; preRemapSlewedVariation[i]=slewedVariation[i];
            preRemapSlewedLegato[i]=slewedLegato[i]; preRemapSlewedAccent[i]=slewedAccent[i];
            preRemapSlewedMelody[i]=slewedMelody[i]; preRemapSlewedOctave[i]=slewedOctave[i];
            preRemapSlewedQmix[i]=slewedQmix[i];
            for(int v=0;v<15;++v){
                preRemapSlewedPolyRhythm[v][i]=slewedPolyRhythm[v][i];
                preRemapSlewedPolyMelody[v][i]=slewedPolyMelody[v][i];
                preRemapSlewedPolyOctave[v][i]=slewedPolyOctave[v][i];
                preRemapSlewedPolyAccent[v][i]=slewedPolyAccent[v][i];
                preRemapSlewedPolyQmix[v][i]=slewedPolyQmix[v][i];
            }
        }
    }
    void remapSlewedByPins(bool doR = true, bool doM = true, bool doQ = true) {
        snapshotPreRemap();   // always snapshot, even on identity skip (pre == post then)
        // Fast identity skip — only over the families we would actually remap.
        bool identity = true;
        for (int v = 0; v < 16 && identity; ++v) {
            if (doR && caRhythmSrc[v] != v) identity = false;
            if (doM && caMelodySrc[v] != v) identity = false;
            if (doQ && caQmixSrc[v]   != v) identity = false;
        }
        if (identity) return;

        // Snapshot mono[16] + poly[15][16] for the seven strands (incl. q-mix, melody family).
        float mR[16], mM[16], mO[16], mQ[16], mA[16], mV[16], mL[16];
        for (int i = 0; i < 16; ++i) {
            mR[i]=slewedRhythm[i]; mM[i]=slewedMelody[i]; mO[i]=slewedOctave[i];
            mQ[i]=slewedQmix[i];
            mA[i]=slewedAccent[i]; mV[i]=slewedVariation[i]; mL[i]=slewedLegato[i];
        }
        static thread_local float pR[15][16], pM[15][16], pO[15][16], pQ[15][16], pA[15][16];
        for (int v = 0; v < 15; ++v) for (int i = 0; i < 16; ++i) {
            pR[v][i]=slewedPolyRhythm[v][i]; pM[v][i]=slewedPolyMelody[v][i];
            pO[v][i]=slewedPolyOctave[v][i]; pQ[v][i]=slewedPolyQmix[v][i];
            pA[v][i]=slewedPolyAccent[v][i];
        }
        // src row → (mono buffer if 0, else poly buffer src-1) for a given strand family.
        auto pickMono = [&](int srcRow, int strand, int i) -> float {
            if (srcRow == 0) {
                switch (strand) {
                    case dotModular::STRAND_MELODY:    return mM[i];
                    case dotModular::STRAND_OCTAVE:    return mO[i];
                    case dotModular::STRAND_QMIX:      return mQ[i];
                    case dotModular::STRAND_RHYTHM:    return mR[i];
                    case dotModular::STRAND_ACCENT:    return mA[i];
                    case dotModular::STRAND_VARIATION: return mV[i];
                    case dotModular::STRAND_LEGATO:    return mL[i];
                    default:                           return 0.5f;  // fallback
                }
            }
            const int v = srcRow - 1;
            switch (strand) {
                case dotModular::STRAND_MELODY: return pM[v][i];
                case dotModular::STRAND_OCTAVE: return pO[v][i];
                case dotModular::STRAND_QMIX:   return pQ[v][i];
                case dotModular::STRAND_RHYTHM: return pR[v][i];
                case dotModular::STRAND_ACCENT: return pA[v][i];
                // VAR/LEG have no per-poly slewed buffer (shared mono §4d) — borrow mono.
                case dotModular::STRAND_VARIATION: return mV[i];
                case dotModular::STRAND_LEGATO:    return mL[i];
                default:                           return 0.5f;  // fallback
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
                slewedMelody[i] = pickMono(caSrcRow(0, dotModular::STRAND_MELODY), dotModular::STRAND_MELODY, i);
                slewedOctave[i] = pickMono(caSrcRow(0, dotModular::STRAND_OCTAVE), dotModular::STRAND_OCTAVE, i);
            }
            if (doQ) {
                slewedQmix[i]   = pickMono(caSrcRow(0, dotModular::STRAND_QMIX),   dotModular::STRAND_QMIX,   i);
            }
        }
        // Poly rows 1..15 → poly buffers 0..14.
        for (int v = 0; v < 15; ++v) {
            const int row = v + 1;
            const int sR = caSrcRow(row, dotModular::STRAND_RHYTHM);
            const int sA = caSrcRow(row, dotModular::STRAND_ACCENT);
            const int sM = caSrcRow(row, dotModular::STRAND_MELODY);
            const int sO = caSrcRow(row, dotModular::STRAND_OCTAVE);
            const int sQ = caSrcRow(row, dotModular::STRAND_QMIX);
            for (int i = 0; i < 16; ++i) {
                if (doR) {
                    slewedPolyRhythm[v][i] = pickMono(sR, dotModular::STRAND_RHYTHM, i);
                    slewedPolyAccent[v][i] = pickMono(sA, dotModular::STRAND_ACCENT, i);
                }
                if (doM) {
                    slewedPolyMelody[v][i] = pickMono(sM, dotModular::STRAND_MELODY, i);
                    slewedPolyOctave[v][i] = pickMono(sO, dotModular::STRAND_OCTAVE, i);
                }
                if (doQ) {
                    slewedPolyQmix[v][i]   = pickMono(sQ, dotModular::STRAND_QMIX,   i);
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
            if (doQ) {
                qmixRandom[i]=slewedQmix[i];
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
                if (doQ) {
                    polyRandom(v, PL_QMIX)[i]=slewedPolyQmix[v][i];
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
        qmixMixApplied   = -999.f;
        recomputeEffectiveRhythm();
        recomputeEffectiveMelody();
        recomputeEffectiveQmix();
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
    float rhythmSlewLatched = 0.f, melodySlewLatched = 0.f, qmixSlewLatched = 0.f;
    float rhythmSlewApplied =-1.f, melodySlewApplied =-1.f, qmixSlewApplied =-1.f;  // force first recompute
    // Live MIX (A<->B blend) latched at control rate; the effective arrays are
    // recomputed when it changes. This is what drives the continuous morph.
    // A/B morph coefficient — GLOBAL per strand family (one scalar for ALL voices).
    // LOAD-BEARING INVARIANT: because this is global and the blend is linear, remapping
    // Change Alley pins at the slewed buffers (post-mix) is provably IDENTICAL to remapping
    // at the A/B candidates (pre-mix): both give A[src] + s*(B[src]-A[src]). If this ever
    // becomes PER-VOICE, that equivalence breaks — post-mix would apply the SOURCE's mix
    // while "own manipulation" demands the CONSUMER's — and the Change Alley remap must
    // move to the A/B candidate buffers. See CHANGE_ALLEY_DESIGN.md §3.
    float rhythmMixLatched = 0.f, melodyMixLatched = 0.f, qmixMixLatched = 0.f;
    float rhythmMixApplied =-1.f, melodyMixApplied =-1.f, qmixMixApplied =-1.f;
    // Scrub recompute guard: also track the counter and slew that the last recompute used, so the
    // no-redraw refresh path recomputes ONLY when (mix, slew, counter) actually changed -- otherwise
    // it re-derived the full K-window every ~90Hz refresh for no reason (idle cost scaling with K).
    int64_t rhythmCtrApplied = INT64_MIN, melodyCtrApplied = INT64_MIN, qmixCtrApplied = INT64_MIN;

    // ── Slew output buffers (Option W) ────────────────────────────────────────
    // slew writes the A/B blend here (step-0 latched). The PUBLIC arrays above
    // (rhythmRandom[] etc.) are the FINAL vectors the sequencer reads:
    //   no Sands  → final = copy of slewedDraw (done when slew re-latches).
    //   Sands     → Sands reads slewedDraw, applies spread at control rate, and
    //               writes the result into the public/final arrays itself.
    float slewedRhythm[16]={}, slewedVariation[16]={}, slewedLegato[16]={}, slewedAccent[16]={};
    float slewedMelody[16]={}, slewedOctave[16]={};
    float slewedQmix[16]={};   // q-mix twin of slewedMelody
    float slewedPolyRhythm[15][16]={}, slewedPolyMelody[15][16]={}, slewedPolyOctave[15][16]={};
    float slewedPolyAccent[15][16]={};
    float slewedPolyQmix[15][16]={};   // q-mix twin of slewedPolyMelody
    // Published snapshots of the slewed buffers — coherent copies the UI thread reads. The audio
    // thread writes slewed* during recomputeEffective* (now ~116µs at r>0), then publishes a
    // snapshot here. Without this, Mono/Macro visuals read slewed* mid-rewrite → torn read →
    // flicker. East already reads published polySpreadEffective (immune). The copy is ~1µs (1k
    // floats), so the race window drops from ~116µs to ~1µs — practically eliminating flicker.
    float pubSlewedRhythm[16]={}, pubSlewedVariation[16]={}, pubSlewedLegato[16]={}, pubSlewedAccent[16]={};
    float pubSlewedMelody[16]={}, pubSlewedOctave[16]={}, pubSlewedQmix[16]={};
    float pubSlewedPolyRhythm[15][16]={}, pubSlewedPolyMelody[15][16]={}, pubSlewedPolyOctave[15][16]={};
    float pubSlewedPolyAccent[15][16]={}, pubSlewedPolyQmix[15][16]={};
    void publishSlewedRhythm() {
        for (int i=0;i<16;++i){ pubSlewedRhythm[i]=slewedRhythm[i]; pubSlewedVariation[i]=slewedVariation[i];
            pubSlewedLegato[i]=slewedLegato[i]; pubSlewedAccent[i]=slewedAccent[i];
            for(int v=0;v<15;++v){ pubSlewedPolyRhythm[v][i]=slewedPolyRhythm[v][i]; pubSlewedPolyAccent[v][i]=slewedPolyAccent[v][i]; } }
    }
    void publishSlewedMelody() {
        for (int i=0;i<16;++i){ pubSlewedMelody[i]=slewedMelody[i]; pubSlewedOctave[i]=slewedOctave[i];
            for(int v=0;v<15;++v){ pubSlewedPolyMelody[v][i]=slewedPolyMelody[v][i]; pubSlewedPolyOctave[v][i]=slewedPolyOctave[v][i]; } }
    }
    void publishSlewedQmix() {
        for (int i=0;i<16;++i){ pubSlewedQmix[i]=slewedQmix[i];
            for(int v=0;v<15;++v) pubSlewedPolyQmix[v][i]=slewedPolyQmix[v][i]; }
    }
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
    float qmixSource[16]      = {};   // q-mix twin of melodySource
    float polyRhythmSource[15][16] = {};
    float polyAccentSource[15][16] = {};
    float polyMelodySource[15][16] = {};
    float polyOctaveSource[15][16] = {};
    float polyQmixSource[15][16]   = {};   // q-mix twin of polyMelodySource

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
    float qmixSeedFloat    = 0.f;   // q-mix twin of melodySeedFloat
    bool  rhythmSeedPending = false;
    bool  melodySeedPending = false;
    bool  qmixSeedPending   = false;   // q-mix twin of melodySeedPending
    float rhythmSeedPendingFloat = 0.f;
    float melodySeedPendingFloat = 0.f;
    float qmixSeedPendingFloat   = 0.f;   // q-mix twin of melodySeedPendingFloat
    // Pending ROLL (dice press) — advance the RNG and redraw WITHOUT reseeding.
    // Distinct from a seed-pending, which reseeds for reproducibility. A dice
    // press should walk the RNG forward (A/B morph), not reset to a fixed seed
    // on every press. The TRIAL variants roll with A anchored (promoteToA=false)
    // so the user auditions candidates against a fixed A; the regular roll
    // promotes B→A (main mode), so A walks forward.
    bool  rhythmRollPending = false;
    bool  rhythmPendingLast = false, melodyPendingLast = false, qmixPendingLast = false; // Last* = invert dice dir this boundary
    bool  melodyRollPending = false;
    bool  qmixRollPending   = false;   // q-mix twin of melodyRollPending
    // Pending RESEED-ROLL — like a (main) roll but ALSO reseeds the RNG from a
    // fresh value, while keeping the A/B morph: promote B→A, reseed, draw fresh
    // B, no firstDraw. Used by the "Reseed on roll" option. Trial rolls never
    // use this — auditioning stays in a controlled space (no entropy injection).
    bool  rhythmReseedRollPending = false;
    bool  melodyReseedRollPending = false;
    bool  qmixReseedRollPending   = false;   // q-mix twin of melodyReseedRollPending
    float rhythmReseedRollFloat = 0.f;
    float melodyReseedRollFloat = 0.f;
    float qmixReseedRollFloat   = 0.f;   // q-mix twin of melodyReseedRollFloat
    // When true, the reseed-roll uses FULL 64-bit internal entropy (the float is
    // ignored). When false, it reseeds from the (lower-precision) CV-derived
    // float. CV seeds are intentionally low-precision (0..10V → seed); internal
    // reseeds get the full state space.
    bool  rhythmReseedRollFull = false;
    bool  melodyReseedRollFull = false;
    bool  qmixReseedRollFull   = false;   // q-mix twin of melodyReseedRollFull
    int   rhythmMode = 0;  // 0=dice, 1=realtime
    int   melodyMode = 0;
    int   qmixMode   = 0;   // q-mix twin of melodyMode

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
        bool    movedR = false, movedM = false, movedQ = false;
        float   rSeedBefore = 0.f, mSeedBefore = 0.f, qSeedBefore = 0.f;
        int64_t rCtrBefore  = 0,   mCtrBefore  = 0,   qCtrBefore  = 0;
        float   rSeedAfter  = 0.f, mSeedAfter  = 0.f, qSeedAfter  = 0.f;
        int64_t rCtrAfter   = 0,   mCtrAfter   = 0,   qCtrAfter   = 0;
    };
    DiceUndoCapture diceUndoPending;

    // First reroll after construction / new seed draws A=B (full strength,
    // preserves seed determinism); slew morph applies on subsequent rerolls.
    bool  rhythmFirstDraw = true;
    bool  melodyFirstDraw = true;
    bool  qmixFirstDraw   = true;   // q-mix twin of melodyFirstDraw

    // ── Mode switch cache ─────────────────────────────────────────────────────
    float cachedMelodySeedFloat  = 0.f;
    float cachedRhythmSeedFloat  = 0.f;
    float cachedQmixSeedFloat    = 0.f;   // q-mix twin of cachedMelodySeedFloat
    bool  melodySeedCached       = false;
    bool  rhythmSeedCached       = false;
    bool  qmixSeedCached         = false;   // q-mix twin of melodySeedCached
    float cachedMelodyPitchV[16] = {};
    bool  cachedRhythmPattern[16]= {};
    float cachedQmix[16]         = {};   // q-mix twin of cachedMelodyPitchV (raw q-mix draw cache)
    int   cachedMelodyStepIndex  = -1;
    int   cachedMelodyLastStepIndex = -1;
    int   cachedRhythmStepIndex  = -1;
    int   cachedRhythmLastStepIndex = -1;
    int   cachedQmixStepIndex    = -1;   // q-mix twin of cachedMelodyStepIndex
    int   cachedQmixLastStepIndex = -1;   // q-mix twin of cachedMelodyLastStepIndex
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
    // q-mix is a value lane (melody-like) with its OWN independent Philox stream keyed off
    // redDot::seed::STREAM_SOURCE_SELECT (=3) — decorrelated from rhythm/melody so "which notes"
    // (q-mix) varies independently of "where they interleave" (melody). Mirror of melodyPhilox.
    redDot::PhiloxRng rhythmPhilox, melodyPhilox, qmixPhilox;
    int64_t   rhythmDrawCtr = 0, melodyDrawCtr = 0, qmixDrawCtr = 0;   // signed: can go negative on reverse
    uint64_t  rhythmCursor  = 0, melodyCursor  = 0, qmixCursor  = 0;   // intra-draw position, reset per redraw

    // Reset the intra-draw cursor at the start of a redraw (called by redrawRhythm/
    // redrawMelody before any unit() calls so the draw maps to its chunk base).
    inline void beginRhythmDraw() { rhythmCursor = 0; }
    inline void beginMelodyDraw() { melodyCursor = 0; }
    inline void beginQmixDraw()   { qmixCursor   = 0; }   // q-mix twin of beginMelodyDraw
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
    inline void zeroQmixIndex()   { qmixDrawCtr   = 0; }   // q-mix twin of zeroMelodyIndex
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
    inline int qmixDrawDir() const {   // q-mix twin of melodyDrawDir
        int base = reverseActive ? -1 : +1;
        return qmixPendingLast ? -base : base;
    }

    inline void advanceRhythmDraw(int dir) { rhythmDrawCtr += (dir < 0 ? -1 : +1); }
    inline void advanceMelodyDraw(int dir) { melodyDrawCtr += (dir < 0 ? -1 : +1); }
    inline void advanceQmixDraw(int dir)   { qmixDrawCtr   += (dir < 0 ? -1 : +1); }

    // Seed a stream's Philox from the same 0..10 float (reseed → new key, counter
    // reset to 0 = sequence restarts) or from full entropy.
    // seed sites so seed/reseed events affect both engines identically.
    // Stream separation via redDot::seed::deriveKey (PHILOX_KEY_DERIVATION_AND_CA_SEED.md
    // Finding 1 fix): the SAME seed float now yields INDEPENDENT rhythm and melody keys
    // (STREAM_RHYTHM=+0, STREAM_MELODY=+1). Previously both used the identical derivation,
    // so patching the single SEED input collapsed the two streams into one.
    inline void seedRhythmPhilox(float seedFloat) {
        rhythmPhilox.seed64(redDot::seed::deriveKey(seedFloat, redDot::seed::STREAM_RHYTHM));
        rhythmDrawCtr = 0; rhythmDrawCache_.clear();   // key+counter changed → cached draws stale
    }
    inline void seedMelodyPhilox(float seedFloat) {
        melodyPhilox.seed64(redDot::seed::deriveKey(seedFloat, redDot::seed::STREAM_MELODY));
        melodyDrawCtr = 0; melodyDrawCache_.clear();
    }
    // q-mix twin of seedMelodyPhilox — its OWN independent stream via STREAM_SOURCE_SELECT (=3),
    // so the same seed float yields a q-mix key decorrelated from rhythm/melody/CA.
    inline void seedQmixPhilox(float seedFloat) {
        qmixPhilox.seed64(redDot::seed::deriveKey(seedFloat, redDot::seed::STREAM_SOURCE_SELECT));
        qmixDrawCtr = 0; qmixDrawCache_.clear();
    }
    inline void seedRhythmPhiloxFull() { rhythmPhilox.seed64(rack::random::u64()); rhythmDrawCtr = 0; rhythmDrawCache_.clear(); }
    inline void seedMelodyPhiloxFull() { melodyPhilox.seed64(rack::random::u64()); melodyDrawCtr = 0; melodyDrawCache_.clear(); }
    inline void seedQmixPhiloxFull()   { qmixPhilox.seed64(rack::random::u64());   qmixDrawCtr   = 0; qmixDrawCache_.clear();   }

    inline float philoxRhythm() {
        uint64_t base = (uint64_t)(rhythmDrawCtr) * DRAW_CHUNK + rhythmCursor++;
        return rhythmPhilox.atUniform(base);
    }
    inline float philoxMelody() {
        uint64_t base = (uint64_t)(melodyDrawCtr) * DRAW_CHUNK + melodyCursor++;
        return melodyPhilox.atUniform(base);
    }
    inline float philoxQmix() {   // q-mix twin of philoxMelody
        uint64_t base = (uint64_t)(qmixDrawCtr) * DRAW_CHUNK + qmixCursor++;
        return qmixPhilox.atUniform(base);
    }

    inline float unitRhythm() { return philoxRhythm(); }
    inline float unitMelody() { return philoxMelody(); }
    inline float unitQmix()   { return philoxQmix(); }

    inline float philoxRhythmAt(int64_t pos, uint64_t cursor) const {
        return rhythmPhilox.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
    }
    inline float philoxMelodyAt(int64_t pos, uint64_t cursor) const {
        return melodyPhilox.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
    }
    inline float philoxQmixAt(int64_t pos, uint64_t cursor) const {   // q-mix twin of philoxMelodyAt
        return qmixPhilox.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
    }
    struct RhythmDraw { float rhythm[16], variation[16], legato[16], accent[16];
                        float polyRhythm[15][16], polyAccent[15][16]; };
    struct MelodyDraw { float melody[16], octave[16];
                        float polyMelody[15][16], polyOctave[15][16]; };
    // q-mix twin of MelodyDraw — one value per step (mono) + per poly voice.
    struct QmixDraw   { float qmix[16];
                        float polyQmix[15][16]; };
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
    // q-mix twin of rawDrawMelodyPatternAt — one mono value + 15 poly values per step.
    inline void rawDrawQmixPatternAt(int64_t pos, QmixDraw& d) const {
        uint64_t c = 0;
        for (int i = 0; i < 16; ++i) {
            d.qmix[i]=philoxQmixAt(pos,c++);
            for (int v=0;v<15;++v) d.polyQmix[v][i]=philoxQmixAt(pos,c++);
        }
    }
    // ── Phase 1 PhiInv cache ───────────────────────────────────────────────────
    // ~91% of patternXAt's cost is PhiInv (Acklam+Halley), not Philox. PhiInv(u) is a PURE
    // FUNCTION OF THE DRAW u, so caching it per (stream, pos) is reversal-neutral — the cached
    // value is identical whether pos is reached forward or backward, and re-derivable from
    // (key, pos) alone. Each cache entry holds the raw draw PLUS the per-value PhiInv, computed
    // once on miss; patternXAt then gathers K PhiInv windows and calls applyZ (no PhiInv in the
    // hot loop). Seed clears the map (counter → 0 invalidates every old pos). Unbounded across a
    // session's walk — accepted per the chosen design (pure-fn-of-pos, hit under scrub/reverse).
    struct CachedRhythmDraw {
        RhythmDraw draw;
        double zRhythm[16], zVariation[16], zLegato[16], zAccent[16];
        double zPolyRhythm[15][16], zPolyAccent[15][16];
    };
    struct CachedMelodyDraw {
        MelodyDraw draw;
        double zMelody[16], zOctave[16], zPolyMelody[15][16], zPolyOctave[15][16];
    };
    struct CachedQmixDraw {
        QmixDraw draw;
        double zQmix[16], zPolyQmix[15][16];
    };
    // mutable: patternXAt is const (read from const engine refs in places) but the cache is a
    // pure-fn-of-pos memo (no observable state change) — so const-correctness is preserved.
    mutable std::unordered_map<int64_t, CachedRhythmDraw> rhythmDrawCache_;
    mutable std::unordered_map<int64_t, CachedMelodyDraw> melodyDrawCache_;
    mutable std::unordered_map<int64_t, CachedQmixDraw>   qmixDrawCache_;
    // Returns the cached entry for `pos`, computing+inserting on miss.
    CachedRhythmDraw& cachedRhythmDraw(int64_t pos) const {
        auto it = rhythmDrawCache_.find(pos);
        if (it != rhythmDrawCache_.end()) return it->second;
        CachedRhythmDraw& e = rhythmDrawCache_[pos];
        rawDrawRhythmPatternAt(pos, e.draw);
        for (int i=0;i<16;++i){ e.zRhythm[i]=redDot::copula::PhiInv(e.draw.rhythm[i]);
            e.zVariation[i]=redDot::copula::PhiInv(e.draw.variation[i]);
            e.zLegato[i]=redDot::copula::PhiInv(e.draw.legato[i]);
            e.zAccent[i]=redDot::copula::PhiInv(e.draw.accent[i]);
            for(int v=0;v<15;++v){ e.zPolyRhythm[v][i]=redDot::copula::PhiInv(e.draw.polyRhythm[v][i]);
                e.zPolyAccent[v][i]=redDot::copula::PhiInv(e.draw.polyAccent[v][i]); } }
        return e;
    }
    CachedMelodyDraw& cachedMelodyDraw(int64_t pos) const {
        auto it = melodyDrawCache_.find(pos);
        if (it != melodyDrawCache_.end()) return it->second;
        CachedMelodyDraw& e = melodyDrawCache_[pos];
        rawDrawMelodyPatternAt(pos, e.draw);
        for (int i=0;i<16;++i){ e.zMelody[i]=redDot::copula::PhiInv(e.draw.melody[i]);
            e.zOctave[i]=redDot::copula::PhiInv(e.draw.octave[i]);
            for(int v=0;v<15;++v){ e.zPolyMelody[v][i]=redDot::copula::PhiInv(e.draw.polyMelody[v][i]);
                e.zPolyOctave[v][i]=redDot::copula::PhiInv(e.draw.polyOctave[v][i]); } }
        return e;
    }
    CachedQmixDraw& cachedQmixDraw(int64_t pos) const {
        auto it = qmixDrawCache_.find(pos);
        if (it != qmixDrawCache_.end()) return it->second;
        CachedQmixDraw& e = qmixDrawCache_[pos];
        rawDrawQmixPatternAt(pos, e.draw);
        for (int i=0;i<16;++i){ e.zQmix[i]=redDot::copula::PhiInv(e.draw.qmix[i]);
            for(int v=0;v<15;++v) e.zPolyQmix[v][i]=redDot::copula::PhiInv(e.draw.polyQmix[v][i]); }
        return e;
    }
    // ── Phase 1: slew as a normal-space moving-average Gaussian copula ──────────
    // Replaces the B2 L1 linear-uniform window (which collapsed variance to ~1/K at low slew —
    // the AVERAGE_POLY failure mode). The window is now K = MovingAverageCopula::K taps over the
    // SAME directly-addressable raw draws pos..pos-K+1 (no carried state, pure fn of pos →
    // reversible, exactly as the 7-tap loop was). The summation is the copula's:
    //   z = Σ_j w_j(r)·PhiInv(u_{pos-j}),  Σ w² = 1,  out = Phi(z)  → EXACTLY uniform for every r.
    //
    // KNOB MAPPING (inverted): the slew KNOB is 1 = sharp/raw (today's single-draw), 0 = smooth.
    // Slew knob is bipolar (-1..+1), matching spread's polarity landmarks:
    //   +1 → r = +R_MAX (max positive correlation = smooth/sustained)
    //    0 → r =  0     (independent = raw draw, bit-identity)
    //   -1 → r = -R_MAX (max negative correlation = hocket/interlock)
    // r = slewKnob · R_MAX (direct, no inversion). See docs/design/SLEW_COPULA_PLAN.md.
    static constexpr int SCRUB_K = 6;   // the SCRUB span (Phase 2); NOT the copula K (64). Kept for the scrub callers.
    // r from a bipolar slew knob value: direct mapping, clamped to [-R_MAX, R_MAX]. Non-finite → 0.
    static inline float slewKnobToR(float slewKnob) {
        if (!(slewKnob >= -1.f && slewKnob <= 1.f)) return 0.f;
        return slewKnob * (float)redDot::MovingAverageCopula::R_MAX;
    }
    // patternXAt: gather K cached draws (pos..pos-K+1, newest first) and apply the copula in
    // normal space. r==0 (slew knob = 1) short-circuits to the raw draw u[0] BITWISE (the
    // bit-identity guarantee) — never routed through PhiInv/Phi. Full K-term recompute each call
    // (never a running sum); the cache memoises the per-draw PhiInv, which is the ~91% cost.
    inline void patternRhythmAt(int64_t pos, float slew, RhythmDraw& out) const {
        const float r = slewKnobToR(slew);
        constexpr std::size_t K = redDot::MovingAverageCopula::K;
        if (r == 0.f) {                   // r == 0: raw draw at pos, bitwise (no PhiInv)
            const CachedRhythmDraw& e = cachedRhythmDraw(pos);
            out = e.draw;
            return;
        }
        // Gather K cached entries' PhiInv windows, newest first.
        const CachedRhythmDraw* win[K];
        for (std::size_t j = 0; j < K; ++j) win[j] = &cachedRhythmDraw(pos - (int64_t)j);
        double zw[K];
        for (int i=0;i<16;++i){
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zRhythm[i];
            out.rhythm[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zVariation[i];
            out.variation[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zLegato[i];
            out.legato[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zAccent[i];
            out.accent[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=0;v<15;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zPolyRhythm[v][i];
                out.polyRhythm[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zPolyAccent[v][i];
                out.polyAccent[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            }
        }
    }
    inline void patternMelodyAt(int64_t pos, float slew, MelodyDraw& out) const {
        const float r = slewKnobToR(slew);
        constexpr std::size_t K = redDot::MovingAverageCopula::K;
        if (r == 0.f) { const CachedMelodyDraw& e = cachedMelodyDraw(pos); out = e.draw; return; }
        const CachedMelodyDraw* win[K];
        for (std::size_t j = 0; j < K; ++j) win[j] = &cachedMelodyDraw(pos - (int64_t)j);
        double zw[K];
        for (int i=0;i<16;++i){
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zMelody[i];
            out.melody[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zOctave[i];
            out.octave[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=0;v<15;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zPolyMelody[v][i];
                out.polyMelody[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zPolyOctave[v][i];
                out.polyOctave[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            }
        }
    }
    // q-mix twin of patternMelodyAt — normal-space K-window copula over pos..pos-K+1.
    inline void patternQmixAt(int64_t pos, float slew, QmixDraw& out) const {
        const float r = slewKnobToR(slew);
        constexpr std::size_t K = redDot::MovingAverageCopula::K;
        if (r == 0.f) { const CachedQmixDraw& e = cachedQmixDraw(pos); out = e.draw; return; }
        const CachedQmixDraw* win[K];
        for (std::size_t j = 0; j < K; ++j) win[j] = &cachedQmixDraw(pos - (int64_t)j);
        double zw[K];
        for (int i=0;i<16;++i){
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zQmix[i];
            out.qmix[i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=0;v<15;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zPolyQmix[v][i];
                out.polyQmix[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            }
        }
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

    // q-mix twin of redrawMelody — advance/begin the q-mix draw, recompute effective, cache source.
    void redrawQmix(const PatternInput& in);

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
    // q-mix mix/slew latched alongside melody (melody family; own stream). applyQmix gates it
    // independently, mirroring applyMelody. Callers that pre-date q-mix pass the melody defaults.
    void latchMix(float rhythmMix, float melodyMix, float qmixMix,
                  float rhythmSlew, float melodySlew, float qmixSlew,
                  bool applyRhythm = true, bool applyMelody = true, bool applyQmix = true);
    void recomputeEffectiveRhythm();   // public[] = A + rhythmMixLatched*(B-A)
    void recomputeEffectiveMelody();   // public[] = A + melodyMixLatched*(B-A)
    void recomputeEffectiveQmix();     // q-mix twin of recomputeEffectiveMelody

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

    /// q-mix twin of setPendingMelodySeed
    void setPendingQmixSeed(float seedValue) {
        qmixSeedPendingFloat = seedValue;
        qmixSeedPending = true;
    }

    /// Arm a rhythm ROLL (dice press) — redraw from the advancing RNG at the next
    /// phrase boundary WITHOUT reseeding. This is the normal dice action.
    void setPendingRhythmRoll() { rhythmRollPending = true; rhythmPendingLast = false; }
    /// Arm a melody ROLL (dice press) — redraw without reseeding.
    void setPendingMelodyRoll() { melodyRollPending = true; melodyPendingLast = false; }
    /// q-mix twin of setPendingMelodyRoll.
    void setPendingQmixRoll() { qmixRollPending = true; qmixPendingLast = false; }

    // LAST-DICE: a roll that steps the draw index the OTHER way at the next boundary —
    // "give me the previous draw." Enabled by Philox addressability. BLOCKED on a
    // reversible stream: there the index↔phase coupling IS the reproducibility contract,
    // and a manual index step (independent of phase) would silently void it. So Last* is
    // a Normal-mode navigation gesture only — same principle that blocks trial/reseed-on
    // -roll (audition/reversible-mode gating removed under the scrub model).
    void setPendingRhythmLastRoll()  { rhythmRollPending = true; rhythmPendingLast = true; }
    void setPendingMelodyLastRoll()  { melodyRollPending = true; melodyPendingLast = true; }
    /// q-mix twin of setPendingMelodyLastRoll.
    void setPendingQmixLastRoll()    { qmixRollPending = true; qmixPendingLast = true; }

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
    /// q-mix twin of setPendingMelodyReseedRoll.
    void setPendingQmixReseedRoll(float seedValue, bool full) { qmixReseedRollFloat = seedValue; qmixReseedRollFull = full; qmixReseedRollPending = true; }

    /// Check if a rhythm dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isRhythmSeedPending() const { return rhythmSeedPending || rhythmRollPending || rhythmReseedRollPending; }

    /// Check if a melody dice action (seed OR roll OR trial OR reseed-roll) is pending.
    bool isMelodySeedPending() const { return melodySeedPending || melodyRollPending || melodyReseedRollPending; }

    /// q-mix twin of isMelodySeedPending.
    bool isQmixSeedPending() const { return qmixSeedPending || qmixRollPending || qmixReseedRollPending; }
    
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
