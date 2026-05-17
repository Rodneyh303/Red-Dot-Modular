#pragma once

#include "rack.hpp"

/**
 * TimingController
 * 
 * Encapsulates all gate-based timing and control logic.
 * 
 * Centralizes:
 *   1. Run gate processing (external input + button toggle)
 *   2. Reset gate processing (external input + button trigger)
 *   3. Reset pulse generation (1ms output pulse)
 *   4. Gate1 assignment handling (4 modes)
 *   5. Gate2 assignment handling (4 modes)
 *   6. Gate rise/fall edge detection
 * 
 * Design: TimingController manages gate state and triggers; orchestrates how
 * gates affect sequencer state (mode switching, seeding, mute, restart, etc.).
 */
class TimingController {
public:
    TimingController(rack::engine::Module* mainModule)
        : mainModule(mainModule) {}
    
    // ──── Run Gate Control ──────────────────────────────────────────────────
    
    /// Process run gate (external input + button) and return new runGateActive state
    bool processRunGate(bool currentlyActive,
                        float runGateInputV,
                        float runGateButtonVal);
    
    /// Get reset pulse output voltage (1ms pulse)
    float getResetPulseOutput(float sampleTime);
    
    // ──── Reset Gate Control ────────────────────────────────────────────────
    
    /// Process reset gate (external input + button) and return true if reset should trigger
    bool processResetGate(float resetInputV, float resetButtonVal);
    
    /// Clear reset armed flag (call after handling reset)
    void clearReset();
    
    /// Check if reset is armed
    bool isResetArmed() const { return resetArmed; }
    
    // ──── Gate Assignment Processing ────────────────────────────────────────
    
    /// Gate1 actions that can be triggered
    enum class Gate1Action {
        None,
        ToggleRhythm,
        ReseedRhythm,
        ReseedMelody,
        Restart
    };
    
    /// Gate2 actions that can be triggered
    enum class Gate2Action {
        None,
        ToggleMelody,
        ReseedMelody,
        SetMute,
        Restart
    };
    
    /// Result of Gate1 processing
    struct Gate1Result {
        Gate1Action action = Gate1Action::None;
    };
    
    /// Result of Gate2 processing
    struct Gate2Result {
        Gate2Action action = Gate2Action::None;
        bool shouldMute = false;
    };
    
    /// Handle Gate1 rising edge based on gate1Assign mode (returns action to take)
    Gate1Result handleGate1Assignment(int gate1Assign, bool gate1Rise);
    
    /// Handle Gate2 state based on gate2Assign mode (returns action to take)
    Gate2Result handleGate2Assignment(int gate2Assign, bool gate2Rise, bool invertMuteLogic);
    
    // ──── Edge Detection ────────────────────────────────────────────────────
    
    /// Process gate inputs and return rise/fall edges
    struct GateEdges {
        bool gate1Rise;
        bool gate2Rise;
    };
    
    GateEdges processGateEdges(float gate1V, float gate2V);
    
    /// Get gate2 high state (useful for gate2Assign mode 2)
    bool getGate2High() const { return lastGate2High; }

private:
    rack::engine::Module* mainModule;
    
    // Gate state
    bool resetArmed = false;
    bool lastGate1High = false;
    bool lastGate2High = false;
    
    // Trigger generators
    rack::dsp::SchmittTrigger runGateTrig;
    rack::dsp::BooleanTrigger runGateBtn;
    rack::dsp::SchmittTrigger resetGateTrig;
    rack::dsp::BooleanTrigger resetGateBtn;
    rack::dsp::SchmittTrigger gate1EdgeTrig;
    rack::dsp::SchmittTrigger gate2EdgeTrig;
    
    // Reset pulse
    rack::dsp::PulseGenerator resetPulse;
    
    // Helper: Process gate with threshold detection
    bool gateHigh_(float v, float threshold = 1.f);
};
