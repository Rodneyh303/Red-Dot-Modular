#include "SequencerEngine.hpp"
#include "../SpreadInterp.hpp"   // Stage 2a: refreshPolyRandomCell -> applyPoly
#include "../VoiceResolver.hpp"  // CC_PROMPT_STRONG_TYPES: VoiceSlot minting (mono/slot) for the typed accessors
#include <cmath>
#include <algorithm>
#include <cstdio>

using dotModular::VoiceResolver;   // the mint authority (VoiceSlot cannot be constructed here)

// RULE2_DEBUG is defined in SequencerEngine.hpp (shared with the manager). The trace uses
// Rack's INFO() logger (writes to log.txt); fprintf(stderr) is discarded under a GUI launch.
#if RULE2_DEBUG
#include <rack.hpp>
#endif

// Note-value data (fraction, allowedPPQN, label) lives in dsp/NoteValues.hpp as
// the single source of truth. The sequencer uses NOTE_VALUES[i].allowedPPQN to
// snap an unsupported selection (e.g. 1/4T at PPQN=4) to the nearest valid one.



// (DNA_LCM is DELETED — CC_PROMPT_KILL_DNA_LCM_UNBOUNDED_CLOCK_PARKED. So is the master
// clock itself — POSITION_HOLDER_MODEL.md.) There is NO global step counter: every walk
// owns a bounded PHASE — a position in its OWN cycle, congruent to its driver's t0 mod
// the walk's OWN period — advanced locally on each advance event by the driver's raw
// tick delta. Every lane position comes from the holder phases through laneTickFor (the
// lane's OWN local mod), so continuity for any length is by construction — including
// negative congruences (backward walks from a fresh start) and lengths beyond 16 — with
// no shared timeline that every read is a function of. What the old DNA_LCM wrap bought
// (every lane exactly back at t=0 when it wrapped) the local mods buy for EVERY t; what
// the old totalStepsElapsed bought (one synchronized timeline) the per-holder phases buy
// WITHOUT the artificial unification — per-voice advance and per-lane LOR are the walks'
// OWN parameters now, not lenses onto a shared clock.

// Closed-form window position for a lane at global tick t — the walk-SHAPE function.
// POSITION_HOLDER_MODEL: the holders evaluate this AT THEIR OWN PHASE (tick = phase+1 —
// the phase is congruent to the walk's t0, and this function is periodic in t0 with the
// walk's OWN period, so any congruent representative gives the identical value).
// Reproduces the accumulator's sequences exactly (verified in test_lane_direction):
//   Forward   t            -> 0,1,2,3,0,1,..
//   Reverse  -t            -> 0,3,2,1,0,3,..            (opposite-to-global; composes with Mode E)
//   Pendulum  triangle     -> 0,1,2,3,2,1,0,1,2,3       (bounce at the LOR endpoint, no repeat)
//   PingPong  triangle+hold-> 0,1,2,3,3,2,1,0,0,1       (endpoint repeats, then bounces)
// The caller feeds the result to getStrandIdx, which applies rot/off — matching the old code,
// where the bounce ran on the raw tick and rot was applied on read.
//
// CONTINUITY (post-DNA_LCM): t is the raw net-step counter and may be NEGATIVE (a backward
// walk from a fresh start) or arbitrarily large. Every branch below is periodic in t with
// the lane's OWN period and wraps negatively (the double-mods), so laneTickFor(d, t, len)
// equals laneTickFor(d, t mod period, len) for ANY t — the local-mod replacement for the
// old global wrap's divisibility argument. Forward/Reverse return the raw (t-1) / -t values;
// their ONLY consumers (getStrandIdx, calcPlayhead, the recomp wrap) mod by the lane length
// locally, so unbounded is safe by construction.
int64_t SequencerEngine::laneTickFor(LaneDir d, int64_t t, int len) {
    const int L = std::max(1, len);
    // The anchor is 1-based (the first advance evaluates the walks at tick 1).
    // Convert to 0-based position so laneTick_ matches stepIndex (also 0-based).
    // Without this, the Sands visual playhead is always 1 step ahead of the
    // Monsoon ring light (which reads stepIndex directly).
    int64_t t0 = t - 1;
    switch (d) {
        // Reverse: -(t0+1) = -t — same formula as before (the -1 and +1 cancel),
        // so Reverse behaviour is unchanged. For t=1: returns -1, wraps to L-1.
        case LaneDir::Reverse: return -(t0 + 1);
        case LaneDir::Pendulum: {
            if (L < 2) return 0;
            const int64_t P = 2 * (L - 1);
            const int64_t u = ((t0 + (L - 1)) % P + P) % P;
            const int64_t m = u - (L - 1);
            return (m < 0) ? -m : m;
        }
        case LaneDir::PingPong: {
            const int64_t P = 2 * L;
            const int64_t u = ((t0 % P) + P) % P;
            return (u < L) ? u : (P - 1 - u);
        }
        case LaneDir::Forward:
        default: return t0;
    }
}

// ── UNIFIED ADDRESSING Stage 1 (CC_PROMPT_UNIFIED_ADDRESSING_SPEC.md) ─────────────
// The GLOBAL-level LOR map: read_position(voice, lane) = laneLOR(voiceLOR(globalLOR(t))).
// Stage 1 inserts the top map; the lane machinery below it (laneTickFor + getStrandIdx)
// is UNTOUCHED — the re-base of lane R from "global step 1" to "voice step 1" is carried
// by the driver change itself (the lane's phase driver becomes the parent's walk).
//
// PASS-THROUGH at the default window (length 16, offset 0): the raw tick returns
// unchanged, so EVERY read — any lane length (incl. the non-divisive 7/5/6 pinned in
// test_unified_addressing), direction, poly voice — is bit-identical to the
// pre-unification engine. That is the Stage-1 GOLDEN PIN.
//
// MAPPED once the window is authored (length < 16 or offset > 0 — the KNOWN ISSUE
// config): the tick becomes the window walk — the 0-based count through
// [offset, offset+length), returned as a 1-based tick for laneTickFor. Default lanes
// (len 16, off 0) then read EXACTLY the global window's steps instead of the full 16;
// a lane with its own window phases against the window walk (the polymeter beat against
// the container length). The playhead's direction of motion is inherited automatically
// (the transport's motion composes with it — the same way lane direction composed with the
// playhead before), and t's motion direction is the only direction input here.
int64_t SequencerEngine::driverTickAt(int64_t t, int len, int off) {
    // (Rodney's correction of the prompt, CC_PROMPT_MACRO_RESOLVER_AND_IDENTITY):
    // this 16 is NOT the bug-23 identity sentinel — it is the RAW-TICK POLYMETER
    // LAW. A len-7 lane under a 16-step pattern free-runs (t−1)%7, DRIFTING
    // against the 16 and realigning only after 112 steps — the drift IS the
    // feature; folding it into a 16-walk ((t−1)%16)%7 realigns every 16 and
    // destroys the polymeter. The pass-through therefore keeps its
    // default-pattern arm AND gains the cap arm (the full-ring window imposes
    // no restriction; congruent for every reader either way). The identity
    // sentinel (voiceLorIdentity, the row seeds, the menus) is the OTHER 16 —
    // that one takes the cap.
    if (len == 16 && off == 0) return t;   // the default-pattern window: the RAW-TICK POLYMETER LAW (unchanged, Rodney's correction)
    int64_t w = (t - 1) % len;             // 0-based walk through the window
    if (w < 0) w += len;
    return w + off + 1;                    // 1-based tick: window start == tick 1
}

// The driver's musical predecessor (the value one step before tNow along the current
// motion), for the lane-sign derivation. At a window wrap the walk JUMPS (that jump IS
// the polymeter beat against the container), so the raw predecessor would read as a
// direction flip; the true neighbor is one step along the motion. Pass-through keeps
// today's exact expression (t - dir). tRawPrev is the driver evaluated at the raw-tick
// predecessor (t - dir), precomputed by the caller.
int64_t SequencerEngine::driverTickBefore(int64_t tNow, int64_t tRawPrev, int dir, int len, int off) {
    if (len == 16 && off == 0) {   // the default-pattern pass-through (the polymeter law): today's exact expression
        return tNow - dir;
    }
    const int64_t dd = tNow - tRawPrev;
    if (dd > 1 || dd < -1) return tNow - dir;   // window wrap: one musical step
    return tRawPrev;
}

void SequencerEngine::reset() {
    pe.reset();
    gs.reset();
    gsStep.reset();
    for (const VoiceSlot vs : VoiceResolver::allSlots()) {   // slots 1..15 = V2..V16 (V1 is gs/gsStep above)
        if (vs == VoiceResolver::mono()) continue;
        voices[vs].gs.reset(); voices[vs].gsStep.reset();
    }
    // restProb values are NOT reset — the caller re-applies them from expander knobs.
    for (int i = 0; i < 16; i++) wasHeldPolyPrev[i] = false;
    // F-1 (RHYTHM_DECISION_MANAGER_PLAN §3.1): the RDM reads the STAGED containment graph —
    // PatternEngine owns the array (the publish contract stages it every block from the
    // Containment module's editor graph; an absent module stages zeros = free voices, D2
    // INTENDED). The pointer is live: a re-staged graph is visible at the next edge with no
    // re-wiring. The source is GRAPH permanently — D2: no engine legacy mode; the
    // all-follow-V1 star is an AUTHORED graph (the Cantonment preset / the test seam), and
    // the suites that pin the legacy sound author it into pe.containmentParents explicitly.
    rdm.setParentSource(RhythmDecisionCore::ParentSource::GRAPH);
    rdm.graphWords = pe.containmentParents.data();
    // F-1 per-container capture state: a voice that has not executed this run is at Rest —
    // it offers no onset and closes its children (fail-closed). F-4: no ghost event either —
    // a voice that has not executed hosts no ghost cell for its children.
    for (int i = 0; i < 16; i++) { polyVerdict_[i] = MonoDecision::Rest; polyNv_[i] = 0; }
    for (int i = 0; i < 16; i++) polyGhostOnset_[i] = false;
    lastStepResult = StepResult{};
    stepIndex = -1;
    lastStepIndex = -1;
    startStep = 0;
    endStep = 15;
    cachedLength = 16;
    cachedOffset = 0;
    // (totalStepsElapsed is GONE — the holders anchor below, after every LOR store is
    // back at its reset value, so the phase periods match the walks that read them.)
    // PER-VOICE STEP-ADVANCE Stage 1: the offsets + per-voice scans re-anchor with
    // the transport (provably unobservable in the locked build — the offsets are
    // always 0, and scanOfsV_[0] is re-zeroed by the seed advance before any
    // decision reads it; Stage 2's divergence makes this the re-unification point).
    for (int i = 0; i < 16; ++i) { advanceOffset_[i] = 0; scanOfsV_[i] = 0; }
    for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
        lorRef(s, LOR_LEN) = 16;   // Length defaults to full 16-step window
        lorRef(s, LOR_OFF) = 0;
        lorRef(s, LOR_ROT) = 0;
        laneTick_[s] = 0;          // per-lane direction: reset walk + default forward
        laneSign_[s] = 1;
        laneSignPending_[s] = 1;
        lanePendulum_[s] = false;
        laneDir_[s] = LaneDir::Forward;
        laneDirPending_[s] = LaneDir::Forward;
        lanePingPongHold_[s] = false;
        macroLaneTick_[s] = 0;
        macroLaneSign_[s] = 1;
        macroLaneDir_[s] = LaneDir::Forward;
        macroPingPongHold_[s] = false;
        for (int v = 1; v <= numPolyVoices; ++v) {
            laneTickV_[v][s] = 0;   // per-voice tick: reset (inherits mono dir)
            laneSignV_[v][s] = 1;   // CRITICAL: must be +1, not 0 — -0 == 0 in integer
                                    // arithmetic, so a bounce flip on 0 is a no-op (the
                                    // Pendulum/PingPong bounce would never reverse direction).
            laneSignVPending_[v][s] = 1;
            laneDirV_[v][s] = LaneDir::Forward;
            laneDirVPending_[v][s] = LaneDir::Forward;
            lanePingPongHoldV_[v][s] = false;
        }
    }
    for (int i = 1; i < 16; i++) {
        const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
        for (int l = 0; l < PL_LANES; ++l) {   // was l < 3 — PL_ACCENT(3) was never reset (pre-existing bug)
            polyLenERef(vs, LaneId{l}) = 16;
            polyOffERef(vs, LaneId{l}) = 0;
            polyRotERef(vs, LaneId{l}) = 0;
        }
        // VARIATION/LEGATO per-voice LOR were NEVER seeded (lorStore_ is zero-init, so len==0 →
        // getStrandIdx's max(1,len) pinned every step to index 0 — NOT identity). Editor-order
        // accessor: these lanes have no PL_ id.
        for (int el : { EDITOR_LANE_VARIATION, EDITOR_LANE_LEGATO }) {
            polyLORRef(vs, LaneId{el}, LOR_LEN) = 16;
            polyLORRef(vs, LaneId{el}, LOR_OFF) = 0;
            polyLORRef(vs, LaneId{el}, LOR_ROT) = 0;
        }
    }
    windowMask = kMaskLenMask;
    locked = false;
    muted = false;
    // UNIFIED ADDRESSING Stage 2: the voice LOR rows seed to IDENTITY (pass-through).
    // (bug 23) identity = the FULL voice window = the cap (was the literal 16).
    for (int v = 0; v < kVoiceSlots; ++v) {
        voiceLorStore_[v][LOR_LEN] = kPatternLengthCap; voiceLorStore_[v][LOR_OFF] = 0; voiceLorStore_[v][LOR_ROT] = 0;
        voiceLorDir_[v] = LaneDir::Forward;
        voiceLorLocalEast_[v] = false;
        voiceDriver_[v] = 0; voiceRead_[v] = 0;
        voiceSign_[v] = 0;   // Stage 3: the walk-sign cue re-derives on the first step
    }
    macroVoiceLor_[LOR_LEN] = kPatternLengthCap; macroVoiceLor_[LOR_OFF] = 0; macroVoiceLor_[LOR_ROT] = 0;
    macroVoiceLorDir_ = LaneDir::Forward;
    macroFrameRead_ = 0; macroFrameSign_ = 0;   // Stage 3: the Macro frame's caches too
    // Stage 4: the output mask seeds to CYCLE everywhere (the all-on degenerate mask,
    // today's behaviour) — no gating at defaults, the goldens' output path untouched.
    for (int v = 0; v < kVoiceSlots; ++v) {
        maskKind_[v] = MASK_CYCLE; maskFollowR_[v] = false;
        maskLocalEast_[v] = false; maskAnchored_[v] = kMaskLenMask; maskAnchorR_[v] = 0;
        maskLastPushed_[v] = kMaskLenMask;
    }
    maskMacroKind_ = MASK_CYCLE; maskMacroFollowR_ = false;
    maskMacroAnchored_ = kMaskLenMask; maskMacroAnchorR_ = 0; maskMacroLastPushed_ = kMaskLenMask;
    runGateActive = false;
    resetArmed = false;
    prevGate1High = false;
    for (int i = 0; i < 16; ++i) pendingCheckpointArrivalV_[i] = false;
    modeSelect = 0;
    ppqnSetting = 24;
    noteVariationMask = 0b111;
    activeSemiCount = 0;
    for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) faderCache[i] = -1.f;
    lastQuantIn = -100.f;
    lastQuantOut = 0.f;
    // THE POSITION-HOLDER ANCHOR (the t=0 congruence — every walk at its own start;
    // the read caches were zeroed above exactly as before, and the first
    // advancePlayhead derives them from the phases bit-identically to the old
    // f(counter) evaluation at t=1).
    anchorTransport(0);
}

bool SequencerEngine::isStepInWindow(int idx) const {
    return (windowMask & (uint64_t(1) << (idx & (kMaskRing - 1)))) != 0;
}

void SequencerEngine::setWindow(int length, int offset) {
    cachedLength = length;
    cachedOffset = offset;
    startStep = offset;
    endStep = (offset + length - 1) & (kMaskRing - 1);

    uint64_t mask = 0;
    for (int i = 0; i < length; ++i) {
        mask |= (uint64_t(1) << ((offset + i) & (kMaskRing - 1)));
    }
    windowMask = mask;

    if (stepIndex != -1 && !isStepInWindow(stepIndex)) {
        stepIndex = (startStep - 1 + kMaskRing) % kMaskRing;
    }
    // THE RE-AUTHOR ANCHOR (the holder law): the global walk's read coordinate re-bounds
    // to the NEW window — (tick−1) mod newLen, exactly the value the old f(counter)
    // closed form evaluated from the next event on. The warm phases are untouched: they
    // carry the RAW clock congruences, which a window re-author does not move.
    globalWalk_ = wrapMod(transportTick_ - 1, std::max(1, length));
}

// ── Stage 2b: the voice-LOR authoring push (see the hpp decl) ──────────────────
// The EditorState float rows are the single authoring authority; this ONE copy per
// control block is the only path they reach the map through (same discipline as the
// lane LOR push in the Sands manager). The clamps mirror the lane LOR ranges exactly
// (L 1..kLaneLengthMax, O 0..kLaneOffsetMax, R 0..L-1, dir 0..3) and the rounding is std::lround — so an identity
// row (16/0/0/0, follow) lands byte-identically on the store the reset() seeded and
// the pass-through branch keeps the Stage-1 goldens verbatim. Pure: reads only its
// arguments, writes only the voice-LOR store (no recompute here — the next step's
// lane walk picks the rows up, the same one-block cadence every authored LOR uses).
void SequencerEngine::pushVoiceLorEditor(
    const float* macroLor, float macroDir,
    const float* eastLor, const float* eastDir, const float* localEast) {
    // STEPS_32 Stage 3 — the clamp domains mirror the AUTHORING SURFACES they serve:
    //   LEN  1..kPatternLengthCap — the plan's D-table row ("LOR_LEN value range | lane
    //        lengths 1..16 authored via the editor pushes | range -> 1..32"): the Sands
    //        grid draws the whole phrase, so a lane window may now span/land in bar 2.
    //   OFF 0..kMaskRing-1 — the grid's drag domain is the phrase (the strip wraps
    //        within STEP_COUNT == the cap); the old 0..15 clamp would SILENTLY DROP the
    //        bar-2 window starts the 32-column grid authors (the Lantern bar-blind
    //        write-path class — a display widening whose authoring path still truncated
    //        at 16). The classification's old "off/rot spans stay 0..15" calibration
    //        decision governed the KNOB/CV spans (applyMonoCV/applyMacroCV/eastDelta)
    //        until CC_FIX_VOICE_RANGE_STRIP_INTERACTION §F overturned it for the LANE
    //        CV paths (the midpoint snap — those spans now follow the named bounds,
    //        OFF 0..kLaneOffsetMax / ROT 0..L-1); the VOICE-LOR CV fold (Monsoon.cpp
    //        VL_HI) keeps the recorded calibration by decision. Either way, not this
    //        push: the push is the editor's own path and must not
    //        truncate what the editor's own surface can author. At <=16 windows the drags
    //        produce 0..15 and every clamp here is a no-op (the GOLDEN LAW domain).
    //   ROT 0..L-1 (CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION, Bug 2) — the voice rows
    //        now clamp R the same as EVERY OTHER LANE's rotation: the lane editor's
    //        convention clamp(R, 0, max(0, L-1)) (SandsVisualEditorV4's START/END resize
    //        and ROTATION drag all apply exactly that), window-RELATIVE — R is the phase
    //        into the window at global step 1 (UAS lines 13-20/22-37), so an R past the
    //        window's end has no meaning and is clamped HERE, at the authoring path —
    //        not silently folded at the read. LEN is clamped FIRST in the same push, so
    //        a window that shrinks below the authored R re-clamps R in the SAME block
    //        (the painted-mask follow then re-pins by the clamp delta — the declared
    //        consequence). At L=16 the clamp domain is 0..15 — byte-identical to the old
    //        ring clamp (the identity/abs1 goldens' precondition).
    auto lorInt = [](float x, float lo, float hi) -> int {
        return (int)std::lround(std::clamp(x, lo, hi));
    };
    // Row item order is the store's: LOR_LEN=0, LOR_OFF=1, LOR_ROT=2 — the editor rows
    // are authored {L,O,R} in the same order (documented at the EditorState arrays).
    macroVoiceLor_[LOR_LEN] = lorInt(macroLor[LOR_LEN], 1.f, (float)dotModular::kPatternLengthCap);
    macroVoiceLor_[LOR_OFF] = lorInt(macroLor[LOR_OFF], 0.f, (float)(dotModular::kMaskRing - 1));
    // R follows the LEN just clamped above (the lanes' convention: hi = max(0, L-1)) —
    // a shrink-below-R re-clamps R in the same push, the store never holds an
    // out-of-window R (the CC_FIX invariant; the CV-fold write path clamps too).
    macroVoiceLor_[LOR_ROT] = lorInt(macroLor[LOR_ROT], 0.f, (float)std::max(0, macroVoiceLor_[LOR_LEN] - 1));
    macroVoiceLorDir_       = (LaneDir)lorInt(macroDir, 0.f, 3.f);
    for (int v = 0; v < kVoiceSlots; ++v) {
        voiceLorStore_[v][LOR_LEN] = lorInt(eastLor[v * 3 + LOR_LEN], 1.f, (float)dotModular::kPatternLengthCap);
        voiceLorStore_[v][LOR_OFF] = lorInt(eastLor[v * 3 + LOR_OFF], 0.f, (float)(dotModular::kMaskRing - 1));
        voiceLorStore_[v][LOR_ROT] = lorInt(eastLor[v * 3 + LOR_ROT], 0.f, (float)std::max(0, voiceLorStore_[v][LOR_LEN] - 1));
        voiceLorDir_[v]            = (LaneDir)lorInt(eastDir[v], 0.f, 3.f);
        voiceLorLocalEast_[v]      = (localEast[v] != 0.f);
    }
}

