# Red Dot Modular - Spread Interpolation System

## Overview

The **spread parameter** enables flexible blending between independent voice randomness and shared sources. Two distinct modes are implemented as a context menu toggle.

---

## Musical Context

### Without Spread
Each voice generates its own independent random values. Result: **Maximum divergence**.

```
Voice 1:  Note=C  Variation=0.3  Legato=0.8  Rest=0.2
Voice 2:  Note=E  Variation=0.6  Legato=0.1  Rest=0.7
Voice 3:  Note=G  Variation=0.2  Legato=0.4  Rest=0.5
↓ Each voice is unique (no correlation)
```

### With Spread = 0.0 (Independent)
Each voice keeps its own random draw. **No blending applied**.

### With Spread > 0.0 (Blending)
Each voice blends toward a shared source. **Two options:**

1. **Poly Average Mode**: Blend toward the average of all poly voices
2. **Mono Source Mode**: Blend toward what the mono (main) sequencer drew

---

## Mode 1: Poly Average (Horizontal Coherence)

### Use Case
When you want **polyphonic voices to cohere internally** while maintaining some voice independence.

### Algorithm
```cpp
float monoRandom = monoSequencer->getRandomValue(dimension);  // What mono drew
float polyAverage = 0.0f;

// Calculate average of all connected voices
for (int v = 0; v < numConnectedVoices; v++) {
    polyAverage += polyVoices[v]->getRandomValue(dimension);
}
polyAverage /= numConnectedVoices;

// Blend: spread=0.0 → all poly, spread=1.0 → all average
for (int v = 0; v < numConnectedVoices; v++) {
    float original = polyVoices[v]->getRandomValue(dimension);
    float blended = lerp(original, polyAverage, spread);
    polyVoices[v]->setDNAWindow(dimension, blended);
}
```

### Sonic Result
- **Spread = 0.0**: Each voice fully random, independent melodies
- **Spread = 0.5**: Voices pull toward each other, some correlation
- **Spread = 1.0**: All voices locked to same random values (unison)

### Musical Application
- **Tight harmonies**: All voices follow similar melodic contour
- **Poly variation**: Same rhythm applied to all voices
- **Ensemble cohesion**: Voices stay "together" musically

### Example
```
Voice 1 random: 0.2  ────┐
Voice 2 random: 0.8  ────┼→ Average = 0.5
Voice 3 random: 0.6  ────┘

With spread=0.6:
Voice 1: lerp(0.2, 0.5, 0.6) = 0.38 (pulled toward average)
Voice 2: lerp(0.8, 0.5, 0.6) = 0.62 (pulled toward average)
Voice 3: lerp(0.6, 0.5, 0.6) = 0.54 (pulled toward average)

Result: Voices sync toward the middle (coherence)
```

---

## Mode 2: Mono Source (Vertical Coherence)

### Use Case
When you want **all poly voices to follow the main sequencer's choices**, creating a "leader-follower" relationship.

### Algorithm
```cpp
float monoRandom = monoSequencer->getRandomValue(dimension);  // Main sequencer's random

// All voices blend toward mono's choice
for (int v = 0; v < numConnectedVoices; v++) {
    float original = polyVoices[v]->getRandomValue(dimension);
    float blended = lerp(original, monoRandom, spread);
    polyVoices[v]->setDNAWindow(dimension, blended);
}
```

### Sonic Result
- **Spread = 0.0**: Each voice fully random, independent
- **Spread = 0.5**: Voices influenced by mono, but not locked
- **Spread = 1.0**: All voices identical to mono (full sync)

### Musical Application
- **Main voice dominance**: Poly voices follow main sequencer's variation
- **Harmonic anchoring**: Mono provides the "core" randomness
- **Structured variation**: Poly voices get variation from mono's stochastic engine
- **Call-and-response**: Mono leads, poly responds

### Example
```
Mono drawn for Rest DNA: 0.4

Voice 1 random: 0.1 ─┐
Voice 2 random: 0.7 ─┼→ Blend toward mono (0.4)
Voice 3 random: 0.6 ─┘

With spread=0.6:
Voice 1: lerp(0.1, 0.4, 0.6) = 0.28 (pulled toward mono)
Voice 2: lerp(0.7, 0.4, 0.6) = 0.52 (pulled toward mono)
Voice 3: lerp(0.6, 0.4, 0.6) = 0.48 (pulled toward mono)

Result: All voices move toward mono's value
```

