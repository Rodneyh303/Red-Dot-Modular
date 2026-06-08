#pragma once
#include <rack.hpp>
#include <array>
#include <algorithm>
#include <cmath>

namespace redDot {

/**
 * SpreadManager
 * 
 * Refactored spread interpolation engine.
 * 
 * Concept:
 *   Spread interpolates between original poly voice random draw and a target:
 *   - Target option 1: Average of active poly voices (creates cohesion)
 *   - Target option 2: Mono voice draw (creates mono-to-poly blending)
 * 
 * Interpolation formula:
 *   interpolated = original + (target - original) * spread
 *   
 * At spread=0.0:    Uses original poly draw (no interpolation)
 * At spread=0.5:    Halfway between original and target
 * At spread=1.0:    Uses target value completely
 * 
 * Active Voice Detection:
 *   For Straits East (7 voices): Uses voices 2-8 (from SequencerEngine)
 *   For Straits West (8 voices): Uses voices 9-16 (from SequencerEngine)
 *   Average is calculated from ONLY active/requested voices
 *   If polyphony=4, average uses only 4 voices
 *   If polyphony=7, average uses all 7 voices
 * 
 * Implementation:
 *   - Macro poly: Same spread applied to all voices (global)
 *   - Per-voice poly: Different spread per voice (independent)
 *   - Mono: No spread (optional feature)
 * 
 * Usage:
 *   SpreadManager mgr(patternEngine);
 *   mgr.setSequencerEngine(sequencerEngine);  // For active voice count
 *   mgr.setInterpolationTarget(AVERAGE_POLY);  // or MONO_DRAW
 *   mgr.setSpread(voiceIdx, lane, 0.5);
 *   float interpolated = mgr.getInterpolatedValue(voiceIdx, lane, step);
 */

struct SpreadManager {
  enum InterpolationTarget {
    AVERAGE_POLY,    // Average of active poly voices
    MONO_DRAW        // Mono voice draw (for reference)
  };
  
  PatternEngine* patternEngine = nullptr;
  SequencerEngine* sequencerEngine = nullptr;  // For active voice count
  InterpolationTarget target = AVERAGE_POLY;
  int numVoices = 7;  // 7 for East, 8 for West, 1 for Mono
  int startVoiceIdx = 0;  // 0 for East (voices 2-8), 8 for West (voices 9-16)
  
  // Spread values: [voice][lane]
  // For macro: all voices use same spread (but we store separately for flexibility)
  // For per-voice: each voice has own spread
  std::array<std::array<float, 3>, 8> spread = {};  // [8 voices][3 lanes]

  // ── Average-poly display cache ───────────────────────────────────────────
  // calculateAveragePolyValue() was called 48x/UI-frame (3 lanes x 16 steps),
  // each independently looping the active voices — heaviest on East (up to 15
  // voices). The averaged grid only changes when the poly arrays or the active
  // voice count change, so cache the 3x16 grid and rebuild it at most once per
  // frame, and only when a cheap checksum of the inputs differs.
  mutable std::array<std::array<float, 16>, 3> avgCache_ = {};
  mutable float avgChecksum_ = -1.f;
  mutable int   avgValid_ = 0;
  
  SpreadManager(PatternEngine* pe = nullptr, int nVoices = 7, int startVoice = 0)
    : patternEngine(pe), numVoices(nVoices), startVoiceIdx(startVoice) {
    // Initialize spreads to 0 (no interpolation)
    for (int v = 0; v < 8; ++v) {
      for (int l = 0; l < 3; ++l) {
        spread[v][l] = 0.0f;
      }
    }
  }
  
  // ===== CONFIGURATION =====
  
  // Set SequencerEngine reference for active voice detection
  // This allows average calculation to use only currently requested voices
  void setSequencerEngine(SequencerEngine* se) {
    sequencerEngine = se;
  }
  
  void setInterpolationTarget(InterpolationTarget t) {
    target = t;
  }
  
  void setSpread(int voiceIdx, int lane, float value) {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      spread[voiceIdx][lane] = rack::math::clamp(value, -1.0f, 1.0f);
    }
  }
  