// ── Stage 4: the output-mask authoring push (see the hpp decl) ─────────────────
// The EditorState mask rows are the single authoring authority; this ONE copy per
// control block is the only path they reach the output layer through (the push
// runs AFTER pushVoiceLorEditor so the base rotation it re-pins against is the
// current one). Clamps: kind 0..2; the pattern bits arrive as the DECIDED uint64
// integer representation, ring-masked (the store boundary clamps on write).
// followR/local != 0. The ALL-CYCLE default (kinds 0, patterns all-on) lands
// byte-identically on the reset() seed — no gating anywhere, the goldens' output
// path is untouched. The ANCHOR LAW lives here (the push IS the per-block
// "per step" check): a pushed pattern that differs from the last pushed pattern is
// a PAINT (or a patch load) — the working copy re-anchors at the pushed base R;
// an unchanged pattern whose base R moved is a KNOB TURN — the working copy
// re-pins (rotates delta steps EARLIER, the content direction) so mask-step-1
// stays pinned to the base rotation. followR-off rows (abs2) never re-pin.
void SequencerEngine::pushMaskEditor(
    const uint64_t* macroPatternBits, float macroKind, float macroFollowR,
    const uint64_t* eastPatternBits, const float* eastKind,
    const float* eastFollowR, const float* localEast) {
    auto kindInt = [](float x) -> int {
        return (int)std::lround(std::clamp(x, 0.f, 2.f));
    };
    // STEPS_32 Stage 2a (the DECIDED block): the editor mask rows ARE the integer
    // representation now (uint64, ring-masked at the store boundary) — the float clamp
    // (0..65535 lround) is gone with the float wall it guarded.
    // Macro tier first (the GLOBAL mask every follower reads).
    {
        const uint64_t painted = macroPatternBits[0] & kMaskLenMask;
        const int kind         = kindInt(macroKind);
        const bool followR     = (macroFollowR != 0.f);
        const int  baseR       = macroVoiceLor_[LOR_ROT];
        if (painted != maskMacroLastPushed_) {              // repaint / load: re-anchor
            maskMacroAnchored_    = painted;
            maskMacroAnchorR_     = baseR;
            maskMacroLastPushed_  = painted;
        } else if (followR && baseR != maskMacroAnchorR_) { // knob turn: re-pin
            maskMacroAnchored_    = rotMaskRing(maskMacroAnchored_, baseR - maskMacroAnchorR_);
            maskMacroAnchorR_     = baseR;
        }
        maskMacroKind_    = kind;
        maskMacroFollowR_ = followR;
    }
    for (int v = 0; v < kVoiceSlots; ++v) {
        const uint64_t painted = eastPatternBits[v] & kMaskLenMask;
        const int      kind    = kindInt(eastKind[v]);
        const bool     followR = (eastFollowR[v] != 0.f);
        maskLocalEast_[v]      = (localEast[v] != 0.f);
        const int  baseR       = voiceLorStore_[v][LOR_ROT];   // the row's base rotation
        if (painted != maskLastPushed_[v]) {                    // repaint / load: re-anchor
            maskAnchored_[v]    = painted;
            maskAnchorR_[v]     = baseR;
            maskLastPushed_[v]  = painted;
        } else if (followR && baseR != maskAnchorR_[v]) {       // knob turn: re-pin
            maskAnchored_[v]    = rotMaskRing(maskAnchored_[v], baseR - maskAnchorR_[v]);
            maskAnchorR_[v]     = baseR;
        }
        maskKind_[v]    = kind;
        maskFollowR_[v] = followR;
    }
}

bool SequencerEngine::advancePlayhead(int dir, const int8_t* voiceMotion) {
    int prevStep = stepIndex;
    lastPlayDir = (dir < 0) ? -1 : +1;
    scanOfsV_[0] = 0;   // THE FREEZE FIX: any motion re-anchors — each arrived step is judged on its own value first

    // ── Per-lane direction — the HOLDER model ───────────────────────────────────
    // Only the flip QUANTIZATION keeps pending state: promote the pending dir at the step edge
    // (Phrase promotion still happens at the wrap, below). Everything else — the position of
    // every lane — advances LOCALLY from the lane's own phase (the holder law; the old
    // recompute-from-the-counter is gone, and no lane carries an accumulator that can drift
    // away from the transport — the phases are congruent to their drivers by construction).
    // See laneTickFor().
    // Promote pending direction at the step edge -- BUT NOT while locked. Direction is LATCH
    // (LOCK_SEMANTICS §9): a direction change made under lock must freeze and take effect at
    // unlock, not commit at the next step edge. locked is this engine's live lock (aliased as
    // Monsoon::locked, toggled by the user). Gating here staged-promotes: pending holds the new
    // dir, and the first unlocked step edge commits it.
    // SCOPE (LOCK_SCOPE_MENU): a strand promotes under lock IF its Sands scope bit is set (direction
    // opted out of the freeze for that axis). dirLive_(strand) = unlocked OR that strand's bit is live.
    if (laneFlipQuant == LaneFlipQuant::StepEdge) {
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            if (!dirLive_(l)) continue;   // this strand's direction stays latched
            laneDir_[l] = laneDirPending_[l];
            // v starts at 0 (resolver step 4 — DIR-SLOT0-ROW-INERT closed): slot 0's poly
            // row now promotes for STORE consistency. The manager resets + pushes pending
            // for all 16 slots; the engine silently dropping slot 0 was the seam. NO
            // consumer reads laneDirV_[0][*]: the recompute loops below stay v = 1..
            // UNTOUCHED (widening THEM would touch laneTickV_/laneSignV_ slot-0 semantics
            // for zero readers), and V1's audio reads the MONO channel laneDir_[l] — this
            // change cannot move the audio.
            for (int v = 0; v <= numPolyVoices; ++v) laneDirV_[v][l] = laneDirVPending_[v][l];
        }
    }

    // ── THE POSITION-HOLDER TICK (POSITION_HOLDER_MODEL.md) ─────────────────────
    // There is no global counter to bump. The advance event is the transport MOTION:
    // every holder advances ITSELF — the global walk ±1 (mod the window), each voice's
    // frame by its effective-clock delta k_v (its OWN motion + its advanceOffset_ diff),
    // every lane walk by its driver's raw tick delta — and all read caches re-derive
    // from the phases. With unchanged parameters this reproduces the old
    // f(totalStepsElapsed) closed forms bit-identically: the walks are periodic in t0
    // and the phases carry exactly that congruence (see tickHolders in the machinery
    // block below). laneSign_* are derived for the UI cue only — nothing reads them to
    // decide position.
    //
    // Why holders and not the old f(counter) (docs/design/POSITION_HOLDER_MODEL.md):
    //   * Every position being a function of ONE global counter conflated VOICE, LANE
    //     and POSITION into one timeline — every per-voice/per-lane feature was fighting
    //     the architecture. The holders are the natural model: independent tracks each
    //     owning their position.
    //   * The old "no drift / reproducible randomness" defenses do not survive scrutiny:
    //     Philox draws are counter-addressed in their OWN space (unchanged here); local
    //     positions are deterministic from decisions + anchor.
    //   * A mid-run LENGTH re-author now re-bounds the wrap WITHOUT teleporting the walk
    //     (length = the wrap point, not part of the address) — which is exactly what
    //     makes 32 steps nearly free later: no global-modulus audit, no wrap-mask hunt.
    tickHolders(dir, dir, voiceMotion);

    bool wrapped;
    if (dir < 0) {
        // ── Reverse traversal: mirror of forward, leftward through the window. ──
        // From the "not started" state, seed just AFTER endStep so the first reverse
        // step lands on endStep (the symmetric counterpart of forward seeding before
        // startStep and stepping onto startStep).
        if (stepIndex == -1) stepIndex = (endStep + 1) & (kMaskRing - 1);
        stepIndex = (stepIndex - 1 + kMaskRing) & (kMaskRing - 1);
        if (!isStepInWindow(stepIndex)) stepIndex = endStep;
        for (int s = 0; s < kMaskRing && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex - 1 + kMaskRing) & (kMaskRing - 1);
        // Phrase boundary (reverse): wrapped back round to endStep.
        wrapped = (prevStep != -1 && stepIndex == endStep);
    } else {
        // ── Forward traversal (unchanged) ──
        if (stepIndex == -1) stepIndex = (startStep - 1 + kMaskRing) % kMaskRing;
        stepIndex = (stepIndex + 1) & (kMaskRing - 1);
        if (!isStepInWindow(stepIndex)) stepIndex = startStep;
        for (int s = 0; s < kMaskRing && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex + 1) & (kMaskRing - 1);
        // Return true if wrapped to start (phrase boundary)
        wrapped = (prevStep != -1 && stepIndex == startStep);
    }

    writeLedger.noteWrite(WriteRole::ENGINE, WriteField::Wrapped); // STEP1 WriteLedger: wrapped (W1/W2) — confirms single ENGINE writer

    // Pendulum/PingPong now bounce at the LOR window endpoint (handled above in the tick
    // advance loop), NOT at the phrase boundary. The old phrase-boundary auto-flip is removed
    // — it was the legacy lanePendulum_ mechanism, superseded by the per-lane endpoint check.
    // (If a lane's LOR length == the phrase length, the endpoint coincides with the wrap, so
    // the behaviour is the same. If the LOR is shorter, the bounce happens sooner — correct.)

    // Phrase: defer the manual flip to the wrap, so the next phrase plays in the new direction.
    // Only the DIR is promoted now — laneSign_/laneSignV_ are derived from the position each
    // step (see the recompute above), so promoting a "pending sign" would be dead motion.
    // Also gated on !locked (LATCH): a direction change under lock waits for unlock, same as
    // the StepEdge path above -- else direction would still commit under lock via Phrase.
    // SCOPE per-strand (LOCK_SCOPE_MENU): promote under lock only for strands whose direction bit is live.
    if (laneFlipQuant == LaneFlipQuant::Phrase && wrapped)
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            if (!dirLive_(l)) continue;
            laneDir_[l] = laneDirPending_[l];
            // v = 0 here too (resolver step 4 — DIR-SLOT0-ROW-INERT closed): slot 0's
            // poly row promotes for STORE consistency on BOTH flip quantizations (the
            // StepEdge site above carries the full note). NO consumer reads
            // laneDirV_[0][*] — the recompute loops stay v = 1.. untouched, and V1's
            // audio reads the MONO channel laneDir_[l] — this cannot move the audio.
            for (int v = 0; v <= numPolyVoices; ++v) laneDirV_[v][l] = laneDirVPending_[v][l];
        }

    return wrapped;
}

// STEP-ADVANCE (CC_PROMPT_GATE_STEP_ADVANCE_SPEC Stages 1-3; the draw source FIXED per
// CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE) — the gate-mode advance DECISION for one rise.
// GATE MODE ONLY (Mode B): in clock/phase mode p_advance is not used and Variation keeps
// its existing meaning (executeModeA never calls this).
dotModular::StepAdvance::Decision SequencerEngine::decideStepAdvance_(VoiceSlot vs, int subLevel, int drawTickDelta) {
    namespace SK = dotModular::SubgateKnobs;
    // The threshold, by LEVEL (the three-level hierarchy): a MAIN gate rise resolves to
    // the VARIATION probability (the per-voice resolver path — warps/correlates/CAs like
    // every lane, NOT the raw knob; BIPOLAR since Stage 2 — the signed advance value,
    // negative = p(backward)); a ratchet/ghost rise resolves to THIS slot's subgate
    // ADVANCE column (the Straits consumer-group store, three-way: own-subpanel claim >
    // {global | follow-main}). subResLive gates the subgate read exactly like the
    // rest/legato/accent reads in executeStep: an engine that has never been through
    // ModeController keeps reading the main value verbatim (rack-free byte-identity).
    const float p = (subLevel < 0 || !voices[vs].subResLive)
                  ? voices[vs].variationProb
                  : voices[vs].subRes[subLevel][SK::ADVANCE];
    // THE SCAN LOOKS WHERE THE PLAYHEAD IS GOING (the backward fix, Rodney): the draw
    // delta is SIGNED by p — p's sign IS the travel direction — so p must be computed
    // FIRST (the old order read u before p existed to sign it, so the scan always
    // walked forward even while the playhead walked backward). Forward travel (p >= 0)
    // reads +delta — byte-identical to the pre-fix engine; backward travel (p < 0)
    // reads -delta: a held rise scans the window BACKWARD for the escape value, and
    // the fall's +1 target becomes -1 (the incoming-rest step BEHIND — where a
    // backward advance would land). ONE signing site covers every caller (the rise
    // scan and the fall target both arrive here as raw deltas/magnitudes).
    const int signedDelta = (p < 0.f) ? -drawTickDelta : drawTickDelta;
    // THE DRAW SOURCE (the fix): u is the VARIATION LANE's step-addressed draw — the
    // same r_vary every lane-style compare reads — NOT a fresh Philox draw (the reserved
    // chunk is DELETED; the advance consumes ZERO new draws). HELD STEP IS CORRECT, NOT
    // A WALL: rereading a held step's r_vary gives the same comparison — to advance OFF
    // a held step you MOVE the Variation knob/CV toward ±1 until it crosses that step's
    // draw. The step-addressed draw is the PER-STEP THRESHOLD (different steps naturally
    // stickier/looser) — the intended live control. At |p|=1 every step's draw is crossed
    // → always move (the pre-feature identity); at p=0 → always hold. All levels read the
    // SAME current-step variation draw — the playhead is singular, mirroring how the
    // subgate rest/legato reads address their level: the THRESHOLD is level-specific,
    // the draw is the shared lane.
    // The draw addressing, parameterized by the SIGNED tick delta (the ONE expression,
    // also exposed as stepAdvanceDrawAt for the test mirror): 0 = the current step
    // (bit-identical to getVariationStep(), sign-irrelevant); +1 = the incoming-rest
    // step forward, -1 the one behind (the fall-advance, CC_PROMPT_TIE_INTO_REST_
    // PROBABILISTIC_ADVANCE — unscanned, the travel-signed target's own value); the
    // RISE passes the accumulated scanOfsV_[0] magnitude — THE FREEZE FIX
    // (CC_PROMPT_STEP_ADVANCE_FREEZE_AND_BIPOLAR): a held rise scans the variation
    // window IN THE TRAVEL DIRECTION (signed above), so a step whose own draw fails
    // the compare is NOT a permanent wall (the escape comes within ~1/|p| held rises,
    // either direction). getStrandIdx wraps by the variation lane's own length,
    // negative ticks included (the double-mod wrap) — the LOCAL window scan, never
    // the global clock.
    const float u = stepAdvanceDrawAt(vs, signedDelta);
    // SCOPE (the ONE PLAYHEAD law, the twin-collapse reading — CC_FIX_PER_VOICE_
    // ADVANCE_WANDER's DRY audit): ONE decision function, N voices — the transport
    // sites pass VoiceResolver::mono() (the ONE clock), the per-voice fold passes the
    // voice (its own threshold + its own window draw → the per-voice offsets). Mono is
    // the special case, not a separate implementation; never a second playhead.
    return dotModular::StepAdvance::decide(p, u);
}

// ── PER-VOICE STEP-ADVANCE: the fold ────────────────────────────────────────────
// Runs at every advance site (both rise executors + the tie-into-rest fall) AFTER the
// mono decision dMain (the transport decision) and BEFORE the transport call
// (advancePlayhead/recomputeLaneTicks) so the recompute sees the new offsets.
// THE EVERY-LANE PRINCIPLE (CC_FIX_PER_VOICE_ADVANCE_WANDER — the fix): advance
// resolves EXACTLY like rest/legato/accent/qmix — every voice v = 1..numPolyVoices
// ALWAYS decides on its OWN window draw (decideStepAdvance_(vs, …): its own resolved
// threshold — the store the resolver already folded global-vs-per-voice into, exactly
// restProb's pattern — and its own draw at its own scan depth (perVoiceScan) or the
// +1 fall target). ONE threshold control, many draws, many outcomes: even on a GLOBAL
// threshold the voices WANDER independently (different own-window draws).
// advanceOffset_[v] += d_v − dMain, so a voice's NET position change across the event
// is its OWN decision. A moved voice re-anchors its scan (0); a held voice scans its
// own window deeper (scanHoldV_); a per-voice arrival latch (its own fall advance)
// clears + rides 0 — "one advance, one arrival" per voice. Delegation NEVER gates the
// decision — it selects only the THRESHOLD source, upstream in the resolver (no lane
// has a "delegated → skip the decision" path, and advance no longer is the exception).
// Reads laneTickV_ PRE-advance (the decision sees the voice's current position); zero
// new draws.
bool SequencerEngine::foldPerVoiceAdvance_(int subLevel, bool perVoiceScan,
                                           dotModular::StepAdvance::Decision dMain, int8_t* motionV) {
    const int dMainStep = (dMain == dotModular::StepAdvance::FORWARD) ? +1
                        : (dMain == dotModular::StepAdvance::BACKWARD) ? -1 : 0;
    for (int i = 0; i < 16; ++i) motionV[i] = (int8_t)dMainStep;
    bool anyMoved = false;
    for (int i = 1; i <= numPolyVoices && i < kVoiceSlots; ++i) {
        const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
        int dStep = 0;                                     // a latched arrival rides 0 (moved at the fall)
        if (pendingCheckpointArrivalV_[i]) {
            pendingCheckpointArrivalV_[i] = false;
        } else {
            const int delta = perVoiceScan ? scanOfsV_[i] : 1;
            const dotModular::StepAdvance::Decision dv = decideStepAdvance_(vs, subLevel, delta);
            dStep = (dv == dotModular::StepAdvance::FORWARD) ? +1
                  : (dv == dotModular::StepAdvance::BACKWARD) ? -1 : 0;
            if (dStep != 0) scanOfsV_[i] = 0;              // moved: re-anchor (its own value first)
            else            scanHoldV_(vs);                // held: scan its own window deeper
        }
        advanceOffset_[i] += (int64_t)(dStep - dMainStep);
        if (dStep != dMainStep) anyMoved = true;
        motionV[i] = (int8_t)dStep;
    }
    return anyMoved;
}

// ── THE POSITION-HOLDER MACHINERY (POSITION_HOLDER_MODEL.md) ─────────────────────
// The walk engine under the holder model. Every walk owns a bounded PHASE — a position
// in its OWN cycle, congruent to its driver's t0 (the 0-based walk coordinate) modulo
// the walk's OWN period — and the read caches derive from the phases through
// laneTickFor evaluated AT the phase (tick = phase + 1). No walk is a function of any
// shared counter; the only absolute-tick math left is the ANCHOR (reset / phase snap),
// where an absolute step legitimately seeds the congruences through the closed forms.
//
// THE CONGRUENCE LAW (why this is bit-identical to the old f(totalStepsElapsed)):
// each closed form is periodic in its t0 with the walk's own period P. A phase that
// stays ≡ t0 (mod P) therefore evaluates to the identical value FOREVER, provided it
// (a) was seeded congruent at an anchor and (b) advances by the driver's raw t0 delta
// each event. (b) is the whole game: a window wrap or an offset seed is a JUMP in the
// driver's tick value, and the phase absorbs the jump mod P — exactly what the closed
// form "sees" when it evaluates the jumped tick. Transport reversals need no extra
// state either: a backward march unwinds the same congruence (the bounce walks are
// time-symmetric — retracing the triangle IS the bounce walk with the phase running
// backward, verified step-for-step against laneTickFor).
//
// THE DRIVER CHAIN: global window walk (globalWalk_) → voice frame (the voiceLOR row
// walk; the effective clock t + advanceOffset_[v] rides INSIDE the frame's driver
// delta) → the lane walks (lanePhase_/lanePhaseV_/macroLanePhase_). The per-voice
// effective-clock delta is k_v = the transport motion + the voice's advanceOffset_
// diff vs advOffSeen_ — which reproduces the old bridge expression driverTickAt(t +
// advanceOffset_[v]) EXACTLY, including hand-seeded offsets (the PLUMB pins) and the
// activation zeroing (the diff absorbs the jump: the voice re-enters at the macro
// position, as the old f(t+0) read did).
//
// THE SIGN LAW (the old recomp's, verbatim): the cue reads the walk value at the phase
// minus the walk value one NORMALIZED musical step back — a driver wrap is a BEAT, not
// a bounce (the cue must not flip there); delta 0 (the PingPong endpoint repeat) keeps
// the previous sign. Evaluated on the phases this is bit-identical to the old
// f(tN) − f(tP) because the normalized predecessor's t0 ≡ phase − (musical step) and
// the walk formulas are evaluated RAW at the congruent representatives (the difference
// of the formula values is congruence-shift-invariant).

int SequencerEngine::wrapMod(int64_t v, int m) {
    int64_t r = v % m;
    if (r < 0) r += m;
    return (int)r;
}

int SequencerEngine::walkPeriod(LaneDir d, int len) {
    const int L = std::max(1, len);
    if (d == LaneDir::Pendulum) return (L < 2) ? 1 : 2 * (L - 1);
    if (d == LaneDir::PingPong) return 2 * L;
    return L;   // Forward / Reverse: the lane's own length IS the period
}

// THIS voice's LOR length for a strand (the walk's wrap point) — the same table the
// old recomp carried as a lambda in two places; one mechanism now.
int SequencerEngine::polyStrandLenOf(int v, int strand) const {
    switch (strand) {
        case dotModular::STRAND_MELODY:    return polyLenE(VoiceResolver::slot(v + 1), LaneId{PL_MELODY});
        case dotModular::STRAND_OCTAVE:    return polyLenE(VoiceResolver::slot(v + 1), LaneId{PL_OCTAVE});
        case dotModular::STRAND_QMIX:      return polyLenE(VoiceResolver::slot(v + 1), LaneId{PL_QMIX});
        case dotModular::STRAND_RHYTHM:    return polyLenE(VoiceResolver::slot(v + 1), LaneId{PL_REST});
        case dotModular::STRAND_ACCENT:    return polyLenE(VoiceResolver::slot(v + 1), LaneId{PL_ACCENT});
        case dotModular::STRAND_VARIATION: return polyLOR(VoiceResolver::slot(v + 1), LaneId{EDITOR_LANE_VARIATION}, LOR_LEN);
        case dotModular::STRAND_LEGATO:    return polyLOR(VoiceResolver::slot(v + 1), LaneId{EDITOR_LANE_LEGATO}, LOR_LEN);
    }
    return 16;
}