---

## Which Makes More Sense Musically?

### Poly Average
✅ **Better for:**
- Creating a poly ensemble that breathes together
- Building harmonic sequences with variation
- Reducing randomness chaos (when you want ALL voices related)
- "Collective intelligence" effect (voices influence each other)

❌ **Less ideal for:**
- Creating distinct voice independence
- Following a main sequencer pattern
- Controlling multiple voices from mono

### Mono Source
✅ **Better for:**
- Mono-to-poly mapping (main sequencer drives poly)
- Creating a leader sequencer with harmonic followers
- Structured variation (mono sets the rules, poly follows)
- "One brain, many voices" effect

❌ **Less ideal for:**
- Preserving voice individuality
- Internal poly coherence without mono dominance

---

## Implementation: Context Menu Toggle

### C++ Structure

```cpp
// In Monsoon.hpp or appropriate engine header
enum class SpreadMode {
    POLY_AVERAGE = 0,    // Blend all voices toward their average
    MONO_SOURCE = 1      // Blend all voices toward mono's random
};

// Member variable
SpreadMode spreadMode = SpreadMode::POLY_AVERAGE;  // Default
```

### Context Menu Implementation

```cpp
// In MonsoonWidget or appropriate widget
void appendContextMenu(Menu* menu) override {
    menu->addChild(new MenuSeparator());
    
    menu->addChild(createIndexPtrSubmenuItem(
        "Spread Mode",
        {
            "Poly Average",  // Voices blend toward their average
            "Mono Source"    // Voices blend toward mono sequencer
        },
        [this]() {
            return (int)(module->spreadMode);
        },
        [this](int i) {
            module->spreadMode = (SpreadMode)i;
        }
    ));
}
```

### Process() Implementation

```cpp
// In Monsoon::process() or PatternEngine::process()
void applySpreadInterpolation(float spread, SpreadMode mode) {
    if (spread <= 0.001f) return;  // No spread, skip
    
    // Process each dimension (Rest, Melody, Octave)
    for (int dim = 0; dim < 3; dim++) {
        float targetSource;
        
        if (mode == SpreadMode::POLY_AVERAGE) {
            // Calculate average of all poly voices
            targetSource = 0.0f;
            for (int v = 0; v < 15; v++) {
                targetSource += polyRandomValue[v][dim];
            }
            targetSource /= 15.0f;
        } 
        else {  // MONO_SOURCE
            // Use mono sequencer's value for this dimension
            targetSource = monoRandomValue[dim];
        }
        
        // Blend each voice toward target
        for (int v = 0; v < 15; v++) {
            float original = polyRandomValue[v][dim];
            float blended = rack::simd::crossfade(
                original,
                targetSource,
                spread  // 0.0 = original, 1.0 = target
            );
            
            // Apply blended value
            sequencerEngine->setPoly DNA(v, dim, blended);
        }
    }
}
```

### Parameter Control

The spread amount itself is controlled by per-voice sliders or a global control:

```cpp
// Per dimension, per voice interpolation sliders
// Range: 0.0 to 1.0
// In widget:
addParam(createParamCentered<SliderVert>(
    mm2px(Vec(205.f, 60 + v*11.f)),  // Rest interp, voice v
    module,
    Monsoon::POLY_REST_INTERP + v
));

// In process():
float spreadAmount = params[POLY_REST_INTERP + v].getValue();  // 0.0 to 1.0
applySpreadInterpolation(spreadAmount, spreadMode);
```

---

## Parameter Naming & UI

### Slider Labels (Per Voice)

```
REST     MELODY     OCTAVE     INTERP (labels in columns)
   ▁▁▁      ▁▁▁        ▁▁▁       R M O  (interp sliders)
                                  ▁ ▁ ▁
```

### Context Menu Text
```
Right-click on Monsoon → "Spread Mode"
  ☑ Poly Average    (voices blend toward their average)
  ☐ Mono Source     (voices blend toward mono sequencer)
```

---

## Technical Considerations