  float getSpread(int voiceIdx, int lane) const {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      return spread[voiceIdx][lane];
    } // Default to 0.0f if out of bounds, meaning no spread
    return 0.0f; 
  }
  
  // Set macro spread (same for all voices)
  void setMacroSpread(int lane, float value) {
    float clamped = rack::math::clamp(value, 0.0f, 1.0f);
    for (int v = 0; v < numVoices; ++v) {
      spread[v][lane] = clamped;
    }
  }
  
  float getMacroSpread(int lane) const {
    // Return first voice's spread (assuming all are same)
    return (numVoices > 0) ? spread[0][lane] : 0.0f;
  }
  
  // ===== INTERPOLATION TARGET CALCULATION =====
  
  /**
   * Get the interpolation target value for a given voice/lane/step.
   * 
   * This is the value we interpolate toward based on the selected target mode.
   */
  float getInterpolationTarget(int voiceIdx, int lane, int step) const {
    if (!patternEngine) return 0.5f;
    
    switch (target) {
      case AVERAGE_POLY:
        return calculateAveragePolyValue(lane, step);
      
      case MONO_DRAW:
        return getMonoDrawValue(lane, step);
      
      default:
        return 0.5f;
    }
  }
  
  /**
   * Calculate average of active poly voice draws for a given lane/step.
   * 
   * Uses only the voices actually requested by current polyphony setting.
   * For Straits East: voices from startVoiceIdx to startVoiceIdx+polyphony-1
   * For Straits West: voices from startVoiceIdx to startVoiceIdx+polyphony-1
   * 
   * Example:
   *   Straits East with polyphony=4 (voices 2-5)
   *   Calculates average of polyRhythmRandom[0-3] (voices 2-5)
   *   
   *   Straits West with polyphony=6 (voices 9-14)
   *   Calculates average of polyRhythmRandom[0-5] (voices 9-14)
   */
  float calculateAveragePolyValue(int lane, int step) const {
    if (!patternEngine || step < 0 || step >= 16 || lane < 0 || lane > 2) return 0.5f;
    refreshAverageCache_();
    return avgCache_[lane][step];
  }

  // Rebuild the cached 3x16 average grid in ONE pass over the active voices,
  // but only when a cheap checksum of the inputs (active count + a sampling of
  // the poly arrays) changes. Replaces 48 independent per-cell voice-loops per
  // frame with a single guarded batch.
  void refreshAverageCache_() const {
    if (!patternEngine) return;
    int activeVoiceCount = std::min(getActiveVoiceCount(), numVoices);
    if (activeVoiceCount <= 0) {
      if (avgValid_ && avgChecksum_ == 0.f) return;
      for (int l = 0; l < 3; ++l) for (int s = 0; s < 16; ++s) avgCache_[l][s] = 0.5f;
      avgChecksum_ = 0.f; avgValid_ = 1; return;
    }
    // Cheap checksum: active count + a few sampled cells. Catches re-rolls and
    // voice-count changes without summing the whole grid every frame.
    float cs = activeVoiceCount * 1000.f;
    cs += patternEngine->polyRhythmRandom[0][0]
        + patternEngine->polyMelodyRandom[activeVoiceCount-1][7]
        + patternEngine->polyOctaveRandom[0][15];
    if (avgValid_ && cs == avgChecksum_) return;   // unchanged → keep cache

    const float inv = 1.f / activeVoiceCount;
    for (int s = 0; s < 16; ++s) {
      float r = 0.f, m = 0.f, o = 0.f;
      for (int v = 0; v < activeVoiceCount; ++v) {
        r += patternEngine->polyRhythmRandom[v][s];
        m += patternEngine->polyMelodyRandom[v][s];
        o += patternEngine->polyOctaveRandom[v][s];
      }
      avgCache_[0][s] = r * inv;
      avgCache_[1][s] = m * inv;
      avgCache_[2][s] = o * inv;
    }
    avgChecksum_ = cs; avgValid_ = 1;
  }

  
  /**
   * Get the number of active voices from SequencerEngine.
   * 
   * This reflects the actual polyphony setting currently in use.
   * Only voices within this count are averaged for AVERAGE_POLY target.
   */
  int getActiveVoiceCount() const {
    if (!sequencerEngine) return numVoices;
    
    // Access polyphony from SequencerEngine
    // The SequencerEngine.polyphony field tracks how many voices are active
    return std::min(sequencerEngine->numPolyVoices, numVoices);
  }
  
  /**
   * Get mono voice draw value for reference interpolation.
   * 
   * This lets poly voices pull toward mono voice behavior.
   */
  float getMonoDrawValue(int lane, int step) const {
    if (!patternEngine || step < 0 || step >= 16) return 0.5f;
    
    switch (lane) {
      case 0:  // REST
        return patternEngine->rhythmRandom[step];
      case 1:  // MELODY
        return patternEngine->melodyRandom[step];
      case 2:  // OCTAVE
        return patternEngine->octaveRandom[step];
      default:
        return 0.5f;
    }
  }
  
  // ===== INTERPOLATION CALCULATION =====
  
  /**
   * Get the original (uninterpolated) value for a voice/lane/step.
   */
  float getOriginalValue(int voiceIdx, int lane, int step) const {
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= numVoices) return 0.5f;
    if (lane < 0 || lane > 2 || step < 0 || step >= 16) return 0.5f;
    
    switch (lane) {
      case 0:  // REST
        return patternEngine->polyRhythmRandom[voiceIdx][step];
      case 1:  // MELODY
        return patternEngine->polyMelodyRandom[voiceIdx][step];
      case 2:  // OCTAVE
        return patternEngine->polyOctaveRandom[voiceIdx][step];
      default:
        return 0.5f;
    }
  }
  
  /**
   * Apply interpolation formula between original and target.
   * 
   * Formula:
   *   interpolated = original + (target - original) * spread
   * 
   * At spread=0.0:  interpolated = original
   * At spread=1.0:  interpolated = target
   * At spread=-1.0: interpolated = (1 - target)
   */
  float interpolate(float original, float targetValue, float spreadAmount) const {
    float result;
    if (spreadAmount == 0.0f) result = original;
    if (spreadAmount > 0.0f) {
      result = original + (targetValue - original) * spreadAmount;
    } else { // spreadAmount < 0.0f
      // Interpolate towards (1 - targetValue)
      result = original + ((1.0f - targetValue) - original) * std::abs(spreadAmount);
    }
    return rack::math::clamp(result, 0.0f, 1.0f);
  }
  
  /**
   * Get the final interpolated value for display and DSP use.
   * 
   * This is what the sequencer will actually use.
   */
  float getInterpolatedValue(int voiceIdx, int lane, int step) const {
    if (!patternEngine) return 0.5f;
    
    float original = getOriginalValue(voiceIdx, lane, step);
    float targetValue = getInterpolationTarget(voiceIdx, lane, step);
    float spreadAmount = getSpread(voiceIdx, lane);
    
    return interpolate(original, targetValue, spreadAmount);
  }
  
  // ===== BATCH OPERATIONS =====
  
  /**
   * Get all interpolated values for a voice/lane (all 16 steps).
   */
  std::array<float, 16> getInterpolatedLane(int voiceIdx, int lane) const {
    std::array<float, 16> result = {};
    for (int step = 0; step < 16; ++step) {
      result[step] = getInterpolatedValue(voiceIdx, lane, step);
    }
    return result;
  }
  
  /**
   * Get all interpolated values for a voice (all 3 lanes, all 16 steps).
   */
  struct InterpolatedVoice {
    std::array<std::array<float, 16>, 3> lanes;  // [lane][step]
  };
  
  InterpolatedVoice getInterpolatedVoice(int voiceIdx) const {
    InterpolatedVoice result = {};
    for (int lane = 0; lane < 3; ++lane) {
      for (int step = 0; step < 16; ++step) {
        result.lanes[lane][step] = getInterpolatedValue(voiceIdx, lane, step);
      }
    }
    return result;
  }
  
  // ===== STATISTICS =====
  
  /**
   * Calculate interpolation statistics for a lane.
   * Useful for debugging and UI display.
   */
  struct InterpolationStats {
    float minOriginal = 1.0f;
    float maxOriginal = 0.0f;
    float avgOriginal = 0.5f;
    
    float minTarget = 1.0f;
    float maxTarget = 0.0f;
    float avgTarget = 0.5f;
    
    float minInterpolated = 1.0f;
    float maxInterpolated = 0.0f;
    float avgInterpolated = 0.5f;
  };
  
  InterpolationStats getInterpolationStats(int voiceIdx, int lane) const {
    InterpolationStats stats;
    
    float originalSum = 0.0f;
    float targetSum = 0.0f;
    float interpolatedSum = 0.0f;
    
    for (int step = 0; step < 16; ++step) {
      float original = getOriginalValue(voiceIdx, lane, step);
      float targetVal = getInterpolationTarget(voiceIdx, lane, step);
      float interpolated = getInterpolatedValue(voiceIdx, lane, step);
      
      // Original stats
      stats.minOriginal = std::min(stats.minOriginal, original);
      stats.maxOriginal = std::max(stats.maxOriginal, original);
      originalSum += original;
      
      // Target stats
      stats.minTarget = std::min(stats.minTarget, targetVal);
      stats.maxTarget = std::max(stats.maxTarget, targetVal);
      targetSum += targetVal;
      
      // Interpolated stats
      stats.minInterpolated = std::min(stats.minInterpolated, interpolated);
      stats.maxInterpolated = std::max(stats.maxInterpolated, interpolated);
      interpolatedSum += interpolated;
    }
    
    stats.avgOriginal = originalSum / 16.0f;
    stats.avgTarget = targetSum / 16.0f;
    stats.avgInterpolated = interpolatedSum / 16.0f;
    
    return stats;
  }
};

/**
 * MacroSpreadManager
 * 
 * Convenience wrapper for macro poly spread (same spread applied to all voices).
 * 
 * Usage:
 *   MacroSpreadManager mgr(patternEngine, 7);
 *   mgr.setSpread(lane, 0.5);  // Applied to all voices
 *   float val = mgr.getInterpolatedValue(voiceIdx, lane, step);
 */

struct MacroSpreadManager : public SpreadManager {
  MacroSpreadManager(PatternEngine* pe = nullptr, int nVoices = 7)
    : SpreadManager(pe, nVoices) {}
  
  // Override: set spread applies to all voices
  void setSpread(int lane, float value) {
    if (lane >= 0 && lane < 3) {
      float clamped = rack::math::clamp(value, -1.0f, 1.0f);
      for (int v = 0; v < numVoices; ++v) {
        SpreadManager::setSpread(v, lane, clamped);
      }
    }
  }
  
  float getSpread(int lane) const {
    return (numVoices > 0) ? SpreadManager::getSpread(0, lane) : 0.0f;
  }
};

}  // namespace redDot