// The voice-frame walk at an effective tick (the A6 row law, UNIFIED ADDRESSING
// Stage 2/A6 + CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION): identity rows pass the
// global-window walk through verbatim. Mapped rows split by base mode:
//   ABS1 (voiceFrameReadsFullPhrase) — UNCHANGED: the walk runs the FULL PHRASE
//   (L→ring) over the GLOBALLY-WRAPPED driver tick, the rotation folded in the old
//   anchor form rho = (R-O) mod L — the one mode that cycles the global pattern
//   length (the doc's keep-exactly law).
//   CYCLE — the window walk folds the UNWRAPPED transport tick through the voice's
//   OWN L (Bug 1: the period is exactly L — the global window's wrap does not touch
//   it; a 12-step voice under a 16-step global no longer resets at the global wrap),
//   and R IS the window-relative phase at global step 1: rho = R (Bug 2, the anchor
//   law — independent of O; R is clamped to 0..L-1 at the authoring paths, so the
//   fold never sees an out-of-window R). The window's PLACEMENT rides the parent
//   frame (voiceRead_ = wnow + offG + O in the derive — the global offset is
//   placement, UAS lines 64/186-187).
// effTick1based may be ANY value congruent to the true 1-based effective clock modulo
// the relevant periods (the walk formulas mod locally). framePhaseOut (when non-null)
// receives the frame phase congruent to the frame walk's t0 (mod the row walk's
// period) — for the cycling fold that is the RAW effective clock's congruence (the
// holder advance and the derive's fold both key off it). Returns the 1-based frame
// tick the voice's LANES hang under.
int64_t SequencerEngine::voiceFrameTickAt(int v, int64_t effTick1based,
                                          int64_t* framePhaseOut) const {
    const int  lenG = std::max(1, cachedLength);
    const int  offG = cachedOffset;
    // (Rodney's correction) the default-pattern pass-through is the POLYMETER LAW (a
    // non-divisive lane free-runs against the pattern) — NOT the identity sentinel
    // (the other 16, which takes the cap in voiceLorIdentity). The cap-wide identity
    // WINDOW passes through the identity BRANCH, not here; an authored 32-pattern maps
    // (bar 2 is new content, the D3 extension law).
    const bool passG = (lenG == 16 && offG == 0);
    const int64_t tNowV = passG ? effTick1based
                                : driverTickAt(effTick1based, lenG, offG);
    if (voiceLorIdentity(v)) {
        if (framePhaseOut) *framePhaseOut = tNowV - 1;   // (identity rows have no frame walk — the lanes hang off this tick)
        return tNowV;
    }
    int L, O, R; LaneDir vd;
    effectiveVoiceLor(v, L, O, R, vd);
    if (L < 1) L = 1;
    // (A6) ABS1: the window walk runs the FULL PHRASE — L→16 for the READ only.
    if (voiceFrameReadsFullPhrase(v)) {
        L = kMaskRing;   // the FULL PHRASE = the ring
        const int64_t rho  = ((((int64_t)(R - O) % L) + L) % L);
        const int64_t wnow = ((((laneTickFor(vd, tNowV, L) + rho) % L) + L) % L);
        if (framePhaseOut) *framePhaseOut = wrapMod(tNowV - 1, walkPeriod(vd, L));
        return wnow + 1;
    }
    // CYCLE: the UNWRAPPED tick through the voice's own L; rho = R window-relative
    // (the store's R is already 0..L-1 — the authoring clamp — the fold adds no
    // silent re-interpretation).
    const int64_t rho  = (int64_t)R;
    const int64_t wnow = ((((laneTickFor(vd, effTick1based, L) + rho) % L) + L) % L);
    if (framePhaseOut) *framePhaseOut = wrapMod(effTick1based - 1, walkPeriod(vd, L));
    return wnow + 1;
}

// THE ANCHOR LAW. Seeds every holder's congruence from the transport's own position
// (the one absolute value the write sites maintain — see transportTick_): the phases
// all take the RAW clock congruence (config-independent — a window or row re-author
// never touches the raw clock, so the warm phases stay valid through any config
// round-trip), and the global walk holder takes (tick-1) mod the CURRENT window
// length (re-taken by setWindow on a mid-run re-author). reset() anchors at 0, the
// Mode E phase snap at the phase-derived tick, Monsoon's Beat-1 hard reset at 0.
void SequencerEngine::anchorTransport(int64_t tick1based) {
    transportTick_ = tick1based;   // THE WRITE SITE: the anchor IS the transport's own position update
    const int  lenG = std::max(1, cachedLength);
    const int64_t t0 = tick1based - 1;
    globalWalk_ = wrapMod(t0, lenG);
    {
        const int rLen = std::max(1, lor(dotModular::STRAND_RHYTHM, LOR_LEN));
        globalT0Phase_ = wrapMod(t0, rLen);
        globalT0Key_   = rLen;
    }
    for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
        const int len = std::max(1, strandLen(s));
        lanePhase_[s] = wrapMod(t0, walkPeriod(laneDir_[s], len));
        laneKey_[s]   = walkKeyOf(laneDir_[s], len);
        const int mlen = std::max(1, (s < dotModular::POLY_LANE_COUNT) ? macroLOR_[s] : strandLen(s));
        macroLanePhase_[s] = wrapMod(t0, walkPeriod(macroLaneDir_[s], mlen));
        macroLaneKey_[s]   = walkKeyOf(macroLaneDir_[s], mlen);
    }
    for (int v = 0; v < kVoiceSlots; ++v) {
        // The voice's RAW effective clock: tEffV = tick + offset (the old bridge
        // expression — the divergence rides THROUGH the anchor exactly as the old
        // f(t + offset) recompute did).
        const int64_t tEffV = tick1based + advanceOffset_[v];
        int64_t fp = 0;
        (void)voiceFrameTickAt(v, tEffV, &fp);
        voiceFramePhase_[v] = (int)fp;   // ≡ tEffV − 1 (mod the row walk's period)
        {
            int L, O, R; LaneDir vd;
            effectiveVoiceLor(v, L, O, R, vd);
            if (L < 1) L = 1;
            if (voiceFrameReadsFullPhrase(v)) L = kMaskRing;   // the FULL PHRASE = the ring
            frameKey_[v] = walkKeyOf(vd, L);
        }
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
            const int vlen = std::max(1, polyStrandLenOf(v, s));
            lanePhaseV_[v][s] = wrapMod(tEffV - 1, walkPeriod(laneDirV_[v][s], vlen));
            laneKeyV_[v][s]   = walkKeyOf(laneDirV_[v][s], vlen);
        }
        advOffSeen_[v] = advanceOffset_[v];   // the transport's shadow syncs at the anchor
    }
}

// THE ADVANCE EVENT. Advances every holder LOCALLY (no walk reads the transport tick —
// it only moved at the anchor), then derives every read cache. The phases advance by
// their RAW clock deltas (the transport motion / the voice's effective-clock delta k_v)
// so each phase stays congruent to its raw t0 mod its OWN period through any config;
// the derives pick the representative per config (the warm-phase law — see the hpp).
void SequencerEngine::tickHolders(int transportMotion, int signMotion, const int8_t* voiceMotion) {
    const int lenG = std::max(1, cachedLength);
    // THE WRITE SITE (the spec's law): the transport's own position advances with the
    // clock event — the ONE absolute value in the engine, consumed only by the anchors.
    // The offsets-only refresh (motion 0) leaves it verbatim.
    transportTick_ += transportMotion;
    const int64_t t0Now = transportTick_ - 1;   // the post-event raw t0 (the re-anchor coordinate)
    // ── the global walk holder (the mapped-config read coordinate) ──
    globalWalk_ = wrapMod((int64_t)globalWalk_ + transportMotion, lenG);
    // ── the raw-clock congruences (config-independent; the RE-ANCHOR LAW on a walk-key
    //    change: a live len/dir change re-bounds the period, the old congruence is
    //    meaningless for it, so the holder takes the closed form's exact value at the
    //    change — f(t, newParams) — from the transport's own position and continues
    //    locally from there, bit-identical to the old recompute in every case) ──
    {
        const int rLen = std::max(1, lor(dotModular::STRAND_RHYTHM, LOR_LEN));
        globalT0Phase_ = (globalT0Key_ != rLen)
            ? wrapMod(t0Now, rLen)
            : wrapMod((int64_t)globalT0Phase_ + transportMotion, rLen);
        globalT0Key_ = rLen;
    }
    for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
        const int mlen = std::max(1, (s < dotModular::POLY_LANE_COUNT) ? macroLOR_[s] : strandLen(s));
        const int key = walkKeyOf(macroLaneDir_[s], mlen);
        macroLanePhase_[s] = (macroLaneKey_[s] != key)
            ? wrapMod(t0Now, walkPeriod(macroLaneDir_[s], mlen))
            : wrapMod((int64_t)macroLanePhase_[s] + transportMotion, walkPeriod(macroLaneDir_[s], mlen));
        macroLaneKey_[s] = key;
    }
    // ── the voice frames + their lane phases (the RAW effective-clock deltas) ──
    for (int v = 0; v < kVoiceSlots; ++v) {
        // THE EFFECTIVE-CLOCK DELTA: the transport motion + this voice's offset diff.
        // The fold accumulates advanceOffset_ BEFORE the transport call, so the diff
        // here is exactly (d_v − dMain) and k_v = d_v — the voice's OWN decision. A
        // hand-seeded or activation-zeroed offset jumps the diff — the same jump the
        // old f(t + offset) read saw. [0] ≡ 0 ALWAYS: k_0 = the transport motion.
        const int64_t kV = (int64_t)transportMotion + (advanceOffset_[v] - advOffSeen_[v]);
        advOffSeen_[v] = advanceOffset_[v];
        const int64_t t0EffNow = t0Now + advanceOffset_[v];   // this voice's raw walk coordinate
        if (!voiceLorIdentity(v)) {
            // Mapped row: the frame phase advances by the RAW delta too (its congruence
            // is the raw effective clock's), and the mapped frame's read path derives
            // from it under the pass-through window (in deriveHolders).
            int L, O, R; LaneDir vd;
            effectiveVoiceLor(v, L, O, R, vd);
            if (L < 1) L = 1;
            if (voiceFrameReadsFullPhrase(v)) L = kMaskRing;   // the FULL PHRASE = the ring
            const int key = walkKeyOf(vd, L);
            voiceFramePhase_[v] = (frameKey_[v] != key)
                ? wrapMod(t0EffNow, walkPeriod(vd, L))
                : wrapMod((int64_t)voiceFramePhase_[v] + kV, walkPeriod(vd, L));
            frameKey_[v] = key;
        }
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
            const int vlen = std::max(1, polyStrandLenOf(v, s));
            const int key = walkKeyOf(laneDirV_[v][s], vlen);
            lanePhaseV_[v][s] = (laneKeyV_[v][s] != key)
                ? wrapMod(t0EffNow, walkPeriod(laneDirV_[v][s], vlen))
                : wrapMod((int64_t)lanePhaseV_[v][s] + kV, walkPeriod(laneDirV_[v][s], vlen));
            laneKeyV_[v][s] = key;
        }
    }
    // ── the MONO lane phases (V1's frame is their driver; k_0 = the transport motion:
    //     the offsets never move on slot 0 — the mono slot IS the transport) ──
    for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
        const int len = std::max(1, strandLen(s));
        const int key = walkKeyOf(laneDir_[s], len);
        lanePhase_[s] = (laneKey_[s] != key)
            ? wrapMod(t0Now, walkPeriod(laneDir_[s], len))
            : wrapMod((int64_t)lanePhase_[s] + transportMotion, walkPeriod(laneDir_[s], len));
        laneKey_[s] = key;
    }
    // ── derive every read cache from the (new) holders ──
    deriveHolders(signMotion, voiceMotion);
}

// THE DERIVATION PASS: all read caches from the CURRENT holders, no phase advance.
// THE REPRESENTATIVE LAW: under the PASS-THROUGH window (16/0) the walks read the raw
// clock congruences (the phases — the only carrier of the raw congruence mod a length
// that need not divide 16); under a MAPPED window every driver coordinate is the
// bounded window walk (exact — globalWalk_ + the offsets), so the walks read those and
// the warm phases wait for a pass-through round-trip. Either representative yields the
// exact values the old f(counter) recompute produced at the same instants.
void SequencerEngine::deriveHolders(int signMotion, const int8_t* voiceMotion) {
    const int  lenG = std::max(1, cachedLength);
    const int  offG = cachedOffset;
    // (Rodney's correction) the default-pattern pass-through is the POLYMETER LAW —
    // NOT the identity sentinel (the other 16).
    const bool passG = (lenG == 16 && offG == 0);
    const int  wG = globalWalk_;
    // ── the MACRO frame (the global window walk; no voice map — it IS the frame) ──
    macroFrameRead_ = wrapMod((int64_t)wG + offG, 16);
    {
        const int64_t gd = signMotion;   // the normalized global walk step (the wrap is a BEAT)
        if (gd > 0)      macroFrameSign_ = +1;
        else if (gd < 0) macroFrameSign_ = -1;   // 0 = held repeat: keep the previous cue
    }
    // ── the per-voice frames + the sign shifts their lanes read ──
    // (PART A, CC_PROMPT_MACRO_RESOLVER_AND_IDENTITY — bugs 19-21's mandate) the
    // loop goes through VoiceResolver: the VOICE NUMBER is the iteration domain
    // (kMonoVoice..kLastVoice), the storage slot is MINTED via voiceSlot(vn),
    // and the liveness is the resolver's isMono/isPoly — no raw v==0, no literal
    // slot 0 (a mono-specific value comes from the resolver, never a literal).
    // BEHAVIOUR-IDENTICAL: the body already resolved every live slot through
    // effectiveVoiceLor/voiceLorIdentity (the EFFECTIVE row), so all followers
    // share the Macro window (test_macro_shared_state gates 1-3 pin it — they
    // PASS on the pre-refactor branch); this is the mandated hygiene so the
    // loop can never diverge from the one addressing convention.
    int frameStep[16] = {};   // the frame's NORMALIZED musical step (the lane-sign shift)
    int frameT0[16]   = {};   // the frame's t0 representative for the lane derives
    for (int vn = dotModular::VoiceResolver::kMonoVoice; vn <= dotModular::VoiceResolver::kLastVoice; ++vn) {
        const int v = dotModular::VoiceResolver::voiceSlot(vn);   // the storage slot (V1→0 … V16→15)
        const int motionV = voiceMotion ? (int)voiceMotion[v] : signMotion;
        const int wGv = wrapMod((int64_t)wG + advanceOffset_[v], lenG);
        const bool live = dotModular::VoiceResolver::isMono(vn)
                       || (dotModular::VoiceResolver::isPoly(vn) && v <= numPolyVoices);
        const int64_t tNowV = passG ? (int64_t)(wGv + 1) : (int64_t)(wGv + offG + 1);
        if (!live) { voiceDriver_[v] = tNowV; voiceRead_[v] = 0; continue; }
        if (voiceLorIdentity(v)) {
            voiceDriver_[v] = tNowV;
            // (bug 17, LOR_EDITING_BUG_LIST) the RING fold — was % 16 (the 16-era
            // leftover): an identity voice's read cell FOLDED to bar 1, so with the
            // global window placed in bar 2 the strip's playhead left the window.
            // The ring fold keeps the read where the walk put it; for every ≤16
            // configuration the value is < 16 either way (bit-identical, goldens).
            voiceRead_[v]   = (int)(((tNowV - 1) % kMaskRing + kMaskRing) % kMaskRing);
#ifdef DOTM_DIAG
            if (std::getenv("DOTM_DIAG") && v < 3) {
                int _L,_O,_R; LaneDir _d; effectiveVoiceLor(v,_L,_O,_R,_d);
                fprintf(stderr, "[DIAG eng] v=%d IDENTITY L=%d O=%d R=%d voiceRead=%d macroFrameRead=%d passG=%d lenG=%d\n",
                        v,_L,_O,_R,voiceRead_[v],macroFrameRead_,(int)passG,lenG);
            }
#endif
            // The identity frame's sign: the walk's musical step with the wrap folded —
            // the driverTickBefore law on the bounded pair (the raw clock never jumps,
            // so pass-through reduces to motionV; the mapped window's jump normalizes).
            int64_t gd = motionV;
            if (!passG) {
                const int64_t tRawPrev = (int64_t)wrapMod((int64_t)wGv - motionV, lenG) + offG + 1;
                const int64_t dd = tNowV - tRawPrev;
                gd = (dd > 1 || dd < -1) ? (int64_t)motionV : dd;
            }
            if (gd > 0)      voiceSign_[v] = +1;
            else if (gd < 0) voiceSign_[v] = -1;
            frameStep[v] = (int)gd;
            frameT0[v]   = passG ? lanePhaseV_[v][0] : (wGv + offG);   // per-strand below; this is the default
            continue;
        }
        int L, O, R; LaneDir vd;
        effectiveVoiceLor(v, L, O, R, vd);
#ifdef DOTM_DIAG
        if (std::getenv("DOTM_DIAG") && v < 3)
            fprintf(stderr, "[DIAG eng] v=%d MAPPED L=%d O=%d R=%d dir=%d passG=%d lenG=%d\n", v,L,O,R,(int)vd,(int)passG,lenG);
#endif
        if (L < 1) L = 1;
        // CC_FIX_VOICE_WINDOW_CYCLE_AND_ROTATION — the base-mode split. ABS1 keeps the
        // landed fold EXACTLY (the globally-wrapped driver representative + rho=(R-O),
        // the one mode that cycles the global length). CYCLE folds the voice's OWN L
        // over the RAW effective-clock congruence (voiceFramePhase_[v] — maintained by
        // the raw kV advance for every mapped row, seeded raw by voiceFrameTickAt), so
        // the period is exactly L and the GLOBAL WINDOW'S WRAP IS A NON-EVENT (Bug 1 —
        // the old bounded representative wGv+offG jumped at the global wrap, making the
        // period the global length); rho = R (Bug 2 — the window-relative phase at
        // global step 1, independent of O; R arrives 0..L-1 from the authoring clamp);
        // and the window's PLACEMENT rides the parent frame — read = wnow + offG + O
        // (the OPEN: the global offset is placement, UAS 64/186-187; the old code
        // routed it into the PHASE through the same wrapped tick Bug 1 removes, so the
        // routing is re-added where the nesting puts it).
        const bool abs1Read = voiceFrameReadsFullPhrase(v);
        int64_t rho, ft0, frameBack;
        if (abs1Read) {
            L = kMaskRing;   // the FULL PHRASE = the ring
            rho = ((((int64_t)(R - O) % L) + L) % L);
            // The frame walk's t0 representative: the RAW effective-clock congruence under
            // the pass-through window (the frame phase), the bounded window walk under the
            // mapped window — the same value laneTickFor evaluated in the old recompute.
            ft0 = passG ? (int64_t)voiceFramePhase_[v] : (int64_t)(wGv + offG);
            // The RAW predecessor's walk value: the effective clock one motionV step back —
            // its t0 representative is (ft0 − frameBack), where frameBack is the true t0
            // gap (the raw clock: exactly motionV; the mapped window: the walk gap).
            frameBack = passG ? (int64_t)motionV
                : (int64_t)wGv - (int64_t)wrapMod((int64_t)wGv - motionV, lenG);
        } else {
            rho       = (int64_t)R;              // window-relative; clamped 0..L-1 at the authoring paths
            ft0       = (int64_t)voiceFramePhase_[v];   // the raw congruence — valid under BOTH windows
            frameBack = (int64_t)motionV;        // the raw clock's step gap (the pass-through law, any window)
        }
        const int64_t wnow = ((((laneTickFor(vd, ft0 + 1, L) + rho) % L) + L) % L);
        voiceDriver_[v] = wnow + 1;
        voiceRead_[v]   = (int)((wnow + (int64_t)(abs1Read ? (int64_t)O : ((int64_t)offG + O))) & (kMaskRing - 1));
        const int64_t wprev = ((((laneTickFor(vd, wrapMod(ft0 - frameBack, walkPeriod(vd, L)) + 1, L) + rho) % L) + L) % L);
        const int64_t dd    = voiceDriver_[v] - (wprev + 1);
        // A wrap (the voice's own window, or the global window beneath it) is a BEAT,
        // not a bounce: the musical predecessor is one step along the current motion.
        const int64_t vdSign  = (vd == LaneDir::Reverse) ? -1 : +1;
        const int64_t dstep   = (dd > 1 || dd < -1) ? (int64_t)motionV * vdSign : dd;
        if (dstep > 0)      voiceSign_[v] = +1;
        else if (dstep < 0) voiceSign_[v] = -1;
        frameStep[v] = (int)dstep;
        frameT0[v]   = (int)wnow;   // the mapped row's lanes hang off the frame VALUE
    }
    // ── the lane walks (the positions every reader consumes + the UI sign cues) ──
    auto laneRecomp = [&](LaneDir d, int len, int64_t t0rep, int motionNorm, int& tickOut, int& signOut) {
        const int P = walkPeriod(d, len);
        // The position: the walk formula AT the driver's t0 representative, wrapped by
        // the lane's OWN length (the old recomp's local wrap; pendulum/pingpong are
        // already in [0, L−1]).
        const int64_t cur = laneTickFor(d, t0rep + 1, len);
        tickOut = (int)(((cur % len) + len) % len);
        // The sign: the walk value one NORMALIZED musical step back (the raw formula
        // values — the difference is congruence-shift-invariant, so this is the old
        // f(tN) − f(tP) exactly). delta 0 = the endpoint repeat: keep the previous sign.
        const int64_t prv = laneTickFor(d, (int64_t)wrapMod(t0rep - motionNorm, P) + 1, len);
        const int64_t delta = cur - prv;
        if (delta > 0)      signOut = +1;
        else if (delta < 0) signOut = -1;
    };
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
        const int len = std::max(1, strandLen(l));   // mono strand length (mono tick)
        // The MONO lanes hang under V1's frame: its t0 representative per strand (the
        // identity row: the raw-clock phase under pass-through, the bounded walk under
        // the mapped window; the mapped row: the frame VALUE).
        const int64_t monoT0 = voiceLorIdentity(0)
            ? (passG ? (int64_t)lanePhase_[l] : (int64_t)(wrapMod((int64_t)wG + advanceOffset_[0], lenG) + offG))
            : (int64_t)frameT0[0];
        laneRecomp(laneDir_[l], len, monoT0, frameStep[0], laneTick_[l], laneSign_[l]);
        for (int v = 1; v <= numPolyVoices; ++v) {
            const int vlen = std::max(1, polyStrandLenOf(v, l));   // THIS voice's LOR length
            const int64_t voiceT0 = voiceLorIdentity(v)
                ? (passG ? (int64_t)lanePhaseV_[v][l] : (int64_t)(wrapMod((int64_t)wG + advanceOffset_[v], lenG) + offG))
                : (int64_t)frameT0[v];
            laneRecomp(laneDirV_[v][l], vlen, voiceT0, frameStep[v], laneTickV_[v][l], laneSignV_[v][l]);
        }
    }
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
        // macroLOR_ is EDITOR-lane indexed for the POLY lanes (0..4 = MEL/OCT/QMIX/REST/ACC);
        // VAR/LEG (5/6) have no global LOR, so they use the mono strandLen (see
        // advancePlayhead's old note — POLY_LANE_COUNT covers every poly lane).
        const int mlen = std::max(1, (l < dotModular::POLY_LANE_COUNT) ? macroLOR_[l] : strandLen(l));
        const int64_t macroT0 = passG ? (int64_t)macroLanePhase_[l] : (int64_t)(wG + offG);
        laneRecomp(macroLaneDir_[l], mlen, macroT0, signMotion, macroLaneTick_[l], macroLaneSign_[l]);
    }
}

