#include "MonsoonUIManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonInterchangeExpander.hpp"
#include "../../MonsoonStraitsExpander.hpp"

using namespace rack;
using namespace MonsoonIds;

// ──── Status Light Updates ──────────────────────────────────────────────────

void UIManager::updateDiceLights(bool rhythmSeedPending, bool melodySeedPending) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[RHYTHM_DICE_LIGHT].setBrightness(rhythmSeedPending ? 1.f : 0.1f);
    lights[MELODY_DICE_LIGHT].setBrightness(melodySeedPending ? 1.f : 0.1f);
}

void UIManager::updateLockLight(bool locked) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[LOCK_LIGHT].setBrightness(locked ? 1.f : 0.f);
}

void UIManager::updateMuteLight(bool muted) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[MUTE_LIGHT].setBrightness(muted ? 1.f : 0.f);
}

void UIManager::updateRunGateLight(bool runGateActive) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[RUN_GATE_LIGHT].setBrightness(runGateActive ? 1.f : 0.f);
}

void UIManager::updateResetLight(bool resetArmed, float sampleTime) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[RESET_LIGHT].setBrightnessSmooth(resetArmed ? 1.f : 0.f, sampleTime);
}

// ──── Expander Indicator Lights ─────────────────────────────────────────────

void UIManager::setExpanderLight_(int lightId, int count) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    
    // Green (ch0): on if exactly 1 connected
    lights[lightId + 0].setBrightness(count == 1 ? 1.f : 0.f);
    // Red (ch1): on if multiple connected (warning)
    lights[lightId + 1].setBrightness(count > 1 ? 1.f : 0.f);
}

void UIManager::updateExpanderLights(int scaleCount, int dnaCount, int polyCount) {
    if (!mainModule) return;
    using namespace MonsoonIds;
    
    setExpanderLight_(SCALE_EXPANDER_LIGHT, scaleCount);
    setExpanderLight_(DNA_EXPANDER_LIGHT, dnaCount);
    setExpanderLight_(POLY_EXPANDER_LIGHT, polyCount);
}

// ──── Mode Selector Lights ──────────────────────────────────────────────────

void UIManager::updateModeLights(int currentMode, int& lastMode) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    // Only update if mode changed to avoid redundant updates
    if (currentMode != lastMode) {
        // One light per mode. Loop rather than a line each, so adding a mode cannot leave a
        // light behind again -- which is exactly how Mode E ended up selectable but unlit.
        for (int i = 0; i < 5; ++i)
            lights[MODE_A_LIGHT + i].setBrightness(currentMode == i ? 1.f : 0.f);
        lastMode = currentMode;
    }
}

// ──── Step Ring Lights ──────────────────────────────────────────────────────

void UIManager::updateStepLights(const float* stepBrightness, int count) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    // Update 16 step ring lights
    for (int i = 0; i < 16 && i < count; ++i) {
        lights[STEP_LIGHTS_START + i].setBrightness(stepBrightness[i]);
    }
}

// ──── Semitone LED Brightness ───────────────────────────────────────────────

void UIManager::updateSemitoneFlashLights(const float* semiLedBrightness, int count) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    // Update red channel (ch1) for each semitone
    // Green channel (ch0) is handled by the VCVLightSlider widget automatically
    for (int i = 0; i < 12 && i < count; ++i) {
        lights[SEMI_LED_START + 2*i + 1].setBrightness(semiLedBrightness[i]);
    }
}

// ──── Button Trigger Processing ─────────────────────────────────────────────

bool UIManager::processDiceButtons(bool& rhythmTriggered, bool& melodyTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;
    
    rhythmTriggered = diceRTrigger.process(params[DICE_R_PARAM].getValue());
    melodyTriggered = diceMTrigger.process(params[DICE_M_PARAM].getValue());
    
    return rhythmTriggered || melodyTriggered;
}

bool UIManager::processTrialButtons(bool& rhythmTriggered, bool& melodyTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;

    rhythmTriggered = trialRTrigger.process(params[DICE_TRIAL_R_PARAM].getValue());
    melodyTriggered = trialMTrigger.process(params[DICE_TRIAL_M_PARAM].getValue());

    return rhythmTriggered || melodyTriggered;
}

bool UIManager::processLastDiceButtons(bool& rhythmTriggered, bool& melodyTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;

    rhythmTriggered = lastDiceRTrigger.process(params[LAST_DICE_R_PARAM].getValue());
    melodyTriggered = lastDiceMTrigger.process(params[LAST_DICE_M_PARAM].getValue());

    return rhythmTriggered || melodyTriggered;
}

bool UIManager::processLastTrialButtons(bool& rhythmTriggered, bool& melodyTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;

    rhythmTriggered = lastTrialRTrigger.process(params[LAST_TRIAL_R_PARAM].getValue());
    melodyTriggered = lastTrialMTrigger.process(params[LAST_TRIAL_M_PARAM].getValue());

    return rhythmTriggered || melodyTriggered;
}

bool UIManager::processLockButton() {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;
    
    return lockTrigger.process(params[LOCK_PARAM].getValue());
}

bool UIManager::processMuteButton() {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;
    
    return muteTrigger.process(params[MUTE_PARAM].getValue());
}

bool UIManager::processModeButton(int& modeSelect) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;
    
    if (modeTrigger.process(params[MODE_PARAM].getValue())) {
        modeSelect = (modeSelect + 1) % 5;
        return true;
    }
    return false;
}

// ──── Batch Updates ────────────────────────────────────────────────────────

void UIManager::updateAllLights(bool rhythmSeedPending,
                                 bool melodySeedPending,
                                 bool locked,
                                 bool muted,
                                 bool runGateActive,
                                 bool resetArmed,
                                 int scaleCount,
                                 int dnaCount,
                                 int polyCount,
                                 int currentMode,
                                 int& lastMode,
                         const float* stepBrightness, int stepCount,
                         const float* semiLedBrightness, int semiCount,
                                 float sampleTime) {
    // Update all status lights
    updateDiceLights(rhythmSeedPending, melodySeedPending);
    updateLockLight(locked);
    updateMuteLight(muted);
    updateRunGateLight(runGateActive);
    updateResetLight(resetArmed, sampleTime);
    
    // Update expander indicators
    updateExpanderLights(scaleCount, dnaCount, polyCount);
    
    // Update mode selector
    updateModeLights(currentMode, lastMode);
    
    // Update step ring
    updateStepLights(stepBrightness, stepCount);
    
    // Update semitone flash feedback
    updateSemitoneFlashLights(semiLedBrightness, semiCount);
}
