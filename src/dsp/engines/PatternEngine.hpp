#pragma once
/**
 * PatternEngine.hpp
 * Stochastic pattern generation for MeloDicer.
 *
 * Owns:
 *   - Both RNG states (rhythm + melody)
 *   - Generated pattern arrays (melodySemitone, melodyPitchV; rhythmPattern RETIRED —
 *     CC_PROMPT_RHYTHMPATTERN_DELETE_AND_POLARITY ITEM 1, the step LEDs read the live
 *     warp row now)
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
#include <array>   // P-B: std::array member state (ARRAY_CONTAINER_AUDIT)
#include "../NoteValues.hpp"   // NUM_NOTE_VALUES + NOTE_VALUES (varyNoteIndex's index domain — DOTTED BUILD)
#include <unordered_map>
#include <utility>
#include "../PhiloxRng.hpp"
#include "../LaneMapping.hpp"   // dotModular::STRAND_* for finalRandomByStrand
#include "../VoiceLaneTypes.hpp"   // VoiceSlot/LaneId — strong per-voice/lane indices (typed overloads below)
#include "../MovingAverageCopula.hpp"   // Phase 1: slew as normal-space moving-average copula (replaces L1 linear-uniform window)
#include "../../tuning/TuningTable.hpp"   // dotModular::TuningTable — shared per-degree tuning (Sikit)

// CC_PROMPT_03 (warp mirror): the per-voice warp bank. Full definition lives in
// ../WarpF.hpp — included by PatternEngine.cpp (and tests) only, keeping this header
// light; the pointer member below needs just the forward declaration.
namespace dotModular { struct WarpBank; }

template<typename T>
static inline T pe_clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }

// ── STEPS_32 Stage 1 (docs/design/STEPS_32_PLAN.md BUILD PLAN; CC_PROMPT_32_STEPS_STAGE1_STRUCTURAL) ──
// kMaxPatternSteps: the pattern-length CEILING the 32-step build targets (one mode, no switch —
// the MODE LAW). The length control's range derives from it (MonsoonConfigurator
// PATTERN_LENGTH_PARAM); do NOT scatter 32.
// kStepStorage: the per-step STORAGE bound — the row dimension of every per-step array and the
// width the mask words reserve for (STEPS_32 Stage 2a names it; 16 → 32 there, before any
// behaviour change). Storage is the MEMORY: entries beyond the usable length stay zero-filled
// and unplayed.
// kPatternLengthCap: the USABLE length bound AND the frame RING the walk/masks land on.
// STAGE 2b FLIPPED (this commit): the ring is 32 — the flip moved the ring, the mask plane
// (lenMask = (1<<ring)-1), the knob ranges (the PR-#310-review close-out: the param ranges
// derive from the cap) and every length-bearing site in ONE move, with the pre-gates green
// (GATE 1's EQUIV section: zero ≤16 differences against the committed 2a-tip oracle; D3's
// documented shift — a window that wrapped the 16-ring now EXTENDS — is part of the flip).
// No other site changes. Every length-bearing site derives from these three (the audit's
// class-A fixes); class-B storage keeps its step dim behind kStepStorage; class-C display /
// class-D deprecated / class-E unrelated-16 stay untouched.
namespace dotModular {
static constexpr int kMaxPatternSteps  = 32;   // the ceiling: pattern/phrase length accepts 1..32
static constexpr int kStepStorage      = 32;   // the per-step array row width (Stage 2a; uint64 masks reserve 64)
static constexpr int kPatternLengthCap = kMaxPatternSteps;   // STAGE 2b: FLIPPED to the ceiling (the ring + the usable bound)
static_assert(kPatternLengthCap <= kMaxPatternSteps, "the usable length cap never exceeds the ceiling");
static_assert(kPatternLengthCap <= kStepStorage,     "the ring never exceeds the per-step storage");
static_assert(kStepStorage <= 64,                    "the storage fits the uint64 length-masked mask words (the DECIDED block)");
// ── STEPS_32 Stage 2a (the DECIDED block in STEPS_32_PLAN.md): the mask plane is
// uint64_t, LENGTH-MASKED on the frame RING. One AUTHORITY here beside the length
// domain; SequencerEngine / Monsoon / the visuals derive (no second definition).
// The ring derives from kPatternLengthCap — 32 since the Stage 2b flip (before it every
// ≤16 configuration was bit-identical, pinned by GATE 1's EQUIV section against the
// committed 2a-tip oracle); the flip widened the ring, lenMask and the
// all-on sentinel in ONE move (uint64 covers the 64-step ceiling, no second
// widening). lenMask = (1<<ring)-1 replaces the 65535 clamp, the c<16 checks and
// the 1<<c magic; the all-on default is the RING's all-on (0xFFFFFFFF at the 32 bound).
static constexpr int      kMaskRing   = kPatternLengthCap;
static constexpr uint64_t kMaskLenMask = (kMaskRing >= 64) ? ~0ULL : ((1ULL << kMaskRing) - 1ULL);
}  // namespace dotModular

// The length/storage/mask domain, visible UNQUALIFIED to the engine layer (the audit's
// one-spelling law — no per-site dotModular:: scatter inside the engines; outboard code
// keeps the dotModular:: qualification).
using dotModular::kMaxPatternSteps;
using dotModular::kStepStorage;
using dotModular::kPatternLengthCap;
using dotModular::kMaskRing;
using dotModular::kMaskLenMask;

// ── Input snapshot — filled by MeloDicer::process() each block ────────────────
// All knob/CV values are pre-read so PatternEngine is Rack-port-free.
struct PatternInput {
    // Per-DEGREE weights 0..1 (with CV applied). Sized MAXN (24) for Micro-24 (Phase 3); only the
    // first tuning.N entries are read (pickSemitone is N-bounded). At N=12 this is the legacy 12-fader
    // mask, byte-identical (the tail 12..23 stays 0 and is never summed).
    float semiWeights[dotModular::TuningTable::MAXN] = {};
    float restProb         = 0.1f;
    float variationAmount  = 0.5f;
    // LOCK Phase 2 (LOCK_SEMANTICS §9): mono BigFive NOTE_VALUE staged on the
    // snapshot so it LATCHES like restProb/variationAmount. NOTE_VALUE was previously passed
    // live at the executeMode call sites. LEGATO/ACCENT/QMIX were staged here too, but Phase A
    // collapsed them: they now live on the mono slot (V1 = voice 0), written by the controller
    // in updatePatternInput — NOT transported through PatternInput.
    // restProb and variationAmount remain here because PatternEngine reads them for
    // pattern generation (snapshotPreRemap, varyNoteIndex, etc.) — not just the step cascade.
    float noteValue        = 4.f;   // mono note-value INDEX 0..11 (4 = 1/4 note; DOTTED BUILD: was 2 pre-interleave)
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

// P-B (ARRAY_CONTAINER_AUDIT): 2-D member alias — std::array<std::array<T, Cols>, Rows>,
// byte-identical to T[Rows][Cols] (Finding D keeps every sizeof-memcpy compiling),
// readable call sites.
template <class T, size_t Rows, size_t Cols> using Matrix2 = std::array<std::array<T, Cols>, Rows>;

// ── PatternEngine ─────────────────────────────────────────────────────────────
struct PatternEngine {

    // P-A (ARRAY_CONTAINER_AUDIT Finding A): the ONE spelling of the voice capacity (mono +
    // 15 poly). Class-local like SequencerEngine's (VoiceResolver.hpp cannot be included here —
    // it includes SequencerEngine.hpp, which includes this file); VoiceResolver's static_assert
    // holds the equality. The STEP capacity is a DIFFERENT 16 (the row length — audit class B,
    // storage): it stays literal until Stage 2 widens it; the length CEILING/USABLE-CAP pair
    // kMaxPatternSteps / kPatternLengthCap above is STEPS_32 Stage 1's naming of the length domain.
    static constexpr int kVoiceSlots = 16;

    // Poly engine lane constants (mirror SequencerEngine::PolyLane) for polyRandom callers in this
    // Lane-index unification: PL_* now matches the VISUAL/EDITOR/STRAND order
    // (MEL=0,OCT=1,QMIX=2,REST=3,ACC=4,VAR=5,LEG=6). Editor==engine==strand.
    // (P-A: hoisted above the members that size lane dims with PL_LANES.)
    enum PolyLane { PL_MELODY = 0, PL_OCTAVE = 1, PL_QMIX = 2, PL_REST = 3, PL_ACCENT = 4, PL_VARIATION = 5, PL_LEGATO = 6, PL_LANES = 7 };

    // ── Unified probability storage (item 4): ONE array for all 16 voices × 7 editor lanes × 16 steps,
    // replacing the separate mono named arrays (rhythmRandom…octaveRandom) and poly polyXRandom[15][16].
    // Indexed [voiceSlot][editorLane][step] — the SAME convention as lorStore_/spread (slot 0 = V1/mono,
    // slots 1..15 = V2..V16; editor lanes MEL=0,OCT=1,QMIX=2,REST=3,ACC=4,VAR=5,LEG=6; VAR/LEG poly-unused).
    // The 7 mono named arrays below are REFERENCE VIEWS onto random_[0][lane] so the ~160 existing
    // rhythmRandom[step] / std::rotate / whole-array sites keep working unchanged; poly access goes via
    // polyRandom(voice,lane). (mono row lane == strand index — identity.)
    std::array<std::array<std::array<float, kStepStorage>, dotModular::NUM_STRANDS>, kVoiceSlots> random_ = {};

    // ── Stage 2a spread cache: per-voice blended spread amount (from combineSpread),
    // cached at control rate for step-rate refresh. [voice][engineLane]. The engine
    // reads polyRandom at step advance via refreshPolyRandomCell, which applies this
    // cached spread to the current step only — NOT all 16 steps at control rate.
    // cachedSpreadLiveR/M/Q mirror the lock axes: when false, refresh skips (frozen).
    std::array<std::array<float, PL_LANES>, kVoiceSlots> cachedPolySpread = {};
    bool cachedSpreadLiveR = true;   // rhythm axis (REST/ACC/VAR/LEG)
    bool cachedSpreadLiveM = true;   // melody axis (MEL/OCT)
    bool cachedSpreadLiveQ = true;   // QMIX own axis (SB_SANDS_Q)
    // False until sync()/processDNA() populates the cache. When false, refreshPolyRandomCell
    // skips (leaves polyRandom at whatever publishSlewed* or the test set). Set true in sync().
    bool cachedSpreadInitialized = false;

    // ── Mono output views (read by MeloDicer, never written externally) — bound to random_[0][lane].
    std::array<float, kStepStorage>& melodyRandom = random_[0][dotModular::STRAND_MELODY];
    std::array<float, kStepStorage>& octaveRandom = random_[0][dotModular::STRAND_OCTAVE];
    std::array<float, kStepStorage>& qmixRandom      = random_[0][dotModular::STRAND_QMIX];
    std::array<float, kStepStorage>& rhythmRandom = random_[0][dotModular::STRAND_RHYTHM];
    std::array<float, kStepStorage>& accentRandom = random_[0][dotModular::STRAND_ACCENT];  // accent strand probabilities
    std::array<float, kStepStorage>& variationRandom = random_[0][dotModular::STRAND_VARIATION];
    std::array<float, kStepStorage>& legatoRandom = random_[0][dotModular::STRAND_LEGATO];

    // q-mix ordering asserts (Task 4a): the q-mix Philox stream key MUST be STREAM_SOURCE_SELECT (=3),
    // and the PL_QMIX poly lane MUST round-trip to editor lane 2 (STRAND_QMIX). If either the RNG
    // stream order or the poly-lane order is renumbered, these fire at compile time.
    static_assert(dotModular::QMIX_STREAM_KEY == redDot::seed::STREAM_SOURCE_SELECT,
                  "q-mix stream key must equal redDot::seed::STREAM_SOURCE_SELECT (=3)");
    static_assert((int)PL_QMIX == (int)dotModular::STRAND_QMIX,
                  "PL_QMIX poly lane must map to editor lane STRAND_QMIX (=2)");

    // STRONG typed indices (CC_PROMPT_STRONG_TYPES): the typed polyRandom/warpRow
    // overloads take MINTED indices — VoiceSlot (via VoiceResolver::mono()/slot(V)/
    // allSlots()) and LaneId (explicit construction, e.g. LaneId{PL_MELODY}). The
    // bare-int versions are MIGRATION shims; the tighten commit deletes them.
    using VoiceSlot = dotModular::VoiceSlot;
    using LaneId    = dotModular::LaneId;
    static_assert((int)PL_LANES == dotModular::kLaneIdCount, "LaneId covers exactly the PL_* lanes");

    // Poly probability view: the caller passes a MINTED VoiceSlot (0=V1, 1..15=V2..V16) and an
    // explicit LaneId. Returns the 16-step row (float(&)[16]) so callers index [step].
    // CC_PROMPT_STRONG_TYPES TIGHTEN: the bare-int twins are DELETED — a raw voice/lane
    // index no longer compiles; every caller mints (VoiceResolver) / constructs LaneId.
    // (The legacy range guards are kept verbatim — minted values are always in range, but
    // the clamp is the old behaviour and costs nothing.)
    std::array<float, kStepStorage>& polyRandom(VoiceSlot vs, LaneId lane) {
        const int v = (vs.v >= 0 && vs.v < kVoiceSlots) ? vs.v : 0;
        const int l = (lane.l >= 0 && lane.l < PL_LANES) ? lane.l : 0;
        return random_[v][l];
    }
    const std::array<float, kStepStorage>& polyRandom(VoiceSlot vs, LaneId lane) const {
        const int v = (vs.v >= 0 && vs.v < kVoiceSlots) ? vs.v : 0;
        const int l = (lane.l >= 0 && lane.l < PL_LANES) ? lane.l : 0;
        return random_[v][l];
    }

    // ── Warp mirror (CC_PROMPT_03 STEP 2): the EFFECTIVE w-mixture over random_ ──
    // ONE array, no raw↔warped display toggle (SESSION_LOG 2026-10-18b §2 — the toggle
    // was dropped: the display ALWAYS shows the effective mix). Each cell =
    //     (1-w)·u + w·F⁻¹(u),   u = random_[v][lane][step]
    // computed in DOUBLE via WarpBank::warpedValue (exact invCDF — the mirror IS the
    // cache, no new LUT; the warp is a CACHED caller) and stored FLOAT (float32
    // monotonicity already pinned by test_warp_f_bank S7). random_ stays underneath
    // as the UNWARPED w=0 basis; warp composes AFTER slew on the settled uniform.
    // Every random_ writer triggers a rebuild of the cells it wrote, so the mirror is
    // consistent with random_ by construction — the single-source keystone that keeps
    // the sounding path (voice v) and the display (displayedVoice_) in agreement.
    std::array<std::array<std::array<float, kStepStorage>, dotModular::NUM_STRANDS>, kVoiceSlots> warped_ = {};

    // The bank the mirror mixes with (Monsoon::editor.warp, wired by the module layer —
    // the TuningTable "populated by the module layer" pattern). null ⇒ no warp anywhere
    // ⇒ consumers read raw random_ (bank-less hosts keep byte-identical legacy paths).
    const dotModular::WarpBank* warpBank_ = nullptr;
    // (v,lane) ⇒ the lane is DELEGATED to Macro (SandsTopology owner()==MACRO, mirrored
    // per control cycle by MonsoonSandsManager — the spreadTargetMode pattern). A
    // delegating cell mixes with Macro's GLOBAL curve (WarpBank's delegated form);
    // default false = the voice's own (v*7+lane) slot.
    std::array<std::array<bool, PL_LANES>, kVoiceSlots> warpDelegated_ = {};

    // Consumer read: the EFFECTIVE row — the mirror when a bank is wired, the raw row
    // otherwise. Same indexing/shape as polyRandom, so consumer call sites are
    // one-token swaps and the fallback needs no branching at the read site.
    // CC_PROMPT_STRONG_TYPES TIGHTEN: minted VoiceSlot + explicit LaneId ONLY — the
    // bare-int twin is DELETED (a bare-0 call no longer compiles; the mono role is
    // VoiceResolver::mono()). Legacy range guards kept verbatim.
    const std::array<float, kStepStorage>& warpRow(VoiceSlot v, LaneId lane) const {
        const int vv = (v.v >= 0 && v.v < kVoiceSlots) ? v.v : 0;
        const int ll = (lane.l >= 0 && lane.l < PL_LANES) ? lane.l : 0;
        return warpBank_ ? warped_[vv][ll] : random_[vv][ll];
    }
    // Wire the bank (module layer, per block — same-pointer is a no-op). First wire /
    // re-wire rebuilds the full mirror from current random_.
    void setWarpBank(const dotModular::WarpBank* b);
    // Delegation matrix entry (module layer; raw setter — the manager triggers the
    // rebuild when the matrix actually changes).
    void setWarpDelegated(int v, int lane, bool d) {
        if (v >= 0 && v < kVoiceSlots && lane >= 0 && lane < PL_LANES) warpDelegated_[v][lane] = d;
    }
    // ── The dirty mask (STEP 3): scoped, IMMEDIATE rebuilds. "Display rebuilds
    // immediately on edit" (§2) — the edit paths call the narrow scope directly;
    // the scope IS the mask, no deferred flags to flush. All no-op without a bank.
    //   rebuildWarpMirrorAll()                — regen / new draw / scrub / pin-remap:
    //                                             1792 cells, 0.064ms worst case
    //   rebuildWarpMirrorVoice(v)             — a whole voice row (7×16 = 112 cells):
    //                                             ForVoice recompute + the managers'
    //                                             mono-row spread passes, ~0.004ms
    //   rebuildWarpMirrorLaneDelegating(lane) — Macro curve/w edit: that lane across
    //                                             DELEGATING voices only (own-slot
    //                                             voices are untouched by a Macro edit)
    //   rebuildWarpMirrorVoiceLane(v, lane)   — Sands (East) edit on (v,lane): 16 cells
    //   rebuildWarpMirrorLane(lane)           — the distribution send-bus' Macro-sent
    //                                             delta on that lane: ALL 16 voices x
    //                                             lane (the cross-voice expander's
    //                                             global delta reaches delegated AND
    //                                             own cells — the one lane scope the
    //                                             Macro-edit scope can't express)
    //   rebuildWarpMirrorCell(v, lane, step)  — the step-rate spread-refresh companion:
    //                                             1 cell, ~0.0006ms
    // The scopes are for CLEANLINESS (don't recompute untouched voices), NOT
    // performance — the verified costs above are all trivial (the prompt's ruling).
    void rebuildWarpMirrorAll();
    void rebuildWarpMirrorVoice(int v);
    void rebuildWarpMirrorLaneDelegating(int lane);
    void rebuildWarpMirrorLane(int lane);
    void rebuildWarpMirrorVoiceLane(int v, int lane);
    void rebuildWarpMirrorCell(int v, int lane, int step);
    // The shared cell compute (CONTRACT: warpBank_ != nullptr — every rebuild entry
    // point checks before calling): DOUBLE mix → FLOAT store.
    float warpMixCell(int v, int lane, int step) const;
    // ── Warp DISPLAY composition (CC_PROMPT_04 feel-test bug 1) ──
    // The display twin of warpMixCell: compose the effective w-mixture over a
    // CALLER-SUPPLIED value (the spread-interpolated display value the bars compute)
    // instead of random_[v][lane][step]. Same per-(voice,lane) (F,w) resolution as the
    // mirror — it calls the SAME WarpBank::warpedValue(v, lane, u, delegated) overload
    // warpMixCell uses, including the Macro-global default vs own-slot delegation —
    // so a bar can never show a value the audio path doesn't consume. (CC_PROMPT_05:
    // callers compose EVERY lane — the per-lane MEL/OCT gate died with the uniform
    // plumbing; boolean/qmix lanes are inert warp slots at their identity defaults.)
    // No bank ⇒ u returned bit-identically (bank-less hosts unchanged).
    float warpDisplayMix(int v, int lane, double u) const;
    // ── The MacroOwn warp composition (CC_PROMPT_UI_TO_RESOLVER_FINISH2 BUG 1) ──
    // The Macro-own twin: the (F,w,tilt,atom) pair is ALWAYS Macro's GLOBAL cell
    // (delegated=TRUE semantics), never the per-(voice,lane) own-slot pair. WHY: the
    // own-slot pair (warpW/warpF/warpTilt/warpAtom*[v*7+lane]) is EAST-AUTHORABLE —
    // East's V1/poly tabs author those bank slots, and warpDelegated_[v][lane] is
    // false on an East-owned lane — so composing the MacroOwn lens through
    // warpDisplayMix let East's curve/w edits modulate Macro's bars and prob-outs on
    // East-owned lanes ("East STILL modulates Macro warp" — the one-layer-down leak:
    // the base layer was already clean, reading pubSlewedPoly*, which no East spread
    // pass writes). Macro never borrows East — in the warp layer too: this reads the
    // globalWarp{W,F,Tilt,AtomLevel,AtomMass}[lane] cells, which only Macro's panel
    // authors. No bank ⇒ u returned bit-identically (bank-less hosts unchanged).
    float warpDisplayMixMacroOwn(int v, int lane, double u) const;

    // Final post-everything (A/B-mix + spread + LOR feed in upstream) probability value for a given
    // ENGINE STRAND at a given step, 0..1. Now a direct index into random_[0] (mono row): strand index
    // IS the editor-lane column (identity), so no table/permutation. Out-of-range
    // falls back to rhythm (matches the old default:). Used by the Sands visual probability CV outs.
    // Spread target mode per lane (0=Anchor V1, 1=Follow CA). Mirrored from the Monsoon's
    // EditorState each control cycle so the spread path (which only has PatternEngine&) can
    // read it without a Monsoon pointer. SPREAD_TARGET_MODES.md.
    std::array<uint8_t, PL_LANES> spreadTargetMode = {0,0,0,0,0,0,0};   // 7 poly lanes (SANDS CONSOLIDATION Step 2)

    // ── Change Alley pin-matrix (CHANGE_ALLEY_DESIGN.md §3-REVISED, PRE-SPREAD) ──
    // The pins remap the SLEWED buffers (post A/B-mix, post-slew, PRE-spread) so that a
    // pinned voice's borrowed draw is then spread with the CONSUMER's own reference —
    // equivalent to pinning the A/B samples (the agreed design). random_ reads stay plain
    // own-bank. Row 0 = mono, rows 1..15 = poly V2..V16. Strand→pool: MELODY/OCTAVE =
    // melody pin; RHYTHM/ACCENT/VARIATION/LEGATO = rhythm pin. Identity = no-op.
    std::array<uint8_t, kVoiceSlots> caRhythmSrc = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    std::array<uint8_t, kVoiceSlots> caMelodySrc = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    // Q-mix source-select plane (QMIX_LANE_PARITY §"The blend"): the NEW green pin plane staged from
    // MonsoonChangeAlleyV2::qmixSrc. STRAND_QMIX's slewed buffer + the downstream per-voice blend
    // THRESHOLD ride THIS array (not caRhythmSrc), so a voice can consume another voice's q-mix
    // probability — CA parity for q-mix. Identity default = no-op (Straits per-voice level as before).
    std::array<uint8_t, kVoiceSlots> caQmixSrc = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    // ── Containment graph (CC_PROMPT_CANTONMENT_BUILD_PLAN.md §"THE STATE") ──
    // The ENGINE-SIDE AUTHORITATIVE home of the containment DAG (the CA seam mirrored:
    // PatternEngine owns the array; the Containment module is the store-backed editor/driver).
    // Bit u of containmentParents[v] set = "voice v is contained in voice u" — a MULTI-PARENT
    // ACYCLIC DAG (Rodney's R1 ruling; containment = the INTERSECTION of the parent
    // containments). Default all-zeros = the all-disconnected graph (R2) = the full-equality
    // case, so an absent module is a no-op exactly like CA's identity diagonal. Staged each
    // control block from the module's editor graph (MonsoonSandsManager::processDNA, the same
    // staging point as caRhythmSrc). The CONSUMER — the gate-clamp recursion in topological
    // order (gate(v) = clamp(own, ∩ resolved parents)) — is the future decision manager
    // (MONO_SPECIAL_IN_LOGIC_AUDIT.md); this array is the contract it will read. Acyclicity is
    // enforced upstream at every edit AND every transform (CantonmentGraph/Transforms); the
    // engine-side reader may assume a DAG unconditionally.
    std::array<uint16_t, kVoiceSlots> containmentParents = {};

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
        int src;
        if (strand == dotModular::STRAND_QMIX) src = (int)caQmixSrc[r];
        else {
            const bool mel = (strand == dotModular::STRAND_MELODY || strand == dotModular::STRAND_OCTAVE);
            src = mel ? (int)caMelodySrc[r] : (int)caRhythmSrc[r];
        }
        // Phase B: CA OOB = IDENTITY. If the resolved source is above the active voice
        // count (V1 + numPolyVoicesHint poly voices), revert to self (identity = no-op).
        // Square matrix: source & target OOB together, one check. Reversible: restore
        // when the index is back in bounds. Identity = own value (neutral, stateless).
        const int activeCount = numPolyVoicesHint + 1;  // V1 (always) + active poly
        if (src >= activeCount || r >= activeCount) return r;  // OOB → identity (self)
        return src;
    }
    // Per-voice q-mix source row for the downstream blend THRESHOLD (which voice's q-mix probability
    // consuming voice row reads). row 0 = mono/voice-0, rows 1..15 = poly V2..V16. Identity default.
    inline int caQmixSrcRow(int row) const {
        const int r = (row >= 0 && row < 16) ? row : 0;
        const int src = (int)caQmixSrc[r];
        const int activeCount = numPolyVoicesHint + 1;  // Phase B: OOB = identity
        if (src >= activeCount || r >= activeCount) return r;
        return src;
    }
    // Per-voice input-CV source row for the blend's QUANTISED-INPUT operand: it rides CA's MELODY
    // plane (QMIX_LANE_PARITY §"The blend" step 2 — "input CV = a CA melody source"), so a voice can
    // quantise another voice's input line. Identity default = each voice reads its own input CV.
    inline int caInputCvSrcRow(int row) const {
        const int r = (row >= 0 && row < 16) ? row : 0;
        const int src = (int)caMelodySrc[r];
        const int activeCount = numPolyVoicesHint + 1;  // Phase B: OOB = identity
        if (src >= activeCount || r >= activeCount) return r;
        return src;
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
    // COLLAPSE: preRemapSlewed*[kVoiceSlots][16] — V1 at [0], V2..V16 at [1..15]. One array per strand.
    std::array<std::array<float, kStepStorage>, kVoiceSlots> preRemapSlewedRhythm  = {}, preRemapSlewedVariation  = {}, preRemapSlewedLegato  = {}, preRemapSlewedAccent  = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> preRemapSlewedMelody  = {}, preRemapSlewedOctave  = {}, preRemapSlewedQmix  = {};
    void snapshotPreRemap() {
        for (int i=0;i<kStepStorage;++i){
            preRemapSlewedRhythm[0][i]=slewedRhythm[0][i]; preRemapSlewedVariation[0][i]=slewedVariation[0][i];
            preRemapSlewedLegato[0][i]=slewedLegato[0][i]; preRemapSlewedAccent[0][i]=slewedAccent[0][i];
            preRemapSlewedMelody[0][i]=slewedMelody[0][i]; preRemapSlewedOctave[0][i]=slewedOctave[0][i];
            preRemapSlewedQmix[0][i]=slewedQmix[0][i];
            for(int v=1;v<kVoiceSlots;++v){
                preRemapSlewedRhythm[v][i]=slewedRhythm[v][i];
                preRemapSlewedMelody[v][i]=slewedMelody[v][i];
                preRemapSlewedOctave[v][i]=slewedOctave[v][i];
                preRemapSlewedAccent[v][i]=slewedAccent[v][i];
                preRemapSlewedQmix[v][i]=slewedQmix[v][i];
                preRemapSlewedVariation[v][i]=slewedVariation[v][i];
                preRemapSlewedLegato[v][i]=slewedLegato[v][i];
            }
        }
    }
    // CC_PROMPT_STRONG_TYPES TIGHTEN: the remap's promote loops, extracted to the .cpp so
    // the poly writes can MINT VoiceSlots (the header cannot include VoiceResolver.hpp).
    // Byte-identical to the inline loops it replaces (same writes, same order).
    void promoteRemappedDraws_(bool doR, bool doM, bool doQ);
    // P-C (ARRAY_CONTAINER_AUDIT Finding B/C): the remap's poly snapshot moved from a
    // static thread_local to INSTANCE state — per-Monsoon, single-owner audio thread,
    // testable per instance (the TLS form was hidden shared state across every Monsoon
    // on the worker; ~7 KB/instance against ~90 KB resident is not an objection).
    Matrix2<float, kVoiceSlots, kStepStorage> remapSnapR{}, remapSnapM{}, remapSnapO{}, remapSnapQ{}, remapSnapA{}, remapSnapV{}, remapSnapL{};

    void remapSlewedByPins(bool doR = true, bool doM = true, bool doQ = true) {
        snapshotPreRemap();   // always snapshot, even on identity skip (pre == post then)
        // Fast identity skip — only over the families we would actually remap.
        bool identity = true;
        for (int v = 0; v < kVoiceSlots && identity; ++v) {
            if (doR && caRhythmSrc[v] != v) identity = false;
            if (doM && caMelodySrc[v] != v) identity = false;
            if (doQ && caQmixSrc[v]   != v) identity = false;
        }
        if (identity) return;

        // Snapshot mono (V1) + poly (V2..V16) for the seven strands. VoiceSlot convention: 0=V1, 1..15=V2..V16.
        std::array<float, kStepStorage> mR{}, mM{}, mO{}, mQ{}, mA{}, mV{}, mL{};   // P-C: zero-init scratch
        for (int i = 0; i < kStepStorage; ++i) {
            mR[i]=slewedRhythm[0][i]; mM[i]=slewedMelody[0][i]; mO[i]=slewedOctave[0][i];
            mQ[i]=slewedQmix[0][i];
            mA[i]=slewedAccent[0][i]; mV[i]=slewedVariation[0][i]; mL[i]=slewedLegato[0][i];
        }
        auto& pR = remapSnapR; auto& pM = remapSnapM; auto& pO = remapSnapO;
        auto& pQ = remapSnapQ; auto& pA = remapSnapA; auto& pV = remapSnapV; auto& pL = remapSnapL;
        for (int v = 1; v < kVoiceSlots; ++v) for (int i = 0; i < kStepStorage; ++i) {
            pR[v][i]=slewedRhythm[v][i]; pM[v][i]=slewedMelody[v][i];
            pO[v][i]=slewedOctave[v][i]; pQ[v][i]=slewedQmix[v][i];
            pA[v][i]=slewedAccent[v][i];
            pV[v][i]=slewedVariation[v][i]; pL[v][i]=slewedLegato[v][i];
        }
        // src row → (mono buffer if 0, else poly buffer [srcRow]) for a given strand family.
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
            const int v = srcRow;  // srcRow is voiceSlot (1=V2..15=V16)
            switch (strand) {
                case dotModular::STRAND_MELODY: return pM[v][i];
                case dotModular::STRAND_OCTAVE: return pO[v][i];
                case dotModular::STRAND_QMIX:   return pQ[v][i];
                case dotModular::STRAND_RHYTHM: return pR[v][i];
                case dotModular::STRAND_ACCENT: return pA[v][i];
                // VAR/LEG are now full poly+spread lanes — read the per-voice snapshot.
                case dotModular::STRAND_VARIATION: return pV[v][i];
                case dotModular::STRAND_LEGATO:    return pL[v][i];
                default:                           return 0.5f;  // fallback
            }
        };
        // Mono row 0 — rhythm family (RHYTHM/ACCENT/VAR/LEG) gated by doR, melody family (MEL/OCT) by doM.
        // (Per-step COMPUTE rides the RING — kPatternLengthCap; only whole-row MEMORY copies ride
        // kStepStorage. Beyond-ring cells stay zero-filled — the Stage-2 gate.)
        for (int i = 0; i < kPatternLengthCap; ++i) {
            if (doR) {
                slewedRhythm[0][i]    = pickMono(caSrcRow(0, dotModular::STRAND_RHYTHM),    dotModular::STRAND_RHYTHM,    i);
                slewedAccent[0][i]    = pickMono(caSrcRow(0, dotModular::STRAND_ACCENT),    dotModular::STRAND_ACCENT,    i);
                slewedVariation[0][i] = pickMono(caSrcRow(0, dotModular::STRAND_VARIATION), dotModular::STRAND_VARIATION, i);
                slewedLegato[0][i]    = pickMono(caSrcRow(0, dotModular::STRAND_LEGATO),    dotModular::STRAND_LEGATO,    i);
            }
            if (doM) {
                slewedMelody[0][i] = pickMono(caSrcRow(0, dotModular::STRAND_MELODY), dotModular::STRAND_MELODY, i);
                slewedOctave[0][i] = pickMono(caSrcRow(0, dotModular::STRAND_OCTAVE), dotModular::STRAND_OCTAVE, i);
            }
            if (doQ) {
                slewedQmix[0][i]   = pickMono(caSrcRow(0, dotModular::STRAND_QMIX),   dotModular::STRAND_QMIX,   i);
            }
        }
        // Poly voices V2..V16 (voiceSlot 1..15).
        for (int v = 1; v < kVoiceSlots; ++v) {
            const int sR = caSrcRow(v, dotModular::STRAND_RHYTHM);
            const int sA = caSrcRow(v, dotModular::STRAND_ACCENT);
            const int sM = caSrcRow(v, dotModular::STRAND_MELODY);
            const int sO = caSrcRow(v, dotModular::STRAND_OCTAVE);
            const int sQ = caSrcRow(v, dotModular::STRAND_QMIX);
            const int sV = caSrcRow(v, dotModular::STRAND_VARIATION);
            const int sL = caSrcRow(v, dotModular::STRAND_LEGATO);
            for (int i = 0; i < kPatternLengthCap; ++i) {   // the ring (compute — see the mono loop's law)
                if (doR) {
                    slewedRhythm[v][i] = pickMono(sR, dotModular::STRAND_RHYTHM, i);
                    slewedAccent[v][i] = pickMono(sA, dotModular::STRAND_ACCENT, i);
                    slewedVariation[v][i] = pickMono(sV, dotModular::STRAND_VARIATION, i);
                    slewedLegato[v][i] = pickMono(sL, dotModular::STRAND_LEGATO, i);
                }
                if (doM) {
                    slewedMelody[v][i] = pickMono(sM, dotModular::STRAND_MELODY, i);
                    slewedOctave[v][i] = pickMono(sO, dotModular::STRAND_OCTAVE, i);
                }
                if (doQ) {
                    slewedQmix[v][i]   = pickMono(sQ, dotModular::STRAND_QMIX,   i);
                }
            }
        }
        // INVARIANT: the pin remap is a property of CHANGE ALLEY ALONE. It must reach the
        // output whether or not any Sands expander is attached -- Sands modules only DISPLAY
        // and MODULATE these probabilities; they are never a precondition for the correlation
        // itself. So the remap ALWAYS re-promotes every lane into random_ here, with no
        // dependence on sandsActive / hasMonoVisual / anything downstream.
        // (This runs BEFORE the spread stage, which re-reads slewed, so when spread is
        //  present and non-zero it simply overwrites random_ with the spread result on top of
        //  the already-correct remapped draw. When spread is absent or zero, this promote is
        //  the value the sequencer reads.)
        // The earlier if (!sandsActive) guard broke exactly this: with Macro attached but no
        // Mono visual, sandsActive was true yet the mono spread block (inside if(hasMonoVisual))
        // never ran, so remapped melody/octave never reached random_ while rhythm did.
        promoteRemappedDraws_(doR, doM, doQ);
        // Warp mirror (CC_PROMPT_03): the promote above rewrote random_ — rebuild the
        // mirror from the settled w=0 basis. Full scope: this runs at roll/pin-change
        // rate (remapSigChanged_ gates re-entry), not per block.
        rebuildWarpMirrorAll();
        // V16-FLICKER FIX (classification (c), two-writer race): PUBLISH here. The remap
        // rewrites slewed* (incl. the mono row every spread-anchored voice tracks) but
        // previously left pubSlewed* at whatever forceRecomputeSlewed() published just
        // BEFORE it — the UN-remapped state. The next ForVoice/refresh publish then
        // copied the REMAPPED slewed*, so the published anchor (and every bar at full
        // spread) alternated between the remapped and un-remapped mono draw across
        // control cycles. Publishing here makes the remap the FINAL writer of the
        // snapshots: pubs are always consistent with the final slewed state. Identity-
        // pinned patches take the early-return above and are byte-identical.
        publishSlewedRhythm();
        publishSlewedMelody();
        publishSlewedQmix();
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
        rhythmZCache_.invalidate();  // z0/z1 cache: force full recompute (not blend-only)
        melodyZCache_.invalidate();
        qmixZCache_.invalidate();
        recomputeEffectiveRhythm();
        recomputeEffectiveMelody();
        recomputeEffectiveQmix();
    }

    inline float finalRandomByStrand(int strand, int step) const {
        const int s = (strand >= 0 && strand < dotModular::NUM_STRANDS) ? strand : dotModular::STRAND_RHYTHM;
        // Warp mirror (CC_PROMPT_05): ALL strands return the EFFECTIVE w-mixture via
        // the ONE uniform source (warpRow) — the Sands bars + prob CV outs carry the
        // mix automatically (the bar-data funnel: this ONE accessor feeds both UIs).
        // The old MEL/OCT-only gate is REMOVED: every lane's engine consumer reads
        // warpRow now, so every lane's display must too (the flip-together rule) —
        // the bars can never show a value the audio doesn't consume. No bank ⇒
        // warpRow degenerates to the raw mono row (bank-less hosts byte-identical);
        // pin remap lives upstream in slewed; out-of-range falls back to rhythm (legacy).
        // STRONG types: the mono row, TYPED — kMonoVoiceSlot (VoiceResolver::mono()'s
        // value; the header cannot include VoiceResolver — engine include cycle).
        return warpRow(dotModular::kMonoVoiceSlot, LaneId{s})[step & (kPatternLengthCap - 1)];   // the frame ring (Stage 2a: cap-derived; 0x0F at the 16 bound)
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
    // Weight-cache: L2-normalised geometric weights w[K] per stream, keyed on r (= slewKnobToR(slew)).
    // Slew r is phrase-fixed, so weights() was recomputed every sumZ call with the same r — pure
    // waste.  Cache w[] per stream; invalidate when r changes (phrase boundary).  Bit-identical.
    redDot::MovingAverageCopula::WeightCache rhythmWeightCache_;
    redDot::MovingAverageCopula::WeightCache melodyWeightCache_;
    redDot::MovingAverageCopula::WeightCache qmixWeightCache_;
    // z0/z1 window-cache: the two sumZ results (window@N-f, window@N-f-1) per step x lane x
    // voice.  Within a phrase (r fixed) + same integer position f, these depend ONLY on
    // position -> compute ONCE, cache, reuse on every frac change (scrub within a position-
    // pair).  Invalidate on: phrase boundary (r changes), position-cross (f changes), or
    // roll (drawCtr advances).  Key: (drawCtr, f, r).  forceRecomputeSlewed() sets invalid.
    // Max fields: rhythm=4*16=64, melody=2*16=32, qmix=1*16=16.  Use 64 for all. The 16 in the
        // buffer shape is the STEP dim -> kStepStorage (Stage 2a).
    struct ZWindowCache {
        static constexpr int MAX_FIELDS = 4 * kVoiceSlots;  // 64 (rhythm max: 4 strands × 16 voices)
        std::array<double, kStepStorage * MAX_FIELDS> z0{};
        std::array<double, kStepStorage * MAX_FIELDS> z1{};
        int64_t cachedCtr = -999;
        int cachedF = -999;
        double cachedR = 999.0;
        bool valid = false;
        bool hit(int64_t ctr, int f_, double r_) const {
            return valid && cachedCtr == ctr && cachedF == f_ && cachedR == r_;
        }
        void invalidate() { valid = false; }
    };
    ZWindowCache rhythmZCache_, melodyZCache_, qmixZCache_;
    // Displayed voice for the Sands visual (set by the UI via setDisplayedVoice, read by
    // refreshVisualCache). Slot 0 = mono/V1, 1..15 = poly V2..V16. Used to scope the
    // display recompute to 1 voice. TYPED (CC_PROMPT_STRONG_TYPES): callers MINT via
    // VoiceResolver::slot(V); the default is the mono anchor (kMonoVoiceSlot).
    VoiceSlot displayedVoice_ = dotModular::kMonoVoiceSlot;
    // The shared voice selection, readable (the SandsDistributionExpander honours
    // it live — "read the selected voice from the shared state" — the spec's
    // single-source-of-truth ruling; the East visual's tabs publish via the setter).
    VoiceSlot displayedVoice() const { return displayedVoice_; }
    // The minting setter — the module layer's ONE entry point for the displayed voice.
    void setDisplayedVoice(VoiceSlot v) { displayedVoice_ = v; }
    // Voice-scoped recompute: blend only the given voice's 16 steps from cached z0/z1.
    // Called by refreshVisualCache for the displayed voice (not all 15 poly voices).
    void recomputeEffectiveRhythmForVoice(VoiceSlot voice);
    void recomputeEffectiveMelodyForVoice(VoiceSlot voice);
    void recomputeEffectiveQmixForVoice(VoiceSlot voice);
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
    // COLLAPSE [15]+mono → [kVoiceSlots][16]: V1 at [0], V2..V16 at [1..15]. One array per strand.
    std::array<std::array<float, kStepStorage>, kVoiceSlots> slewedRhythm  = {}, slewedVariation  = {}, slewedLegato  = {}, slewedAccent  = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> slewedMelody  = {}, slewedOctave  = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> slewedQmix  = {};   // q-mix twin of slewedMelody
    // DELETED: slewedPoly* (15][16] arrays — V2..V16 now at slewed*[1..15].
    // Old: slewedRhythm[16] (mono/V1) + slewedPolyRhythm[15][16] (V2..V16, poly[0]=V2).
    // New: slewedRhythm[kVoiceSlots][16] — V1 at [0], V2 at [1], ..., V16 at [15].
    // Access: mono was slewedRhythm[i] → slewedRhythm[0][i]; poly was slewedPolyRhythm[v][i] → slewedRhythm[v][i].
    // Published snapshots of the slewed buffers — coherent copies the UI thread reads. The audio
    // thread writes slewed* during recomputeEffective* (now ~116µs at r>0), then publishes a
    // snapshot here. Without this, Mono/Macro visuals read slewed* mid-rewrite → torn read →
    // flicker. East already reads published polySpreadEffective (immune). The copy is ~1µs (1k
    // floats), so the race window drops from ~116µs to ~1µs — practically eliminating flicker.
    // COLLAPSE: deleted 7 mono pubSlewed*[16] arrays — they were redundant twins of
    // pubSlewedPoly*[0]. Readers now use pubSlewedPoly*[0] for V1 directly.
    std::array<std::array<float, kStepStorage>, kVoiceSlots> pubSlewedPolyRhythm  = {}, pubSlewedPolyMelody  = {}, pubSlewedPolyOctave  = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> pubSlewedPolyAccent  = {}, pubSlewedPolyQmix  = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> pubSlewedPolyVariation  = {}, pubSlewedPolyLegato  = {};
    void publishSlewedRhythm() {
        for (int i=0;i<kStepStorage;++i){ for(int v=0;v<kVoiceSlots;++v){ pubSlewedPolyRhythm[v][i]=slewedRhythm[v][i];
            pubSlewedPolyAccent[v][i]=slewedAccent[v][i];
            pubSlewedPolyVariation[v][i]=slewedVariation[v][i]; pubSlewedPolyLegato[v][i]=slewedLegato[v][i]; } }
    }
    void publishSlewedMelody() {
        for (int i=0;i<kStepStorage;++i){ for(int v=0;v<kVoiceSlots;++v){ pubSlewedPolyMelody[v][i]=slewedMelody[v][i];
            pubSlewedPolyOctave[v][i]=slewedOctave[v][i]; } }
    }
    void publishSlewedQmix() {
        for (int i=0;i<kStepStorage;++i){ for(int v=0;v<kVoiceSlots;++v) pubSlewedPolyQmix[v][i]=slewedQmix[v][i]; }
    }
    // Set true when any Sands visual expander owns the spread→final stage this
    // cycle. When false, slew copies slewedDraw → final.
    bool  sandsActive = false;
    // Active poly voice count mirrored from SequencerEngine (for Sands display
    // ensemble sizing — the audio path uses SequencerEngine::numPolyVoices).
    int   numPolyVoicesHint = 0;
    // (recomputeDivider_ / RECOMPUTE_DIV / recomputeGate_ REMOVED — slew is now consumed
    //  at the phrase boundary only, not at control rate. The roll already calls
    //  recomputeEffective* with the latched slew; mid-phrase slew changes are latched
    //  but do NOT trigger recompute. See slew_phrase_boundary_cc_brief.md.)

    // ── Source DNA Cache (Original draws before rotation/scramble) ───────────
    // COLLAPSE: [kVoiceSlots][16] — V1 at [0], V2..V16 at [1..15]. One array per strand.
    std::array<std::array<float, kStepStorage>, kVoiceSlots> rhythmSource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> variationSource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> legatoSource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> accentSource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> melodySource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> octaveSource = {};
    std::array<std::array<float, kStepStorage>, kVoiceSlots> qmixSource = {};

    // Caches for UI/Lights to reflect the current state
    // (rhythmPattern[16] RETIRED — ITEM 1: a stale generation-time note/rest prediction
    // whose only consumer was step-LED brightness; the LEDs now evaluate the LIVE warp
    // row vs the live resolved rest threshold in getStepLightBrightness.)
    int   melodySemitone[kStepStorage]  = {};
    float melodyPitchV[kStepStorage]    = {};

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
    float cachedMelodyPitchV[kStepStorage] = {};
    // (cachedRhythmPattern[16] RETIRED — ITEM 1, with the array it cached)
    float cachedQmix[kStepStorage]         = {};   // q-mix twin of cachedMelodyPitchV (raw q-mix draw cache)
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
    // CHUNK must cover the phrase's own draw window: the FILL is ring×fields
    // (STEPS_32 Stage 2b re-derivation — the binding constraint):
    //   rhythm  kPatternLengthCap * 64 = 32*64 = 2048   (4 mono + 4*15 poly fields/step)
    //   melody  kPatternLengthCap * 32 = 1024
    //   qmix    kPatternLengthCap * 16 = 512
    // and the unit()-cursor worst case stays under it (melody 32*(2+30)=1024, rhythm
    // 32*19=608). 2048 = the rhythm fill exactly (the Stage-1 relationship: fill ==
    // CHUNK at the ring bound) and a clean power of two; pos owns [pos*CHUNK,
    // (pos+1)*CHUNK) with no aliasing into pos+1.
    static constexpr uint64_t DRAW_CHUNK = 2048;
    // Draws are always Philox (counter-based). The legacy Xoroshiro A/B path is gone.
    // q-mix is a value lane (melody-like) with its OWN independent Philox stream keyed off
    // redDot::seed::STREAM_SOURCE_SELECT (=3) — decorrelated from rhythm/melody so "which notes"
    // (q-mix) varies independently of "where they interleave" (melody). Mirror of melodyPhilox.
    redDot::PhiloxRng rhythmPhilox, melodyPhilox, qmixPhilox;
    // OU-bridge interpolation noise: SEPARATE keys per stream (S+4/5/6), position-index keyed.
    redDot::PhiloxRng bridgeRhythmPhilox, bridgeMelodyPhilox, bridgeQmixPhilox;
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
    // OU-bridge noise: reseed alongside the dice streams (user-seedable, per-stream keys S+4/5/6).
    inline void seedBridgeRhythmPhilox(float sf) { bridgeRhythmPhilox.seed64(redDot::seed::deriveKey(sf, redDot::seed::STREAM_BRIDGE_R)); }
    inline void seedBridgeMelodyPhilox(float sf) { bridgeMelodyPhilox.seed64(redDot::seed::deriveKey(sf, redDot::seed::STREAM_BRIDGE_M)); }
    inline void seedBridgeQmixPhilox(float sf)   { bridgeQmixPhilox.seed64(redDot::seed::deriveKey(sf, redDot::seed::STREAM_BRIDGE_Q)); }

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

    // ── THE STEP-RATE UNIFORM (CC_PROMPT_GATE_STEP_ADVANCE_SPEC Stage 1) ─────────
    // The gate-mode step-advance decision draws u per rise from the EXISTING rhythm
    // Philox stream — the SAME key (NOT a new stream: the additive-domain law keeps
    // (The step-advance reserved chunk + stepRateUniform are DELETED —
    // CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE: the advance decision reads the
    // VARIATION LANE's step-addressed draw (the same r_vary the lane compares
    // read), NOT a fresh per-rise uniform. An additional Philox draw was ruled a
    // HARD NO; the "would freeze with the held step" reasoning was INVERTED —
    // the freeze IS the intended per-step threshold. The advance consumes ZERO
    // new draws.)

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
    // COLLAPSE: [kVoiceSlots][16] — V1 at [0], V2..V16 at [1..15]. One array per strand.
    // P-C (ARRAY_CONTAINER_AUDIT Finding C): the Philox fill-block scratch moved from TLS
    // to INSTANCE state (mutable — the raw-draw fns are logically const; the buffer is
    // scratch, never observable state). Single audio-thread owner per instance.
    alignas(16) mutable std::array<float, kStepStorage * 64> rhythmDrawBuf{};   // STORAGE width (kStepStorage × 64 fields); the phrase's OWN block is the first kPatternLengthCap × 64 = 1024 today. alignas: fillBlock4xSimd _mm_store_ps (the TLS arrays had it from the ABI; members must spell it)
    alignas(16) mutable std::array<float, kStepStorage * 32> melodyDrawBuf{};   // STORAGE width (kStepStorage × 32 fields)
    alignas(16) mutable std::array<float, kStepStorage * 16> qmixDrawBuf{};    // STORAGE width (kStepStorage × 16 fields)

    struct RhythmDraw { Matrix2<float, kVoiceSlots, kStepStorage> rhythm{}, variation{}, legato{}, accent{}; };
    struct MelodyDraw { Matrix2<float, kVoiceSlots, kStepStorage> melody{}, octave{}; };
    struct QmixDraw   { Matrix2<float, kVoiceSlots, kStepStorage> qmix{}; };
    // Step A: 4x batching — fillBlock computes each Philox block ONCE (4 outputs),
    // then scatter into struct fields in the same order. Bit-identical to the old
    // per-cursor philox*At(pos, c++) calls (same Philox outputs, 4x fewer block computations).
    //
    // THE DRAW-WINDOW LAW (STEPS_32 Stage 2a): a phrase's Philox block is EXACTLY the
    // pre-Stage-2 window — DRAW_CHUNK (1024) cursors from pos*DRAW_CHUNK, the fill and
    // the scatter both bounded by the RING (kPatternLengthCap × fields = 1024/512/256
    // today). Cursor mapping, bit values and cost are IDENTICAL to Stage 1; cells
    // beyond the ring stay ZERO (the gate: 32-entry arrays zero-filled beyond length).
    // WHY the fill does not widen to kStepStorage yet: the cursor STRIDE is DRAW_CHUNK
    // (pos owns [pos*CHUNK, (pos+1)*CHUNK)) — a wider fill would read PAST the phrase's
    // own window into phrase pos+1's draws (aliased steps 16..31). Stage 2b's flip
    // widens DRAW_CHUNK (headroom re-derived: worst unit() calls grow to ring×fields)
    // and this fill/scatter widens with the ring — one flip commit, pinned by the
    // 32-step pins then.
    inline void rawDrawRhythmPatternAt(int64_t pos, RhythmDraw& d) const {
        // 4 mono + 4*15 poly = 64 per step × ring steps = 1024 (the pre-Stage-2 block VERBATIM)
        float* buf = rhythmDrawBuf.data();
        rhythmPhilox.fillBlock((uint64_t)pos * DRAW_CHUNK, buf, kPatternLengthCap * 64);
        int k = 0;
        for (int i = 0; i < kPatternLengthCap; ++i) {
            d.rhythm[0][i]=buf[k++]; d.variation[0][i]=buf[k++];
            d.legato[0][i]=buf[k++]; d.accent[0][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.rhythm[v][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.accent[v][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.variation[v][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.legato[v][i]=buf[k++];
        }
    }
    inline void rawDrawMelodyPatternAt(int64_t pos, MelodyDraw& d) const {
        float* buf = melodyDrawBuf.data();
        melodyPhilox.fillBlock((uint64_t)pos * DRAW_CHUNK, buf, kPatternLengthCap * 32);   // ring × 32 fields
        int k = 0;
        for (int i = 0; i < kPatternLengthCap; ++i) {
            d.melody[0][i]=buf[k++]; d.octave[0][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.melody[v][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.octave[v][i]=buf[k++];
        }
    }
    inline void rawDrawQmixPatternAt(int64_t pos, QmixDraw& d) const {
        float* buf = qmixDrawBuf.data();
        qmixPhilox.fillBlock((uint64_t)pos * DRAW_CHUNK, buf, kPatternLengthCap * 16);    // ring × 16 fields
        int k = 0;
        for (int i = 0; i < kPatternLengthCap; ++i) {
            d.qmix[0][i]=buf[k++];
            for (int v=1;v<kVoiceSlots;++v) d.qmix[v][i]=buf[k++];
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
    // COLLAPSE: z arrays [kVoiceSlots][16] — V1 at [0], V2..V16 at [1..15].
    struct CachedRhythmDraw {
        RhythmDraw draw;
        std::array<std::array<double, kStepStorage>, kVoiceSlots> zRhythm  = {}, zVariation  = {}, zLegato  = {}, zAccent  = {};
    };
    struct CachedMelodyDraw {
        MelodyDraw draw;
        std::array<std::array<double, kStepStorage>, kVoiceSlots> zMelody  = {}, zOctave  = {};
    };
    struct CachedQmixDraw {
        QmixDraw draw;
        std::array<std::array<double, kStepStorage>, kVoiceSlots> zQmix  = {};
    };
    // mutable: patternXAt is const (read from const engine refs in places) but the cache is a
    // pure-fn-of-pos memo (no observable state change) — so const-correctness is preserved.
    mutable std::unordered_map<int64_t, CachedRhythmDraw> rhythmDrawCache_;
    mutable std::unordered_map<int64_t, CachedMelodyDraw> melodyDrawCache_;
    mutable std::unordered_map<int64_t, CachedQmixDraw>   qmixDrawCache_;
    // Voice-count opt: track the hint used at cache-fill time. On poly count INCREASE,
    // existing cache entries are missing z values for the delta voices — fillDeltaZ_
    // retroactively computes PhiInv for them (pure fn of draw → just evaluate it).
    mutable int prevNumPolyVoicesHint_ = 0;
    void fillDeltaZ_(int oldCount, int newCount) {
        if (newCount <= oldCount) return;
        newCount = std::min(newCount, 15);
        for (auto& kv : rhythmDrawCache_) {
            auto& e = kv.second;
            for (int v = oldCount + 1; v <= newCount; ++v)
                for (int i = 0; i < kPatternLengthCap; ++i) {   // the ring (PhiInv compute)
                    e.zRhythm[v][i] = redDot::copula::PhiInv(e.draw.rhythm[v][i]);
                    e.zAccent[v][i] = redDot::copula::PhiInv(e.draw.accent[v][i]);
                    e.zVariation[v][i] = redDot::copula::PhiInv(e.draw.variation[v][i]);
                    e.zLegato[v][i] = redDot::copula::PhiInv(e.draw.legato[v][i]);
                }
        }
        for (auto& kv : melodyDrawCache_) {
            auto& e = kv.second;
            for (int v = oldCount + 1; v <= newCount; ++v)
                for (int i = 0; i < kPatternLengthCap; ++i) {   // the ring (PhiInv compute)
                    e.zMelody[v][i] = redDot::copula::PhiInv(e.draw.melody[v][i]);
                    e.zOctave[v][i] = redDot::copula::PhiInv(e.draw.octave[v][i]);
                }
        }
        for (auto& kv : qmixDrawCache_) {
            auto& e = kv.second;
            for (int v = oldCount + 1; v <= newCount; ++v)
                for (int i = 0; i < kPatternLengthCap; ++i)   // the ring (PhiInv compute)
                    e.zQmix[v][i] = redDot::copula::PhiInv(e.draw.qmix[v][i]);
        }
    }
public:
    // Called from SequencerEngine::resetActivatedPolyVoices_ on each step.
    // Detects numPolyVoicesHint increase and retroactively fills delta voices' z.
    void syncPolyVoiceCount() {
        if (numPolyVoicesHint > prevNumPolyVoicesHint_) {
            fillDeltaZ_(prevNumPolyVoicesHint_, numPolyVoicesHint);
            prevNumPolyVoicesHint_ = numPolyVoicesHint;
        } else if (numPolyVoicesHint < prevNumPolyVoicesHint_) {
            prevNumPolyVoicesHint_ = numPolyVoicesHint;  // decrease: just track (z stays in cache, unused)
        }
    }
    // Returns the cached entry for pos, computing+inserting on miss.
    CachedRhythmDraw& cachedRhythmDraw(int64_t pos) const {
        auto it = rhythmDrawCache_.find(pos);
        if (it != rhythmDrawCache_.end()) return it->second;
        CachedRhythmDraw& e = rhythmDrawCache_[pos];
        rawDrawRhythmPatternAt(pos, e.draw);
        for (int i=0;i<kPatternLengthCap;++i){ e.zRhythm[0][i]=redDot::copula::PhiInv(e.draw.rhythm[0][i]);   // the ring (PhiInv compute)
            e.zVariation[0][i]=redDot::copula::PhiInv(e.draw.variation[0][i]);
            e.zLegato[0][i]=redDot::copula::PhiInv(e.draw.legato[0][i]);
            e.zAccent[0][i]=redDot::copula::PhiInv(e.draw.accent[0][i]);
            for(int v=1;v<=numPolyVoicesHint;++v){ e.zRhythm[v][i]=redDot::copula::PhiInv(e.draw.rhythm[v][i]);
                e.zAccent[v][i]=redDot::copula::PhiInv(e.draw.accent[v][i]);
                e.zVariation[v][i]=redDot::copula::PhiInv(e.draw.variation[v][i]);
                e.zLegato[v][i]=redDot::copula::PhiInv(e.draw.legato[v][i]); } }
        return e;
    }
    CachedMelodyDraw& cachedMelodyDraw(int64_t pos) const {
        auto it = melodyDrawCache_.find(pos);
        if (it != melodyDrawCache_.end()) return it->second;
        CachedMelodyDraw& e = melodyDrawCache_[pos];
        rawDrawMelodyPatternAt(pos, e.draw);
        for (int i=0;i<kPatternLengthCap;++i){ e.zMelody[0][i]=redDot::copula::PhiInv(e.draw.melody[0][i]);   // the ring (PhiInv compute)
            e.zOctave[0][i]=redDot::copula::PhiInv(e.draw.octave[0][i]);
            for(int v=1;v<=numPolyVoicesHint;++v){ e.zMelody[v][i]=redDot::copula::PhiInv(e.draw.melody[v][i]);
                e.zOctave[v][i]=redDot::copula::PhiInv(e.draw.octave[v][i]); } }
        return e;
    }
    CachedQmixDraw& cachedQmixDraw(int64_t pos) const {
        auto it = qmixDrawCache_.find(pos);
        if (it != qmixDrawCache_.end()) return it->second;
        CachedQmixDraw& e = qmixDrawCache_[pos];
        rawDrawQmixPatternAt(pos, e.draw);
        for (int i=0;i<kPatternLengthCap;++i){ e.zQmix[0][i]=redDot::copula::PhiInv(e.draw.qmix[0][i]);   // the ring (PhiInv compute)
            for(int v=1;v<=numPolyVoicesHint;++v) e.zQmix[v][i]=redDot::copula::PhiInv(e.draw.qmix[v][i]); }
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
        for (int i=0;i<kPatternLengthCap;++i){   // the ring (copula compute)
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zRhythm[0][i];
            out.rhythm[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zVariation[0][i];
            out.variation[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zLegato[0][i];
            out.legato[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zAccent[0][i];
            out.accent[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=1;v<=numPolyVoicesHint;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zRhythm[v][i];
                out.rhythm[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zAccent[v][i];
                out.accent[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zVariation[v][i];
                out.variation[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zLegato[v][i];
                out.legato[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
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
        for (int i=0;i<kPatternLengthCap;++i){   // the ring (copula compute)
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zMelody[0][i];
            out.melody[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zOctave[0][i];
            out.octave[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=1;v<=numPolyVoicesHint;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zMelody[v][i];
                out.melody[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zOctave[v][i];
                out.octave[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
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
        for (int i=0;i<kPatternLengthCap;++i){   // the ring (copula compute)
            for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zQmix[0][i];
            out.qmix[0][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            for(int v=1;v<=numPolyVoicesHint;++v){
                for(std::size_t j=0;j<K;++j) zw[j]=win[j]->zQmix[v][i];
                out.qmix[v][i]=(float)redDot::MovingAverageCopula::applyZ(zw,r);
            }
        }
    }

    static constexpr uint64_t MAX_U64 = 0xFFFFFFFFFFFFFFFFULL;

    void reset();

    // ── Core generation ───────────────────────────────────────────────────────

    // Pick a DEGREE (0..n-1) weighted by fader values using a provided random value. n is the active
    // degree count (tuning.N): 12 for the built-in/Sikit/Micro-12, up to 24 for Micro-24. At n=12 this
    // is bit-identical to the legacy pickSemitone (same sum + walk + float-safety return).
    int pickSemitone(const float weights[], int n, float r_val);

    // Generate a pitch voltage (1V/oct, 0..5V) and return the semitone.
    float genPitch(int& outSemitone, const PatternInput& in);

    // Generate a pitch voltage using provided random floats for semitone and octave selection.
    float genPitchLive(int& outSemitone, const PatternInput& in, float r_semi, float r_oct);

    // QMIX_OCTAVE (CC_PROMPT_QMIX_OCTAVE + QMIX_OCTAVE_PLAN.md): genPitchLive's own pitch
    // split into its FORMULA-GROUPED components — the same rolls, the same float-op
    // association, so genPitchLive re-expressed on it is bit-identical. The axes the
    // two-read q-mix split composes from:
    //   sem       — the picked DEGREE (pickSemitone(r_semi)); 0 when the pick failed
    //   picked    — false = the pick failed (sem<0): genPitchLive's 0.f early-out, outSemitone 0
    //   octTerm   — float(oct) - 4 (the octave term, whole volts, BOTH formulas)
    //   degTerm   — 12-TET: (sem + transpose)/12 (the transpose rides the DEGREE term — the
    //               formula's own placement); Micro: tuning.degreeVolts(sem)
    //   shiftTerm — Micro only: round(transpose/12) (transpose is OCTAVES-ONLY — it rides
    //               the OCTAVE term); 12-TET: 0
    // v == (octTerm + degTerm) + shiftTerm == genPitchLive's v exactly (association-exact).
    struct PitchSplit { bool picked = false; int sem = 0; float octTerm = 0.f; float degTerm = 0.f; float shiftTerm = 0.f; };
    PitchSplit genPitchSplit(const PatternInput& in, float r_semi, float r_oct);

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

    // ── Playable slew/mix latch (control rate) ─────────────────────────────────
    // SPLIT (slew-latch regression): SLEW and MIX latch through SEPARATE calls —
    // slew must NEVER sit behind the ABMix-liveness gate (the old shape latched it
    // inside latchMix, so a locked ABMix left *SlewLatched STALE and the roll
    // re-derived with the OLD slew).
    // latchSlew: UNCONDITIONAL, every control cycle — slew is an independent
    // distributional param (NOT an A/B-mix property) and has NO own Lock control.
    // Consumed ONLY at the roll: redrawRhythm/Melody/Qmix advance the counter,
    // recomputeEffective* read *SlewLatched via slewKnobToR, and the new r misses
    // the ZCache → re-derive. Mid-phrase slew changes are latched but do NOT
    // trigger recompute (a control-rate recompute is the CPU spike — the roll is
    // the correct trigger; see the remapSigChanged_ spec comment in
    // MonsoonSandsManager.cpp).
    // Per-stream semantics: a stream whose A/B is FROZEN still latches SLEW here —
    // only its MIX holds (see latchMix). Freezing slew per-stream would need its
    // OWN lock control (out of scope; today slew is unlocked = always latches).
    void latchSlew(float rhythmSlew, float melodySlew, float qmixSlew);
    // latchMix: MIX ONLY (the A<->B scrub position). applyRhythm/applyMelody/
    // applyQmix (LOCK_SCOPE_MENU) gate each stream independently — a frozen A/B
    // axis holds its latched mix. Default all true = latch all (unlocked). The mix
    // change is consumed by refreshVisualCache (for-voice recompute at the display
    // rate), not here.
    void latchMix(float rhythmMix, float melodyMix, float qmixMix,
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
    // The slewedDraw buffers (slewedRhythm[kVoiceSlots][16], slewedMelody[kVoiceSlots][16], etc.) are
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