void SequencerEngine::recomputeLaneTicks(const int8_t* voiceMotion) {
    // THE OFFSETS-ONLY REFRESH: the clock held (transportMotion 0) but diverged voices
    // moved — the holders tick by the per-voice offset diffs alone, then everything
    // re-derives. The sign cues read lastPlayDir (the edge's motion direction), exactly
    // what the old recompute passed as dirMotion after external events.
    tickHolders(0, lastPlayDir, voiceMotion);
}

// PHASE TIER Stage 1 — THE PHASE-STEP RULE (one rule, both sites).
// The phase addresses the pattern by its MUSICAL CYCLE position: the unwrapped absStep
// (pulsePos/p16 — grows across span wraps) is reduced mod the ACTIVE length, then seated
// in the window at startStep. This is the generalisation of the spec's hardcoded
// `phaseStep = absStep % 16` (on the old 16-ring, ring == length == cycle and all three
// reductions coincided; on the 32-ring they are three different numbers and the cycle
// reduction is the one that closes the loop with the transport walk, which cycles the
// window). Consequences, both pinned in test_phase_tier:
//   - at the default window (16 / offset 0) this == absStep % 16 EXACTLY — bar-1
//     landings bit-identical, and the drift check stays consistent with the walk
//     advance in every bar (no per-sample re-snap);
//   - at 32 the cycle runs 0..31 — the full-span sweep covers all 32 steps, period 32;
//   - a bar-2+ jump lands on the MUSICAL step (cycle k -> window cell k) instead of
//     parking at startStep — the pre-Stage-1 behaviour here was a steps-32 regression:
//     the kMaskRing flip left snapToPhaseStep reducing mod 32, so a bar-2 landing fell
//     outside a 16-window, the walk parked it at startStep, and the drift check (still
//     mod 16) disagreed with the parked step every sample — an endless per-sample
//     re-snap strobe. The cycle rule repairs it.
// startStep + cycle is always in-window by construction (the window IS
// [startStep, startStep+length) ring-wrapped); the caller's window walk below stays as
// defence-in-depth and never fires.
int SequencerEngine::phaseStepFor(long absStep) const {
    const int len = cachedLength > 0 ? cachedLength : 1;
    int cycle = (int)(((absStep % len) + len) % len);      // the musical cycle position
    return (startStep + cycle) & (kMaskRing - 1);          // seated in the window (ring is a power of two)
}

void SequencerEngine::snapToPhaseStep(long pulsePos, int pps) {
    // Snap the transport (stepIndex + the holder congruences) to the phase-derived
    // pulse position, then re-derive the lane caches from the anchor. Used after a Mode
    // E phase jump to eliminate the rounding drift from jumpSteps: the replay
    // advances by the ROUNDED step count, but the actual phase position is at
    // pulsePos/pps (which may be fractional). Without this snap, each sweep of the
    // phase knob lands on a different step.
    // CC_PROMPT_PPQN_48: the divider is PULSES_PER_STEP (6, fixed) — the STEP-indexed
    // division at any PPQN (at 24 the old p16, bit-identical).
    long absStep = pulsePos / pps;               // absolute step (unwrapped, 0-based; grows across span wraps)
    // PHASE TIER Stage 1: the landing step resolves through THE PHASE-STEP RULE (above)
    // — the musical cycle position seated in the window. Was absStep % kMaskRing + the
    // window walk (which parked any out-of-window landing at startStep — the bar-2
    // strobe regression the cycle rule repairs).
    int phaseStep = phaseStepFor(absStep);
    // THE ANCHOR: the phase-derived tick is 1-based (the old counter was; the walks'
    // t0 = tick − 1). The per-voice offsets ride THROUGH the snap (the anchor seeds each
    // voice's phase at tick + offset — the divergence persists, exactly as f(t+offset) did).
    anchorTransport((int64_t)absStep + 1);
    stepIndex = phaseStep;
    // Apply window checking (match advancePlayhead's behavior) — defence-in-depth;
    // phaseStepFor is in-window by construction, so this never fires.
    if (!isStepInWindow(stepIndex)) {
        stepIndex = startStep;
        for (int s = 0; s < kMaskRing && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex + 1) & (kMaskRing - 1);
    }
    deriveHolders(lastPlayDir, nullptr);
}

void SequencerEngine::updateWindow(float lenParam, float lenCv, bool lenPatched, float offParam, float offCv, bool offPatched) {
    // STEPS_32 Stage 1 close-out (Rodney's PR #310 review): the knob's range derives from
    // kPatternLengthCap (the USABLE bound — MonsoonConfigurator configures 1..cap), so every
    // request is deliverable; the clamp stays as defence-in-depth for the CV path and any
    // future caller. Identical at cap 16.
    int length = pe_clamp<int>((int)std::round(lenParam), 1, dotModular::kPatternLengthCap);
    int offset = (int)std::round(offParam) & (kMaskRing - 1);   // Stage 2a: the offset domain derives from the ring (0..ring-1)

    if (lenPatched) {
        float cv = pe_clamp(lenCv, 0.f, 10.f);
        // STEPS_32 Stage 1: cap-derived — *15 at the storage bound 16 (bit-identical mapping);
        // D1's *31 rescale lands automatically when Stage 2 flips kPatternLengthCap.
        length = pe_clamp<int>((int)std::round(cv / 10.f * (dotModular::kPatternLengthCap - 1)) + 1, 1, dotModular::kPatternLengthCap);
    }
    if (offPatched) {
        float cv = pe_clamp(offCv, 0.f, 10.f);
        offset = ((int)std::round(cv / 10.f * (float)(kMaskRing - 1))) & (kMaskRing - 1);   // cap-derived (was *15)
    }
    setWindow(length, offset);
}

int SequencerEngine::computeNoteLengthIdx(int requestedIdx, int ppqnMask) const {
    int idx = pe_clamp(requestedIdx, 0, NUM_NOTE_VALUES - 1);
    while (idx > 0 && !(NOTE_VALUES[idx].allowedPPQN & ppqnMask)) {
        idx--;
    }
    return idx;
}

int SequencerEngine::getNoteLenIdx(float baseNoteParam, const PatternInput& input, float r) {
    int baseIdx = pe_clamp<int>((int)std::round(baseNoteParam), 0, NUM_NOTE_VALUES - 1);
    // CC_PROMPT_PPQN_48: the availability mask reflects the REAL ppqnSetting (the
    // old note hardcoded 4 = PPQN24 while the stale comment claimed 24/48/96 — the
    // PPQN_48_96_NOT_SAFE reconciliation). At 48 every one of the 13 values
    // resolves to an integer pulse count (bit 8), so the mask unlocks nothing new
    // there; at 24 it is the same bit-4 set as today. (The legacy 1/4 PPQN
    // settings map to their own bits; they are unreachable — the menu offers
    // 24/48 and persistence migrates anything below 24.)
    int ppqnMask = ppqnBitOf(ppqnSetting);
    baseIdx = computeNoteLengthIdx(baseIdx, ppqnMask);
    return pe.varyNoteIndex(baseIdx, input, r);
}

void SequencerEngine::rebuildScaleCache(const float weights[], int n) {
    activeSemiCount = 0;
    for (int s = 0; s < n; ++s) {
        float w = pe_clamp<float>(weights[s], 0.f, 1.f);
        faderCache[s] = w;
        if (w > 0.f) {
            activeSemiList[activeSemiCount] = s;
            activeSemiWeight[activeSemiCount] = w;
            ++activeSemiCount;
        }
    }
}

float SequencerEngine::getStepLightBrightness(int lightIdx) const {
    // ── STEPS_32 Stage 3 — the twice-around-16 display lap (D10 as amended by D15) ──
    // lightIdx is the LAMP index (0..15 — the panel's 16 ring lamps STAY, the D15
    // 16-stay form). Lamp k carries masters k and k+16, and the rendered master is the
    // CURRENT LAP's: the lap is D10's split test on the playhead itself (stepIndex >= 16),
    // the SAME variable the BAR LEDs consume — one gate, two surfaces, no second lap
    // source exists. At a ≤16 pattern the playhead never leaves bar 1, so lapBase is 0
    // and master == lightIdx — bit-identical to the pre-twice-around read (the GOLDEN
    // LAW's display face).
    const int lapBase = (stepIndex >= 16) ? 16 : 0;
    const int master  = lapBase + lightIdx;
    bool inWindow = isStepInWindow(master);
    float baseActive = 0.0f;

    if (inWindow) {
        // Calculate distance from current playhead to map physical LED to drifting DNA content
        int relCurrent = (stepIndex - startStep + kMaskRing) % kMaskRing;
        // relLight reads the CURRENT-LAP master (was lightIdx — the D15 re-scope: the
        // lamp shows the content of the master it carries on the lap being rendered).
        int relLight = (master - startStep + kMaskRing) % kMaskRing;
        // POSITION_HOLDER_MODEL: the light's DNA-content tick is the GLOBAL window
        // walk's t0 shifted by the phrase offsets — a congruence no mod-16 walk
        // carries (the RHYTHM lane's own length need not divide 16), so it has its
        // OWN holder: globalT0Phase_ ≡ the offset-less global walk coordinate (mod
        // rLen), advanced by the same deltas the global walk takes. The read adds
        // the window offset (pass-through: 0 — the expression below is then the old
        // f(counter) read verbatim). Subtract nothing else: the phase IS the 0-based
        // walk coordinate (matching laneTick_, which laneTickFor returns 0-based).
        // getStrandIdx mods by the same len below, so this is congruent, negative-safe,
        // read-identical.
        const int rLen = std::max(1, lor(dotModular::STRAND_RHYTHM, LOR_LEN));
        const int64_t tick64 = (int64_t)globalT0Phase_ + cachedOffset - relCurrent + relLight;
        const int tickForLight = (int)(((tick64 % rLen) + rLen) % rLen);

        int dnaIdx = getStrandIdx(tickForLight, lor(dotModular::STRAND_RHYTHM,LOR_LEN), lor(dotModular::STRAND_RHYTHM,LOR_OFF), lor(dotModular::STRAND_RHYTHM,LOR_ROT));
        // (ITEM 1) The step LED's "is this a note" — the corrected LIVE prediction
        // (POLARITY_TABLE §3, Rodney-confirmed): the COMPLEMENT of the audio's rest
        // law (`r_rest < restTh` → rest), i.e. `draw >= restTh` is a note — evaluated
        // on the LIVE warp row + the live resolved per-voice threshold, so the LEDs
        // track REST edits, LOR moves and warp as they happen. The old precomputed
        // rhythmPattern cache (a stale generation-time prediction, persisted
        // pointlessly) is DELETED. The subgate's per-subLevel tier is a play-time
        // refinement the prediction does not chase — the base threshold is shown.
        const VoiceSlot dispV = pe.displayedVoice();
        bool isNote = pe.warpRow(dispV, LaneId{PL_REST})[dnaIdx] >= voices[dispV].restProb;
        baseActive = isNote ? 0.35f : 0.07f;
    } else {
        // THE GHOST ARC (D10 (1)): an out-of-window lamp lights a dim ghost instead of
        // the dead 0.0. The four standing levels keep their values VERBATIM — playhead
        // 1.0 > comet trail 0.5 > in-window note 0.35 > in-window rest 0.07 — and the
        // ghost pins STRICTLY BELOW the in-window rest, so a ghost can never read as a
        // rest (the hierarchy is the law; the exact value is a P5 calibration lever).
        // Live/post-CV for free: windowMask is rebuilt by setWindow from updateWindow's
        // post-CV values, and the ring feed re-renders every UI frame — knob, CV and
        // D3-wrap windows all render the same way.
        baseActive = kStepGhostBrightness;
    }

    // The moving playhead should always follow the global timeline index. Shown for
    // stepped modes A/B/C (0/1/2), the phase modes E/F (4/5); Mode D (3) is continuous
    // (no discrete playhead step). Mode F (5, Q3b) is a phase-1/16 stepped quantiser, so it
    // has a discrete playhead exactly like the phase origin.
    bool steppedMode = (modeSelect == 0 || modeSelect == 1 || modeSelect == 2);  // clock/gate/phase (collapse)
    // Playhead lamp: lightIdx == stepIndex % 16 — the ONE sanctioned display-path % 16
    // (the D15 ring-feed law). The stepIndex >= 0 guard keeps the pre-start park (-1)
    // dark: the old `lightIdx == stepIndex` comparison was accidentally -1-safe, the
    // & 15 landing is not (-1 & 15 == 15 would light lamp 15 before the first tick).
    float current = (steppedMode && stepIndex >= 0 && lightIdx == (stepIndex % 16)) ? 1.0f : 0.0f;

    // Direction cue (Mode E especially): a one-LED comet trail BEHIND the playhead in
    // the travel direction, so forward vs reverse is visibly distinct. The trailing
    // LED is the step the playhead just left: master (stepIndex - lastPlayDir) on the
    // ring; its LAMP is that master & 15 (twice-around), and its window test reads the
    // TRAIL MASTER itself — crossing the bar line backwards, the step just left belongs
    // to the other lap, whose master is not the lamp's current-lap one. (The 2b flip
    // widened kMaskRing to 32 and silently killed the trail on the whole second bar —
    // `lightIdx == trailIdx` could never match a 16..31 trailIdx; the lamp landing
    // repairs it. The stepIndex >= 0 guard makes the pre-start park deterministic.)
    if (steppedMode && stepIndex >= 0 && current < 1.0f) {
        int trailMaster = ((stepIndex - lastPlayDir) % kMaskRing + kMaskRing) % kMaskRing;
        if (lightIdx == (trailMaster % 16) && isStepInWindow(trailMaster))
            baseActive = std::max(baseActive, 0.5f);
    }

    return std::max(baseActive, current);
}

// ── STEPS_32 Stage 3 — the BAR 1/BAR 2 LEDs (D10 (2) as amended by D15) ──────────
// The inner-disc bar indicator's two lamps INSIDE the ring's unused center (upper =
// bar 1, lower = bar 2 — the disc's hemisphere cut; 16-step patterns keep the disc
// dark-except-when-playing: the playhead never leaves bar 1, so BAR 2 stays dim and
// BAR 1 carries the light — the "hidden disc" lean's two-LED form). Lit EXCLUSIVELY
// from the playhead's own gate and variable — the same steppedMode expression and the
// same stepIndex the ring's current-step lamp consumes (one gate, two surfaces — the
// sync must never fork). Where no discrete playhead exists (mode D, 3) — and before
// the first tick (stepIndex -1) — BOTH are silent. The inactive bar's LED DIMS, never
// dark: the disc's "every other segment dims" law, clearly distinct from both the lit
// state and the unlit ring lamps. The exact dim is a P5 calibration lever.
float SequencerEngine::getBarLightBrightness(int bar) const {
    bool steppedMode = (modeSelect == 0 || modeSelect == 1 || modeSelect == 2);
    if (!steppedMode || stepIndex < 0) return 0.f;
    const bool bar2 = stepIndex >= 16;   // D10's split test — reads the bar layer, wraps nothing
    return ((bar == 1) == bar2) ? 1.f : kBarIndicatorDimBrightness;
}

int SequencerEngine::getOffsetStep() const {
    // Legacy support for global pattern engine visual lookups.
    // It now uses the Melody strand as the anchor for visualization.
    return getMelodyStep();
}

bool SequencerEngine::shouldTriggerStep(int ppqn) const {
    if (stepIndex == -1) return false;
    if (ppqn <= 1) {
        // Only trigger on main 4 beats relative to startStep + offset
        int relative = (stepIndex - startStep + kMaskRing) % kMaskRing;
        return (relative & 0x03) == 0;
    }
    return true; 
}

