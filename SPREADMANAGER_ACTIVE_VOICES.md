# SpreadManager - Active Voice Averaging

## Overview

When using AVERAGE_POLY as the interpolation target, the spread manager calculates the average from **only the actively requested voices**, not all available voices.

---

## Active Voice Detection

### How It Works

SpreadManager requires a reference to **SequencerEngine** to determine how many voices are currently active:

```cpp
PolyVoiceSandsParameterManager mgr(patternEngine, sequencerEngine, 7);
//                                  pattern    sequencer     numVoices

mgr.spreadMgr.setSequencerEngine(sequencerEngine);  // Enable active voice tracking
```

The SequencerEngine tracks `polyphony` - the number of voices currently in use:

```cpp
sequencerEngine->polyphony;  // Returns current voice count (1-7 for East, 1-8 for West)
```

---

## Examples

### Straits East (7 Voices)

#### Scenario 1: User requests 4 voices

```
SequencerEngine.polyphony = 4
polyRhythmRandom[0][5] = 0.2  (voice 2)
polyRhythmRandom[1][5] = 0.8  (voice 3)
polyRhythmRandom[2][5] = 0.6  (voice 4)
polyRhythmRandom[3][5] = 0.4  (voice 5)
polyRhythmRandom[4][5] = 0.9  (voice 6 - NOT USED)
polyRhythmRandom[5][5] = 0.3  (voice 7 - NOT USED)
polyRhythmRandom[6][5] = 0.7  (voice 8 - NOT USED)

Average calculation:
  Average = (0.2 + 0.8 + 0.6 + 0.4) / 4 = 0.5
  (NOT including voices 6, 7, 8 because polyphony=4)

At spread=0.5:
  Voice 0 (0.2): 0.2 + (0.5 - 0.2) * 0.5 = 0.35
  Voice 1 (0.8): 0.8 + (0.5 - 0.8) * 0.5 = 0.65
  Voice 2 (0.6): 0.6 + (0.5 - 0.6) * 0.5 = 0.55
  Voice 3 (0.4): 0.4 + (0.5 - 0.4) * 0.5 = 0.45
  Voice 4 (0.9): NOT averaged (only 4 voices active)
  Voice 5 (0.3): NOT averaged (only 4 voices active)
  Voice 6 (0.7): NOT averaged (only 4 voices active)
```

#### Scenario 2: User requests 7 voices

```
SequencerEngine.polyphony = 7
polyRhythmRandom[0][5] = 0.2  (voice 2)
polyRhythmRandom[1][5] = 0.8  (voice 3)
polyRhythmRandom[2][5] = 0.6  (voice 4)
polyRhythmRandom[3][5] = 0.4  (voice 5)
polyRhythmRandom[4][5] = 0.9  (voice 6)
polyRhythmRandom[5][5] = 0.3  (voice 7)
polyRhythmRandom[6][5] = 0.7  (voice 8)

Average calculation:
  Average = (0.2 + 0.8 + 0.6 + 0.4 + 0.9 + 0.3 + 0.7) / 7 = 0.557
  (ALL 7 voices because polyphony=7)

At spread=0.5:
  Voice 0 (0.2): 0.2 + (0.557 - 0.2) * 0.5 = 0.379
  Voice 1 (0.8): 0.8 + (0.557 - 0.8) * 0.5 = 0.679
  Voice 2 (0.6): 0.6 + (0.557 - 0.6) * 0.5 = 0.579
  Voice 3 (0.4): 0.4 + (0.557 - 0.4) * 0.5 = 0.479
  Voice 4 (0.9): 0.9 + (0.557 - 0.9) * 0.5 = 0.729
  Voice 5 (0.3): 0.3 + (0.557 - 0.3) * 0.5 = 0.429
  Voice 6 (0.7): 0.7 + (0.557 - 0.7) * 0.5 = 0.629
```

### Straits West (8 Voices)

Same concept, but:
- Voice indices 0-7 map to Monsoon voices 9-16
- Maximum polyphony = 8
- Average includes up to 8 voices

---

## Implementation Details

### SpreadManager.getActiveVoiceCount()

```cpp
int getActiveVoiceCount() const {
  if (!sequencerEngine) return numVoices;  // Fallback: use all
  
  return std::min(sequencerEngine->polyphony, numVoices);
}
```

Returns the number of voices to include in the average.

### SpreadManager.calculateAveragePolyValue()

