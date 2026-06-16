#pragma once

#include <cmath>
#include "rack.hpp"

struct MonsoonInterchangeExpander;
struct MonsoonStraitsEastExpander;

/**
 * ParameterManager
 * 
 * Encapsulates all parameter reading logic from Monsoon, centralizing:
 *   1. Knob value retrieval
 *   2. CV input application and attenuverter scaling
 *   3. Expander value fetching (semitone, octave, poly rest)
 *   4. CV offset tracking (cv2Offsets, cv1LoOffset, cv1HiOffset)
 *   5. Clamping and range validation
 * 
 * By centralizing parameter logic, we reduce Monsoon class complexity and
 * make parameter behavior consistent and easier to understand.
 */
class ParameterManager {
public:
    ParameterManager(rack::engine::Module* mainModule,
                     MonsoonInterchangeExpander** cachedExpander,
                     MonsoonStraitsEastExpander** cachedPolyVoiceExpander)
        : mainModule(mainModule),
          cachedExpander(cachedExpander),
          cachedPolyVoiceExpander(cachedPolyVoiceExpander) {}
    
    // ──── Core Parameter Getters ────────────────────────────────────────────
    
    /// Note length grid value with CV2 offset applied
    float getNoteValue() const;
    
    /// Variation probability (0–1) with CV2 offset
    float getVariation() const;
    
    /// Legato probability (0–1) with CV2 offset
    float getLegato() const;
    
    /// Rest probability (0–1) with CV2 offset
    float getRest() const;
    
    /// Accent probability (0–1) with direct CV input (0–10V = 0–100%)
    float getAccent() const;

    /// Big-5 effective values NORMALISED to 0..1 (for modulation visualisation).
    /// These reuse the effective-value getters above; NOTE_VALUE is divided by 8
    /// so all five share a 0..1 range the widgets can compare to knob position.
    float getNoteValueNorm() const { return getNoteValue() / 8.f; }
    float getVariationNorm() const { return getVariation(); }
    float getLegatoNorm()    const { return getLegato(); }
    float getRestNorm()      const { return getRest(); }
    float getAccentNorm()    const { return getAccent(); }
    /// True if any big-5 modulation source is currently contributing an offset.
    bool  anyBig5Modulated() const {
        for (int i = 0; i < 5; ++i) if (surgeOffsets[i] != 0.f) return true;
        for (int i = 0; i < 4; ++i) if (cv2Offsets[i]   != 0.f) return true;
        for (int i = 0; i < 5; ++i) if (cv2Offsets[i]   != 0.f) return true;
        return false;
    }
    /// Slew/mix effective values (all native 0..1). True if any CV3 offset active.
    float getRhythmSlewNorm() const { return getRhythmSlew(); }
    float getMelodySlewNorm() const { return getMelodySlew(); }
    float getRhythmMixNorm()  const { return getRhythmMix(); }
    float getMelodyMixNorm()  const { return getMelodyMix(); }
    bool  anyCv3Modulated() const {
        for (int i = 0; i < 4; ++i) if (cv3Offsets[i] != 0.f) return true;
        return false;
    }
    /// Pitch sliders, normalised 0..1 (semitones native 0..1; octaves /8).
    float getSemitoneNorm(int i) const { return getSemitone(i); }
    float getOctaveLoNorm() const { return getOctaveLo() / 8.f; }
    float getOctaveHiNorm() const { return getOctaveHi() / 8.f; }
    /// True if any pitch slider's effective value differs from its raw param
    /// (i.e. expander/CV1 is modulating it). Compared with a small epsilon.
    bool  anyPitchModulated() const;
    
    /// Transpose in semitones (-12 to +12)
    float getTranspose() const;

    /// Playable dice slew amounts (0–1)
    float getRhythmSlew() const;
    float getMelodySlew() const;
    float getRhythmMix() const;
    float getMelodyMix() const;
    
    // ──── Octave Range Getters ──────────────────────────────────────────────
    
    /// Lowest playable octave (0–8) with expander CV and transient CV1 offset
    float getOctaveLo() const;
    
