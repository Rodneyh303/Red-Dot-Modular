#include "MonsoonTimingController.hpp"
#include "../../Monsoon.hpp"

using namespace rack;

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
        // Fire the reset pulse on an ACTUAL reset so RESET_TRIGGER_OUTPUT signals reset
        // (expanders like Intertropical read this to sync back to scene 1). Previously the
        // pulse only fired on run-toggle, so RESET_TRIGGER_OUTPUT never pulsed on reset.
        resetPulse.trigger(1e-3f);
        return true;
    }
    return false;
}

void TimingController::clearReset() {
    resetArmed = false;
}

// ──── Gate Edge Detection ───────────────────────────────────────────────────

TimingController::GateEdges TimingController::processGateEdges(float gate1V, float gate2V) {
    // Schmitt triggers give hysteresis (0.1V low / 1.0V high): the rise edge fires on
    // the low→high transition and the internal `.state` holds the hysteresis-filtered
    // level.  This is more robust against noisy/dipping gates than a raw `>= threshold`.
    // `getGate1SchmittHigh()` exposes `.state` for the legato grace timer (IMPL 2b).
    bool gate1Rise = gate1EdgeTrig.process(gate1V, 0.1f, 1.f);
    bool gate2Rise = gate2EdgeTrig.process(gate2V, 0.1f, 1.f);

    lastGate1High = gate1EdgeTrig.state;
    lastGate2High = gate2EdgeTrig.state;

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
