#include "MonsoonUIManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonInterchangeExpander.hpp"
#include "../../MonsoonStraitsExpander.hpp"

using namespace rack;
using namespace MonsoonIds;

// ──── Status Light Updates ──────────────────────────────────────────────────

void UIManager::updateDiceLights(bool rhythmSeedPending, bool melodySeedPending, bool qmixSeedPending) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;
    
    lights[RHYTHM_DICE_LIGHT].setBrightness(rhythmSeedPending ? 1.f : 0.1f);
    lights[MELODY_DICE_LIGHT].setBrightness(melodySeedPending ? 1.f : 0.1f);
    lights[QMIX_DICE_LIGHT].setBrightness(qmixSeedPending ? 1.f : 0.1f);
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
        // One light per mode. Loop rather than a line each, so a future mode add cannot leave a
        // light behind. MODE_COLLAPSE_6_TO_3: three timing origins (clock/gate/phase). The panel
        // still wires 6 light positions (Phase 3 reduces to 3); 3..5 stay off.
        for (int i = 0; i < 6; ++i)
            lights[MODE_A_LIGHT + i].setBrightness((i < 3 && currentMode == i) ? 1.f : 0.f);
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

void UIManager::updateSemitoneFlashLights(const float* seqBrightness, const float* quantBrightness, int count) {
    if (!mainModule) return;
    auto& lights = mainModule->lights;
    using namespace MonsoonIds;

    // FADER_SEQ_QUANT_COLOURS: 4ch WhiteRgbLight per fader. ch0 (white weight) is driven by the
    // slider widget from the param value; here we drive the 3 flash channels (ch1 red / ch2 green /
    // ch3 blue). Per degree a note is EITHER seq OR quant per voice, so "both" (blue) arises only
    // across different poly voices on the same fader in one frame. With no quant source there are
    // no quant hits => ch2/ch3 stay dark => red-only (exactly as pre-collapse).
    for (int i = 0; i < 12 && i < count; ++i) {
        const float s = seqBrightness[i];
        const float q = quantBrightness[i];
        const bool  both = (s > 0.f && q > 0.f);
        lights[SEMI_LED_START + 4*i + 1].setBrightness(both ? 0.f : s);             // red   (seq)
        lights[SEMI_LED_START + 4*i + 2].setBrightness(both ? 0.f : q);             // green (quant)
        lights[SEMI_LED_START + 4*i + 3].setBrightness(both ? std::max(s, q) : 0.f); // blue  (both)
    }
}

// ──── Button Trigger Processing ─────────────────────────────────────────────

bool UIManager::processDiceButtons(bool& rhythmTriggered, bool& melodyTriggered, bool& qmixTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;
    
    rhythmTriggered = diceRTrigger.process(params[DICE_R_PARAM].getValue());
    melodyTriggered = diceMTrigger.process(params[DICE_M_PARAM].getValue());
    qmixTriggered   = diceQTrigger.process(params[DICE_Q_PARAM].getValue());  // Task 4 (QMIX)
    
    return rhythmTriggered || melodyTriggered || qmixTriggered;
}

bool UIManager::processLastDiceButtons(bool& rhythmTriggered, bool& melodyTriggered, bool& qmixTriggered) {
    if (!mainModule) return false;
    auto& params = mainModule->params;
    using namespace MonsoonIds;

    rhythmTriggered = lastDiceRTrigger.process(params[LAST_DICE_R_PARAM].getValue());
    melodyTriggered = lastDiceMTrigger.process(params[LAST_DICE_M_PARAM].getValue());
    qmixTriggered   = lastDiceQTrigger.process(params[LAST_DICE_Q_PARAM].getValue());  // Task 4 (QMIX)

    return rhythmTriggered || melodyTriggered || qmixTriggered;
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
        modeSelect = (modeSelect + 1) % 3;   // MODE_COLLAPSE_6_TO_3: clock→gate→phase→clock
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
    
    // Update semitone flash feedback (updateAllLights is currently unused; pass a zero quant array
    // so this dead path compiles against the 3-arg FADER_SEQ_QUANT_COLOURS signature).
    static const float zeroQuant[12] = {};
    updateSemitoneFlashLights(semiLedBrightness, zeroQuant, semiCount);
}
