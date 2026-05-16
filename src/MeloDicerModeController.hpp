#pragma once

#include "rack.hpp"
#include "SequencerEngine.hpp"
#include "ClockEngine.hpp"
#include "MeloDicerParameterManager.hpp"

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
    // Callback function type for phrase boundary events
    using PhraseCallback = std::function<void()>;
    
    ModeController(SequencerEngine& engine,
                   const ClockEngine& clock,
                   ParameterManager& paramManager)
        : engine(engine),
          clock(clock),
          paramManager(paramManager),
          lastStepIndex(-1) {}
    
    // ──── Mode Execution ────────────────────────────────────────────────────
    
    /// Execute Mode A: Clock-driven sequencing
    /// Triggers on clock sixteenth edges
    /// Returns true if a new step was taken
    bool executeMode A(const rack::engine::Module::ProcessArgs& args,
                       PhraseCallback onPhraseBoundary);
    
    /// Execute Mode B: Gate-driven sequencing
    /// Triggers on GATE1 rising edge or continuous hold
    /// Returns true if a new step was taken
    bool executeModeB(const rack::engine::Module::ProcessArgs& args,
                      bool gate1Rise,
                      bool gate1High,
                      PhraseCallback onPhraseBoundary);
    
    /// Execute Mode C: Quantizer mode 1 (CV2 latch on quarter notes)
    /// Triggers on clock quarter-note edges
    /// Returns true if a new step was taken
    bool executeModeC(const rack::engine::Module::ProcessArgs& args,
                      float cv2Voltage,
                      PhraseCallback onPhraseBoundary);
    
    /// Execute Mode D: Quantizer mode 2 (GATE2 driven)
    /// Triggers based on GATE2 state and CV2 voltage
    /// Returns true if a new step was taken
    bool executeModeD(const rack::engine::Module::ProcessArgs& args,
                      bool gate2High,
                      float cv2Voltage,
                      PhraseCallback onPhraseBoundary);
    
    // ──── High-Level Dispatcher ──────────────────────────────────────────────
    
    /// Execute the appropriate mode based on modeId (0–3)
    /// Returns true if a new step was taken
    bool executeMode(int modeId,
                     const rack::engine::Module::ProcessArgs& args,
                     bool gate1Rise,
                     bool gate1High,
                     bool gate2High,
                     float cv2Voltage,
                     PhraseCallback onPhraseBoundary);
    
    // ──── State Accessors ────────────────────────────────────────────────────
    
    /// Get the last step index (used for change detection)
    int getLastStepIndex() const { return lastStepIndex; }
    
    /// Update last step index (call after mode execution)
    void updateLastStepIndex() { lastStepIndex = engine.stepIndex; }
    
private:
    SequencerEngine& engine;
    const ClockEngine& clock;
    ParameterManager& paramManager;
    
    int lastStepIndex;
    
    // ──── Helper Methods ────────────────────────────────────────────────────
    
    /// Common post-execution logic: handle phrase boundaries and poly voices
    void postExecute_(const StepResult& result, PhraseCallback onPhraseBoundary);
    
    /// Set poly voice rest probabilities from parameter manager
    void updatePolyVoiceRest_();
};
