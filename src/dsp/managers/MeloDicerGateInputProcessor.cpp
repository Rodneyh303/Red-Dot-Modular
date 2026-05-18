#include "MeloDicerGateInputProcessor.hpp"

using namespace rack;

// ──── Helper: Gate High Detection ───────────────────────────────────────────

bool GateInputProcessor::gateHigh_(float v, float threshold) {
    return v >= threshold;
}

// ──── Run Gate Processing ───────────────────────────────────────────────────

bool GateInputProcessor::processRunGate(bool currentlyActive,
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

float GateInputProcessor::getResetPulseOutput(float sampleTime) {
    return resetPulse.process(sampleTime) ? 10.f : 0.f;
}

// ──── Reset Gate Processing ────────────────────────────────────────────────

bool GateInputProcessor::processResetGate(float resetInputV, float resetButtonVal) {
    if (!mainModule) return false;
    
    bool resetTrigHigh = resetGateTrig.process(resetInputV, 0.1f, 2.f);
    bool resetBtnHigh = resetGateBtn.process(resetButtonVal);
    
    if (resetTrigHigh || resetBtnHigh) {
        resetArmed = true;
        return true;
    }
    return false;
}

void GateInputProcessor::clearReset() {
    resetArmed = false;
}

// ──── Gate Edge Detection ───────────────────────────────────────────────────

GateInputProcessor::GateEdges GateInputProcessor::processGateEdges(float gate1V, float gate2V) {
    bool gate1Now = gateHigh_(gate1V);
    bool gate2Now = gateHigh_(gate2V);
    
    bool gate1Rise = gate1Now && !lastGate1High;
    bool gate2Rise = gate2Now && !lastGate2High;
    
    lastGate1High = gate1Now;
    lastGate2High = gate2Now;
    
    return {gate1Rise, gate2Rise};
}

// ──── Gate1 Assignment Handling ─────────────────────────────────────────────

void GateInputProcessor::handleGate1Assignment(
    int gate1Assign,
    bool gate1Rise,
    const std::function<void(int)>& onRhythmModeToggle,
    const std::function<void()>& onRhythmReseed,
    const std::function<void()>& onMelodyReseed,
    const std::function<void()>& onRestart) {
    
    if (!gate1Rise) return;
    
    switch (gate1Assign) {
        case 0: // Toggle Dice R MODE
            if (onRhythmModeToggle) onRhythmModeToggle(1); // toggle: 0 <-> 1
            break;
            
        case 1: // Re-dice R (arm) — only if in dice-mode
            if (onRhythmReseed) onRhythmReseed();
            break;
            
        case 2: // Re-dice M (arm) — only if in dice-mode
            if (onMelodyReseed) onMelodyReseed();
            break;
            
        case 3: // Restart now
            if (onRestart) onRestart();
            break;
    }
}

// ──── Gate2 Assignment Handling ─────────────────────────────────────────────

void GateInputProcessor::handleGate2Assignment(
    int gate2Assign,
    bool gate2Rise,
    bool gate2High,
    bool invertMuteLogic,
    const std::function<void(int)>& onMelodyModeToggle,
    const std::function<void()>& onMelodyReseed,
    const std::function<void(bool)>& onMuteChange,
    const std::function<void()>& onRestart) {
    
    switch (gate2Assign) {
        case 0: // Toggle Dice M — on rising edge
            if (gate2Rise && onMelodyModeToggle) {
                onMelodyModeToggle(1); // toggle
            }
            break;
            
        case 1: // Re-dice M — rising edge, only in dice-mode
            if (gate2Rise && onMelodyReseed) {
                onMelodyReseed();
            }
            break;
            
        case 2: // MUTE — level-based
            {
                bool shouldMute = invertMuteLogic ? !gate2High : gate2High;
                if (onMuteChange) {
                    onMuteChange(shouldMute);
                }
            }
            break;
            
        case 3: // RESTART — rising edge
            if (gate2Rise && onRestart) {
                onRestart();
            }
            break;
    }
}
