# SpreadManager - Spread Interpolation Engine

## Overview

SpreadManager refactors spread control into a dedicated engine that handles interpolation between original probability draws and switchable targets.

**Core Concept:**
```
Spread interpolates between:
  Original: Individual poly voice random draw
  Target:   Switchable choice of:
    1. Average of all poly voices
    2. Mono voice draw (reference)
```

---

## Interpolation Formula

```
interpolated = original + (target - original) * spread

At spread = 0.0:    interpolated = original (no change)
At spread = 0.5:    interpolated = halfway between original and target
At spread = 1.0:    interpolated = target (completely)
```

---

## Configuration

### Interpolation Targets

**AVERAGE_POLY** (default)
- Target: Average of all poly voice draws for that parameter
- Effect: Creates cohesion - all voices pulled toward collective average
- Use case: Blend voices together while maintaining some variation

Example:
```
Original voice draws:   [0.2, 0.8, 0.6, 0.4, 0.9, 0.3, 0.7]
Average target:         0.583 (all voices pulled toward this)

At spread=0.0: Voice 0 plays 0.2
At spread=0.5: Voice 0 plays 0.29 (halfway to average)
At spread=1.0: Voice 0 plays 0.583 (average)
```

**MONO_DRAW**
- Target: Mono voice draw for same parameter
- Effect: Blends poly voices toward mono reference
- Use case: Cross-fade between mono and poly behavior

Example:
```
Mono draw:             0.5
Original voice draws:  [0.2, 0.8, 0.6, ...]

At spread=0.0: Voice 0 plays 0.2
At spread=0.5: Voice 0 plays 0.35 (halfway to mono)
At spread=1.0: Voice 0 plays 0.5 (mono)
```

### Spread Modes

**Macro Spread (Global)**
```cpp
MacroSpreadManager mgr(patternEngine, 7);

// Same spread applied to all voices
mgr.setSpread(lane, 0.5);  // Sets all 7 voices to spread=0.5

// Read back
float spread = mgr.getSpread(lane);  // Gets first voice's spread
```

**Per-Voice Spread (Individual)**
```cpp
SpreadManager mgr(patternEngine, 7);

// Each voice has independent spread
mgr.setSpread(voiceIdx, lane, 0.5);  // Voice 0, lane 0, spread=0.5
mgr.setSpread(1, lane, 0.8);         // Voice 1, lane 0, spread=0.8

// Different voices, different spreads
float v0 = mgr.getSpread(0, lane);  // Returns 0.5
float v1 = mgr.getSpread(1, lane);  // Returns 0.8
```

---

## Usage

### Basic Setup

```cpp
#include "managers/SpreadManager.hpp"

// Create manager
SpreadManager mgr(patternEngine, 7);

// Set target
mgr.setInterpolationTarget(SpreadManager::AVERAGE_POLY);

// Set spreads
mgr.setSpread(0, 0, 0.5);  // Voice 0, lane REST, spread=0.5
mgr.setSpread(0, 1, 0.3);  // Voice 0, lane MELODY, spread=0.3
mgr.setSpread(0, 2, 0.7);  // Voice 0, lane OCTAVE, spread=0.7

// Get interpolated value
float interpolated = mgr.getInterpolatedValue(voiceIdx, lane, step);
```

### With Visual Editor

```cpp
struct StraitsEastWidget : ModuleWidget {
  PolyVoiceSandsParameterManager* paramMgr;
  SandsVisualEditorV4* editors[7];
  
  void step() override {
    if (paramMgr) {
      // Update interpolation target from parameter
      bool useMono = module->params[TARGET_PARAM].getValue() > 0.5f;
      auto target = useMono ? 
        SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY;
      paramMgr->spreadMgr.setInterpolationTarget(target);
      
      // Update spreads from knobs
      for (int v = 0; v < 7; ++v) {
        for (int l = 0; l < 3; ++l) {
          int paramId = SPREAD_V0_R + v*3 + l;
          float spread = module->params[paramId].getValue();
          paramMgr->setSpread(v, l, spread);
        }
      }
      
      // Sync editor (shows interpolated values)
      paramMgr->syncPatternEngineToEditor(selectedVoice, editors[selectedVoice]->currentState);
    }
  }
};
```

### Macro Poly (Global Spread)

```cpp
struct MonsoonStraitSandsWidget : ModuleWidget {
  PolySandsParameterManager* paramMgr;
  
  void step() override {
    if (paramMgr) {
      // Set interpolation target
      auto target = module->params[USE_MONO_PARAM].getValue() > 0.5f ?
        SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY;
      paramMgr->spreadMgr.setInterpolationTarget(target);
      
      // Set spreads (same for all voices)
      paramMgr->spreadMgr.setSpread(0, 0, module->params[SPREAD_REST].getValue());
      paramMgr->spreadMgr.setSpread(0, 1, module->params[SPREAD_MELODY].getValue());
      paramMgr->spreadMgr.setSpread(0, 2, module->params[SPREAD_OCTAVE].getValue());
      
      // Sync (all voices now use same spread)
      paramMgr->syncPatternEngineToEditor(editor->currentState);
    }
  }
};
```

---

## Visual Editor Integration

