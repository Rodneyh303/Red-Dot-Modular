#pragma once
#include <rack.hpp>
#include <cstdint>

// ============================================================================================
// LockManager -- single source of truth for lock CATEGORY decisions (LOCK_SEMANTICS.md §9).
//
// Lock STATE lives in the engine (PatternEngine::locked): a single global bool. The engine keeps
// its OWN freeze checks (redrawRhythm/redrawMelody/applyPendingSeedsAndRedraw) because those are
// LIVE-critical on the audio thread and must not round-trip through a manager.
//
// This manager owns the MODEL: for each control, its lock CATEGORY, and a single liveNow(Control)
// query that call sites use INSTEAD of a bespoke `if (!engine.locked)`. The category logic lives
// here; call sites stop encoding "am I locked?" decisions ad hoc.
//
// Phase 1 (this file): the category map + liveNow. This is a REFACTOR of already-correct behaviour
// -- every audited LATCH site currently does `if (!locked)`, and liveNow(LATCH) == !locked, so the
// consolidation is behaviour-preserving (the ~23 lock test assertions must stay green).
//
// Phase 2 (later): boundary/unlock EVENTS + the QUEUE for arm-and-fire-at-boundary controls
// (Change Alley scatter), absorbing the caV2PrevStep_/caV2PrevLocked_ shadow state currently in
// MonsoonExpanderManager. Also the OPEN rulings that actually CHANGE behaviour (transpose->LIVE,
// direction->LATCH). Not in phase 1.
// ============================================================================================

namespace dotModular {

// The lock category of a control (LOCK_SEMANTICS.md §9).
enum class LockCategory : uint8_t {
    LATCH,   // obeys lock: frozen at lock-on, commits at unlock (generation-section shaping)
    LIVE,    // never obeys lock: always applies (transport / post-generation output mapping)
    QUEUE,   // arm-and-fire at the next phrase boundary (events, e.g. Change Alley scatter)
};

// ── Lock SCOPE bitmask (LOCK_SCOPE_MENU.md) ─────────────────────────────────────────────────
// The context-menu "Lock Scope" lets the user opt individual generative surfaces OUT of the lock
// freeze, split by rhythm vs melody where the data allows. A SET bit means "keep this group LIVE
// under lock" (exclude it from the freeze); mask 0 = whole-module lock (every group frozen) = the
// pre-menu Phase-2 behaviour. Groups map to the §9 control classes; R/M are separate bits because
// nearly every datum is already stored as separate rhythm/melody arrays. CV rides its target's bit
// automatically (gates sit downstream of CV folding — LOCK_SCOPE_MENU §1a), so there is NO CV bit.
enum ScopeBit : uint32_t {
    SCOPE_NONE        = 0,
    SB_BIG5_R         = 1u << 0,   // Big-5 / articulation (rhythm axis): REST/VAR/LEG/ACC/NOTE_VALUE + poly R/A
    SB_SCALE_M        = 1u << 1,   // scale sliders + OCT LO/HI (melody axis)
    SB_SANDS_R        = 1u << 2,   // Sands DNA rhythm: LOR + spread + direction/owner on rhythm strands
    SB_SANDS_M        = 1u << 3,   // Sands DNA melody: LOR + spread + direction/owner on melody strands
    SB_CA_R           = 1u << 4,   // Change Alley rhythm: pins + transforms + scatter (rhythmSrc / type==0 rows)
    SB_CA_M           = 1u << 5,   // Change Alley melody: pins + transforms + scatter (melodySrc / type==1 rows)
    SB_ABRESEED_R     = 1u << 6,   // A/B mix + reseed (rhythm stream)
    SB_ABRESEED_M     = 1u << 7,   // A/B mix + reseed (melody stream)
    SB_DICE_R         = 1u << 8,   // rhythm dice redraw (incl. live-mode per-cycle reroll) under lock
    SB_DICE_M         = 1u << 9,   // melody dice redraw (incl. live-mode per-cycle reroll) under lock
};

// Controls that have a lock category. This enum is the vocabulary call sites use with liveNow().
// Grouped by category per the audit; the category is assigned centrally in categoryOf() below.
enum class Control : uint8_t {
    // --- LATCH: generation-section shaping (read/shape the probability arrays) ---
    Spread,        // spread + spread attenuverters (all managers' spread writes)
    Lor,           // DNA length/offset/rotation (base + global + interp)
    ABMix,         // A/B mix blend (upstream generation, pre-spread pre-pins) -- ModeController
    Pins,          // Change Alley pin matrix (correlation shaping)
    BigFive,       // the 5 RHYTHM KNOBS: NOTE_VALUE / VARIATION / LEGATO / REST / ACCENT (+ CV)
    NoteSliders,   // the 12 per-semitone note light-sliders (scale weights -> semiWeights)
    OctaveRange,   // the 2 octave sliders: OCT LO / OCT HI range
    Reseed,        // reseed-on-restart / seed application (generation)
    Direction,     // per-lane traversal direction (editor.laneDir). Array READ, like LOR -> LATCH.
                   // LOCK_MODE_AUDIT:185. Gate the PUSH into engine traversal (setStrand analogue).
                   // BEHAVIOUR CHANGE (Phase 2): direction does not currently latch.
    Owner,         // per-lane owner select. Twin with Direction (LOCK_MODE_AUDIT:183-184) -> LATCH.
                   // May already latch "for free" via the LOR push gate (owner selects the base that
                   // feeds baseLen) -- verify before adding a call site; entry may be model-only.