    /// Highest playable octave (0–8) with expander CV and transient CV1 offset
    float getOctaveHi() const;
    
    /// Set transient CV1 Lo offset (from CV1 mode 2, not persisted)
    void setCv1LoOffset(float offset) { cv1LoOffset = offset; }
    
    /// Set transient CV1 Hi offset (from CV1 mode 3, not persisted)
    void setCv1HiOffset(float offset) { cv1HiOffset = offset; }
    
    // ──── Semitone Probability Getters ──────────────────────────────────────
    
    /// Get semitone probability (0–1) with expander CV input applied
    /// voiceIdx: 0 = voice 2, ..., 6 = voice 8 (for poly rest)
    float getSemitone(int semIdx) const;
    
    // ──── Poly Voice Rest Probability ───────────────────────────────────────
    
    /// Get rest probability for poly voice (voiceIdx 0–6 maps to voices 2–8)
    /// Falls back to 0.1 if no poly voice expander
    float getPolyRest(int voiceIdx) const;
    
    // ──── CV2 Offset Management ──────────────────────────────────────────────
    
    /// Set CV2 offset for a parameter (0 = note value, 1 = variation, 2 = legato, 3 = rest, 4 = accent)
    void setCv2Offset(int paramIdx, float offset) { // Now supports 5 parameters (0-4)
        if (paramIdx >= 0 && paramIdx < 5) {
            cv2Offsets[paramIdx] = offset;
        }
    }
    
    /// Get all CV2 offsets (for debugging/persistence) - now 5 elements
    const float* getCv2Offsets() const { return cv2Offsets; }
    
    /// Clear all CV2 offsets
    void clearCv2Offsets() {
        for (int i = 0; i < 4; ++i) cv2Offsets[i] = 0.f;
        for (int i = 0; i < 5; ++i) cv2Offsets[i] = 0.f;
    }

    // ──── CV3 Offset Management ──────────────────────────────────────────────

    /// Set CV3 offset (0=R Slew, 1=M Slew, 2=R Mix, 3=M Mix)
    void setCv3Offset(int paramIdx, float offset) {
        if (paramIdx >= 0 && paramIdx < 4) {
            cv3Offsets[paramIdx] = offset;
        }
    }

    /// Clear all CV3 offsets
    void clearCv3Offsets() {
        for (int i = 0; i < 4; ++i) cv3Offsets[i] = 0.f;
    }

    // ──── Surge Expander Offset Management ───────────────────────────────────
    // Big-5 CV (x attenuverter) offsets from the Surge expander, summed into the
    // matching getters. Index: 0 note value, 1 variation, 2 legato, 3 rest, 4 accent.
    void setSurgeOffset(int i, float offset) {
        if (i >= 0 && i < 5) surgeOffsets[i] = offset;
    }
    void clearSurgeOffsets() {
        for (int i = 0; i < 5; ++i) surgeOffsets[i] = 0.f;
    }

    // CV1 BPM Offset
    float cv1BpmOffset = 0.f;
    void clearCv1BpmOffset() { cv1BpmOffset = 0.f; }

private:
    rack::engine::Module* mainModule;
    MonsoonInterchangeExpander** cachedExpander;
    MonsoonStraitsEastExpander** cachedPolyVoiceExpander;
    
    // CV2-aware offsets for note value, variation, legato, rest, accent
    float cv2Offsets[5] = {0.f, 0.f, 0.f, 0.f, 0.f}; // Increased size for Accent
    // CV3 offsets for rhythm slew, melody slew, rhythm mix, melody mix
    float cv3Offsets[4] = {0.f, 0.f, 0.f, 0.f};
    // Surge expander offsets: note value, variation, legato, rest, accent
    float surgeOffsets[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
    
    // Transient CV1 mode offsets (never persisted)
    float cv1LoOffset = 0.f;
    float cv1HiOffset = 0.f;
    
    // Helper: read parameter safely with bounds
    float readParam_(int paramId, float minVal, float maxVal) const;
    
    // Helper: read input voltage safely
    float readInput_(int inputId) const;
};
