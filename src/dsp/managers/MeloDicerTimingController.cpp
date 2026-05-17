#include "MeloDicerTimingController.hpp"

using namespace rack;

// ──── Helper: Gate High Detection ───────────────────────────────────────────

bool TimingController::gateHigh_(float v, float threshold) {
    return v >= threshold;
}

// ──── Run Gate Processing ───────────────────────────────────────────────────

bool TimingController::processRunGate(bool currentlyActive,
                                       float runGateInputV,
                                       float runGateButtonVal) {
    if (!mainModule) return currentlyActive;
    
    bool runGateTrigHigh = runGateTrig.process(runGateInputV, 0.1f, 2.f);
    bool runGateBtnHigh = runGateBtn.process(runGateButtonVal);
    
    bool toggle = runGateTrigHigh || runGateBtnHigh;
    if (toggle) {
        resetPulse.trigger(1e-3f);
        return !currentlyActive;
    }
    return currentlyActive;
}

float TimingController::getResetPulseOutput(float sampleTime) {
    return resetPulse.process(sampleTime) ? 10.f : 0.f;
}

// ──── Reset Gate Processing ────────────────────────────────────────────────

bool TimingController::processResetGate(float resetInputV, float resetButtonVal) {
    if (!mainModule) return false;
    
    bool resetTrigHigh = resetGateTrig.process(resetInputV, 0.1f, 2.f);
    bool resetBtnHigh = resetGateBtn.process(resetButtonVal);
    
    if (resetTrigHigh || resetBtnHigh) {
        resetArmed = true;
        return true;
    }
    return false;
}

void TimingController::clearReset() {
    resetArmed = false;
}

// ──── Gate Edge Detection ───────────────────────────────────────────────────

TimingController::GateEdges TimingController::processGateEdges(float gate1V, float gate2V) {
    bool gate1Now = gateHigh_(gate1V);
    bool gate2Now = gateHigh_(gate2V);
    
    bool gate1Rise = gate1Now && !lastGate1High;
    bool gate2Rise = gate2Now && !lastGate2High;
    
    lastGate1High = gate1Now;
    lastGate2High = gate2Now;
    
    return {gate1Rise, gate2Rise};
}

// ──── Gate1 Assignment Handling ─────────────────────────────────────────────

TimingController::Gate1Result TimingController::handleGate1Assignment(
    int gate1Assign,
    bool gate1Rise) {
    
    Gate1Result result;
    
    if (!gate1Rise) return result;
    
    switch (gate1Assign) {
        case 0: // Toggle Dice R MODE
            result.action = Gate1Action::ToggleRhythm;
            break;
            
        case 1: // Re-dice R (arm)
            result.action = Gate1Action::ReseedRhythm;
            break;
            
        case 2: // Re-dice M (arm)
            result.action = Gate1Action::ReseedMelody;
            break;
            
        case 3: // Restart now
            result.action = Gate1Action::Restart;
            break;
    }
    
    return result;
}

// ──── Gate2 Assignment Handling ─────────────────────────────────────────────

TimingController::Gate2Result TimingController::handleGate2Assignment(
    int gate2Assign,
    bool gate2Rise,
    bool invertMuteLogic) {
    
    Gate2Result result;
    
    switch (gate2Assign) {
        case 0: // Toggle Dice M — on rising edge
            if (gate2Rise) {
                result.action = Gate2Action::ToggleMelody;
            }
            break;
            
        case 1: // Re-dice M — rising edge
            if (gate2Rise) {
                result.action = Gate2Action::ReseedMelody;
            }
            break;
            
        case 2: // MUTE — level-based
            {
                bool shouldMute = invertMuteLogic ? !lastGate2High : lastGate2High;
                result.action = Gate2Action::SetMute;
                result.shouldMute = shouldMute;
            }
            break;
            
        case 3: // RESTART — rising edge
            if (gate2Rise) {
                result.action = Gate2Action::Restart;
            }
            break;
    }
    
    return result;
}
