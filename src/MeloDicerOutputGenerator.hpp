#pragma once

#include "rack.hpp"

// Forward declarations
class SequencerEngine;
namespace MeloDicerIds {
    enum OutputIds;
}

/**
 * OutputGenerator
 * 
 * Encapsulates all output voltage generation logic.
 * 
 * Centralizes:
 *   1. Gate output from sequencer engine
 *   2. Mute logic on gate output
 *   3. Tie gate output (MonoDecision::Tie feedback)
 *   4. Accent gate output
 *   5. Melodic/tonal/poly trigger outputs
 *   6. Poly voice gate outputs
 * 
 * Design: OutputGenerator reads sequencer state and generates appropriate
 * output voltages; no decision-making, pure translation from state to voltage.
 */
class OutputGenerator {
public:
    OutputGenerator() {}
    
    // ──── Main Output Generation ────────────────────────────────────────────
    
    /// Generate all outputs based on sequencer state
    void generateOutputs(SequencerEngine& engine,
                         rack::engine::Output* outputs,
                         float currentPitchV,
                         bool muted);
    
    /// Generate gate output specifically
    void setGateOutput(rack::engine::Output& gateOut, float gateV, bool muted);
    
    /// Generate tie gate output based on mono decision
    void setTieGateOutput(rack::engine::Output& tieGateOut,
                          bool isTie);
    
    /// Generate accent gate output
    void setAccentGateOutput(rack::engine::Output& accentGateOut,
                             bool isAccented);
    
    /// Generate poly voice trigger outputs
    void setPolyVoiceOutputs(rack::engine::Output* outputs,
                             SequencerEngine& engine,
                             int numPolyVoices);

private:
    // Helper: Set individual gate output with mute masking
    void setGateWithMute_(rack::engine::Output& out, float gateV, bool muted);
};
