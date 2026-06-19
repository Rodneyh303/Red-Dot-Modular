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
    
    /// Execute Mode A: Clock-driven sequencing
    /// Triggers on clock sixteenth edges
    /// Returns true if a new step was taken
    bool executeModeA();
    bool executeModeE();   // Mode E: phase-ramp driven (forward; reverse next branch)
    
    /// Execute Mode B: Gate-driven sequencing
    /// Triggers on GATE1 rising edge or continuous hold
    /// Returns true if a new step was taken
    bool executeModeB(bool gate1Rise,
                      bool gate1High);
    
    /// Execute Mode C: Quantizer mode 1 (CV2 latch on quarter notes)
    /// Triggers on clock quarter-note edges
    /// Returns true if a new step was taken
    bool executeModeC(float cv2Voltage);
    
    /// Execute Mode D: Quantizer mode 2 (GATE2 driven)
    /// Triggers based on GATE2 state and CV2 voltage
    /// Returns true if a new step was taken
    bool executeModeD(bool gate2High,
                      float cv2Voltage);
    
    // ──── High-Level Dispatcher ──────────────────────────────────────────────
    
    /// Execute the appropriate mode based on modeId (0–3)
    /// Returns true if a new step was taken
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
    
    // ──── Helper Methods ────────────────────────────────────────────────────
    
    /// Assemble a PatternInput snapshot for the engine
    PatternInput assemblePatternInput_();

    /// Common post-execution logic: handle phrase boundaries and poly voices
    void postExecute_(const StepResult& result);
    
    /// Set poly voice rest probabilities from parameter manager
    void updatePolyVoiceRest_();
};