### 1. Calculation Order
```
Spread calculation happens AFTER all random draws:

1. Mono sequencer generates: monoRandom[rest, melody, octave]
2. Each poly voice generates: polyRandom[v][rest, melody, octave]
3. Apply spread interpolation based on mode
4. Store blended values in DNA windows
5. Apply DNA windows to step values
```

### 2. Connected Voices
```cpp
// Spread only affects connected voices
int numConnected = inputs[POLY_GATE_IN].getChannels();

for (int v = 0; v < numConnected; v++) {
    // Apply spread interpolation
}
// Voices beyond numConnected are unaffected
```

### 3. Range Constraints
```cpp
// Spread parameter: 0.0 to 1.0
// 0.0 = No spread (off)
// 0.5 = 50% blend toward target
// 1.0 = 100% locked to target
```

### 4. CPU Efficiency
```cpp
// Only recalculate if:
// • Spread value changed
// • Mode changed
// • New voices connected

// Cache interpolation results if possible
// Avoid per-sample calculations if spread is static
```

---

## Musical Examples

### Example 1: Poly Harmony (Poly Average Mode)
```
Spread = 0.7, Mode = Poly Average

• Mono sequencer: plays random notes
• Each voice: gets random notes initially
• Spread effect: All 4 voices pull toward their average rhythm/note
• Result: 4-voice chord that moves together, slight variation per voice
```

### Example 2: Leader-Follower (Mono Source Mode)
```
Spread = 0.8, Mode = Mono Source

• Mono sequencer: draws Rest=0.3 (long notes)
• Voice 1: drawn 0.6, blends to 0.36 (closer to mono)
• Voice 2: drawn 0.1, blends to 0.18 (closer to mono)
• Voice 3: drawn 0.9, blends to 0.72 (closer to mono)
• Result: All voices adopt mono's "long note" tendency
```

### Example 3: Varied Unison (Mono Source, Lower Spread)
```
Spread = 0.3, Mode = Mono Source

• Mono sequencer: draws variation for melody DNA
• All voices: influenced by mono but retain 70% independence
• Result: Voices play similar shapes but with individual character
• Use case: Rich unison without lockstep repetition
```

---

## UI/UX Recommendations

### Visual Feedback
- **Slider color**: Match the DNA dimension (Rest=Red, Melody=Green, Octave=Blue)
- **Context menu**: Include tooltip describing both modes
- **Label clarity**: "Spread" column header + per-dimension labels (R, M, O)

### Control Layout
```
┌────────────────────────────────────┐
│ DNA CONTROLS                       │
├─ REST   MELODY   OCTAVE   SPREAD  │
│ [Knob] [Knob]   [Knob]   [Slider] │
│ [Knob] [Knob]   [Knob]   [Slider] │
│ (voice rows...)                    │
└────────────────────────────────────┘
```

### Context Menu Placement
```cpp
// Right-click anywhere on Monsoon module
→ Context Menu
  ├─ Font size: [selector]
  ├─ Theme: [selector]
  ├─ ─────────────────
  └─ Spread Mode: [Poly Average | Mono Source]
```

---

## Default Behavior

**Default Spread Mode**: `POLY_AVERAGE`
- More intuitive for users discovering the feature
- Creates pleasing harmonic coherence out of the box
- Less "controlling" than mono-source approach

**Default Spread Amount**: `0.0` (per voice)
- No spread by default (full independence)
- User explicitly enables via slider

---

## Testing Checklist

- [ ] Poly Average mode blends voices correctly
- [ ] Mono Source mode pulls voices toward mono
- [ ] Context menu toggle switches modes smoothly
- [ ] Works with 1, 4, 8, 16 connected voices
- [ ] Spread = 0.0 produces no audible change
- [ ] Spread = 1.0 produces full unison (mode-dependent)
- [ ] CPU load minimal (no real-time performance hit)
- [ ] Knob/slider positions match parameter values

---

## Future Enhancements

- [ ] Add fade/glide between modes (smooth transition)
- [ ] Implement "mode per-dimension" (different modes for Rest/Melody/Octave)
- [ ] Add probability-based spread (spread applies only sometimes)
- [ ] Create presets for common spread configurations
- [ ] Visualize spread effect in step display (show "pull" direction)

---

**Recommendation**: Implement **both modes** with context menu toggle. They serve complementary musical purposes and give users maximum creative flexibility.
