#pragma once
#include <rack.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include "../ScaleMaskArbiter.hpp"   // rotateMask12 / normaliseToTonic (TONIC_TRANSPOSE)

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

    // ── TONIC_TRANSPOSE_BUILD_BRIEF: authored scales are TRANSPOSABLE by the live root ──────────────
    // authoredRelative=false → authoredMask is ABSOLUTE pitch-classes (hand-authoring, WYSIWYG: click D
    // → D sounds). authoredRelative=true → authoredMask is ROOT-RELATIVE (tonic at bit 0); the LIVE
    // scaleRoot rotates it, exactly like a built-in scale (C Major vs F Major = one pattern, two roots).
    // The tonic's absolute position is therefore scaleRoot itself — no separate tonic index needed.
    // "Set as tonic p" converts absolute→relative pinning the same notes (see setTonicAbsolute); "unset"
    // bakes back to absolute. Loading a transposable .dmtune sets relative; a non-transposable one stays
    // absolute. OVERRIDE (Shophouse) is always pushed pre-rotated (absolute) — not re-rotated here.
    bool     authoredRelative = false;
    // TONIC_TRANSPOSE / naming: user label for the authored scale, written to a saved .dmtune's "name"
    // and set from a loaded file's name. Empty → Save falls back to a default label.
    std::string authoredName;

    ScaleManager(Monsoon* module) : module(module) {}

    /// Recalculates the mask and applies fader locks/redistribution if enabled
    void updateScaleMask();

    // ── Explicit-mask control (MONSOON_SCALE_AUTHORING) ────────────────────────────────────────────
    /// Set/clear the Monsoon's own authored base mask (from its enable band). 12-bit, &0xFFF.
    /// `relative` marks it as tonic-relative (transposed by the live root); default absolute (WYSIWYG).
    void setAuthoredMask(uint16_t mask, bool relative = false) {
        authoredMask = mask & 0x0FFF; authoredValid = true; authoredRelative = relative; updateScaleMask();
    }
    void clearAuthoredMask()            { authoredValid = false; authoredRelative = false;    updateScaleMask(); }

    // The authored mask AS APPLIED: absolute → verbatim; relative → rotated up by the live scaleRoot,
    // exactly like a built-in scale. This is what the arbiter/engine reads for the authored authority.
    uint16_t effectiveAuthoredMask() const {
        return authoredRelative ? dotModular::rotateMask12(authoredMask, scaleRoot)
                                : (uint16_t)(authoredMask & 0x0FFF);
    }

    // ── TONIC designation (TONIC_TRANSPOSE STEP 1/2) ───────────────────────────────────────────────
    // Designate absolute pitch-class `p` as the tonic: the current sounding scale is pinned, then made
    // root-relative with the live root moved to p, so the SAME notes keep sounding but now transpose
    // when the root changes. Only a currently-enabled degree can be the tonic.
    void setTonicAbsolute(int p) {
        if (p < 0 || p > 11) return;
        beginAuthoringFromActive();
        const uint16_t abs = effectiveAuthoredMask();     // current sounding pitch-classes
        if (!(abs & (1u << p))) return;                    // tonic must be an enabled degree
        authoredMask     = dotModular::normaliseToTonic(abs, p);  // store relative (tonic → bit 0)
        authoredRelative = true;
        scaleRoot        = p;                              // live root now sits on the tonic
        updateScaleMask();
    }
    // Remove the tonic: bake the currently-sounding (rotated) mask back to ABSOLUTE, so notes are
    // unchanged but the scale no longer transposes with the root.
    void unsetTonic() {
        if (!authoredValid || !authoredRelative) return;
        authoredMask     = effectiveAuthoredMask();        // freeze at the current root
        authoredRelative = false;
        updateScaleMask();
    }
    bool hasTonic() const { return authoredValid && authoredRelative; }
    // The tonic's ABSOLUTE pitch-class = the live root (relative masks pin the tonic to bit 0). -1 none.
    int  tonicPitchClass() const { return hasTonic() ? ((scaleRoot % 12) + 12) % 12 : -1; }
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