The visual editor displays interpolated probabilities:

```cpp
// In SandsVisualEditorV4

void syncPatternEngineToEditor(const PolyVoiceSandsParameterManager& mgr,
                                int voiceIdx) {
  for (int lane = 0; lane < 3; ++lane) {
    for (int step = 0; step < 16; ++step) {
      // Show interpolated value
      float displayProb = mgr.getDisplayProbability(voiceIdx, lane, step);
      currentState.lanes[lane].probabilities[step] = displayProb;
    }
  }
}
```

At spread=0, editor shows original poly draws.
At spread>0, editor shows interpolated values (pulling toward target).

---

## Interpolation Targets Explained

### AVERAGE_POLY Example

Scenario: 7 poly voices for voice DNA

```
Voice 0: REST probability = 0.2
Voice 1: REST probability = 0.8
Voice 2: REST probability = 0.6
Voice 3: REST probability = 0.4
Voice 4: REST probability = 0.9
Voice 5: REST probability = 0.3
Voice 6: REST probability = 0.7

Average = (0.2 + 0.8 + 0.6 + 0.4 + 0.9 + 0.3 + 0.7) / 7 = 0.583
```

With spread controls:

```
Voice 0 (orig 0.2):
  spread=0.0 → 0.2     (no change)
  spread=0.5 → 0.39    (halfway to average)
  spread=1.0 → 0.583   (full average)

Voice 4 (orig 0.9):
  spread=0.0 → 0.9     (no change)
  spread=0.5 → 0.74    (halfway to average)
  spread=1.0 → 0.583   (full average)
```

Effect: Spread acts as a cohesion knob. Higher spread = more synchronized behavior.

### MONO_DRAW Example

Scenario: Cross-fading poly to mono

```
Mono REST draw:        0.5
Poly Voice 0 draw:     0.2

With spread=0.5:
  interpolated = 0.2 + (0.5 - 0.2) * 0.5 = 0.35

Result: Voice 0 interpolates between poly (0.2) and mono (0.5)
```

Effect: Blend knob between poly independence and mono consistency.

---

## API Reference

### SpreadManager

```cpp
// Configuration
void setInterpolationTarget(InterpolationTarget target);
void setSpread(int voiceIdx, int lane, float value);
float getSpread(int voiceIdx, int lane) const;

// Calculation
float getOriginalValue(int voiceIdx, int lane, int step) const;
float getInterpolationTarget(int voiceIdx, int lane, int step) const;
float interpolate(float original, float target, float spread) const;
float getInterpolatedValue(int voiceIdx, int lane, int step) const;

// Batch operations
std::array<float, 16> getInterpolatedLane(int voiceIdx, int lane) const;
InterpolatedVoice getInterpolatedVoice(int voiceIdx) const;

// Statistics
InterpolationStats getInterpolationStats(int voiceIdx, int lane) const;
```

### MacroSpreadManager

```cpp
// Override: applies to all voices
void setSpread(int lane, float value);
float getSpread(int lane) const;

// Inherited from SpreadManager
void setInterpolationTarget(InterpolationTarget target);
float getInterpolatedValue(int voiceIdx, int lane, int step) const;
```

### PolyVoiceSandsParameterManager

```cpp
// Spread control
void setSpread(int voiceIdx, int lane, float value);
float getSpread(int voiceIdx, int lane) const;
void setInterpolationTarget(SpreadManager::InterpolationTarget target);

// Syncing
void syncEditorToPatternEngine(int voiceIdx, const VoiceState& state);
void syncPatternEngineToEditor(int voiceIdx, VoiceState& state);

// Display
float getDisplayProbability(int voiceIdx, int lane, int step) const;
```

### PolySandsParameterManager

```cpp
// Spread control (via macro manager)
// setSpread(lane) applies to all voices

// Syncing
void syncEditorToPatternEngine(const VoiceState& state);
void syncPatternEngineToEditor(VoiceState& state);

// Display
float getDisplayProbability(int lane, int step) const;
```

---

## Implementation Checklist

- [ ] SpreadManager created and tested
- [ ] MacroSpreadManager (wrapper for global spread)
- [ ] PolyVoiceSandsParameterManager uses SpreadManager
- [ ] PolySandsParameterManager uses MacroSpreadManager
- [ ] Visual editor shows interpolated values
- [ ] Macro poly: 3 spread knobs (one per lane)
- [ ] Per-voice poly: 21 spread knobs (7 voices × 3 lanes)
- [ ] Target selector UI (AVERAGE_POLY vs MONO_DRAW)
- [ ] Integration with existing DNA/sands system
- [ ] Testing: compare spread=0 (original) vs spread>0 (interpolated)

---

## Performance Notes

- Interpolation is cheap (linear math, no loops)
- Target calculation happens once per step (not per-sample)
- Visual editor update: O(16) values per lane per voice
- DSP use: O(1) lookup per step per voice

---

## Future Extensions

1. **Smooth Interpolation:** Add easing curves instead of linear
2. **Blend Modes:** Multiply, cross-fade, mix strategies
3. **Per-Step Targets:** Different target for each step
4. **Automation:** Modulation inputs for spread/target selection
5. **Humanization:** Add dither or smoothing to interpolated values