StepResult SequencerEngine::executeStep(int nvIdx, float r_rest, float r_legato_tie, float r_accent, float r_qmix, const PatternInput& input, bool wasHeld, bool hadTail, int subLevel) {
    // Phase A: ALL V1 per-voice params (rest, legato, accent, qmix, variation) now read from
    // voices[monoSlot].* — no separate parameters. V1 = voice 0 of the unified per-voice system.
    // The mono slot is populated by the controller (updatePatternInput), not by the mode executors.
    // CC_PROMPT_STRONG_TYPES: the mono ROLE is TYPED — monoSlot is the minted anchor
    // (was the bare voices[0] hardcode, indistinguishable from residue).
    const VoiceSlot monoSlot = VoiceResolver::mono();
    // SUBGATE TIER (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC): subLevel — −1 = the MAIN gate
    // edge (read the main thresholds, the pre-subgate behaviour verbatim); otherwise a
    // SubgateKnobs level (RATCHET/GHOST) whose edge this is — read the RESOLVED per-voice
    // subgate thresholds (voices[monoSlot].subRes, written by ModeController through the
    // three-way resolver). BYTE-IDENTITY: with nothing authored the resolved values EQUAL
    // the main values exactly, so a ratchet/ghost edge shapes bit-identically to before.
    // The variation-strand gates are NOT re-pointed here: ghost candidacy stays on the
    // main VARIATION lane, and the ADVANCE columns are consumed UPSTREAM of this call —
    // at the advance site (decideStepAdvance_, before advancePlayhead; the playhead
    // decision precedes this step's articulation). The rolls below stay
    // rest/legato/accent (the step-advance Stage 1/3 build).
    namespace SK = dotModular::SubgateKnobs;
    const float restTh = (subLevel >= 0 && voices[monoSlot].subResLive) ? voices[monoSlot].subRes[subLevel][SK::REST]
                                                                        : voices[monoSlot].restProb;
    const float legTh  = (subLevel >= 0 && voices[monoSlot].subResLive) ? voices[monoSlot].subRes[subLevel][SK::LEGATO]
                                                                        : voices[monoSlot].legatoProb;
    const float accTh  = (subLevel >= 0 && voices[monoSlot].subResLive) ? voices[monoSlot].subRes[subLevel][SK::ACCENT]
                                                                        : voices[monoSlot].accentProb;
    // ── Fractional notes (1/4T=2.667, 1/8T=1.333, 1/32=0.5 steps) & legato/tie ──
    // These notes end MID-STEP (closed by the gateSecRemain seconds-timer), not on
    // a 1/16 grid edge. Legato/tie decisions only happen AT an edge and require the
    // previous note to still be sounding (wasHeld). That gives three rules, all
    // verified to hold with the current engine — they are by-design, not bugs:
    //
    //  1. A fractional note CANNOT be the FIRST/source note of a legato or tie.
    //     It has already closed before the next edge, so wasHeld is false there and
    //     the legato/tie branch is unreachable — the next step starts fresh. This
    //     is musically correct: a sub-step note leaves a real gap, which is not
    //     legato. (Design choice "A": accepted, not patched.)
    //
    //  2. A fractional note CAN be the LAST/destination note of a legato or tie.
    //     The gate is already open from the held source note; extendHold()/
    //     slideNote() add the fractional length and re-arm gateSecRemain on the
    //     summed hold, so the gate rides open to the exact sub-step end.
    //     e.g. 1/8→1/4T = 4.667 steps, 1/4→1/8T = 5.333, 1/8→1/32 = 2.5 (exact).
    //
    //  3. A fractional note CANNOT be a MIDDLE note of a 3-note tie. To be a middle
    //     note it would have to be both a destination (rule 2, OK) AND a source
    //     (rule 1, impossible). After it closes mid-step, the third note finds
    //     wasHeld=false at its edge and retriggers as a NewNote — the tie chain
    //     breaks cleanly at the fractional note, with no false reopened gate.
    StepResult result;
    result.nvIdx = nvIdx;

    // Mid-note: previous note's hold is still counting down.
    // Poly voices seeing MidNote will tick their own holds and return.
    //
    // The guard must also stay in MidNote while a fractional sub-step tail is
    // still sounding: a triplet (1/4T = 2.667 steps, 1/8T = 1.333) leaves
    // holdRemain < 1 on its final partial step while the precise gate timer
    // (gateSecRemain) is still open. Without the gateSecRemain check, that last
    // step fired a NEW note and cut the triplet to a whole-step length — 1/4T
    // played as 1/8, 1/8T as 1/16. (1/32 = 0.5 steps closes within its own step
    // before any edge, so it was unaffected — matching the observed scope.)
    if (gs.holdRemain >= 1.f || gs.gatePulseRemain > 0) {
        result.decision = MonoDecision::MidNote;
        result.accented = lastStepResult.accented;
        result.forStep = stepIndex; lastStepResult = result;
        return result;
    }

    int   sem    = 0;
    // ── QMIX per-step source-select (mono/voice-0), CA-routed downstream of Change Alley ─────────
    // In quantiser modes, q-mix decides PER STEP whether the CV-out pitch comes from the quantised
    // external CV (default) or the internally generated melody. Per QMIX_LANE_PARITY §"The blend"
    // BOTH mux operands and the threshold are CA outputs (positioned AFTER CA):
    //   • THRESHOLD  — r_qmix is drawn from the q-mix probability array, which is scattered by the
    //     green qmixSrc plane (caQmixSrc): PatternEngine::remapSlewedByPins remaps slewedQmix by
    //     caSrcRow(row, STRAND_QMIX) → caQmixSrc, so the draw voice-0 thresholds on is already the
    //     CA-selected source voice's q-mix. No extra indexing here (would double-permute).
    //   • INPUT-CV operand — routed inside voicePitch via caInputCvSrcRow (CA melody plane).
    //   • GENERATED operand — genPitchLive, unchanged.
    // Level scales the probability; qmixSrc scatters WHICH voice's q-mix. Identity qmixSrc + level 0
    // → always GENERATED (safe default), level 1 → always quantised input. Gated by
    // quantiserPitchSource so it is inert outside quantiser modes (A/B byte-identical). Computed HERE
    // (before voicePitch) so it steers the pitch source; result.qmixHit below uses the SAME decision
    // on starting steps.
    // POLARITY (MODE_COLLAPSE_6_TO_3 §29): 0 = generated, 1 = quantised. The draw crosses UPWARD
    // (r >= level → use generated), so level 0 forces generated, level 1 forces quantised. (Pre-collapse
    // this was r < level, i.e. high q-mix = generated — the opposite.)
    const bool qmixUseGenerated = quantiserPitchSource && (r_qmix >= voices[monoSlot].qmixLevel);
    // QUANTISER (Q1): mono/voice-0 pitch = quantised external CV when in a quantiser mode, else the
    // internal melody+octave draw. voicePitch bypasses genPitchLive (no RNG/lane perturbation) when
    // quantiserPitchSource is set; off = byte-identical legacy path. qmixUseGenerated forces the
    // generated branch on a q-mix "use generated" step.
    // FADER_SEQ_QUANT_COLOURS: isQuant = this note's pitch came from the QUANTISED external CV (not
    // generated). Routes the fader flash to the green (quant) timer instead of red (seq). Per voice a
    // note is EITHER seq OR quant; q-mix selects one source. isQuant is threaded into the note-on calls.
    const bool isQuant = quantiserPitchSource && !qmixUseGenerated;
    float pitchV = voicePitch(monoSlot, sem, input,
                              // Warp mirror (CC_PROMPT_03 STEP 4a): the mono voice's
                              // sounding pitch reads the effective w-mixture (== the raw
                              // row when no warp is authored — byte-identical legacy).
                              pe.warpRow(monoSlot, LaneId{PL_MELODY})[getMelodyStep()],
                              pe.warpRow(monoSlot, LaneId{PL_OCTAVE})[getOctaveStep()],
                              /*forceGenerated=*/qmixUseGenerated);

    // ── Leading-edge legato (STEP 2, the only legato model) ──
    // The PREVIOUS starting note recorded, at its own onset, whether it intends to hold
    // its gate forward into THIS note (gs.slurForward). It survives untouched through the
    // previous note's held steps (the MidNote guard above returns early). Capture it now,
    // BEFORE this note's cascade recomputes gs.slurForward at the bottom. This onset
    // commitment — not a fresh roll at the join — governs whether THIS note connects back.
    const bool prevSlur = gs.slurForward;

    // The previous PLAYED step's decision (lastStepResult is set at the end of each executeStep,
    // so here it is the step played immediately before this one — in PLAY ORDER, correct in both
    // directions because it is temporal, not step-numbered). A legato/tie means "connected to the
    // note played just before"; if that step was a REST (or the initial Inactive), there is no
    // note to connect to → this must NOT be a connection. This is the direction-correct
    // predecessor test. wasHeld alone is NOT sufficient: in reverse, wasHeld (a gate read) can be
    // true from a note that is not the play-order-adjacent predecessor, so a teal could otherwise
    // be emitted with a rest immediately before it in play order (the reverse isolated-teal bug).
    const MonoDecision prevPlayedDec = lastStepResult.decision;
    const bool prevPlayedSounded = (prevPlayedDec == MonoDecision::NewNote)
                                || (prevPlayedDec == MonoDecision::Legato)
                                || (prevPlayedDec == MonoDecision::LegatoMax)
                                || (prevPlayedDec == MonoDecision::Tie)
                                || (prevPlayedDec == MonoDecision::MidNote);

    // Structural Rest Roll:
    // canRest is true only if the previous note finished exactly on a step edge 
    // or was already silent. Fractional tails (e.g. triplets) block the rest roll
    // to ensure rhythmic alignment.
    bool canRest = (gs.holdRemain <= 0.0001f && !hadTail);

    // Whether THIS note connects (tie/legato) to the previous one: the previous note's onset
    // commitment — prevSlur — decides it (leading-edge). The joining onset does NOT re-roll;
    // r_legato_tie is consumed only at the LEAD commitment (gs.slurForward, at the bottom of
    // this cascade), i.e. by the note that produces the next note's prevSlur.
    // legatoProb>=0.999 (LegatoMax) still forces connection.
    const bool legatoConnects = (legTh >= 0.999f) || prevSlur;

    // A genuine committed slur reaching THIS note: it connects AND has a held predecessor to
    // connect to (exactly the condition the legato branch below uses). "Rest beats legato"
    // toggle: when OFF, such a slur IGNORES the rest roll here (slur wins → plays as Legato/
    // Tie); when ON (default), the rest branch takes priority (rest cancels the slur). A
    // fractional TAIL still always outranks rest (canRest, below), regardless of the toggle.
    const bool slurReachesHere    = legatoConnects && (wasHeld || hadTail) && prevPlayedSounded;
    const bool slurSuppressesRest = !generatedRestBeatsLegato && slurReachesHere;

    if (legTh >= 0.999f) {
        gs.slideMax(pitchV, sem, nvIdx, isQuant);
        gsStep.triggerNote(pitchV, sem, nvIdx, isQuant);            // STEP: re-strike (un-fused)
        result.decision = MonoDecision::LegatoMax;
    }
    else if ((r_rest < restTh) && !slurSuppressesRest) {
        // Rest precedence:
        //  - A fractional NOTE TAIL always outranks a rest (physical — !canRest). Unchanged.
        //  - "Rest beats legato" ON (default): a rest CANCELS an optional slur reach ("can't
        //    slur into silence") — even a committed prevSlur yields to the rest (N+1 silent).
        //    OFF: slurSuppressesRest skips this branch, so the slur wins and the note falls
        //    through to the legato branch below (rest ignored, N+1 plays as Legato/Tie with
        //    its own drawn pitch). See RHYTHM_BEHAVIOUR_POLICIES.md.
        if (canRest) {
            gs.gateHeld = false;
            gsStep.gateHeld = false;                       // STEP: mirror the rest (close)
            result.decision = MonoDecision::Rest;
        } else {
            // Precedence: note tail takes priority over pattern structural rest.
            gs.gateHeld = true; // Re-activate gate to allow the fractional tail to finish
            result.decision = MonoDecision::MidNote;
        }
    }
    else if (legatoConnects && (wasHeld || hadTail) && prevPlayedSounded) {
        // A slur can only CONNECT if the previous note actually held its gate into this step
        // (wasHeld || hadTail) — the documented invariant "legato/tie requires the previous
        // note still sounding". legatoConnects (== prevSlur, the predecessor's onset
        // commitment) expresses the INTENT to connect, but intent alone isn't enough:
        // prevSlur can be true while the predecessor left NO held gate (it ended / was cut),
        // which would otherwise slide into nothing — producing an isolated Legato cell (teal
        // note connected to no predecessor). Requiring a held predecessor here makes the
        // connection real; otherwise fall through to NewNote (the slur had nothing to land
        // on → a fresh note). (Supersedes master's standalone r_legato_tie<legatoProb form:
        // the joining onset no longer re-rolls — prevSlur carries the decision.)
        if (sem == gs.lastSemitone) {
            gs.extendHold(sem, nvIdx, isQuant);
            gsStep.triggerNote(gs.currentPitchV, sem, nvIdx, isQuant);  // STEP: re-strike, same pitch
            result.decision = MonoDecision::Tie;
        } else {
            gs.slideNote(pitchV, sem, nvIdx, /*wasHeld=*/true, isQuant);
            gsStep.triggerNote(pitchV, sem, nvIdx, isQuant);        // STEP: re-strike (un-fused)
            result.decision = MonoDecision::Legato;
        }
    }
    else {
        gs.triggerNote(pitchV, sem, nvIdx, isQuant);
        gsStep.triggerNote(pitchV, sem, nvIdx, isQuant);            // STEP: same as fused (fresh note)
        result.decision = MonoDecision::NewNote;
    }

    // Determine if this note is accented. 
    // Decision happens on NewNote, or Legato shifts from a dead state.
    bool monoStarting = (result.decision == MonoDecision::NewNote) || 
                        ((result.decision == MonoDecision::Legato || result.decision == MonoDecision::LegatoMax) && !wasHeld && !hadTail);

    if (monoStarting) {
        result.accented = (r_accent < accTh);
    } else if (result.decision == MonoDecision::Rest) {
        result.accented = false;
    } else {
        // Sustain/Inherit: Tie, MidNote, or Legato shift during an existing held note
        result.accented = lastStepResult.accented;
    }

    // Task 4 (QMIX): threshold the mono q-mix draw against QMIX_LEVEL, mirroring accent. A "hit"
    // fires when the draw crosses the level UPWARD (draw >= level → generated); on sustains it
    // inherits, on rests false. This is the SAME decision that steered the pitch source above
    // (qmixUseGenerated): on a starting step qmixHit == "this note used the GENERATED pitch". A
    // held/tied step reuses the prior note's pitch (no fresh voicePitch draw), so it inherits the
    // prior qmixHit — the source latches with the note. Outside quantiser modes qmixUseGenerated is
    // always false; qmixHit keeps its raw-draw semantics via the r_qmix>=qmixLevel form (behaviour-
    // inert there, matching prior code). POLARITY: 0 = generated, 1 = quantised (§29).
    if (monoStarting) {
        result.qmixHit = (r_qmix >= voices[monoSlot].qmixLevel);
    } else if (result.decision == MonoDecision::Rest) {
        result.qmixHit = false;
    } else {
        result.qmixHit = lastStepResult.qmixHit;
    }

    // ── LEAD COMMITMENT (leading-edge legato) — sets the next note's prevSlur ─────
    // A note that is STARTING commits here (at its own onset) to hold its gate forward into
    // the next note, iff BOTH: (a) its own legato draw fires (r_legato_tie < legatoProb, or
    // LegatoMax), AND (b) its length is grid-aligned so it can cleanly reach the next onset
    // (noteCanLeadLegato). This is the sole consumer of r_legato_tie: the roll happens once,
    // at the LEAD, and is recorded on gs.slurForward — which the NEXT step reads as prevSlur
    // to decide its connect (see legatoConnects above). Only meaningful on a starting note;
    // cleared otherwise.
    // A note "starts" (and may thus lead the chain FORWARD) if it is a real sounding note
    // at this onset: NewNote, Legato, LegatoMax — AND Tie. A Tie (same-pitch continuation)
    // is a sounding note that can carry a 3+ note legato chain onward to the next note, IF
    // its own length is grid-aligned and it rolls forward. Excluding Tie would break any
    // chain at a tie (chain of 3+ can only continue past note 2 if note 2 is itself an
    // eligible lead — see LEGATO_TIE_REDESIGN.md). MidNote (mid-hold, not an onset) and Rest
    // (silent) correctly do NOT start. slurForward set at a note's onset survives its held
    // steps (the MidNote guard returns early before this block).
    bool leStarting = (result.decision == MonoDecision::NewNote)
                   || (result.decision == MonoDecision::Legato)
                   || (result.decision == MonoDecision::LegatoMax)
                   || (result.decision == MonoDecision::Tie);
    bool leadsSlur = leStarting
                  && noteCanLeadLegato(nvIdx, ppqnSetting)
                  && (legTh >= 0.999f || r_legato_tie < legTh);
    gs.slurForward = leadsSlur;
    gs.slurMember  = prevSlur || leadsSlur;   // SLEG mask: this note leads OR continues a slur

    result.forStep = stepIndex; lastStepResult = result;
    return result;
}

void SequencerEngine::handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime) {
    if (isMelodyRealtime) {
        pe.melodySeedPendingFloat = pe.melodySeedFloat; // Or sample from module
        pe.melodySeedPending = true;
    }
    if (isRhythmRealtime) {
        pe.rhythmSeedPendingFloat = pe.rhythmSeedFloat;
        pe.rhythmSeedPending = true;
    }
    pe.applyPendingSeedsAndRedraw(input);
}

StepResult SequencerEngine::executeModeA(const redDot::RateManager& rm, float noteVal, const PatternInput& input, int dir) {
    lastNoteVal_ = noteVal;   // poly voices derive their own nvIdx from this (stage 2)
    // Phase A: voices[0] (V1) is populated by the controller (updatePatternInput),
    // not here — same single-writer pattern as voices[1..15] (updatePolyVoiceRest_).
    resetActivatedPolyVoices_();  // Phase B: reset newly-activated voices' stale gs state
    StepResult result;
    // RATE MANAGER (S3b): the Step tier — the dispatch fed it from the same sixteenth
    // edge (clock or phase mode), so this is the old raw sixteenth-edge check without
    // the engine re-reading a clock object. Level-within-sample by design: the Mode E
    // jump replay calls this once per crossed 1/16, all legitimately stepping.
    if (!rm.ticked(redDot::Rate::Step) || muted) return result;

    bool wrapped = advancePlayhead(dir);

    // "Boundary interrupt" toggle: at the phrase boundary (wrap), force a fresh start by
    // clearing any held gate BEFORE wasHeld is captured — so the note at step 0 sees no held
    // predecessor and can't slur/tie across the loop. Result: every lap is identical (no
    // cross-lap memory). Default OFF = CONTINUE (gate carries across the loop, laps can differ
    // — see the lap-1-vs-lap-2 illustration in RHYTHM_BEHAVIOUR_POLICIES.md). Applied to mono
    // and all poly voices.
    if (wrapped && boundaryInterrupt) {
        gs.gateHeld = false;
        gs.holdRemain = 0.f;
        gs.slurForward = false;
        gsStep.gateHeld = false; gsStep.holdRemain = 0.f;      // STEP: stop with the fused gate
        for (int i = 1; i <= numPolyVoices; ++i) {
            const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
            voices[vs].gs.gateHeld   = false;
            voices[vs].gs.holdRemain = 0.f;
            voices[vs].gs.slurForward = false;
            voices[vs].participating = false;
            voices[vs].gsStep.gateHeld = false; voices[vs].gsStep.holdRemain = 0.f;
        }
    }

    // Warp mirror (CC_PROMPT_05): the mono feeders read EVERY lane through the ONE
    // uniform source pe.warpRow (mirror when a bank is wired, raw row otherwise —
    // byte-identical at the identity defaults). No per-lane gate, no mono/poly branch.
    const VoiceSlot monoSlot = VoiceResolver::mono();   // the mono ROLE, typed (was the bare [0])
    float r_vary   = pe.warpRow(monoSlot, LaneId{PL_VARIATION})[getVariationStep()];
    float r_rest   = pe.warpRow(monoSlot, LaneId{PL_REST})[getRhythmStep()];
    float r_legato = pe.warpRow(monoSlot, LaneId{PL_LEGATO})[getLegatoStep()];
    float r_accent = pe.warpRow(monoSlot, LaneId{PL_ACCENT})[getAccentStep()];  // New: accent strand
    float r_qmix   = pe.warpRow(monoSlot, LaneId{PL_QMIX})[getQmixStep()];      // Task 4: q-mix strand
    
    int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

    float prevHold = gs.holdRemain;
    wasHeldMono = gs.gateHeld || (prevHold > 0.0001f);
    gs.tick(ClockEngine::pulsesPer16th(ppqnSetting));
    gsStep.tick(ClockEngine::pulsesPer16th(ppqnSetting));
    hadMonoTail = (prevHold > 0.0001f && prevHold < 0.999f);

    for (int i = 1; i <= numPolyVoices; ++i) {
        const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
        wasHeldPolyPrev[i] = voices[vs].gs.gateHeld || (voices[vs].gs.holdRemain > 0.0001f);
        float ph = voices[vs].gs.holdRemain;
        voices[vs].gs.tick(ClockEngine::pulsesPer16th(ppqnSetting));
        voices[vs].gsStep.tick(ClockEngine::pulsesPer16th(ppqnSetting));
        hadPolyTail[i] = (ph > 0.0001f && ph < 0.999f);
    }
    
    result = executeStep(nvIdx, r_rest, r_legato, r_accent, r_qmix, input, wasHeldMono, hadMonoTail);
    result.stepped = true;
    result.wrapped = wrapped;
    // executeStep already assigned lastStepResult (BEFORE wrapped/stepped were set on the local
    // result), so lastStepResult.wrapped was stale-false. Re-sync so consumers that read
    // engine.lastStepResult.wrapped (e.g. the Shophouse boundary sampler) see the real value.
    lastStepResult = result;
    return result;
}

// advanceOnTieIntoRest checkpoint (LEGATO_GATE_GAP_BUG.md §345). Called from the module layer on a
// Gate-1 FALL while a slur is pending (tieAcrossRests && advanceOnTieIntoRest).
//
// advanceOnTieIntoRest ON = PROBABILISTIC fall-advance (option 2,
// CC_PROMPT_TIE_INTO_REST_PROBABILISTIC_ADVANCE): the fall into a tie-rest advances
// the playhead with probability = the rest step's Variation draw (the SAME mechanism
// as the rise step-advance — decideStepAdvance_ with the incoming-rest step's
// r_vary), so the rest re-draws (and may end the slur chain) probabilistically. The
// flag is the ENABLE; OFF = never fall-advance (the module layer never calls here).
// OPTION 3 (future, if Rodney decides the flag isn't worth keeping): drop the flag,
// always probabilistically fall-advance.
//
// BIPOLAR by necessity (the SHARED playhead): the fall takes the FULL
// {BACKWARD, HOLD, FORWARD} decision — p>0 forward (the rest re-draws at the NEXT
// step), p<0 backward (advancePlayhead(-1), the 0->15 wrap), near 0 HOLD (no
// advance, the rest passive — the chain carries). A forward-only fall mid-backward
// passage would LURCH the playhead. Re-evaluates the slur candidacy at the landed
// step: the legato draw there decides whether the slur REMAINS a candidate
// (survives across the rest) or ENDS. NOT a note onset — a silent rest checkpoint
// (the gate stays low). Sets pendingCheckpointArrivalV_[0] so the next rise SKIPS its
// own advance ("one advance, one arrival" — no double-advance jump; direction-
// agnostic) + the per-voice latches for Local-East voices that moved on their own
// fall decisions (PER-VOICE STEP-ADVANCE Stage 2, the signoff's decision 2).
// HOLD sets NO arrival (nothing advanced — the next rise decides normally). The SOLE
// advance-on-fall (edge-driven-playhead violation), opt-in.
void SequencerEngine::legatoCheckpointOnFall() {
    if (muted) { pendingCheckpointArrivalV_[0] = true; return; }
    // THE DECISION (one mechanism): the incoming-rest step's variation draw vs the
    // resolved bipolar Variation — the same decideStepAdvance_ the rise uses, with
    // the draw addressed at the step being advanced into (targetTickDelta +1).
    // PER-VOICE (the signoff's decision 2 — the fall decision is the SAME stochastic
    // mechanism as the rise, so it goes per-voice WITH it): the fold runs the same
    // decision per voice at ITS OWN +1 target (its incoming-rest step) — the
    // every-lane principle (one threshold control, many own draws, many outcomes).
    // An offset moves only where a voice's own fall decision diverged from the mono one.
    const dotModular::StepAdvance::Decision d = decideStepAdvance_(VoiceResolver::mono(), -1, /*targetTickDelta=*/+1);
    std::array<int8_t, kVoiceSlots> motionV{};
    const bool anyVoiceMoved = foldPerVoiceAdvance_(-1, /*perVoiceScan=*/false, d, motionV.data());
    // The per-voice arrival latches: every voice that moved on its OWN fall decision
    // latches its own "one advance, one arrival" (its next rise is a no-decision
    // arrival for IT). [0] is the mono latch (below — unchanged semantics).
    for (int i = 1; i <= numPolyVoices && i < kVoiceSlots; ++i)
        if (motionV[i] != 0) pendingCheckpointArrivalV_[i] = true;
    bool wrapped = false;
    if (d == dotModular::StepAdvance::FORWARD) {
        wrapped = advancePlayhead(+1, motionV.data());
    } else if (d == dotModular::StepAdvance::BACKWARD) {
        wrapped = advancePlayhead(-1, motionV.data());
    } else {
        // HOLD: no advance — the rest stays passive (the chain carries,
        // slurForward stays pending; NO arrival: the next rise decides normally).
        // Per-voice: a voice that DID move on its own fall decision refreshes its
        // reads (the clock held — the offsets-only recompute).
        if (anyVoiceMoved) recomputeLaneTicks(motionV.data());
        return;
    }
    if (wrapped && boundaryInterrupt) {
        gs.slurForward = false;          // phrase boundary breaks the chain
        pendingCheckpointArrivalV_[0] = true;  // the fall still advanced; the rise plays this step, no 2nd advance
        return;
    }
    // The legato draw at the rested step decides slur survival (generatedRestBeatsLegato is
    // IRRELEVANT — the incoming rest already makes it silent; only the slur candidacy is in play).
    // Phase A: legato threshold read from the mono slot's legatoProb (V1 = voice 0, TYPED).
    const VoiceSlot monoSlot = VoiceResolver::mono();
    float r_legato = pe.warpRow(monoSlot, LaneId{PL_LEGATO})[getLegatoStep()];
    bool survives = (voices[monoSlot].legatoProb >= 0.999f) || (r_legato < voices[monoSlot].legatoProb);
    if (!survives) gs.slurForward = false;   // chain ENDS at the rest checkpoint
    // else slurForward stays pending across the rest (tieAcrossRests bridge carries it to the rise)
    pendingCheckpointArrivalV_[0] = true;
}

