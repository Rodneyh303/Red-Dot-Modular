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
 *   - Target option 1: Average of all poly voices (creates cohesion)
 *   - Target option 2: Mono voice draw (creates mono-to-poly blending)
 * 
 * Interpolation formula:
 *   interpolated = original + (target - original) * spread
 *   
 * At spread=0.0:    Uses original poly draw (no interpolation)
 * At spread=0.5:    Halfway between original and target
 * At spread=1.0:    Uses target value completely
 * 
 * Implementation:
 *   - Macro poly: Same spread applied to all voices (global)
 *   - Per-voice poly: Different spread per voice (independent)
 *   - Mono: No spread (optional feature)
 * 
 * Usage:
 *   SpreadManager mgr(patternEngine);
 *   mgr.setInterpolationTarget(AVERAGE_POLY);  // or MONO_DRAW
 *   mgr.setSpread(voiceIdx, lane, 0.5);
 *   float interpolated = mgr.getInterpolatedValue(voiceIdx, lane, step);
 */

struct SpreadManager {
  enum InterpolationTarget {
    AVERAGE_POLY,    // Average of all poly voices
    MONO_DRAW        // Mono voice draw (for reference)
  };
  
  PatternEngine* patternEngine = nullptr;
  InterpolationTarget target = AVERAGE_POLY;
  int numVoices = 7;  // 7 for East, 8 for West, 1 for Mono
  
  // Spread values: [voice][lane]
  // For macro: all voices use same spread (but we store separately for flexibility)
  // For per-voice: each voice has own spread
  std::array<std::array<float, 3>, 8> spread = {};  // [8 voices][3 lanes]
  
  SpreadManager(PatternEngine* pe = nullptr, int nVoices = 7)
    : patternEngine(pe), numVoices(nVoices) {
    // Initialize spreads to 0 (no interpolation)
    for (int v = 0; v < 8; ++v) {
      for (int l = 0; l < 3; ++l) {
        spread[v][l] = 0.0f;
      }
    }
  }
  
  // ===== CONFIGURATION =====
  
  void setInterpolationTarget(InterpolationTarget t) {
    target = t;
  }
  
  void setSpread(int voiceIdx, int lane, float value) {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      spread[voiceIdx][lane] = rack::math::clamp(value, 0.0f, 1.0f);
    }
  }
  
  float getSpread(int voiceIdx, int lane) const {
    if (voiceIdx >= 0 && voiceIdx < numVoices && lane >= 0 && lane < 3) {
      return spread[voiceIdx][lane];
    }
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
   * Calculate average of all poly voice draws for a given lane/step.
   * 
   * This creates cohesion - all voices pulled toward their collective average.
   */
  float calculateAveragePolyValue(int lane, int step) const {
    if (!patternEngine || step < 0 || step >= 16) return 0.5f;
    
    float sum = 0.0f;
    int count = 0;
    
    for (int v = 0; v < numVoices; ++v) {
      float val = 0.0f;
      
      switch (lane) {
        case 0:  // REST
          val = patternEngine->polyRhythmRandom[v][step];
          break;
        case 1:  // MELODY
          val = patternEngine->polyMelodyRandom[v][step];
          break;
        case 2:  // OCTAVE
          val = patternEngine->polyOctaveRandom[v][step];
          break;
      }
      
      sum += val;
      count++;
    }
    
    return (count > 0) ? (sum / count) : 0.5f;
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
   */
  float interpolate(float original, float targetValue, float spreadAmount) const {
    if (spreadAmount <= 0.0f) return original;
    spreadAmount = rack::math::clamp(spreadAmount, 0.0f, 1.0f);
    return original + (targetValue - original) * spreadAmount;
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
      float clamped = rack::math::clamp(value, 0.0f, 1.0f);
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