```cpp
float calculateAveragePolyValue(int lane, int step) const {
  int activeVoiceCount = getActiveVoiceCount();  // Get current polyphony
  
  // Sum only the active voices
  for (int v = 0; v < activeVoiceCount; ++v) {
    val = patternEngine->polyRhythmRandom[v][step];  // (or melody/octave)
    sum += val;
  }
  
  return sum / activeVoiceCount;
}
```

Only includes voices within the current polyphony setting.

---

## Integration with Widgets

### For StraitsEastSandsWidget

```cpp
struct StraitsEastSandsWidget : ModuleWidget {
  PolyVoiceSandsParameterManager* paramMgr;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) {
    // Create parameter manager
    // IMPORTANT: Pass SequencerEngine for active voice tracking
    auto monsoon = mod->rightExpander.module;
    auto seqEngine = &(dynamic_cast<Monsoon*>(monsoon)->sequencerEngine);
    
    paramMgr = new PolyVoiceSandsParameterManager(
      &seqEngine->patternEngine,
      seqEngine,
      7,  // 7 voices for East
      0   // Start at index 0 (voices 2-8)
    );
  }
  
  void step() override {
    if (paramMgr) {
      // Average will dynamically use only active voices
      paramMgr->syncPatternEngineToEditor(selectedVoice, 
                                          visualEditors[selectedVoice]->currentState);
    }
  }
};
```

### For MonsoonStraitSandsWidget (Macro)

```cpp
struct MonsoonStraitSandsWidget : ModuleWidget {
  PolySandsParameterManager* paramMgr;
  
  MonsoonStraitSandsWidget(MonsoonStraitSandsExpander* mod) {
    // Create parameter manager
    // IMPORTANT: Pass SequencerEngine for active voice tracking
    auto monsoon = mod->rightExpander.module;
    auto seqEngine = &(dynamic_cast<Monsoon*>(monsoon)->sequencerEngine);
    
    paramMgr = new PolySandsParameterManager(
      &seqEngine->patternEngine,
      seqEngine,
      mod,
      7  // 7 voices (all poly voices: 2-8)
    );
  }
  
  void step() override {
    if (paramMgr) {
      // Average will dynamically use only active voices
      // If polyphony=3: average of 3 voices
      // If polyphony=7: average of all 7 voices
      paramMgr->syncPatternEngineToEditor(visualEditor->currentState);
    }
  }
};
```

---

## Behavior Across Polyphony Changes

### Real-Time Polyphony Change

When user changes polyphony dynamically:

```
Time T0: polyphony=4, average calculated from 4 voices
  Average = (0.2 + 0.8 + 0.6 + 0.4) / 4 = 0.5
  Voice 0 at spread=0.5: 0.35

Time T1: User changes polyphony to 7
  Average recalculated from all 7 voices
  Average = (0.2 + 0.8 + 0.6 + 0.4 + 0.9 + 0.3 + 0.7) / 7 = 0.557
  Voice 0 at spread=0.5: 0.379 (changed!)

Voice 0's interpolated value smoothly adjusts to new average
No discontinuity, but values shift as polyphony changes
```

---

## Without SequencerEngine (Fallback)

If SequencerEngine is not provided:

```cpp
PolyVoiceSandsParameterManager mgr(patternEngine, nullptr, 7);
//                                  pattern       null   numVoices

// Falls back to using numVoices for average
Average = sum of all 7 voices / 7
```

This works but is less responsive to polyphony changes.

---

## Visual Editor Display

The visual editor shows the **interpolated values** which reflect the current average:

### At 4 voices:

```
Step 5:  ▄ ▆ ▆ ▅ (4 interpolated values)
         Voices 2-5 interpolate toward average of those 4
```

### At 7 voices:

```
Step 5:  ▄ ▆ ▆ ▅ ▇ ▄ ▆ (7 interpolated values)
         Voices 2-8 interpolate toward average of all 7
```

When user changes polyphony, the bars shift to reflect new averaging.

---

## Benefits

✅ **Dynamic averaging**: Reflects actual poly configuration  
✅ **Fair blending**: Only uses voices you're actually using  
✅ **Responsive**: Adjusts as polyphony changes  
✅ **Intuitive**: Spread behavior feels "organic"  
✅ **Non-destructive**: Can switch back to spread=0 anytime  

---

## Configuration Checklist

When setting up Straits East/West or Mono Strait Sands:

- [ ] Pass `sequencerEngine` to parameter manager
- [ ] Manager calls `setSequencerEngine(sequencerEngine)` internally
- [ ] SpreadManager accesses `sequencerEngine->polyphony`
- [ ] Average recalculates dynamically as polyphony changes
- [ ] Visual editor shows interpolated values reflecting current average
- [ ] Test with different polyphony settings (e.g., 4, 6, 7)