StepResult SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, float noteVal, const PatternInput& input) {
    lastNoteVal_ = noteVal;
    // Phase A: voices[0] (V1) is populated by the controller (updatePatternInput).
    resetActivatedPolyVoices_();  // Phase B: reset newly-activated voices' stale gs state
    StepResult result;
    if (muted) {
        prevGate1High = gate1High;
        return result;
    }
    bool wrapped = false;
    bool triggered = false;

    if (gate1Rise) {
        if (pendingCheckpointArrivalV_[0]) {
            // advanceOnTieIntoRest: the falling edge already advanced one step into the incoming-rest
            // (legatoCheckpointOnFall). This rise is the ARRIVAL — play the already-stepped-to step,
            // do NOT advance again ("one advance, one arrival" — no double-advance jump).
            // PER-VOICE: the [0] latch governs the TRANSPORT skip (bit-identical); the
            // fold still runs — an unlatched voice decides at this rise, a latched one
            // clears + rides 0 (its position already moved at the fall). Voices that
            // move refresh via the offsets-only recompute (the clock does not move on
            // an arrival).
            pendingCheckpointArrivalV_[0] = false;
            std::array<int8_t, kVoiceSlots> motionV{};
            if (foldPerVoiceAdvance_(-1, /*perVoiceScan=*/true, dotModular::StepAdvance::REPEAT, motionV.data()))
                recomputeLaneTicks(motionV.data());
            wrapped = false;
        } else if (stepIndex == -1) {
            // The seed: initialization, not a sequence-motion decision (no draw consumed).
            wrapped = advancePlayhead();
        } else {
            const dotModular::StepAdvance::Decision d = decideStepAdvance_(VoiceResolver::mono(), -1, scanOfsV_[0]);
            // STAGE 1 (CC_PROMPT_GATE_STEP_ADVANCE_SPEC): the main-gate advance is
            // PROBABILISTIC — u < the resolved Variation probability in gate mode moves
            // the playhead. p=1 is the GOLDEN PIN: decide(1, u) == FORWARD for every
            // u in [0,1) → the playhead moves on every rise, byte-identical to the
            // pre-feature engine (the identity-with-today regression gate).
            // STAGE 2 (CC_PROMPT_STEP_ADVANCE_CONSUMPTION): the resolved value is
            // BIPOLAR [-1,+1] (the unclamped knob+CV2+Junction sum) — p < 0 walks the
            // playhead BACKWARD this rise (probabilistic retrograde) through
            // advancePlayhead's reverse traversal, which wraps past step 0 to step 15
            // ((stepIndex - 1 + 16) & 0x0F) — the spec's wrap-at-boundary law, the
            // same machinery the deterministic dir parameter uses. ONE PLAYHEAD: the
            // direction is whatever THIS event's threshold sign gave.
            // PER-VOICE STEP-ADVANCE (the WANDER fix — the every-lane principle): the
            // fold decides per voice on its OWN window draw (the threshold is the
            // resolver-resolved store — global when delegated, the override when not,
            // exactly restProb's pattern) and accumulates the offsets BEFORE the
            // transport call, so the recompute inside advancePlayhead sees them. Even
            // on a GLOBAL threshold the voices wander independently.
            std::array<int8_t, kVoiceSlots> motionV{};
            const bool anyVoiceMoved = foldPerVoiceAdvance_(-1, /*perVoiceScan=*/true, d, motionV.data());
            if (d == dotModular::StepAdvance::FORWARD)
                wrapped = advancePlayhead(+1, motionV.data());     // resets the scan ([0])
            else if (d == dotModular::StepAdvance::BACKWARD)
                wrapped = advancePlayhead(-1, motionV.data());      // resets the scan ([0])
            else {
                scanHold_();                                // HELD: scan the window forward (the freeze fix)
                if (anyVoiceMoved)
                    recomputeLaneTicks(motionV.data());            // the clock held; diverged voices refresh
            }
        }
        // else REPEAT (the corrected semantics, Rodney 91b100c): the playhead does not
        // move — the current step is unchanged, so every downstream read hits the SAME
        // step-addressed draw it would anyway. No re-draw branch exists; the held step
        // re-outputs itself identically for as long as the rises keep repeating.
        triggered = true;
    } else if (gate1High && !prevGate1High && stepIndex == -1) {
        advancePlayhead();
        triggered = true;
    }

    if (triggered) {
        // Boundary interrupt (see executeModeA): clear held gate at wrap so every lap starts
        // fresh. Applied before wasHeld capture, mono + poly.
        if (wrapped && boundaryInterrupt) {
            gs.gateHeld = false; gs.holdRemain = 0.f; gs.slurForward = false;
            gsStep.gateHeld = false; gsStep.holdRemain = 0.f;      // STEP: stop with the fused gate
            for (int i = 1; i <= numPolyVoices; ++i) {
                const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
                voices[vs].gs.gateHeld = false; voices[vs].gs.holdRemain = 0.f;
                voices[vs].gs.slurForward = false; voices[vs].participating = false;
                voices[vs].gsStep.gateHeld = false; voices[vs].gsStep.holdRemain = 0.f;
            }
        }
        const VoiceSlot monoSlot = VoiceResolver::mono();   // the mono ROLE, typed (was the bare [0])
        float r_vary   = pe.warpRow(monoSlot, LaneId{PL_VARIATION})[getVariationStep()];
        float r_rest   = pe.warpRow(monoSlot, LaneId{PL_REST})[getRhythmStep()];
        float r_legato = pe.warpRow(monoSlot, LaneId{PL_LEGATO})[getLegatoStep()];
        float r_accent = pe.warpRow(monoSlot, LaneId{PL_ACCENT})[getAccentStep()];  // New: accent strand
        float r_qmix   = pe.warpRow(monoSlot, LaneId{PL_QMIX})[getQmixStep()];      // Task 4: q-mix strand
        
        // Mode B: the note DURATION is Gate 1's width, so the INTERNAL note length is nullified
        // to a single 1/16 step (index 6 in NoteValues.hpp = 1.0 step). Using the controller's
        // noteVal here armed a MULTI-step hold (holdRemain=16 for the 1/1 default), and the Lantern
        // renders each note's bar at that length -- so a played note's 16-step bar painted OVER all
        // the rest cells that followed it, hiding rests unless a long run of consecutive rests had
        // no preceding note to cover them (the "only very high rest shows some rests" symptom,
        // ground-truthed via the MODEB logs: dec/gateHeld were already correct, only the LENGTH was
        // wrong). One step = the bar occupies only its own cell; the audible gate width still comes
        // from Gate 1 (generateOutputs). Variation is intentionally bypassed in Mode B.
        int nvIdx = (modeSelect == 1) ? NV_GATE_NULLIFY : getNoteLenIdx(noteVal, input, r_vary);

        float prevHold = gs.holdRemain;
        wasHeldMono = gs.gateHeld || (prevHold > 0.0001f);
        gs.tick();
        gsStep.tick();
        hadMonoTail = (prevHold > 0.0001f && prevHold < 0.999f);

        for (int i = 1; i <= numPolyVoices; ++i) {
            const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
            wasHeldPolyPrev[i] = voices[vs].gs.gateHeld || (voices[vs].gs.holdRemain > 0.0001f);
            float ph = voices[vs].gs.holdRemain;
            voices[vs].gs.tick();
            voices[vs].gsStep.tick();
            hadPolyTail[i] = (ph > 0.0001f && ph < 0.999f);
        }

        // ── Mode B: nullify internal note-length (MODE_B_SPEC.md §3) ──────────────────────
        // In Mode B the note DURATION is Gate 1's width, NOT an internal note-length countdown.
        // The controller passes a neutral noteVal, so triggerNote (called by executeStep below)
        // would arm a MULTI-STEP hold. That hold makes executeStep's MidNote early-return
        // (holdRemain >= 1 || gatePulseRemain > 0, ~line 426) SWALLOW the next gate rise without
        // re-rolling rest/legato -> "every note a long held note; rest/legato have no effect".
        // gatePulseRemain only decays on the internal CLOCK pulse (tickPulse), which is async to
        // the external gate rate, so it cannot be relied on to clear on its own. Explicitly clear
        // the internal countdown HERE -- AFTER wasHeldMono/hadMonoTail were captured (so a genuine
        // legato still sees the previous note as sounding: wasHeldMono stays true across gates,
        // reducing legato to the leading-edge prevSlur MODEL 1; a REST leaves gateHeld=false so the
        // next note reads wasHeldMono=false and cannot connect into the rest) and BEFORE executeStep
        // (so the MidNote guard can't early-return). executeStep re-arms on its own articulation, so
        // this only defeats the swallow; the actual gate width comes from Gate 1 (see generateOutputs).
        // Confined to Mode B -- this is executeModeB; executeStep itself stays mode-agnostic.
        gs.holdRemain = 0.f;     gs.gatePulseRemain = -1;
        gsStep.holdRemain = 0.f; gsStep.gatePulseRemain = -1;

        result = executeStep(nvIdx, r_rest, r_legato, r_accent, r_qmix, input, wasHeldMono, hadMonoTail);
        result.stepped = true;
        result.wrapped = wrapped;
        lastStepResult = result;   // re-sync wrapped/stepped (executeStep set lastStepResult before they were known)
    }

    prevGate1High = gate1High;
    return result;
}

// ── Mode B + subGate (Gate 3) subdivision ─────────────────────────────────────
// subGate (Gate 3) is the fine-grid clock; the main gate (gate1 in Mode B, gate2 in Mode D)
// is the note-event stream.  At each subGate onset the playhead advances; where the main gate
// is HIGH, executeStep shapes the sub-cell (rest/legato/accent + all pitch lanes draw, Tie
// emergent from pitch equality — ratchet default).  Where the main gate is LOW (gap), the
// playhead still advances but no note shapes (forced Rest).  The two tie scopes — intra-gate
// (tie across subGate cells within one main gate) and inter-gate (slur across a main-gate
// boundary) — are BOTH handled by executeStep's existing wasHeld/prevSlur legato machinery;
// no forced re-articulation at a coincident main-gate + subGate edge (the spec's TRAP).
// See GATE_SUBDIVISION_STEP_GATE.md §"Two edge streams" + §"TWO tie scopes".
StepResult SequencerEngine::executeModeBSubdivided(bool mainGateRise, bool mainGateHigh, bool subGateRise,
                                                    float noteVal,
                                                    const PatternInput& input,
                                                    bool ghostRise, bool ghostHigh) {
    lastNoteVal_ = noteVal;
    // Phase A: voices[0] (V1) is populated by the controller (updatePatternInput).
    resetActivatedPolyVoices_();  // Phase B: reset newly-activated voices' stale gs state
    StepResult result;
    // Any of the three edge streams advances the playhead + shapes a step:
    //   mainGateRise (main onset), subGateRise (ratchet, in-gate), ghostRise (ghost, in-gap).
    // The module layer ensures ratchet fires only in-gate and ghost only in-gap; the engine is
    // region-agnostic — executeStep's wasHeld/prevSlur handle intra-gate, inter-gate, AND
    // ghost<->main legato uniformly (the leading-edge model composes across all boundaries).
    const bool anyEdge = mainGateRise || subGateRise || ghostRise;
    if (muted || !anyEdge) return result;
    (void)mainGateHigh; (void)ghostHigh;   // engine is region-agnostic; IMPL 2b (caller) uses these for width

    // STAGE 3 (CC_PROMPT_GATE_STEP_ADVANCE_SPEC): the THREE-LEVEL advance hierarchy.
    // Each edge decides on its OWN level's threshold — a MAIN rise: the resolved
    // Variation probability; a RATCHET rise: the resolved ratchet-advance column (the
    // burst WALKS through the sequence vs STUTTERS on one step); a GHOST rise: the
    // resolved ghost-advance column (the sequence DRIFTS FORWARD in the gaps vs ghosts
    // that fill without moving the playhead). ONE PLAYHEAD (the spec's constraint):
    // exactly ONE advance-or-repeat per edge event — the regions are exclusive (ratchet
    // in-gate, ghost in-gap), and a coincident main+ratchet rise IS the main onset (main
    // dominates: it is the event that starts the gate). p=1 everywhere is the GOLDEN PIN
    // (byte-identical to the pre-feature unconditional advance); REPEAT moves nothing
    // (the corrected held semantics — the step-addressed data re-outputs itself, no
    // re-draw branch exists).
    bool wrapped = false;
    if (stepIndex == -1) {
        wrapped = advancePlayhead();   // the seed: initialization, not a motion decision (no draw)
    } else {
        const int advLevel = mainGateRise ? -1
                           : (subGateRise ? dotModular::SubgateKnobs::RATCHET
                                          : dotModular::SubgateKnobs::GHOST);
        const dotModular::StepAdvance::Decision adv = decideStepAdvance_(VoiceResolver::mono(), advLevel, scanOfsV_[0]);
        // FORWARD advances; BACKWARD (Stage 2 bipolar — a negative threshold at ANY
        // level: the bipolar main through follow-main, or a bipolar mod delta into
        // the level's ADVANCE jack) walks the SHARED playhead backward with wrap
        // (advancePlayhead's reverse traversal — ONE PLAYHEAD, the direction set by
        // the last event that advanced); REPEAT moves nothing (the held semantics —
        // the step-addressed data re-outputs itself, no re-draw branch).
        // PER-VOICE STEP-ADVANCE (the WANDER fix — the every-lane principle): the
        // fold decides per voice at THIS level's threshold (its own
        // subRes[level][ADVANCE] — the poly ADVANCE columns' consumer) on its OWN
        // window draw, before the transport call; the threshold source is the
        // resolver-resolved store, exactly rest/legato's pattern.
        std::array<int8_t, kVoiceSlots> motionV{};
        const bool anyVoiceMoved = foldPerVoiceAdvance_(advLevel, /*perVoiceScan=*/true, adv, motionV.data());
        if (adv == dotModular::StepAdvance::FORWARD)
            wrapped = advancePlayhead(+1, motionV.data());     // resets the scan
        else if (adv == dotModular::StepAdvance::BACKWARD)
            wrapped = advancePlayhead(-1, motionV.data());      // resets the scan
        else {
            scanHold_();                                // HELD: scan the window forward (the freeze fix)
            if (anyVoiceMoved)
                recomputeLaneTicks(motionV.data());            // the clock held; diverged voices refresh
        }
    }

    // Boundary interrupt (same as executeModeA/B): clear held gate at wrap so every lap
    // starts fresh.  Applied before wasHeld capture, mono + poly.
    if (wrapped && boundaryInterrupt) {
        gs.gateHeld = false; gs.holdRemain = 0.f; gs.slurForward = false;
        gsStep.gateHeld = false; gsStep.holdRemain = 0.f;
        for (int i = 1; i <= numPolyVoices; ++i) {
            const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
            voices[vs].gs.gateHeld = false; voices[vs].gs.holdRemain = 0.f;
            voices[vs].gs.slurForward = false; voices[vs].participating = false;
            voices[vs].gsStep.gateHeld = false; voices[vs].gsStep.holdRemain = 0.f;
        }
    }

    const VoiceSlot monoSlot = VoiceResolver::mono();   // the mono ROLE, typed (was the bare [0])
    float r_rest   = pe.warpRow(monoSlot, LaneId{PL_REST})[getRhythmStep()];
    float r_legato = pe.warpRow(monoSlot, LaneId{PL_LEGATO})[getLegatoStep()];
    float r_accent = pe.warpRow(monoSlot, LaneId{PL_ACCENT})[getAccentStep()];
    float r_qmix   = pe.warpRow(monoSlot, LaneId{PL_QMIX})[getQmixStep()];

    // Mode B nullification: the note DURATION is the main gate's width, not an internal
    // note-length.  NV_GATE_NULLIFY (a 1-step value) so the internal countdown does not
    // govern — the NAMED sentinel, immune to table reordering.
    int nvIdx = NV_GATE_NULLIFY;

    float prevHold = gs.holdRemain;
    wasHeldMono = gs.gateHeld || (prevHold > 0.0001f);
    gs.tick();
    gsStep.tick();
    hadMonoTail = (prevHold > 0.0001f && prevHold < 0.999f);

    for (int i = 1; i <= numPolyVoices; ++i) {
        const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
        wasHeldPolyPrev[i] = voices[vs].gs.gateHeld || (voices[vs].gs.holdRemain > 0.0001f);
        float ph = voices[vs].gs.holdRemain;
        voices[vs].gs.tick();
        voices[vs].gsStep.tick();
        hadPolyTail[i] = (ph > 0.0001f && ph < 0.999f);
    }

    // Nullify the internal note-length countdown (Mode B): the gate width comes from the
    // main gate (driven by the module layer's IMPL 2b), not holdRemain/gatePulseRemain.
    gs.holdRemain = 0.f;     gs.gatePulseRemain = -1;
    gsStep.holdRemain = 0.f; gsStep.gatePulseRemain = -1;

    // GHOST CANDIDACY (CC_PROMPT_STEP_ADVANCE_FIX_DRAW_SOURCE, the companion change):
    // there is NO separate candidacy gate anymore — the old variation-strand gate
    // (r_vary >= variationRateProb) is DELETED, and a ghost cell's existence IS its
    // SUBGATE REST decision below: executeStep (called with level GHOST) rolls the
    // rest lane against subRes[GHOST][REST] (or the main restProb un-primed) — ONE
    // rest authority per cell, no double silencing, no candidacy gate on top. This is
    // what frees r_vary (now consumed ONLY by the advance decision) — the freeing is
    // CAUSED by this move, not incidental.
    // REST<->SLUR (CC_PROMPT_SUBGATE_REST_SLUR_CONSISTENT, verified by the R-SLUR
    // suite): a rested ghost follows the SHARED rest branch's flag law —
    // generatedRestBeatsLegato ON (default): the rest cancels the slur (N+1 silent,
    // the chain ends); OFF: slurSuppressesRest bridges it (the ghost plays Legato/
    // Tie, the chain survives) — EXACTLY a rested main step. One flag, one logic,
    // every level (nothing subgate-specific is hardcoded here).
    // A main or ratchet onset ends any sustaining ghost (the ghost only lives in the gap).
    if (mainGateRise || subGateRise) ghostActive = false;

    // Shape this step.  A main onset, a ratchet sub-cell, or a ghost candidate all run executeStep —
    // rest/legato/accent + all pitch lanes draw, Tie/Legato emergent from pitch equality.  wasHeldMono
    // + prevSlur carry the predecessor state across ALL boundary types (intra-gate, inter-gate,
    // ghost<->main) — the leading-edge model composes, so legato flows both ways with no special
    // case (the spec's TRAP).  The gate width comes from whichever external gate is high (main or
    // ghost) via the module-layer IMPL 2b.
    result = executeStep(nvIdx, r_rest, r_legato, r_accent,
                         r_qmix, input, wasHeldMono, hadMonoTail,
                         ghostRise ? dotModular::SubgateKnobs::GHOST
                                   : (subGateRise ? dotModular::SubgateKnobs::RATCHET : -1));
    // POLY SUBGATE LEVEL: hand the cascade the same edge level (consumed + cleared
    // by executePolyVoices at entry; the controller calls it once per block after
    // the mode exec).
    polySubLevel_ = ghostRise ? dotModular::SubgateKnobs::GHOST
                              : (subGateRise ? dotModular::SubgateKnobs::RATCHET : -1);
    // The sustain latch, by the SHARED branch's outcome: a ghost that played
    // (decision != Rest — including the generatedRestBeatsLegato-OFF bridged
    // Legato/Tie) sustains; a rested one (ON) does not. One flag, one logic,
    // every level.
    if (ghostRise) ghostActive = (result.decision != MonoDecision::Rest);
    result.stepped = true;
    result.wrapped = wrapped;
    lastStepResult = result;   // re-sync (executeStep set lastStepResult before wrapped/stepped)
    return result;
}

// ── Poly voice execution ──────────────────────────────────────────────────────
//
// Rules (see design doc):
//   MidNote  — mono note still in progress; poly ticks its hold and returns.
//   Rest     — mono rested; poly cannot initiate a new note this step.
//   Tie      — mono held the same pitch; poly voices that are sounding extend
//              their own hold.  Voices already at rest remain silent.
//   Legato / LegatoMax / NewNote — mono played something new; each poly voice
//              independently rolls its own rest probability then draws its
//              own pitch (legacy note). Gate type (retrigger vs slide)
//              follows mono's decision.  Within the legato zone a poly-internal
//              tie still emerges naturally when the voice's random pitch
//              matches its own previous semitone.

// Stage 2a: refresh ONE cell of polyRandom at step advance, using the cached spread
// from the last control-rate sync(). If the lane's lock axis is not live, return without
// writing (frozen value persists — same as the old loop skipping when locked).
void SequencerEngine::refreshPolyRandomCell(VoiceSlot voice, LaneId engLane, int step) {
    // Determine which lock axis this lane rides.
    bool live;
    switch (engLane.l) {
        case PL_REST:     live = pe.cachedSpreadLiveR; break;   // rhythm axis
        case PL_ACCENT:   live = pe.cachedSpreadLiveR; break;
        case PL_VARIATION: live = pe.cachedSpreadLiveR; break;
        case PL_LEGATO:   live = pe.cachedSpreadLiveR; break;
        case PL_MELODY:   live = pe.cachedSpreadLiveM; break;   // melody axis
        case PL_OCTAVE:   live = pe.cachedSpreadLiveM; break;
        case PL_QMIX:     live = pe.cachedSpreadLiveQ; break;   // QMIX own axis
        default: return;
    }
    if (!pe.cachedSpreadInitialized || !live) return;   // not yet cached or frozen — leave existing value
    pe.polyRandom(voice, engLane)[step & (kMaskRing - 1)] =
        redDot::SpreadInterp::applyPoly(pe, engLane.l, voice.v, step & (kMaskRing - 1),
                                        pe.cachedPolySpread[voice.v][engLane.l]);
    // Warp mirror (CC_PROMPT_03/05): this cell of random_ just moved — refresh exactly
    // that mirror cell so the next warpRow read (the same call sequence in
    // executePolyVoice) sees the effective w-mixture of the CURRENT value. One
    // warpedValue eval per step per lane; no-op without a bank.
    pe.rebuildWarpMirrorCell(voice.v, engLane.l, step & (kMaskRing - 1));
}

// Phase B (see the header): reset newly-activated voices' stale gs state. Definition
// lives here (not inline in the header) because the loop MINTS VoiceSlots — the
// header cannot include VoiceResolver.hpp (engine include cycle).
void SequencerEngine::resetActivatedPolyVoices_() {
    if (numPolyVoices > prevNumPolyVoices_) {
        for (int i = prevNumPolyVoices_ + 1; i <= numPolyVoices && i < kVoiceSlots; ++i) {
            const VoiceSlot vs = VoiceResolver::slot(i + 1);   // i = voiceSlot; slot(V) takes the VOICE number
            voices[vs].gs.gateHeld = false;
            voices[vs].gs.holdRemain = 0.f;
            voices[vs].gs.slurForward = false;
            voices[vs].gs.gatePulseRemain = -1;
            voices[vs].gsStep.gateHeld = false;
            voices[vs].gsStep.holdRemain = 0.f;
            voices[vs].gsStep.gatePulseRemain = -1;
            voices[vs].participating = false;
            // PER-VOICE STEP-ADVANCE (Stage 1): a (re-)activated voice re-enters at
            // the macro position — no stale offset/scan from a previous activation
            // (the locked build never moves them; Stage 2's divergence makes this
            // live). Indexed by the voice slot, like laneTickV_.
            advanceOffset_[i] = 0;
            scanOfsV_[i] = 0;
        }
    }
    pe.syncPolyVoiceCount();  // Voice-count opt: fill delta voices' z in cache on increase
    prevNumPolyVoices_ = numPolyVoices;
}

