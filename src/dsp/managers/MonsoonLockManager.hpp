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

// Controls that have a lock category. This enum is the vocabulary call sites use with liveNow().
// Grouped by category per the audit; the category is assigned centrally in categoryOf() below.
enum class Control : uint8_t {
    // --- LATCH: generation-section shaping (read/shape the probability arrays) ---
    Spread,        // spread + spread attenuverters (all managers' spread writes)
    Lor,           // DNA length/offset/rotation (base + global + interp)
    ABMix,         // A/B mix blend (upstream generation, pre-spread pre-pins) -- ModeController
    Pins,          // Change Alley pin matrix (correlation shaping)
    BigFive,       // NOTE_VALUE/VARIATION/LEGATO/REST/ACCENT sliders + their CV
    ScaleMask,     // SEMI scale toggles, OCT LO/HI range, Shophouse mask values
    Reseed,        // reseed-on-restart / seed application (generation)

    // --- LIVE: transport + post-generation output mapping ---
    Clock,         // BPM/RUN/RESET/MODE/PHASE
    Mute,          // mute
    Display,       // themes / lantern / display controls
    // Transpose is OPEN-leaning-LIVE; deferred to phase 2 (behaviour change). Not listed yet.

    // --- QUEUE: arm-and-fire at phrase boundary ---
    Scatter,       // Change Alley scatter gate (event, not a held value)

    NUM_CONTROLS
};

class LockManager {
public:
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
            case Control::ScaleMask:
            case Control::Reseed:
                return LockCategory::LATCH;
            // LIVE set
            case Control::Clock:
            case Control::Mute:
            case Control::Display:
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
    bool liveNow(Control c) const {
        switch (categoryOf(c)) {
            case LockCategory::LIVE:  return true;
            case LockCategory::LATCH: return !locked_;
            case LockCategory::QUEUE: return !locked_;   // phase-1 placeholder; phase-2 = queue
        }
        return !locked_;
    }

    bool isLocked() const { return locked_; }

private:
    const bool& locked_;   // reference to engine's lock state (not owned)
};

} // namespace dotModular
