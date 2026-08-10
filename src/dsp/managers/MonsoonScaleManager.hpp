#pragma once
#include <rack.hpp>
#include <vector>
#include <string>
#include <cstdint>

struct ScaleType {
    std::string name;
    std::vector<int> intervals; 
};

extern const std::vector<ScaleType> MONSOON_SCALES;

struct Monsoon;
class ParameterManager;

/**
 * ScaleManager
 * 
 * Encapsulates all scale-related state and logic.
 */
class ScaleManager {
public:
    int scaleRoot = 0;
    int lastSelectedScale = -1;
    bool lockScaleNotes = false;
    uint16_t activeScaleMask = 0xFFF;

    // ── MONSOON_SCALE_AUTHORING: two explicit-mask authorities above the factory (scale,root) path.
    // AUTHORED = the Monsoon's own hand-authored enable-band mask (its BASE scale). OVERRIDE = pushed
    // by the regular Shophouse when a slot is active (boundary-quantised), reverted on detach. Priority
    // override > authored > factory > all-12 lives in dotModular::resolveScaleMask (ScaleMaskArbiter).
    // Both default INVALID so an untouched Monsoon is byte-identical (factory/all-12 path unchanged).
    uint16_t authoredMask   = 0x0FFF;
    bool     authoredValid  = false;
    uint16_t overrideMask   = 0x0FFF;
    bool     overrideValid  = false;

    ScaleManager(Monsoon* module) : module(module) {}

    /// Recalculates the mask and applies fader locks/redistribution if enabled
    void updateScaleMask();

    // ── Explicit-mask control (MONSOON_SCALE_AUTHORING) ────────────────────────────────────────────
    /// Set/clear the Monsoon's own authored base mask (from its enable band). 12-bit, &0xFFF.
    void setAuthoredMask(uint16_t mask) { authoredMask = mask & 0x0FFF; authoredValid = true;  updateScaleMask(); }
    void clearAuthoredMask()            { authoredValid = false;                               updateScaleMask(); }
    /// Set/clear the Shophouse override mask (boundary-quantised push; cleared on detach/no-active).
    void setOverrideMask(uint16_t mask) { overrideMask = mask & 0x0FFF; overrideValid = true;  updateScaleMask(); }
    void clearOverrideMask()            { overrideValid = false;                               updateScaleMask(); }

    // ── Per-degree band authoring (Monsoon enable band, MSA Phase B) ───────────────────────────────
    // D1: the first band edit SEEDS the authored mask from whatever is currently active (factory scale
    // or all-12), so "pick a factory scale then tweak" is one continuous mask, not two schemes.
    void beginAuthoringFromActive() {
        if (!authoredValid) { authoredMask = activeScaleMask & 0x0FFF; authoredValid = true; }
    }
    // Set one pitch-class (0..11) on/off. Guard: never allow the mask to reach all-off (a silent scale);
    // the last remaining enabled degree can't be disabled. Absolute pitch-classes (D2), no root offset.
    void setAuthoredDegree(int d, bool on) {
        if (d < 0 || d > 11) return;
        beginAuthoringFromActive();
        uint16_t next = on ? (uint16_t)(authoredMask | (1u << d))
                           : (uint16_t)(authoredMask & ~(1u << d));
        next &= 0x0FFF;
        if (next == 0) return;                 // refuse an all-off (silent) scale
        authoredMask = next;
        updateScaleMask();
    }
    void toggleAuthoredDegree(int d) {
        if (d < 0 || d > 11) return;
        beginAuthoringFromActive();
        setAuthoredDegree(d, !(authoredMask & (1u << d)));
    }
    bool authoredDegreeOn(int d) const { return d >= 0 && d <= 11 && (authoredMask & (1u << d)); }

    /// Returns the effective semitone weight, respecting the scale mask if locked
    float getSemitoneWeight(int semIdx, const ParameterManager& pm) const;

    /// Resets scale state to defaults
    void reset() {
        scaleRoot = 0;
        lastSelectedScale = -1;
        lockScaleNotes = false;
        authoredValid = false;
        overrideValid = false;
        updateScaleMask();
    }

    /// Calculate a bitmask where bits 0-11 represent semitones C-B in the given scale
    static uint16_t calculateMask(int root, int scaleIdx);

    /// Redistribute probability weights from out-of-scale notes to the nearest in-scale neighbors
    static void redistributeWeights(uint16_t mask, float* weights);

private:
    Monsoon* module;
};