// R-2 seam (RHYTHM_DECISION_MANAGER_PLAN §2.3): the mono voice as PARENT. The step's decision
// + note length (lastStepResult — written by executeStep, read-only from here on), the live
// mono gate (gs.gateHeld), and the pre-tick captures wasHeldMono/hadMonoTail (the "still
// sounding" read the executors made before ticking). The byte-identity gate for every
// Stage-R commit: test_rhythm_decision_differential.
ParentView SequencerEngine::monoParentView() const {
    // F-4 (D5): the mono's ghost event is its OWN stream latch (ghostActive, §8 — the mono
    // ghost machinery is the mono's own articulation; D7's drive bounds its audibility). At
    // the moment executePolyVoices reads this view, ghostActive is exactly "the mono's
    // this-edge onset was a played ghost": latched at the ghost rise (executeModeBSubdivided),
    // cleared at every main/ratchet onset BEFORE the cascade runs.
    return ParentView{ lastStepResult.decision, lastStepResult.nvIdx,
                       gs.gateHeld, wasHeldMono, hadMonoTail, ghostActive };
}

// ── F-1: the per-container view (RHYTHM_DECISION_MANAGER_PLAN §3.1) ─────────────────
// u is a STORAGE slot (0 = V1 … 15 = V16). Slot 0 is the mono voice — the R-2 producer,
// unchanged. Slots 1..15 are poly containers: the verdict + drawn length their own cascade
// captured THIS edge (polyVerdict_/polyNv_), the gate state read live from their VoiceBank
// slice (the same four scalars V1 exposes, per container).
// F-2 NOTE (supersedes the F-1 caveat): executePolyVoices visits voices in the MATERIALIZED
// topological order (rdm.materializeIfStale() at the top of the visit; the graph-change
// boundaries are the publish point per block and the edge visit itself). Every container —
// upward (u < v) or DOWNWARD (u > v) — has therefore already executed when its child runs, and
// containerView reads THIS-edge captures EVERYWHERE. The F-1 slot-order staleness (a higher-slot
// container read one edge stale) no longer exists; the forest suite's downward-edge row pins
// the fresh-read behavior via double-run determinism.
ParentView SequencerEngine::containerView(int u) const {
    if (u <= 0) return monoParentView();
    if (u >= 16) return ParentView{};              // defensive: fail-closed neutral view
    const VoiceSlot us = VoiceResolver::slot(u + 1);
    // F-4 (D5): the container's ghost event rides the view (the capture its own cascade wrote
    // this edge — the polyGhostOnset_ discipline) so a child's decideMask fold can require
    // every onsetting container to be ghosting (the ghost-cell ∀). Topo order guarantees the
    // capture is THIS-edge fresh (the same guarantee the verdict/length captures ride).
    return ParentView{ polyVerdict_[u], polyNv_[u],
                       voices[us].gs.gateHeld, wasHeldPolyPrev[u], hadPolyTail[u],
                       polyGhostOnset_[u] };
}

void SequencerEngine::executePolyVoice(VoiceSlot voiceIdx, const PatternInput& input, bool wasHeldPoly, bool hadPolyTail, int subLevel) {
    PolyVoice& v = voices[voiceIdx];
    // SUBGATE TIER (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC): the voice's own three-way
    // thresholds at THIS edge's level (−1 = main — the main probs verbatim, the
    // pre-subgate behaviour). BYTE-IDENTITY: nothing authored ⟹ subRes == the main
    // values exactly. Same scope line as the mono path: the variation-strand gates
    // (ghost placement below) stay on the main lane; the ADVANCE columns have NO poly
    // consumer — the playhead is singular (ONE PLAYHEAD, the spec's law), the advance
    // decision is mono-only (decideStepAdvance_, V1's row), so this voice's ADVANCE
    // cell stays resolved + stored (a per-voice playhead is Stage-4/atom territory).
    namespace SK = dotModular::SubgateKnobs;
    const float restTh = (subLevel >= 0 && v.subResLive) ? v.subRes[subLevel][SK::REST]   : v.restProb;
    const float legTh  = (subLevel >= 0 && v.subResLive) ? v.subRes[subLevel][SK::LEGATO] : v.legatoProb;
    const float accTh  = (subLevel >= 0 && v.subResLive) ? v.subRes[subLevel][SK::ACCENT] : v.accentProb;
    // F-1 (plan §3.1): the fold runs over the REAL containment graph. parentsOf(v) reads the
    // staged containmentParents (MonsoonSandsManager::processDNA stages it every block; a
    // zero row is an uncontained voice). An EMPTY mask is a FREE voice (D2/R2): the
    // self-rooted tuple E(v) = Own(v) — the voice's own window drives its onset cadence
    // (mono's MidNote-guard predicate), its own rest draw thins it, its own slur commitment
    // slides it. A non-empty mask folds the per-container views (containerView); the star
    // graph (bit 0 only) reduces to the R-2 mono view exactly — the branches below are
    // unchanged by F-1 for every graph shape.
    const uint16_t parents = rdm.parentsOf(voiceIdx.v);
    const RhythmDecision rd = parents
        ? rdm.decideMask(parents, [this](int u) { return containerView(u); })
        : RhythmDecisionCore::freeDecision(
              (v.gs.holdRemain >= 1.f) || (v.gs.gatePulseRemain > 0),   // mono's MidNote guard
              v.gs.slurForward,
              wasHeldPoly || hadPolyTail);
    // F-4 (D5): the ghost-placed-onset capture — default NO ghost event this edge; the play
    // path below overwrites it when this onset was ghost-placed (the polyVerdict_ discipline:
    // every executed edge writes, children read THIS-edge truth). A rested/rested-ghost/
    // landing/mid-hold edge leaves it false — none of those host a child ghost cell.
    polyGhostOnset_[voiceIdx.v] = false;
    const bool inGhostCell = rd.ghostCell;   // F-4 (D5): read once, used by the placement
                                             // block and the play capture (the block may
                                             // return early on the rested-ghost path).
    // Stage 2a: refresh VARIATION cell before nvIdxForVoice reads it (nvIdxForVoice
    // reads the uniform via pe.warpRow at the voice's variation step to pick the note length).
    refreshPolyRandomCell(voiceIdx, LaneId{PL_VARIATION}, getVariationStepForVoice(voiceIdx) & (kMaskRing - 1));
    // Stage 2: this voice's note length. Identical to lastStepResult.nvIdx when the voice's VAR
    // LOR is identity (delegated → reads mono's step). Clamped so the voice can never hold past
    // mono's next event — articulation is subtractive.
    const int nvV = nvIdxForVoice(voiceIdx, input);
    // F-1 capture: this voice's drawn window this edge — the C3 ceiling its children clamp
    // against (containerView reads polyNv_). Captured before the branches; the value is the
    // own draw under the parents' ceiling, exactly what nvIdxForVoice returned.
    polyNv_[voiceIdx.v] = nvV;

    // Start Detection (C1): the tuple's mayOnset — a new note/retrigger on NewNote, or a
    // Legato shift from a dead state (low gate and no tail); Stage F folds the union (D3).
    if (rd.mayOnset) {
        // This is the decision point for this entire mono gate.
        // Each strand indexes with ITS OWN lane LOR (rest/melody/octave).
        // Per-voice tick (inherits mono's per-lane direction) so a reversed mono lane reverses
        // this voice's read on the same lane.
        int restIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_REST}),   polyLenE(voiceIdx, LaneId{PL_REST}),   polyOffE(voiceIdx, LaneId{PL_REST}),   polyRotE(voiceIdx, LaneId{PL_REST}));
        int melIdx  = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_MELODY}), polyLenE(voiceIdx, LaneId{PL_MELODY}), polyOffE(voiceIdx, LaneId{PL_MELODY}), polyRotE(voiceIdx, LaneId{PL_MELODY}));
        int octIdx  = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_OCTAVE}), polyLenE(voiceIdx, LaneId{PL_OCTAVE}), polyOffE(voiceIdx, LaneId{PL_OCTAVE}), polyRotE(voiceIdx, LaneId{PL_OCTAVE}));
        refreshPolyRandomCell(voiceIdx, LaneId{PL_REST}, restIdx);
        float r_rest = pe.warpRow(voiceIdx, LaneId{PL_REST})[restIdx];

        // ── Phase 5: per-voice ghost placement (§398/§227; F-4 = plan §3.3's D5 forest form) ──
        // At a GHOST CELL of the containers (rd.ghostCell: every ONSETTING container is itself
        // ghosting this edge), each poly voice rolls its OWN variation to decide if it ghosts
        // HERE. A voice whose variation does not pass is a RESTED GHOST — transparent (silent;
        // the slur passes through it, §458), not part of the chain.
        //
        // F-4 DE-PRIVILEGING (the "mono not special" objective, D5 RULED "agree
        // intersection"): this block keyed on the GLOBAL mono latch (engine.ghostActive) —
        // exact on the star (every voice's container IS the mono), wrong under a forest (a
        // voice under V2 would ghost-ride V1's stream). The key is now the RDM's ghostCell:
        // the containers' own ghost events (ParentView::ghostEvent — the mono's stream latch
        // for slot 0, the child's polyGhostOnset_ capture for poly containers), folded as
        // "every onsetting container is ghosting". Candidacy is UNCHANGED (D5): the voice's
        // OWN variation draw at its own VAR step. Delegated voices read mono's VAR cell (D6
        // RULED: the delegated default keeps reading the root's cell — the draw production is
        // not the decision structure) → the shared ghost rhythm (+1 correlation) rides the
        // DEFAULT delegation, not the graph; Local-East voices read their own East VARIATION
        // LOR step = independent placement (0). The graded [-1,+1] copula correlation is a
        // future refinement (variation is not yet a spread lane); this is the per-voice read
        // the doc says "already exists, just not read by anything today" (§227).
        // The ghost note's EXTENT is the landed C5 intersection (the per-sample drive — the
        // audibility bound: every container's fused envelope must hold the note open; a
        // container's ghost holds its own gate high via its ghost/ghost-placed-note term).
        // GHOST CANDIDACY (the companion change, poly side): the variation-strand
        // gate is DELETED — the voice's ghost cell plays iff its rest decision
        // passes (restTh below = subRes[GHOST][REST] when primed), the ONE rest
        // authority. REST<->SLUR: the SHARED branch's flag law applies here too —
        // generatedRestBeatsLegato ON: the rested ghost is transparent (silent,
        // not part of the chain); OFF + a slur reaching: the rest is suppressed
        // and the voice plays Legato/Tie (uniform with main + mono).

        if (r_rest < restTh) {
            // Decide to Rest: Stick with it until mono gate drops. No accent while resting.
            v.accented = false;
            if (v.gs.holdRemain > 0.0001f) v.gs.gateHeld = true; // allow previous note tail to finish
            else { v.gs.gateHeld = false; v.gsStep.gateHeld = false; }  // STEP: mirror rest close
            // Rule 2: a resting voice starts no note, so it commits no forward slur, and it
            // is NOT part of this chain — landings must skip it (distinct from an opted-out
            // voice whose gate merely closed).
            v.gs.slurForward = false; v.participating = false;
            v.gs.slurMember = false;   // SLEG: a resting voice is not part of a slur
            polyVerdict_[voiceIdx.v] = MonoDecision::Rest;   // F-1: a rest closes the children
            return;
        }
        
        // Decide to Play: Draw pitch and follow mono's triggering behavior.
        int sem = 0;
        // QMIX per-voice source-select (mirrors the mono path at executeStep), CA-routed downstream of
        // Change Alley. Per QMIX_LANE_PARITY §"The blend" BOTH operands + the threshold are CA outputs:
        //   • THRESHOLD  — r_qmix_voice is drawn from warpRow(v, PL_QMIX), which is ALREADY
        //     scattered by the green qmixSrc plane: remapSlewedByPins remaps slewedPolyQmix by
        //     caSrcRow(row, STRAND_QMIX) → caQmixSrc, so this voice thresholds on the CA-selected
        //     SOURCE voice's q-mix probability. No extra caQmixSrc indexing here (would double-permute).
        //   • INPUT-CV operand — routed inside voicePitch via caInputCvSrcRow (CA melody plane), so a
        //     voice can quantise another voice's input line.
        //   • GENERATED operand — melody+octave draw (also CA-remapped), unchanged.
        //   r_qmix_voice >= voice's qmixLevel → use GENERATED (mode-A pitch for this voice).
        //   otherwise                         → use CA-routed quantised external CV.
        // Level scales the probability; qmixSrc scatters WHICH voice's q-mix. Identity qmixSrc + level 0
        // → always GENERATED, level 1 → always quantised input (the knob reads as before at identity).
        // qmixUseGenerated is only ever true in quantiser modes (voicePitch gates forceGenerated on
        // quantiserPitchSource); outside them it's inert and byte-identical to the legacy poly path.
        // POLARITY (§29): 0 = generated, 1 = quantised (draw crosses upward: r >= level → generated).
        int qmixIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_QMIX}), polyLenE(voiceIdx, LaneId{PL_QMIX}), polyOffE(voiceIdx, LaneId{PL_QMIX}), polyRotE(voiceIdx, LaneId{PL_QMIX}));
        refreshPolyRandomCell(voiceIdx, LaneId{PL_QMIX}, qmixIdx);
        float r_qmix_voice = pe.warpRow(voiceIdx, LaneId{PL_QMIX})[qmixIdx];
        bool qmixUseGenerated = quantiserPitchSource && (r_qmix_voice >= v.qmixLevel);
        const bool isQuant = quantiserPitchSource && !qmixUseGenerated;   // FADER_SEQ_QUANT_COLOURS (green flash)
        // QUANTISER (Q1): this voice's pitch = quantised external CV (its own channel) in quantiser
        // mode, else the internal melody+octave draw. voiceIdx is the voiceSlot (1..15 = V2..V16;
        // slot 0 = the mono/executeStep path). voicePitch reads quantiserCV[slot] DIRECTLY — no +1
        // (an older comment here described a pre-unification quantiserCV[voiceIdx+1] that no longer
        // exists in the code). When qmixUseGenerated, forceGenerated pushes voicePitch through
        // genPitchLive (mode-A pitch).
        float pitchV = voicePitch(voiceIdx, sem, input,
                                  (refreshPolyRandomCell(voiceIdx, LaneId{PL_MELODY}, melIdx), pe.warpRow(voiceIdx, LaneId{PL_MELODY})[melIdx]),
                                  (refreshPolyRandomCell(voiceIdx, LaneId{PL_OCTAVE}, octIdx), pe.warpRow(voiceIdx, LaneId{PL_OCTAVE})[octIdx]),
                                  qmixUseGenerated);
        // Accent as a poly lane (modelled after rest): this voice draws its OWN accent at
        // its own accent LOR and compares to its own accentProb — not shared from mono.
        int accIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_ACCENT}), polyLenE(voiceIdx, LaneId{PL_ACCENT}), polyOffE(voiceIdx, LaneId{PL_ACCENT}), polyRotE(voiceIdx, LaneId{PL_ACCENT}));
        refreshPolyRandomCell(voiceIdx, LaneId{PL_ACCENT}, accIdx);
        v.accented = (pe.warpRow(voiceIdx, LaneId{PL_ACCENT})[accIdx] < accTh);
        if (rd.gateType == MonoDecision::NewNote)
            v.gs.triggerNote(pitchV, sem, nvV, isQuant);
        else if (wasHeldPoly || hadPolyTail)
            // Mono is slurring/tying and THIS poly voice had a held predecessor → real slide.
            v.gs.slideNote(pitchV, sem, nvV, /*wasHeld=*/true, isQuant);
        else
            // Mono slurs but this poly voice had NO held gate (it was resting/silent on its
            // previous played step) — sliding would produce an isolated Legato cell (teal note
            // with no predecessor on this lane), the poly analogue of the mono isolated-teal
            // bug. Trigger a fresh note instead. (Direction-independent; surfaced in reverse
            // mode but present forward too.)
            v.gs.triggerNote(pitchV, sem, nvV, isQuant);
        v.gsStep.triggerNote(pitchV, sem, nvV, isQuant);   // STEP: every played onset re-strikes (un-fused)
        // F-1 capture: what this onset WAS — a fresh trigger (NewNote) or a real slide to a
        // held state (Legato). Children fold this as the container's verdict (D4: an onsetting
        // container names the opportunity type).
        polyVerdict_[voiceIdx.v] = (rd.gateType != MonoDecision::NewNote && (wasHeldPoly || hadPolyTail))
                                       ? MonoDecision::Legato : MonoDecision::NewNote;
        // F-4 (D5): this onset was ghost-placed (the voice PLAYED at a ghost cell of its
        // containers) — the ghost event its children will fold. A ghost-placed note that
        // SLID (the container's verdict was a Legato-from-dead offer) is still a ghost-cell
        // note: the event is "this note lives inside the containers' ghost cell", not "this
        // note re-struck". The rested-ghost / rest / landing / mid-hold paths leave the
        // top-of-edge false above.
        polyGhostOnset_[voiceIdx.v] = inGhostCell;

        // ── Rule 2 LEAD (per-voice leading-edge slur roll) ────────────────────────────────
        // Mirror mono's LEAD commitment (executeStep), per voice: this note commits to slur its
        // gate forward into the NEXT note iff its OWN legato draw fires. The draw reads the
        // legato strand at THIS voice's cell (getLegatoStepForVoice → mono's cell when delegated
        // (default) so it matches V1, own cell when Local East). Phase A: the THRESHOLD is
        // per-voice (v.legatoProb, with the mono slot legatoProb = V1 threshold); mono/V1 reads
        // the same voice-0 cell. Rolled here at the leading edge and consumed at the NEXT landing
        // (v.gs.slurForward → prevSlur). Delegated voices read V1's legato cell → their slur
        // matches V1's; Local-East voices read their own.
        // NOTE: this ROLLS the commitment; the poly landing consumes it (Rule 2 CONSUME below).
        {
            // This voice played at the chain onset → it is part of the chain for its life.
            v.participating = true;
            bool leStartingV = (rd.gateType == MonoDecision::NewNote)
                            || (rd.gateType == MonoDecision::Legato)
                            || (rd.gateType == MonoDecision::LegatoMax)
                            || (rd.gateType == MonoDecision::Tie);
            int legIdx = getLegatoStepForVoice(voiceIdx);
            refreshPolyRandomCell(voiceIdx, LaneId{PL_LEGATO}, legIdx);
            float r_polyLegato = pe.warpRow(voiceIdx, LaneId{PL_LEGATO})[legIdx];
            v.gs.slurForward = leStartingV
                             && noteCanLeadLegato(nvV)
                             && (legTh >= 0.999f || r_polyLegato < legTh);
            v.gs.slurMember = v.gs.slurForward;   // SLEG: onset member iff it leads a slur
#if RULE2_DEBUG
            INFO("[R2 roll ] v=%2d step=%3lld LE=%d legCell=%2d monoCell=%2d legLOR=(%2d,%2d,%2d) "
                 "r=%.3f prob=%.3f nvV=%d canLead=%d -> slur=%d",
                voiceIdx.v, (long long)globalWalk_, (int)!varlegDelegated(voiceIdx.v, 1),
                getLegatoStepForVoice(voiceIdx), getLegatoStep(),
                polyLOR(voiceIdx, LaneId{EDITOR_LANE_LEGATO}, LOR_LEN),
                polyLOR(voiceIdx, LaneId{EDITOR_LANE_LEGATO}, LOR_OFF),
                polyLOR(voiceIdx, LaneId{EDITOR_LANE_LEGATO}, LOR_ROT),
                r_polyLegato, voices[VoiceResolver::mono()].legatoProb, nvV, (int)noteCanLeadLegato(nvV),
                (int)v.gs.slurForward);
#endif
        }
        return;
    }

    // If mono is Sustaining or Resting, poly must stick to its current role.
    if (rd.forcedRest) {   // C2: a container rested → force-close, no draws
        v.gs.gateHeld = false;
        v.gsStep.gateHeld = false;   // STEP: mirror mono rest
        v.gs.slurMember = false;     // SLEG: mono rested
        v.participating = false;   // Rule 2: mono rested → the chain ends for this voice.
        polyVerdict_[voiceIdx.v] = MonoDecision::Rest;   // F-1: the rest propagates to children
    } else {
        // A LANDING = mono connects at this edge (Legato / LegatoMax / Tie). MidNote is a
        // mid-hold, not an onset edge, so it never consumes or rolls a slur.
        const bool monoConnectLanding = (rd.gateType == MonoDecision::Legato)
                                      || (rd.gateType == MonoDecision::LegatoMax)
                                      || (rd.gateType == MonoDecision::Tie);

        if (monoConnectLanding) {
            // ── Rule 2 CONSUME (per-voice landing) ────────────────────────────────────────
            // A participating voice decides its OWN connect at this edge from its OWN prevSlur
            // (committed at its previous onset): prevSlur → connect (slide/extend, tracking mono's
            // articulation shape); else RE-ARTICULATE a fresh note (clamped ≤ mono via nvV). A
            // rester (participating==false) stays silent — this is why the latch is explicit, not
            // read off the gate (an opted-out voice whose short note closed is still in the chain).
            // Then re-roll this voice's forward commitment for the NEXT landing, exactly as mono
            // re-rolls at every sounding onset, so 3+ note chains carry forward per voice.
            if (!v.participating) {
                v.gs.gateHeld = false;   // rested at the onset → not part of this chain
                v.gsStep.gateHeld = false; v.gs.slurMember = false;   // STEP/SLEG mirror
                polyVerdict_[voiceIdx.v] = MonoDecision::Rest;   // F-1: silent at the landing
            } else {
                const bool prevSlur = v.gs.slurForward;
                const bool isTie    = (rd.gateType == MonoDecision::Tie);
                // Mono invariant: "a slur can only connect to a still-sounding predecessor."
                // A participating voice whose short note already closed via the Stage-2 clamp
                // (gatePulseRemain hit 0 mid-step) is NOT still sounding at this edge, so it must
                // RE-ARTICULATE rather than connect — even though it remains in the chain
                // (participating). Without this guard, a short legato lead followed by a clamp gap
                // (the visible/audible "rest") would draw a false teal/violet continuation at the
                // next landing, because prevSlur (committed at the lead) is only cleared by a real
                // rest DECISION (line ~568), not by an early gate close. wasHeldPoly/hadPolyTail
                // here are the still-sounding read, made accurate by the MidNote re-assert fix
                // (line ~712) which keeps a clamped voice's gateHeld from sticking high.
                const bool stillSounding = wasHeldPoly || hadPolyTail;
                const bool connect = prevSlur && stillSounding;
#if RULE2_DEBUG
                INFO("[R2 land ] v=%2d step=%3lld LE=%d part=%d prevSlur=%d sound=%d isTie=%d -> %s",
                    voiceIdx.v, (long long)globalWalk_, (int)!varlegDelegated(voiceIdx.v, 1),
                        (int)v.participating, (int)prevSlur, (int)stillSounding, (int)isTie,
                    connect ? "CONNECT" : "REARTICULATE");
#endif
                if (connect && isTie) {
                    // committed tie → hold own pitch and extend (no redraw), like the follow-mono tie
                    v.gs.extendHold(v.gs.lastSemitone, nvV);
                    v.gsStep.triggerNote(v.gs.currentPitchV, v.gs.lastSemitone, nvV);  // STEP
                    polyVerdict_[voiceIdx.v] = MonoDecision::Tie;   // F-1: an extend names Tie
                } else {
                    // committed legato → slide to own new pitch; opted out / not still sounding →
                    // re-articulate fresh. Either way this voice draws its OWN pitch (melody/octave
                    // LOR), never mono's.
                    int melIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_MELODY}), polyLenE(voiceIdx, LaneId{PL_MELODY}), polyOffE(voiceIdx, LaneId{PL_MELODY}), polyRotE(voiceIdx, LaneId{PL_MELODY}));
                    int octIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_OCTAVE}), polyLenE(voiceIdx, LaneId{PL_OCTAVE}), polyOffE(voiceIdx, LaneId{PL_OCTAVE}), polyRotE(voiceIdx, LaneId{PL_OCTAVE}));
                    int sem = 0;
                    refreshPolyRandomCell(voiceIdx, LaneId{PL_MELODY}, melIdx);
                    refreshPolyRandomCell(voiceIdx, LaneId{PL_OCTAVE}, octIdx);
                    float pitchV = pe.genPitchLive(sem, input, pe.warpRow(voiceIdx, LaneId{PL_MELODY})[melIdx], pe.warpRow(voiceIdx, LaneId{PL_OCTAVE})[octIdx]);
                    if (connect) {
                        v.gs.slideNote(pitchV, sem, nvV, /*wasHeld=*/true);   // connect (keep chain accent)
                        polyVerdict_[voiceIdx.v] = MonoDecision::Legato;   // F-1: a connect-slide
                    } else {
                        // re-articulate: a fresh onset, so draw this voice's OWN accent (its accent
                        // LOR vs its own accentProb), exactly as the chain onset does — a re-struck
                        // note isn't stuck with the accent the chain opened on.
                        int accIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_ACCENT}), polyLenE(voiceIdx, LaneId{PL_ACCENT}), polyOffE(voiceIdx, LaneId{PL_ACCENT}), polyRotE(voiceIdx, LaneId{PL_ACCENT}));
                        refreshPolyRandomCell(voiceIdx, LaneId{PL_ACCENT}, accIdx);
                        v.accented = (pe.warpRow(voiceIdx, LaneId{PL_ACCENT})[accIdx] < accTh);
                        v.gs.triggerNote(pitchV, sem, nvV);                   // re-articulate
                        polyVerdict_[voiceIdx.v] = MonoDecision::NewNote;   // F-1: a re-strike
                    }
                    v.gsStep.triggerNote(pitchV, sem, nvV);   // STEP: slide/re-artic both re-strike
                }
                // Re-roll the forward commitment for the NEXT landing (mirror mono's LEAD, per voice).
                { int legIdx2 = getLegatoStepForVoice(voiceIdx);
                  refreshPolyRandomCell(voiceIdx, LaneId{PL_LEGATO}, legIdx2);
                float r_polyLegato = pe.warpRow(voiceIdx, LaneId{PL_LEGATO})[legIdx2];
                v.gs.slurForward = noteCanLeadLegato(nvV)
                                 && (legTh >= 0.999f || r_polyLegato < legTh);
                v.gs.slurMember = prevSlur || v.gs.slurForward;  // SLEG: continues OR leads
                }
            }
        } else {
            // ── MidNote hold path (mono is sustaining mid-note, not at a landing edge) ─────
            // Poly follows mono gate presence strictly IF it was already active ("in").
            if (rd.holdFollow && wasHeldPoly) {   // C4: the tuple's hold-follow; own armed window below
                if (rd.gateType == MonoDecision::Legato || rd.gateType == MonoDecision::LegatoMax) {
                    // Mono slid to a NEW pitch (a real pitch move) → poly draws its OWN new pitch
                    // (its own melody/octave LOR) and slides to it. Poly tracks mono's articulation
                    // shape, never mono's actual notes.
                    int melIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_MELODY}), polyLenE(voiceIdx, LaneId{PL_MELODY}), polyOffE(voiceIdx, LaneId{PL_MELODY}), polyRotE(voiceIdx, LaneId{PL_MELODY}));
                    int octIdx = getStrandIdx(polyLaneTick(voiceIdx, LaneId{PL_OCTAVE}), polyLenE(voiceIdx, LaneId{PL_OCTAVE}), polyOffE(voiceIdx, LaneId{PL_OCTAVE}), polyRotE(voiceIdx, LaneId{PL_OCTAVE}));
                    int sem = 0;
                    refreshPolyRandomCell(voiceIdx, LaneId{PL_MELODY}, melIdx);
                    refreshPolyRandomCell(voiceIdx, LaneId{PL_OCTAVE}, octIdx);
                    float pitchV = pe.genPitchLive(sem, input, pe.warpRow(voiceIdx, LaneId{PL_MELODY})[melIdx], pe.warpRow(voiceIdx, LaneId{PL_OCTAVE})[octIdx]);
                    v.gs.slideNote(pitchV, sem, nvV, wasHeldPoly);
                    v.gsStep.triggerNote(pitchV, sem, nvV); v.gs.slurMember = true;  // STEP/SLEG
                    polyVerdict_[voiceIdx.v] = MonoDecision::Legato;   // F-1: a mid-hold slide
                } else if (rd.gateType == MonoDecision::Tie) {
                    // Mono held the same pitch (tie) → poly holds its own current pitch, extends hold.
                    v.gs.extendHold(v.gs.lastSemitone, nvV);
                    v.gsStep.triggerNote(v.gs.currentPitchV, v.gs.lastSemitone, nvV); v.gs.slurMember = true;  // STEP/SLEG
                    polyVerdict_[voiceIdx.v] = MonoDecision::Tie;   // F-1: a mid-hold extend
                } else {
                    // MidNote: mono is HOLDING a note still sounding — nothing was chosen this step
                    // (the executeStep guard returned early). Poly follows mono's hold ONLY while THIS
                    // voice's own armed gate window is still open. Setting gateHeld=true unconditionally
                    // here would RE-OPEN a gate that tickPulse already closed (gatePulseRemain == -1) for
                    // a short/Stage-2-clamped voice — and since tickPulse only closes gates it armed
                    // (gatePulseRemain >= 0), that re-opened gate would stick high and pollute
                    // wasHeldPolyPrev at the next landing, producing a false teal/violet continuation
                    // after a rest. So keep the gate high only if the voice is genuinely still sounding;
                    // otherwise honour the length clamp and leave it released.
                    v.gs.gateHeld = (v.gs.gatePulseRemain >= 0) || (v.gs.holdRemain > 0.0001f);
                    polyVerdict_[voiceIdx.v] = MonoDecision::MidNote;   // F-1: a mid-hold, nothing chosen
                }
            } else {
                // Mono gate is low OR poly chose to rest for this current high cycle.
                v.gs.gateHeld = false;
                v.gsStep.gateHeld = false; v.gs.slurMember = false;  // STEP/SLEG mirror
                polyVerdict_[voiceIdx.v] = MonoDecision::Rest;   // F-1: gate closed, no onset offer
            }
        }
    }
}

