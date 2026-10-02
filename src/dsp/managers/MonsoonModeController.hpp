#pragma once

#include "rack.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../engines/ClockEngine.hpp"
#include "MonsoonParameterManager.hpp"

struct Monsoon; // Forward declaration
struct InputState; // Forward declaration

/**
 * ModeController
 * 
 * Encapsulates all mode-specific sequencing logic (Modes A, B, C, D).
 * 
 * Centralizes:
 *   1. Mode dispatch logic
 *   2. Per-mode execution handlers
 *   3. Parameter application for each mode
 *   4. Poly voice initialization per mode
 *   5. Step boundary/phrase boundary handling
 * 
 * This class takes the sequencer engine and parameter manager, then orchestrates
 * how modes update the sequencer state and generate step results.
 * 
 * Design: ModeController is responsible for "how to execute a mode given a trigger".
 * It is NOT responsible for triggering conditions (those stay in process()).
 */
class ModeController {
public:
    ModeController(Monsoon* mainModule,
                   SequencerEngine& engine,
                   const ClockEngine& clock,
                   ParameterManager& paramManager)
        : mainModule(mainModule),
          engine(engine),
          clock(clock),
          paramManager(paramManager),
          lastStepIndex(-1) {}
    
    // ──── Mode Execution ────────────────────────────────────────────────────
    // MODE_COLLAPSE_6_TO_3: three TIMING ORIGINS — clock(0) / gate(1) / phase(2). Pitch origin
    // (generate vs quantise) is the q-mix AXIS, engaged per-step in executeMode() via
    // beginQuantiserSource_ (not a mode). The three execute* fns below are the GENERATE twins of
    // each timing origin; the quantiser is "that origin with q-mix up" (no separate mode).
    
    /// Clock origin (mode 0): clock-driven sequencing. Triggers on clock sixteenth edges.
    /// (Was Mode A; the old Mode C "clock quantiser" is now clock + q-mix up.)
    bool executeModeA();

    /// Phase origin (mode 2): phase-ramp driven (forward; reverse via phaseReverse). Triggers on
    /// phase sixteenth edges. (Was Mode E; the old Mode F "phase quantiser" is now phase + q-mix up.)
    bool executeModeE();

    // Mode E playhead direction, set from the PhaseEngine each block before dispatch.
    bool phaseReverse = false;
    void setPhaseReverse(bool rev) { phaseReverse = rev; }
    
    /// Gate origin (mode 1): gate-driven sequencing. Triggers on GATE1 rising edge or continuous
    /// hold, or on subGate edges when useSubGate. useSubGate: Gate 2 = ratchet (in-gate) + Gate 3 =
    /// ghost (in-gap); gate1 is the main gate. (Was Mode B; the old Mode D "gate quantiser" is now
    /// gate + q-mix up.) The InputState carries the ratchet/ghost edges + ghost level.
    /// Returns true if a new step was taken.
    bool executeModeB(const InputState& input,
                      bool useSubGate = false);
    
    // ──── High-Level Dispatcher ──────────────────────────────────────────────
    
    /// Execute the appropriate timing origin based on modeId (0=clock, 1=gate, 2=phase) and engage
    /// the q-mix pitch-source axis per step. Returns true if a new step was taken.
    bool executeMode(int modeId,
                     const InputState& input,
                     bool gate2High);

    /// Update the internal PatternInput snapshot from current parameters
    void updatePatternInput();

    PatternInput currentPatternInput; // Cached PatternInput
    
    // ──── State Accessors ────────────────────────────────────────────────────
    
    /// Get the last step index (used for change detection)
    int getLastStepIndex() const { return lastStepIndex; }
    
    /// Update last step index (call after mode execution)
    void updateLastStepIndex() { lastStepIndex = engine.stepIndex; }
    
private:
    Monsoon* mainModule;
    SequencerEngine& engine;
    const ClockEngine& clock;
    ParameterManager& paramManager;
    
    int lastStepIndex;

    // LOCK Phase 2 one-shot prime (LOCK_SEMANTICS §9). The LATCH controls threaded in
    // updatePatternInput() hold their pre-lock value under lock by SKIPPING the per-block refresh
    // (the currentPatternInput field persists across blocks, like the LOR engine-state push-gate).
    // But lock STATE is persisted (PersistenceManager) while these struct fields are NOT, so a patch
    // SAVED+LOADED while locked would leave them at cold-start defaults (semiWeights all-0 -> blank
    // pitch). This flag forces a full unconditional populate on the FIRST updatePatternInput() after
    // construction/load, seeding from the restored knobs; every block after obeys the lock gate.
    bool patternInputPrimed_ = false;

    // Poly REST/ACCENT LATCH prime. voices[].restProb/accentProb are the poly Big-5 (§9 Tier V, LATCH
    // like mono). updatePolyVoiceRest_ is their sole writer; skipping it under lock holds the pre-lock
    // value (same mechanism as mono). Needs its OWN prime (not patternInputPrimed_) because it runs in
    // postExecute_, AFTER updatePatternInput already set that flag — so it must track its own first-write.
    // Covers the locked-load case (voices[] default to 0.0, not persisted → first write populates).
    bool polyVoiceCachePrimed_ = false;
    
    // ──── Helper Methods ────────────────────────────────────────────────────
    
    /// Assemble a PatternInput snapshot for the engine
    PatternInput assemblePatternInput_();

    /// Common post-execution logic: handle phrase boundaries and poly voices
    void postExecute_(const StepResult& result);

    /// Quantiser (Q1b): enable the engine's external-CV pitch source + load quantiserCV for this step.
    /// Caller clears engine.quantiserPitchSource after postExecute_ (poly voices draw pitch there too).
    void beginQuantiserSource_(float cv2Voltage);
    
    /// Set poly voice rest probabilities from parameter manager
    void updatePolyVoiceRest_();
};
