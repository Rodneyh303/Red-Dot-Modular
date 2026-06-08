#include "MonsoonTimingController.hpp"
#include "../../Monsoon.hpp"

using namespace rack;

// ──── Helper: Gate High Detection ───────────────────────────────────────────
// All about gates  - does it belong with Gatestate or ModeController instead? It's mostly used for timing-related gate processing, so TimingController seems reasonable for now. Can refactor later if needed.
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

void TimingController::handleGate1Assignment(
    int gate1Assign,
    bool gate1Rise) {
    
    if (!gate1Rise || !mainModule) return;
    
    switch (gate1Assign) {
        case 0: // Toggle Dice R MODE
            mainModule->rhythmMode = 1 - mainModule->rhythmMode;
            break;
            
        case 1: // Re-dice R (arm) — only if in dice-mode
            mainModule->diceRhythm();
            break;
            
        case 2: // Re-dice M (arm) — only if in dice-mode
            mainModule->diceMelody();
            break;
            
        case 3: // Restart now
            mainModule->handleRestart(true, true);
            break;
    }
}

// ──── Gate2 Assignment Handling ─────────────────────────────────────────────

void TimingController::handleGate2Assignment(
    int gate2Assign,
    bool gate2Rise,
    bool gate2High,
    bool invertMuteLogic) {

    if (!mainModule) return;
    
    switch (gate2Assign) {
        case 0: // Toggle Dice M — on rising edge
            if (gate2Rise) {
                mainModule->melodyMode = 1 - mainModule->melodyMode;
            }
            break;
            
        case 1: // Re-dice M — rising edge, only in dice-mode
            if (gate2Rise) {
                mainModule->diceMelody();
            }
            break;
            
        case 2: // MUTE — level-based
            {
                bool shouldMute = invertMuteLogic ? !gate2High : gate2High;
                if (shouldMute != mainModule->muted) {
                    mainModule->muted = shouldMute;
                    if (!mainModule->muted && mainModule->restartOnUnmute) {
                        mainModule->handleRestart(true, true);
                    }
                }
            }
            break;
            
        case 3: // RESTART — rising edge
            if (gate2Rise) {
                mainModule->handleRestart(true, true);
            }
            break;
    }
}