    // --- LIVE: transport + post-generation output mapping ---
    Clock,         // BPM/RUN/RESET/MODE/PHASE
    Mute,          // mute
    Display,       // themes / lantern / display controls
    Transpose,     // pitch transpose -- applied at OUTPUT time (genPitchLive), downstream of the
                   // freeze, so already LIVE (changes audibly transpose the frozen pattern under
                   // lock). Entry for model completeness; no call-site gate exists to migrate.

    // --- QUEUE: arm-and-fire at phrase boundary ---
    Scatter,       // Change Alley scatter gate (event, not a held value)

    NUM_CONTROLS
};

class LockManager {
public:
    // Lock SCOPE (LOCK_SEMANTICS §7): how much of the LATCH set actually latches when lock is on.
    // Whole-module is the v1 default; Section/PerLane are the future refinements the context menu
    // exposes. The enum + storage + menu ship now; only WholeModule is functionally wired (Section/
    // PerLane fall back to whole-module gating until their section/lane mapping is defined).
    enum class LockScope : uint8_t {
        WholeModule,   // default: the entire LATCH set latches
        Section,       // latch one section (e.g. melody prep) but keep the other live -- future
        PerLane,       // latch selected lanes only (matches per-lane owner/direction) -- future
    };
    LockScope scope = LockScope::WholeModule;

    // Bind to the engine's lock bool (single source of lock STATE). The manager does not own the
    // bool -- it reads it. Constructed with a reference so it always sees the current lock state.
    explicit LockManager(const bool& lockedRef) : locked_(lockedRef) {}

    // The category of a control (the §9 table, as code). Single source of truth.
    static constexpr LockCategory categoryOf(Control c) {
        switch (c) {
            // LATCH set
            case Control::Spread:
            case Control::Lor:
            case Control::ABMix:
            case Control::Pins:
            case Control::BigFive:
            case Control::NoteSliders:
            case Control::OctaveRange:
            case Control::Reseed:
            case Control::Direction:
            case Control::Owner:
                return LockCategory::LATCH;
            // LIVE set
            case Control::Clock:
            case Control::Mute:
            case Control::Display:
            case Control::Transpose:
                return LockCategory::LIVE;
            // QUEUE set
            case Control::Scatter:
                return LockCategory::QUEUE;
            default:
                return LockCategory::LATCH;   // safe default: unknown control latches
        }
    }

    // Should this control APPLY right now?  The query call sites use instead of `if (!locked)`.
    //   LATCH -> live only when unlocked (frozen under lock; commits at unlock elsewhere).
    //   LIVE  -> always live (never obeys lock).
    //   QUEUE -> not "live now" in the continuous sense; it arms and fires at a boundary. For the
    //            phase-1 continuous-write call sites this returns !locked (matching current
    //            behaviour); the true arm-and-fire semantics arrive with the phase-2 queue.
    // SCOPE (§7): only WholeModule is functionally wired -- it latches the entire LATCH set (current
    // behaviour). Section/PerLane would NARROW this (keep some latch-set controls live under lock)
    // once their section/lane mapping exists; until then they behave as WholeModule. So scope does
    // not yet alter liveNow -- the field is stored/persisted/menu-exposed, ready to branch here.
    bool liveNow(Control c) const {
        switch (categoryOf(c)) {
            case LockCategory::LIVE:  return true;
            case LockCategory::LATCH: return !locked_;
            case LockCategory::QUEUE: return !locked_;   // phase-1 placeholder; phase-2 = queue
        }
        return !locked_;
    }

    bool isLocked() const { return locked_; }

