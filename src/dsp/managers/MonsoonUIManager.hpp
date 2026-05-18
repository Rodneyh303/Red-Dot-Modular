#pragma once

#include "rack.hpp"

/**
 * UIManager
 * 
 * Encapsulates all LED/light management and visual state handling.
 * 
 * Centralizes:
 *   1. Status light updates (dice, lock, mute, run, reset)
 *   2. Expander indicator lights (green for valid, red for multiple)
 *   3. Mode selector lights (A, B, C, D)
 *   4. Step ring indicator lights
 *   5. Semitone LED brightness (red channel for note playing flash)
 *   6. Light update throttling via divider
 *   7. Button trigger processing (dice, lock, mute, mode)
 * 
 * Design: UIManager reads module state and updates lights.
 * It is NOT responsible for what logic triggers state changes (that's in process()).
 * It IS responsible for translating module state into visual feedback.
 */
class UIManager {
public:
    UIManager(rack::engine::Module* mainModule,
              rack::dsp::ClockDivider& lightDivider)
        : mainModule(mainModule),
          lightDivider(lightDivider) {}
    
    // ──── Status Light Updates ──────────────────────────────────────────────
    
    /// Update dice lights (rhythm and melody)
    void updateDiceLights(bool rhythmSeedPending, bool melodySeedPending);
    
    /// Update lock indicator light
    void updateLockLight(bool locked);
    
    /// Update mute indicator light
    void updateMuteLight(bool muted);
    
    /// Update run gate light
    void updateRunGateLight(bool runGateActive);
    
    /// Update reset light with smoothing
    void updateResetLight(bool resetArmed, float sampleTime);
    
    // ──── Expander Indicator Lights ─────────────────────────────────────────
    
    /// Update expander indicator lights (green=1 connected, red=multiple)
    void updateExpanderLights(int scaleCount, int dnaCount, int polyCount);
    
    // ──── Mode Selector Lights ──────────────────────────────────────────────
    
    /// Update mode selection lights (only one mode light on at a time)
    void updateModeLights(int currentMode, int& lastMode);
    
    // ──── Step Ring Lights ──────────────────────────────────────────────────
    
    /// Update 16-step ring indicator lights based on step brightness values
    void updateStepLights(const float* stepBrightness, int count);
    
    // ──── Semitone LED Brightness ───────────────────────────────────────────
    
    /// Update red channel of semitone LEDs (for note-playing flash feedback)
    void updateSemitoneFlashLights(const float* semiLedBrightness, int count);
    
    // ──── Button Trigger Processing ─────────────────────────────────────────
    
    /// Process dice button triggers and return if any triggered
    bool processDiceButtons(bool& rhythmTriggered, bool& melodyTriggered);
    
    /// Process lock button trigger
    bool processLockButton();
    
    /// Process mute button trigger
    bool processMuteButton();
    
    /// Process mode selector button trigger (cycles A -> B -> C -> D -> A)
    bool processModeButton(int& modeSelect);
    
    // ──── Batch Updates ────────────────────────────────────────────────────
    
    /// Update all lights at once (called once per lightDivider process)
    void updateAllLights(bool rhythmSeedPending,
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
                         float sampleTime);

private:
    rack::engine::Module* mainModule;
    rack::dsp::ClockDivider& lightDivider;
    
    // Button trigger state
    rack::dsp::SchmittTrigger diceRTrigger;
    rack::dsp::SchmittTrigger diceMTrigger;
    rack::dsp::SchmittTrigger lockTrigger;
    rack::dsp::SchmittTrigger muteTrigger;
    rack::dsp::SchmittTrigger modeTrigger;
    
    // Helper: Set expander light with dual-channel (green/red)
    void setExpanderLight_(int lightId, int count);
};