void SequencerEngine::executePolyVoices(const PatternInput& input) {
    // SUBGATE TIER: consume the edge level executeModeBSubdivided staged for this
    // block's cascade (−1 = main). Cleared immediately — one consume per cascade;
    // a stray call without a subdivided edge reads the main thresholds verbatim.
    const int subLevel = polySubLevel_;
    polySubLevel_ = -1;
    // F-2 (plan §3.2, D8): the forest materializes at the graph-change boundary — per block at
    // the publish point (MonsoonSandsManager::processDNA) and per edge HERE (which also covers
    // test sites that author pe.containmentParents directly). The visit order is the MATERIALIZED
    // topological order: every container — upward OR downward — executes before its children, so
    // each child's containerView reads THIS-edge captures everywhere (the F-1 slot-order caveat
    // is gone). The STAR's order is slot order (smallest-index-first Kahn) — byte-identical to
    // the pre-F-2 loop. The per-sample drive (Monsoon.cpp) reads the same materialized state and
    // does NO graph work (D8: no per-sample forest work).
    rdm.materializeIfStale();
    for (int k = 0; k < rdm.topoCount; ++k) {
        const int i = rdm.topoOrder[k];
        // F-3 (D7): the materialized order now includes slot 0 — the mono voice. Its cascade is
        // executeStep, which already ran (the mode executors call it before this visit); the
        // EDGE visit stays poly-only. V1's position in the order is the per-sample DRIVE's
        // concern (Monsoon.cpp: a poly container completes before V1's AND, V1 before its poly
        // children read the mono view).
        if (i == 0) continue;
        if (i > numPolyVoices) continue;   // voices beyond the active count are not executed
        // Use the state from BEFORE the tick to decide if we were active.
        // This ensures that if a note ended exactly at the boundary, 
        // we still consider it "active" for slaving to a continuing mono gate.
        // hadPolyTail is kept for safety with fractional start/ends.
        bool wasHeldPoly = wasHeldPolyPrev[i] || hadPolyTail[i];
        // i is the voiceSlot (V2..); slot(V) takes the VOICE number (= i+1)
        executePolyVoice(VoiceResolver::slot(i + 1), input, wasHeldPoly, hadPolyTail[i], subLevel);
    }
}

// Name the sounding DEGREE of a quantized pitch, for the flash LEDs / lastSemitone. At the 12-TET
// default this is the EXACT legacy expression (round(frac*12)%12) → byte-identical for N=12 / Sikit-
// at-default / standalone. Under a custom tuning (Micro publishing non-default cents, or a Micro-24)
// it's the table's nearest degree so the RIGHT fader flashes (a 24-tone degree isn't a 12-TET slot).
int SequencerEngine::degreeOf(float pitchV) const {
    float frac = pitchV - std::floor(pitchV);
    if (pe.tuning.isDefault12TET)
        return int(std::round(frac * 12.f)) % 12;
    return pe.tuning.nearestDegree(frac);
}

// Dead code removed (MODE_COLLAPSE_6_TO_3): the engine's executeModeC/D were never called by the
// controller (C→engine.executeModeA, D→engine.executeModeB) — the old fixed-quarter / sample-while-
// high Vermona logic. The quantiser is now the q-mix axis (engaged per-step in the dispatch), so these
// are gone.

float SequencerEngine::quantize(float vIn) {
    if (std::abs(vIn - lastQuantIn) < 1e-6f) return lastQuantOut;
    lastQuantIn = vIn;

    if (activeSemiCount == 0) return pe_clamp(vIn, 0.f, 5.f);
    int octave = (int)vIn;
    float bestScore = -1.f;
    float bestV = vIn;

    // Within-octave voltage of a cached ACTIVE degree. At the 12-TET default (isDefault12TET) this is
    // the EXACT legacy s/12 (byte-identical — the whole Mode C/D quantizer is unchanged for N=12,
    // any Sikit-at-default, and Monsoon standalone). Under a custom tuning (Micro publishing non-
    // default cents, or a Micro-24) it becomes the table's degreeVolts(s) so quantize snaps to the
    // ACTUAL tuning degrees, not 12-TET semitone slots.
    auto degV = [&](int s) -> float {
        return pe.tuning.isDefault12TET ? (s / 12.f) : pe.tuning.degreeVolts(s);
    };

    for (int ai = 0; ai < activeSemiCount; ++ai) {
        int s = activeSemiList[ai];
        float w = activeSemiWeight[ai];
        float radius = w * (1.f / 12.f);
        for (int o = octave - 1; o <= octave + 1; ++o) {
            float cand = o + degV(s);
            float dist = std::fabs(vIn - cand);
            if (dist <= radius + 1e-4f) {
                float score = w / (dist + 1e-4f);
                if (score > bestScore) { bestScore = score; bestV = cand; }
            }
        }
    }
    if (bestScore < 0.f) {
        float minDist = 1e9f;
        for (int ai = 0; ai < activeSemiCount; ++ai) {
            int s = activeSemiList[ai];
            for (int o = octave - 1; o <= octave + 1; ++o) {
                float cand = o + degV(s);
                float dist = std::fabs(vIn - cand);
                if (dist < minDist) { minDist = dist; bestV = cand; }
            }
        }
    }
    lastQuantOut = pe_clamp<float>(bestV, 0.f, 5.f);
    return lastQuantOut;
}
// Shared base index calculation with strand-specific phase and windowing
int SequencerEngine::getStrandIdx(int tickCount, int len, int off, int mutation) const
{
    int safeLen = std::max(1, len);
    // 1. (tickCount + mutation) % len  => Drift inside the DNA loop
    // Wrap properly for negative ticks (used by LED distance logic)
    int timelineIdx = ((tickCount + mutation) % safeLen + safeLen) % safeLen;
    // 2. ... + off                     => Slide the entire DNA window across the source buffer
    return (timelineIdx + off) % kMaskRing;
}

// ── EAST_EXTRA_LANES stage 2 ──────────────────────────────────────────────────────────────────
int SequencerEngine::getVariationStepForVoice(VoiceSlot bank) const {
    // Delegated (default) → read mono's VARIATION position, so the voice mirrors mono.
    // D6 SWEEP (F-4, RULED engine-internal): this default is the DELIBERATE boundary under a
    // forest — the delegated voice keeps reading V1's cell ("the root's cell") regardless of
    // the graph: the LOR windows produce DRAWS (lane positions), not permissions, and the
    // draw production is not the decision structure (plan §3.7). V1's own containment does
    // not touch its lane positions, so the delegated read is well-defined under any forest
    // shape (V1-as-leaf included). varlegLocalEast_ remains the per-voice opt-out.
    if (varlegDelegated(bank, 0)) return getVariationStep();
    // Local East: read THIS voice's own window, advanced on its per-voice tick (which inherits
    // mono's per-lane direction — a reversed mono VARIATION lane reverses this voice too).
    return getStrandIdx(laneTickV_[bank.v][dotModular::STRAND_VARIATION],
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_LEN),
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_OFF),
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_ROT));
}

int SequencerEngine::getLegatoStepForVoice(VoiceSlot bank) const {
    // Delegated (default) → read mono's LEGATO position, so the voice's slur roll matches mono.
    // D6 SWEEP (F-4): same boundary as the VAR getter above — the delegated default keeps
    // reading V1's cell under any forest shape (draw production, not the decision structure);
    // varlegLocalEast_ is the per-voice opt-out.
    if (varlegDelegated(bank, 1)) return getLegatoStep();
    // Local East: read THIS voice's own window, advanced on its per-voice tick (inherits mono's
    // per-lane direction — a reversed mono LEGATO lane reverses this voice's slur roll too).
    return getStrandIdx(laneTickV_[bank.v][dotModular::STRAND_LEGATO],
                        polyLOR(bank, LaneId{EDITOR_LANE_LEGATO}, LOR_LEN),
                        polyLOR(bank, LaneId{EDITOR_LANE_LEGATO}, LOR_OFF),
                        polyLOR(bank, LaneId{EDITOR_LANE_LEGATO}, LOR_ROT));
}

int SequencerEngine::nvIdxForVoice(VoiceSlot bank, const PatternInput& input) const {
    if (bank.v < 1 || bank.v >= 16) return monoParentView().nvIdx;
    // The probability array stays MONO (one shared shape); only the reading position is per-voice.
    const int idx = getVariationStepForVoice(bank) & (kMaskRing - 1);
    const float r = pe.warpRow(bank, LaneId{PL_VARIATION})[idx];
    const int nv  = const_cast<SequencerEngine*>(this)->getNoteLenIdx(lastNoteVal_, input, r);
    // CLAMP to the parents' event grid (C3 lengthCeiling, F-1: the REAL fold over the staged
    // graph): a voice may release early, never hold past any container's next note.
    // DOTTED BUILD: the table is no longer index-monotone — lengthCeiling compares
    // FRACTIONS (noteShorterOf), so the SHORTER window wins by duration, not index.
    // A FREE voice (empty mask, D2) holds its own drawn window — nothing clamps it.
    const uint16_t parents = rdm.parentsOf(bank.v);
    if (!parents) return nv;
    int ceiling = nv;
    if (parents & 0x0001u) ceiling = lengthCeiling(ceiling, monoParentView());
    for (int u = 1; u < 16; ++u)
        if (parents & (1u << u)) ceiling = lengthCeiling(ceiling, containerView(u));
    return ceiling;
}

int SequencerEngine::getRhythmStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_RHYTHM],    lor(dotModular::STRAND_RHYTHM,LOR_LEN), lor(dotModular::STRAND_RHYTHM,LOR_OFF), lor(dotModular::STRAND_RHYTHM,LOR_ROT)); }
int SequencerEngine::getVariationStep() const { return getStrandIdx(laneTick_[dotModular::STRAND_VARIATION], lor(dotModular::STRAND_VARIATION,LOR_LEN), lor(dotModular::STRAND_VARIATION,LOR_OFF), lor(dotModular::STRAND_VARIATION,LOR_ROT)); }
int SequencerEngine::stepAdvanceStrandIdx(VoiceSlot bank, int drawTickDelta) const {
    // THE ONE scanned-position expression (CC_PROMPT_VARIATION_SCAN_DISPLAY, factored
    // from stepAdvanceDrawAt — a PURE factoring; CC_FIX_PER_VOICE_ADVANCE_WANDER's DRY
    // audit collapsed the mono/per-voice twins): ONE function, N voices — the mono slot
    // (and any out-of-range slot) reads the MONO window (laneTick_ + lor), a poly voice
    // its OWN (laneTickV_[v] + its own LOR — the decision structure; draw production
    // keeps its delegation, the D6 SWEEP ruling). TWO reader kinds, ONE function: the
    // value-read (stepAdvanceDrawAt → the advance decision) and the display (the Sands
    // VAR chevron) — the chevron sits at EXACTLY the index the engine read.
    // Display-reads-engine: no parallel tick math anywhere.
    if (bank.v < 1 || bank.v >= 16) {
        return getStrandIdx(laneTick_[dotModular::STRAND_VARIATION] + drawTickDelta,
                            lor(dotModular::STRAND_VARIATION, LOR_LEN),
                            lor(dotModular::STRAND_VARIATION, LOR_OFF),
                            lor(dotModular::STRAND_VARIATION, LOR_ROT));
    }
    return getStrandIdx(laneTickV_[bank.v][dotModular::STRAND_VARIATION] + drawTickDelta,
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_LEN),
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_OFF),
                        polyLOR(bank, LaneId{EDITOR_LANE_VARIATION}, LOR_ROT));
}
int SequencerEngine::stepAdvanceScanDelta(VoiceSlot bank) const {
    // The display's SIGNED scan (CC_PROMPT_VARIATION_SCAN_DISPLAY + the backward fix):
    // the scan magnitude, signed live by the SAME p the decision signs by (the slot's
    // variationProb — the travel direction). Mono/out-of-range reads [0]. No cached
    // sign member (nothing to desync); magnitude unchanged.
    if (bank.v < 1 || bank.v >= 16) bank = VoiceResolver::mono();
    return (voices[bank].variationProb < 0.f) ? -scanOfsV_[bank.v] : scanOfsV_[bank.v];
}
float SequencerEngine::stepAdvanceDrawAt(VoiceSlot bank, int drawTickDelta) const {
    // THE ONE advance-draw expression (decideStepAdvance_ delegates here; the test
    // mirror reads the same): the variation draw at the scanned position. Pure —
    // reading never mutates the ring (zero new draws).
    return pe.warpRow(bank, LaneId{PL_VARIATION})[stepAdvanceStrandIdx(bank, drawTickDelta)];
}
int SequencerEngine::getLegatoStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_LEGATO],    lor(dotModular::STRAND_LEGATO,LOR_LEN), lor(dotModular::STRAND_LEGATO,LOR_OFF), lor(dotModular::STRAND_LEGATO,LOR_ROT)); }
int SequencerEngine::getAccentStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_ACCENT],    lor(dotModular::STRAND_ACCENT,LOR_LEN), lor(dotModular::STRAND_ACCENT,LOR_OFF), lor(dotModular::STRAND_ACCENT,LOR_ROT)); }
int SequencerEngine::getMelodyStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_MELODY],    lor(dotModular::STRAND_MELODY,LOR_LEN), lor(dotModular::STRAND_MELODY,LOR_OFF), lor(dotModular::STRAND_MELODY,LOR_ROT)); }
int SequencerEngine::getOctaveStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_OCTAVE],    lor(dotModular::STRAND_OCTAVE,LOR_LEN), lor(dotModular::STRAND_OCTAVE,LOR_OFF), lor(dotModular::STRAND_OCTAVE,LOR_ROT)); }
int SequencerEngine::getQmixStep() const      { return getStrandIdx(laneTick_[dotModular::STRAND_QMIX],      lor(dotModular::STRAND_QMIX,LOR_LEN),   lor(dotModular::STRAND_QMIX,LOR_OFF),   lor(dotModular::STRAND_QMIX,LOR_ROT)); }

void SequencerEngine::syncVisuals(const PatternInput& in) {
    pe.refreshVisualCache(in);
}