    // -------------------------------------------------------------------------------------------
    // Boundary / unlock EVENTS (phase 2). Absorbs the caV2PrevStep_/caV2PrevLocked_ shadow state
    // that MonsoonExpanderManager used to keep. Ticked once per control cycle with the current
    // step index and lock state; computes the phrase-boundary edge (step wrapped) and the unlock
    // edge (locked->unlocked this tick). QUEUE controls (Change Alley scatter, Raffles gates) fire
    // when queueFires() is true: at a phrase boundary while unlocked, OR at the unlock edge (flush).
    //
    // This is lock/TRANSPORT logic, not expander topology -- it belongs here, driven off the
    // engine's step index (the single transport authority), not re-derived in a topology manager.
    void tick(int stepIndex) {
        boundaryNow_ = (stepIndex < prevStep_);          // step wrapped => phrase boundary
        unlockNow_   = (prevLocked_ && !locked_);         // locked -> unlocked this tick
        prevStep_    = stepIndex;
        prevLocked_  = locked_;
    }
    bool boundaryNow() const { return boundaryNow_; }
    bool unlockNow()   const { return unlockNow_; }

    // Should QUEUE-category pending events fire this tick?
    //   (phrase boundary while unlocked) OR (unlock edge -- flush what was queued during lock).
    bool queueFires() const { return (boundaryNow_ && !locked_) || unlockNow_; }

    // ── Lock SCOPE mapping (LOCK_SCOPE_MENU.md) ────────────────────────────────────────────
    // Which scope bit governs a control, given the axis the CALL SITE is acting on. Single-axis
    // controls ignore melodyAxis (Big-5 is always rhythm; scale/range always melody). Dual-axis
    // controls (Sands LOR/Spread/Direction/Owner, Change Alley Pins/Scatter, A/B+Reseed, Dice)
    // pick R or M. LIVE controls have no bit (never frozen) -> 0.
    static constexpr uint32_t scopeBitFor(Control c, bool melodyAxis) {
        switch (c) {
            case Control::BigFive:     return SB_BIG5_R;                          // rhythm axis (+ poly R/A)
            case Control::NoteSliders:                                            // scale sliders
            case Control::OctaveRange: return SB_SCALE_M;                         // OCT LO/HI — melody axis
            case Control::Lor:
            case Control::Spread:
            case Control::Direction:
            case Control::Owner:       return melodyAxis ? SB_SANDS_M : SB_SANDS_R;
            case Control::Pins:
            case Control::Scatter:     return melodyAxis ? SB_CA_M    : SB_CA_R;
            case Control::ABMix:
            case Control::Reseed:      return melodyAxis ? SB_ABRESEED_M : SB_ABRESEED_R;
            default:                   return SCOPE_NONE;                         // LIVE set: no scope bit
        }
    }

    // Static form: evaluate a control's live-now decision against an explicitly-provided lock
    // state. Lets call sites that hold their own lock bool (e.g. managers reading a specific
    // engine's `locked`) route through the SAME category model without needing a LockManager
    // instance or a Monsoon* -- and provably preserve which lock bool they read.
    //
    // SCOPE-AWARE overload: a LATCH control is ALSO live when its scope bit is set in scopeLiveMask
    // (the user opted its group out of the freeze -- LOCK_SCOPE_MENU.md). mask 0 = whole-module lock
    // (every LATCH frozen) = the pre-menu behaviour, so the 2-arg form below delegates with mask 0
    // and every existing call site is unchanged. melodyAxis selects the R/M bit for dual-axis controls.
    static bool liveNow(Control c, bool locked, uint32_t scopeLiveMask, bool melodyAxis = false) {
        switch (categoryOf(c)) {
            case LockCategory::LIVE:  return true;
            case LockCategory::LATCH:
            case LockCategory::QUEUE:                                             // phase-1 placeholder; phase-2 = queue
                if (!locked) return true;
                return (scopeLiveMask & scopeBitFor(c, melodyAxis)) != 0;         // opted out of freeze?
        }
        return !locked;
    }

    // Back-compat 2-arg form: whole-module lock (mask 0). Every LATCH freezes under lock, exactly as
    // before the scope menu. Existing call sites keep compiling and behaving identically.
    static bool liveNow(Control c, bool locked) {
        return liveNow(c, locked, /*scopeLiveMask=*/0u, /*melodyAxis=*/false);
    }

private:
    const bool& locked_;   // reference to engine's lock state (not owned)
    int  prevStep_    = 0;      // phrase-boundary edge detect (step-wrap)
    bool prevLocked_  = false;  // unlock edge detect
    bool boundaryNow_ = false;
    bool unlockNow_   = false;
};

} // namespace dotModular
